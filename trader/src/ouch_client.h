#ifndef NS_TRADER_OUCH_CLIENT_H_
#define NS_TRADER_OUCH_CLIENT_H_

#include <cstdint>
#include <string>

#include "ouch_messages.h"
#include "soup_bin_tcp_client.h"

// OuchClient wraps SoupBinTcpClient and provides typed send/receive methods
// for OUCH 5.0 messages (Trader → Exchange order entry channel).
class OuchClient {
 public:
  // Connects to host:port and completes the SoupBinTCP login handshake.
  // username and password are passed through to SoupBinTcpClient.
  OuchClient(const std::string& host, uint16_t port,
             const std::string& username, const std::string& password);

  OuchClient(const OuchClient&)            = delete;
  OuchClient& operator=(const OuchClient&) = delete;

  // Sends an Enter Order ('O') message.
  // symbol must be 1–8 characters (will be space-padded to 8).
  // cl_ord_id must be 1–14 characters (will be space-padded to 14).
  // price has implied 4 decimal places (e.g. $1.50 → 15000).
  void EnterOrder(uint32_t user_ref_num,
                  char side,
                  uint32_t quantity,
                  const std::string& symbol,
                  uint64_t price,
                  char time_in_force = '0',  // day order
                  char display       = 'Y',  // visible
                  char capacity      = 'A',  // agency
                  char iso_eligible  = 'N',
                  char cross_type    = 'N',
                  const std::string& cl_ord_id = "");

  // Sends a Cancel Order ('X') message. quantity=0 cancels the full order.
  void CancelOrder(uint32_t user_ref_num, uint32_t quantity = 0);

  // Blocking receive of one OUCH outbound message from the exchange.
  // packet_type_out receives the OUCH message type byte ('A', 'E', 'C', 'J'…).
  // Returns payload length written to buf, or -1 on error.
  int Receive(char* packet_type_out, void* buf, uint16_t buf_len);

  const std::string& session() const { return tcp_.session(); }
  int TcpFd() const { return tcp_.fd(); }

 private:
  SoupBinTcpClient tcp_;
};

#endif  // NS_TRADER_OUCH_CLIENT_H_
