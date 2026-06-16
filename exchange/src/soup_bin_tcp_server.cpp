#include "soup_bin_tcp_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

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

// ─── ClientSession ────────────────────────────────────────────────────────────

ClientSession::ClientSession(int fd, std::string session_id)
    : fd_(fd), session_id_(std::move(session_id)) {}

ClientSession::~ClientSession() {
  if (fd_ >= 0) close(fd_);
}

void ClientSession::SendPacket(char packet_type,
                               const void* payload,
                               uint16_t payload_len) {
  uint16_t packet_length = htons(static_cast<uint16_t>(1 + payload_len));
  uint8_t header[3];
  std::memcpy(header, &packet_length, 2);
  header[2] = static_cast<uint8_t>(packet_type);
  send(fd_, header, 3, payload_len > 0 ? MSG_MORE : 0);
  if (payload_len > 0) {
    send(fd_, payload, payload_len, 0);
  }
}

int ClientSession::ReceivePacket(char* packet_type_out,
                                 void* buf,
                                 uint16_t buf_len) {
  uint16_t packet_length_be;
  if (!ReadExact(fd_, &packet_length_be, 2)) return -1;
  uint16_t packet_length = ntohs(packet_length_be);
  if (packet_length == 0) return -1;

  char packet_type;
  if (!ReadExact(fd_, &packet_type, 1)) return -1;
  *packet_type_out = packet_type;

  uint16_t payload_len = packet_length - 1;
  if (payload_len == 0) return 0;

  uint16_t read_len = std::min(payload_len, buf_len);
  if (!ReadExact(fd_, buf, read_len)) return -1;

  // Drain excess bytes we cannot fit in buf.
  uint16_t drain = payload_len - read_len;
  if (drain > 0) {
    uint8_t discard[256];
    while (drain > 0) {
      uint16_t chunk = std::min(drain, static_cast<uint16_t>(sizeof(discard)));
      if (!ReadExact(fd_, discard, chunk)) return -1;
      drain -= chunk;
    }
  }

  return static_cast<int>(read_len);
}

void ClientSession::SendSequenced(const void* payload, uint16_t payload_len) {
  SendPacket(kSoupSequenced, payload, payload_len);
  ++next_seq_;
}

void ClientSession::SendHeartbeat() {
  SendPacket(kSoupServerHeartbeat, nullptr, 0);
}

void ClientSession::SendEndOfSession() {
  SendPacket(kSoupEndOfSession, nullptr, 0);
}

void ClientSession::SendLoginAccepted(const std::string& session_id,
                                      uint64_t next_seq) {
  struct [[gnu::packed]] {
    char session[10];
    char sequence_number[20];
  } payload{};
  std::memset(payload.session,         ' ', sizeof(payload.session));
  std::memset(payload.sequence_number, ' ', sizeof(payload.sequence_number));
  std::memcpy(payload.session, session_id.c_str(),
              std::min(session_id.size(), sizeof(payload.session)));
  // Write next_seq as right-aligned ASCII numeric into sequence_number field.
  std::string seq_str = std::to_string(next_seq);
  size_t offset = sizeof(payload.sequence_number) - seq_str.size();
  std::memcpy(payload.sequence_number + offset, seq_str.c_str(), seq_str.size());
  SendPacket(kSoupLoginAccepted, &payload, sizeof(payload));
}

// ─── SoupBinTcpServer ────────────────────────────────────────────────────────

SoupBinTcpServer::SoupBinTcpServer(uint16_t port,
                                   const std::string& session_id)
    : session_id_(session_id) {
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    throw std::runtime_error("SoupBinTcpServer: failed to create socket");
  }

  int reuse = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(listen_fd_);
    throw std::runtime_error("SoupBinTcpServer: bind failed on port " +
                             std::to_string(port));
  }

  if (listen(listen_fd_, /*backlog=*/5) < 0) {
    close(listen_fd_);
    throw std::runtime_error("SoupBinTcpServer: listen failed");
  }

  std::cout << "[SoupBinTCP] Listening on port " << port << "\n";
}

SoupBinTcpServer::~SoupBinTcpServer() {
  if (listen_fd_ >= 0) close(listen_fd_);
}

ClientSession* SoupBinTcpServer::Accept() {
  sockaddr_in client_addr{};
  socklen_t client_len = sizeof(client_addr);
  int client_fd = accept(listen_fd_,
                         reinterpret_cast<sockaddr*>(&client_addr),
                         &client_len);
  if (client_fd < 0) {
    std::cerr << "[SoupBinTCP] accept() failed\n";
    return nullptr;
  }

  char ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
  std::cout << "[SoupBinTCP] Connection from " << ip_str << "\n";

  auto* session = new ClientSession(client_fd, session_id_);

  // Perform login handshake.
  char type;
  uint8_t buf[sizeof(SoupLoginRequest)]{};
  int len = session->ReceivePacket(&type, buf, sizeof(buf));

  if (type != kSoupLoginRequest ||
      len < static_cast<int>(sizeof(SoupLoginRequest))) {
    std::cerr << "[SoupBinTCP] Expected Login Request, got '" << type << "'\n";
    delete session;
    return nullptr;
  }

  session->SendLoginAccepted(session_id_, /*next_seq=*/1);
  std::cout << "[SoupBinTCP] Login accepted for session \"" << session_id_
            << "\"\n";

  return session;
}
