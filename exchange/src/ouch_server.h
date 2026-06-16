#ifndef NS_EXCHANGE_OUCH_SERVER_H_
#define NS_EXCHANGE_OUCH_SERVER_H_

#include <cstdint>
#include <string>
#include <unordered_map>

#include "exchange_order_book.h"
#include "exchange.h"
#include "soup_bin_tcp_server.h"

// OuchServer accepts SoupBinTCP connections from traders, processes OUCH
// inbound messages, routes fills via OUCH outbound messages, and instructs
// the ItchServer to broadcast ITCH market data for each book change.
class OuchServer {
 public:
  // port:       TCP port to listen on for trader connections
  // session_id: 10-char session identifier shared with SoupBinTcpServer
  // itch:       ItchServer reference for broadcasting market data
  OuchServer(uint16_t port, const std::string& session_id,
             ItchServer* itch);

  OuchServer(const OuchServer&)            = delete;
  OuchServer& operator=(const OuchServer&) = delete;

  // Block waiting for one trader to connect, then process their messages until
  // disconnect. Intended to be called in a loop to handle multiple traders.
  void AcceptAndServe();

 private:
  // Dispatches one OUCH inbound message from a connected ClientSession.
  void HandleMessage(ClientSession* session,
                     char msg_type,
                     const uint8_t* payload,
                     uint16_t payload_len);

  void HandleEnterOrder(ClientSession* session, const uint8_t* payload,
                        uint16_t payload_len);
  void HandleCancelOrder(ClientSession* session, const uint8_t* payload,
                         uint16_t payload_len);

  // Broadcasts an ITCH AddOrderNoMpid ('A') message for a newly resting order.
  void BroadcastItchAddOrder(uint16_t locate, uint64_t order_ref_num,
                             char side, uint32_t shares,
                             const char symbol[8], uint64_t ouch_price);

  // Broadcasts an ITCH OrderExecuted ('E') message for a fill.
  void BroadcastItchOrderExecuted(uint16_t locate,
                                  uint64_t order_ref_num,
                                  uint32_t executed_shares,
                                  uint64_t match_number);

  // Returns the Stock Locate code for a symbol, assigning one if new.
  uint16_t LocateForSymbol(const char symbol[8]);

  // Returns nanoseconds since midnight (wall clock, not ITCH timestamp).
  static uint64_t NowNs();

  SoupBinTcpServer tcp_server_;
  ItchServer*      itch_;

  // symbol (8 chars, space-padded) → order book
  std::unordered_map<std::string, ExchangeOrderBook> books_;

  // symbol → Stock Locate code
  std::unordered_map<std::string, uint16_t> locate_map_;
  uint16_t next_locate_ = 1;

  // Monotonically increasing exchange order reference number (for ITCH).
  uint64_t next_order_ref_num_ = 1;

  // Monotonically increasing UserRefNum tracker per session (for validation).
  uint64_t last_user_ref_num_ = 0;
};

#endif  // NS_EXCHANGE_OUCH_SERVER_H_
