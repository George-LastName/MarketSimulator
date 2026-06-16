#include "soup_bin_tcp_client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

// Fills a fixed-width ASCII field with spaces, then copies src (truncated).
static void FillAsciiField(char* dest, size_t dest_len, const std::string& src) {
  std::memset(dest, ' ', dest_len);
  std::memcpy(dest, src.c_str(), std::min(src.size(), dest_len));
}

// Blocking read of exactly n bytes from fd. Returns false on EOF or error.
static bool ReadExact(int fd, void* buf, size_t n) {
  size_t remaining = n;
  uint8_t* ptr = static_cast<uint8_t*>(buf);
  while (remaining > 0) {
    ssize_t received = recv(fd, ptr, remaining, 0);
    if (received <= 0) return false;
    ptr += received;
    remaining -= static_cast<size_t>(received);
  }
  return true;
}

SoupBinTcpClient::SoupBinTcpClient(const std::string& host, uint16_t port,
                                   const std::string& username,
                                   const std::string& password) {
  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ < 0) {
    throw std::runtime_error("SoupBinTcpClient: failed to create socket");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(port);

  // Accept either an IP string or a hostname.
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    struct hostent* he = gethostbyname(host.c_str());
    if (he == nullptr) {
      close(socket_fd_);
      throw std::runtime_error("SoupBinTcpClient: could not resolve host: " + host);
    }
    std::memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));
  }

  if (connect(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(socket_fd_);
    throw std::runtime_error("SoupBinTcpClient: connect failed to " + host);
  }

  Login(username, password);
}

SoupBinTcpClient::~SoupBinTcpClient() {
  if (socket_fd_ >= 0) close(socket_fd_);
}

void SoupBinTcpClient::SendPacket(char packet_type,
                                  const void* payload,
                                  uint16_t payload_len) {
  // Packet Length = type byte (1) + payload.
  uint16_t packet_length = htons(static_cast<uint16_t>(1 + payload_len));

  // Write header + type in one syscall where possible.
  uint8_t header[3];
  std::memcpy(header, &packet_length, 2);
  header[2] = static_cast<uint8_t>(packet_type);
  send(socket_fd_, header, 3, MSG_MORE);

  if (payload_len > 0) {
    send(socket_fd_, payload, payload_len, 0);
  }
}

void SoupBinTcpClient::SendUnsequenced(const void* payload,
                                       uint16_t payload_len) {
  SendPacket('U', payload, payload_len);
}

void SoupBinTcpClient::SendHeartbeat() {
  SendPacket('R', nullptr, 0);
}

int SoupBinTcpClient::ReceivePacket(char* packet_type_out,
                                    void* buf,
                                    uint16_t buf_len) {
  uint16_t packet_length_be;
  if (!ReadExact(socket_fd_, &packet_length_be, 2)) return -1;
  uint16_t packet_length = ntohs(packet_length_be);
  if (packet_length == 0) return -1;

  char packet_type;
  if (!ReadExact(socket_fd_, &packet_type, 1)) return -1;
  *packet_type_out = packet_type;

  uint16_t payload_len = packet_length - 1;
  if (payload_len == 0) return 0;

  uint16_t read_len = std::min(payload_len, buf_len);
  if (!ReadExact(socket_fd_, buf, read_len)) return -1;

  // Drain any excess bytes we couldn't fit in buf.
  uint16_t drain = payload_len - read_len;
  if (drain > 0) {
    uint8_t discard[256];
    while (drain > 0) {
      uint16_t chunk = std::min(drain, static_cast<uint16_t>(sizeof(discard)));
      if (!ReadExact(socket_fd_, discard, chunk)) return -1;
      drain -= chunk;
    }
  }

  return static_cast<int>(read_len);
}

void SoupBinTcpClient::Login(const std::string& username,
                             const std::string& password) {
  SoupLoginRequest req{};
  FillAsciiField(req.username, sizeof(req.username), username);
  FillAsciiField(req.password, sizeof(req.password), password);
  // Request any session and next available sequence number (all spaces).
  std::memset(req.session,         ' ', sizeof(req.session));
  std::memset(req.sequence_number, ' ', sizeof(req.sequence_number));

  SendPacket('L', &req, sizeof(req));

  char type;
  uint8_t payload[64]{};
  int len = ReceivePacket(&type, payload, sizeof(payload));

  if (type == 'A' && len >= static_cast<int>(sizeof(SoupLoginAccepted))) {
    auto* accepted = reinterpret_cast<SoupLoginAccepted*>(payload);
    session_ = std::string(accepted->session, sizeof(accepted->session));
    std::cout << "[SoupBinTCP] Login accepted. Session: \""
              << session_ << "\"\n";
  } else if (type == 'J' && len >= 1) {
    auto* rejected = reinterpret_cast<SoupLoginRejected*>(payload);
    throw std::runtime_error(
        std::string("SoupBinTcpClient: login rejected, reason: ") +
        rejected->reject_reason_code);
  } else {
    throw std::runtime_error("SoupBinTcpClient: unexpected response to login");
  }
}
