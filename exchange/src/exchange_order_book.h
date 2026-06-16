#ifndef NS_EXCHANGE_EXCHANGE_ORDER_BOOK_H_
#define NS_EXCHANGE_EXCHANGE_ORDER_BOOK_H_

#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include <vector>

// Forward declaration — the exchange order book hands fills back to the caller
// (OuchServer), which is responsible for sending OUCH acks and ITCH broadcasts.
class ClientSession;

struct RestingOrder {
  uint64_t       order_ref_num;  // exchange-assigned ITCH reference number
  uint32_t       user_ref_num;   // from OUCH Enter Order
  ClientSession* session;        // TCP session to send OUCH acks back to
  uint32_t       quantity;       // remaining shares
  uint64_t       price;          // implied 4 decimal places (OUCH 8-byte price)
  char           side;           // 'B' or 'S'
};

struct Fill {
  uint64_t match_number;
  uint32_t quantity;
  uint64_t price;              // price at which the fill occurred
  RestingOrder resting;        // copy of the resting order (for ack routing)
};

// Authoritative order book for one symbol on the exchange.
// Matches incoming orders against resting orders in price-time priority.
class ExchangeOrderBook {
 public:
  ExchangeOrderBook() = default;

  // Attempt to match an incoming order against resting orders.
  // Fills any matches into `fills_out`. If `quantity` has a remainder after
  // matching and `time_in_force` is '0' (day), the remainder is added to the
  // book and `order_ref_num_out` is set to the assigned reference number.
  // For IOC ('3'), the unmatched remainder is discarded.
  // Returns the number of shares that could not be filled (0 = fully filled).
  uint32_t AddOrder(uint32_t user_ref_num,
                    ClientSession* session,
                    char side,
                    uint32_t quantity,
                    uint64_t price,
                    char time_in_force,
                    uint64_t order_ref_num,
                    std::vector<Fill>* fills_out);

  // Cancel or reduce a resting order identified by user_ref_num.
  // new_quantity=0 cancels the entire remaining order.
  // Returns the number of shares actually removed, or 0 if not found.
  uint32_t CancelOrder(uint32_t user_ref_num, uint32_t new_quantity);

 private:
  // bids_: highest price first → iterate rbegin
  // asks_: lowest  price first → iterate begin
  std::map<uint64_t, std::deque<RestingOrder>> bids_;
  std::map<uint64_t, std::deque<RestingOrder>> asks_;

  // user_ref_num → (side, price) for O(1) cancel lookup
  struct OrderLocation {
    char     side;
    uint64_t price;
  };
  std::unordered_map<uint32_t, OrderLocation> location_;

  uint64_t next_match_number_ = 1;
};

#endif  // NS_EXCHANGE_EXCHANGE_ORDER_BOOK_H_
