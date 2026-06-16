#include "exchange_order_book.h"

#include <iostream>

uint32_t ExchangeOrderBook::AddOrder(uint32_t user_ref_num,
                                     ClientSession* session,
                                     char side,
                                     uint32_t quantity,
                                     uint64_t price,
                                     char time_in_force,
                                     uint64_t order_ref_num,
                                     std::vector<Fill>* fills_out) {
  uint32_t remaining = quantity;

  if (side == 'B') {
    // Buy: match against asks starting at the lowest ask.
    for (auto it = asks_.begin(); it != asks_.end() && remaining > 0; ) {
      if (it->first > price) break;  // no ask is cheap enough

      auto& queue = it->second;
      while (!queue.empty() && remaining > 0) {
        RestingOrder& resting = queue.front();
        uint32_t fill_qty = std::min(remaining, resting.quantity);

        Fill fill;
        fill.match_number = next_match_number_++;
        fill.quantity     = fill_qty;
        fill.price        = resting.price;
        fill.resting      = resting;  // copy for ack routing
        fills_out->push_back(fill);

        remaining          -= fill_qty;
        resting.quantity   -= fill_qty;

        if (resting.quantity == 0) {
          location_.erase(resting.user_ref_num);
          queue.pop_front();
        }
      }

      if (queue.empty()) {
        it = asks_.erase(it);
      } else {
        ++it;
      }
    }
  } else {
    // Sell: match against bids starting at the highest bid.
    for (auto it = bids_.rbegin(); it != bids_.rend() && remaining > 0; ) {
      if (it->first < price) break;  // no bid is high enough

      auto& queue = it->second;
      while (!queue.empty() && remaining > 0) {
        RestingOrder& resting = queue.front();
        uint32_t fill_qty = std::min(remaining, resting.quantity);

        Fill fill;
        fill.match_number = next_match_number_++;
        fill.quantity     = fill_qty;
        fill.price        = resting.price;
        fill.resting      = resting;
        fills_out->push_back(fill);

        remaining          -= fill_qty;
        resting.quantity   -= fill_qty;

        if (resting.quantity == 0) {
          location_.erase(resting.user_ref_num);
          queue.pop_front();
        }
      }

      if (queue.empty()) {
        // erase via base iterator of reverse iterator
        it = decltype(it){bids_.erase(std::next(it).base())};
      } else {
        ++it;
      }
    }
  }

  // Add remainder to book if this is a day order and shares are left.
  if (remaining > 0 && time_in_force == '0') {
    RestingOrder resting{};
    resting.order_ref_num = order_ref_num;
    resting.user_ref_num  = user_ref_num;
    resting.session       = session;
    resting.quantity      = remaining;
    resting.price         = price;
    resting.side          = side;

    auto& book_side = (side == 'B') ? bids_ : asks_;
    book_side[price].push_back(resting);
    location_[user_ref_num] = {side, price};
  }

  return remaining;
}

uint32_t ExchangeOrderBook::CancelOrder(uint32_t user_ref_num,
                                        uint32_t new_quantity) {
  auto loc_it = location_.find(user_ref_num);
  if (loc_it == location_.end()) {
    std::cout << "[book] CancelOrder: user_ref_num " << user_ref_num
              << " not found\n";
    return 0;
  }

  const OrderLocation& loc = loc_it->second;
  auto& book_side = (loc.side == 'B') ? bids_ : asks_;
  auto level_it = book_side.find(loc.price);
  if (level_it == book_side.end()) return 0;

  auto& queue = level_it->second;
  for (auto it = queue.begin(); it != queue.end(); ++it) {
    if (it->user_ref_num != user_ref_num) continue;

    uint32_t removed = it->quantity - new_quantity;
    if (new_quantity == 0) {
      removed = it->quantity;
      queue.erase(it);
      if (queue.empty()) book_side.erase(level_it);
      location_.erase(loc_it);
    } else {
      it->quantity = new_quantity;
    }
    return removed;
  }
  return 0;
}
