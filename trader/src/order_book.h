#ifndef NS_TRADER_ORDER_BOOK_H_
#define NS_TRADER_ORDER_BOOK_H_

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "itch_message_types.h"

struct BookSnapshot {
    std::vector<uint32_t> bid_prices;
    std::vector<uint32_t> bid_shares;
    std::vector<uint32_t> ask_prices;
    std::vector<uint32_t> ask_shares;
};

enum TradingState {
    kHalt,
    kPaused,
    kQuotation,
    kTrading
};

struct Order {
    char indicator; // 'B' or 'S'
    uint32_t shares;
    uint32_t price;
    std::string mpid;
};

class OrderBook {
public:
    OrderBook() = default;
    explicit OrderBook(std::string_view stock);

    const std::string& GetName()  const { return stock_name_; }
    bool IsInitialised()          const { return !stock_name_.empty(); }
    TradingState GetState()       const { return state_; }
    void SetState(char new_state);

    BookSnapshot GetSnapshot(size_t top_n) const;

    template<typename T>
    void Add(const T* order);

    void Execute(const OrderExecuted* order);
    void Execute(const OrderExecutedWithPrice* order);
    void Cancel(const OrderCancel* order);
    void Delete(const OrderDelete* order);
    void Replace(const OrderReplace* order);

private:
    std::string stock_name_;
    TradingState state_ = kHalt;
    std::map<uint32_t, uint32_t> bids_; // price → total shares
    std::map<uint32_t, uint32_t> asks_;
    std::unordered_map<uint64_t, Order> orders_; // order ref → order
};

#endif // NS_TRADER_ORDER_BOOK_H_
