#include "order_book.h"

#include <arpa/inet.h>
#include <iostream>
#include <cstdlib>

OrderBook::OrderBook(std::string_view stock)
    : stock_name_(stock), state_(kHalt) {}

void OrderBook::SetState(char new_state) {
    switch (new_state) {
        case 'H': state_ = kHalt;      break;
        case 'P': state_ = kPaused;    break;
        case 'Q': state_ = kQuotation; break;
        case 'T': state_ = kTrading;   break;
        default:
            std::cout << "Unknown trading state for " << stock_name_
                      << ": " << new_state << "\n";
            std::exit(1);
    }
}

template<typename T>
void OrderBook::Add(const T* order) {
    Order new_order;
    if constexpr (std::is_same_v<T, AddOrderMpid>) {
        new_order.mpid = std::string(order->attribution, 4);
    }
    new_order.indicator = order->buy_sell_indicator;
    new_order.shares    = ntohl(order->shares);
    new_order.price     = ntohl(order->price);

    if (new_order.indicator != 'B' && new_order.indicator != 'S') {
        std::cout << "Invalid side for " << stock_name_
                  << ": |" << new_order.indicator << "|\n";
        std::exit(1);
    }

    orders_.insert({order->order_reference_number, new_order});
    auto& side = (new_order.indicator == 'B') ? bids_ : asks_;
    side[new_order.price] += new_order.shares;
}

template void OrderBook::Add(const AddOrderNoMpid*);
template void OrderBook::Add(const AddOrderMpid*);

BookSnapshot OrderBook::GetSnapshot(size_t top_n) const {
    BookSnapshot snap;
    size_t count = 0;
    for (auto it = bids_.rbegin(); it != bids_.rend() && count < top_n; ++it, ++count) {
        snap.bid_prices.push_back(it->first);
        snap.bid_shares.push_back(it->second);
    }
    count = 0;
    for (auto it = asks_.begin(); it != asks_.end() && count < top_n; ++it, ++count) {
        snap.ask_prices.push_back(it->first);
        snap.ask_shares.push_back(it->second);
    }
    return snap;
}

void OrderBook::Execute(const OrderExecuted* order) {
    auto it = orders_.find(order->order_reference_number);
    if (it == orders_.end()) {
        std::cout << "Execute: order ref not found: "
                  << order->order_reference_number << "\n";
        return;
    }
    Order& o = it->second;
    uint32_t exec_shares = ntohl(order->executed_shares);
    auto& side = (o.indicator == 'B') ? bids_ : asks_;
    side[o.price] -= exec_shares;
    if (side[o.price] == 0) side.erase(o.price);
    o.shares -= exec_shares;
    if (o.shares == 0) orders_.erase(it);
}

void OrderBook::Execute(const OrderExecutedWithPrice* order) {
    auto it = orders_.find(order->order_reference_number);
    if (it == orders_.end()) {
        std::cout << "ExecuteWithPrice: order ref not found: "
                  << order->order_reference_number << "\n";
        return;
    }
    Order& o = it->second;
    uint32_t exec_shares = ntohl(order->executed_shares);
    auto& side = (o.indicator == 'B') ? bids_ : asks_;
    side[o.price] -= exec_shares;
    if (side[o.price] == 0) side.erase(o.price);
    o.shares -= exec_shares;
    if (o.shares == 0) orders_.erase(it);
}

void OrderBook::Cancel(const OrderCancel* order) {
    auto it = orders_.find(order->order_reference_number);
    if (it == orders_.end()) {
        std::cout << "Cancel: order ref not found: "
                  << order->order_reference_number << "\n";
        return;
    }
    Order& o = it->second;
    uint32_t canceled_shares = ntohl(order->canceled_shares);
    auto& side = (o.indicator == 'B') ? bids_ : asks_;
    side[o.price] -= canceled_shares;
    if (side[o.price] == 0) side.erase(o.price);
    o.shares -= canceled_shares;
    if (o.shares == 0) orders_.erase(it);
}

void OrderBook::Delete(const OrderDelete* order) {
    auto it = orders_.find(order->order_reference_number);
    if (it == orders_.end()) {
        std::cout << "Delete: order ref not found: "
                  << order->order_reference_number << "\n";
        return;
    }
    Order& o = it->second;
    auto& side = (o.indicator == 'B') ? bids_ : asks_;
    side[o.price] -= o.shares;
    if (side[o.price] == 0) side.erase(o.price);
    orders_.erase(it);
}

void OrderBook::Replace(const OrderReplace* order) {
    auto it = orders_.find(order->original_order_reference_number);
    if (it == orders_.end()) {
        std::cout << "Replace: original order ref not found: "
                  << order->original_order_reference_number << "\n";
        return;
    }
    Order& old = it->second;
    auto& side = (old.indicator == 'B') ? bids_ : asks_;
    side[old.price] -= old.shares;
    if (side[old.price] == 0) side.erase(old.price);

    Order new_order;
    new_order.indicator = old.indicator;
    new_order.shares    = ntohl(order->shares);
    new_order.price     = ntohl(order->price);
    new_order.mpid      = old.mpid;

    orders_.erase(it);
    orders_.insert({order->new_order_reference_number, new_order});
    side[new_order.price] += new_order.shares;
}
