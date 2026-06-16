#ifndef NS_TRADER_SOUP_BIN_TCP_CLIENT_H_
#define NS_TRADER_SOUP_BIN_TCP_CLIENT_H_

#include <cstdint>
#include <string>

// SoupBinTCP client (Trader side).
//
// Packet framing (big-endian):
//   [2] Packet Length  — covers Packet Type byte + Packet Data
//   [1] Packet Type
//   [N] Packet Data
//
// Relevant packet types:
//   'L' Login Request          (client → server)
//   'A' Login Accepted         (server → client)
//   'J' Login Rejected         (server → client)
//   'U' Unsequenced Data       (client → server)  wraps outbound OUCH
//   'S' Sequenced Data         (server → client)  wraps inbound  OUCH
//   'R' Client Heartbeat       (client → server)
//   'H' Server Heartbeat       (server → client)
//   'Z' End of Session         (server → client)

// Login Request payload (client → server), all ASCII space-padded.
struct [[gnu::packed]] SoupLoginRequest {
    char username[6];
    char password[10];
    char session[10];           // spaces = any session
    char sequence_number[20];   // ASCII numeric, spaces = next available
};

// Login Accepted payload (server → client).
struct [[gnu::packed]] SoupLoginAccepted {
    char session[10];
    char sequence_number[20];   // ASCII numeric: next seq the server will send
};

// Login Rejected payload (server → client).
struct [[gnu::packed]] SoupLoginRejected {
    char reject_reason_code;    // 'A'=Not Authorized, 'S'=Session not available
};

class SoupBinTcpClient {
 public:
  // Connects to host:port and performs the SoupBinTCP login handshake.
  // username and password are padded/truncated to the spec field widths.
  // Throws std::runtime_error on connection or login failure.
  SoupBinTcpClient(const std::string& host, uint16_t port,
                   const std::string& username, const std::string& password);
  ~SoupBinTcpClient();

  SoupBinTcpClient(const SoupBinTcpClient&)            = delete;
  SoupBinTcpClient& operator=(const SoupBinTcpClient&) = delete;

  // Sends an Unsequenced Data packet wrapping an outbound OUCH message.
  void SendUnsequenced(const void* payload, uint16_t payload_len);

  // Sends a Client Heartbeat ('R').
  void SendHeartbeat();

  // Blocking receive of one packet from the server.
  // Fills packet_type and copies up to buf_len bytes of payload into buf.
  // Returns payload length, or -1 on error / connection closed.
  int ReceivePacket(char* packet_type_out, void* buf, uint16_t buf_len);

  const std::string& session() const { return session_; }
  int fd() const { return socket_fd_; }

 private:
  void SendPacket(char packet_type, const void* payload, uint16_t payload_len);

  // Performs SoupBinTCP login handshake after TCP connect.
  void Login(const std::string& username, const std::string& password);

  int socket_fd_ = -1;
  std::string session_;
};

#endif  // NS_TRADER_SOUP_BIN_TCP_CLIENT_H_
