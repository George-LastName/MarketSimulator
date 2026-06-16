#ifndef NS_EXCHANGE_SOUP_BIN_TCP_SERVER_H_
#define NS_EXCHANGE_SOUP_BIN_TCP_SERVER_H_

#include <cstdint>
#include <functional>
#include <string>

// Packet type bytes used by SoupBinTCP (server perspective).
inline constexpr char kSoupLoginRequest    = 'L';
inline constexpr char kSoupLoginAccepted   = 'A';
inline constexpr char kSoupLoginRejected   = 'J';
inline constexpr char kSoupUnsequenced     = 'U';  // client → server (OUCH in)
inline constexpr char kSoupSequenced       = 'S';  // server → client (OUCH out)
inline constexpr char kSoupClientHeartbeat = 'R';
inline constexpr char kSoupServerHeartbeat = 'H';
inline constexpr char kSoupEndOfSession    = 'Z';

// Login Request payload (client → server).
struct [[gnu::packed]] SoupLoginRequest {
  char username[6];
  char password[10];
  char session[10];
  char sequence_number[20];
};

// Represents one connected trader session.
// SoupBinTcpServer owns the listening socket; each ClientSession wraps
// an accepted TCP connection and its SoupBinTCP state.
class ClientSession {
 public:
  explicit ClientSession(int fd, std::string session_id);
  ~ClientSession();

  ClientSession(const ClientSession&)            = delete;
  ClientSession& operator=(const ClientSession&) = delete;

  // Blocking receive of one SoupBinTCP packet.
  // Returns payload length, -1 on error/disconnect.
  int ReceivePacket(char* packet_type_out, void* buf, uint16_t buf_len);

  // Send a Sequenced Data packet ('S') wrapping an inbound OUCH message.
  void SendSequenced(const void* payload, uint16_t payload_len);

  // Send a Server Heartbeat ('H').
  void SendHeartbeat();

  // Send End of Session ('Z').
  void SendEndOfSession();

  // Send Login Accepted ('A'). Called by SoupBinTcpServer during handshake.
  void SendLoginAccepted(const std::string& session_id, uint64_t next_seq);

  const std::string& session_id() const { return session_id_; }
  int fd() const { return fd_; }

 private:
  void SendPacket(char packet_type, const void* payload, uint16_t payload_len);

  int fd_ = -1;
  std::string session_id_;
  uint64_t next_seq_ = 1;
};

// Listens for SoupBinTCP TCP connections on a given port.
// Call Accept() to block until a client connects and completes the login
// handshake, returning a ClientSession.
class SoupBinTcpServer {
 public:
  // Binds to port and starts listening. Throws std::runtime_error on failure.
  explicit SoupBinTcpServer(uint16_t port, const std::string& session_id);
  ~SoupBinTcpServer();

  SoupBinTcpServer(const SoupBinTcpServer&)            = delete;
  SoupBinTcpServer& operator=(const SoupBinTcpServer&) = delete;

  // Blocks until a client connects. Performs the login handshake and returns
  // the new ClientSession. Returns nullptr on accept error.
  ClientSession* Accept();

 private:
  int listen_fd_ = -1;
  std::string session_id_;
};

#endif  // NS_EXCHANGE_SOUP_BIN_TCP_SERVER_H_
