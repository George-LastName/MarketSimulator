#include "src/trader.h"
#include "src/order_book.h"
#include "src/itch_message_types.h"
#include "src/ouch_client.h"
#include "src/ouch_messages.h"

#include <arpa/inet.h>
#include <sys/select.h>
#include <cstring>
#include <iostream>
#include <unordered_map>

// ── MoldUDP64 constants ───────────────────────────────────────────────────────

static constexpr size_t   kMoldHeaderSize = 20;
static constexpr uint16_t kBufSize        = 2048;

// ── ITCH dispatch ─────────────────────────────────────────────────────────────

static void DispatchItchMessage(
    const uint8_t* msg,
    uint16_t msg_len,
    std::unordered_map<uint16_t, OrderBook>& stock_books)
{
  if (msg_len < sizeof(ItchHeader)) return;

  const auto*    hdr    = reinterpret_cast<const ItchHeader*>(msg);
  const uint8_t* body   = msg + sizeof(ItchHeader);
  const uint16_t locate = hdr->GetLocate();

  auto get_book = [&]() -> OrderBook& {
    return stock_books[locate];
  };

  switch (hdr->type) {
    case 'R': {
      const auto* sd = reinterpret_cast<const StockDir*>(body);
      stock_books.emplace(locate, OrderBook{sd->GetStock()});
      break;
    }
    case 'H': {
      const auto* ta = reinterpret_cast<const StockTradingAction*>(body);
      get_book().SetState(ta->trading_state);
      break;
    }
    case 'A':
      get_book().Add(reinterpret_cast<const AddOrderNoMpid*>(body));
      break;
    case 'F':
      get_book().Add(reinterpret_cast<const AddOrderMpid*>(body));
      break;
    case 'E':
      get_book().Execute(reinterpret_cast<const OrderExecuted*>(body));
      break;
    case 'C':
      get_book().Execute(reinterpret_cast<const OrderExecutedWithPrice*>(body));
      break;
    case 'X':
      get_book().Cancel(reinterpret_cast<const OrderCancel*>(body));
      break;
    case 'D':
      get_book().Delete(reinterpret_cast<const OrderDelete*>(body));
      break;
    case 'U':
      get_book().Replace(reinterpret_cast<const OrderReplace*>(body));
      break;
    default:
      break;
  }
}

// ── ITCH packet handler ───────────────────────────────────────────────────────

static void HandleItchPacket(
    const uint8_t* buf,
    ssize_t received,
    uint64_t& expected_seq,
    std::unordered_map<uint16_t, OrderBook>& stock_books)
{
  if (static_cast<size_t>(received) < kMoldHeaderSize) return;

  uint64_t seq_num;
  uint16_t msg_count;
  std::memcpy(&seq_num,   buf + 10, 8);
  std::memcpy(&msg_count, buf + 18, 2);
  seq_num   = __builtin_bswap64(seq_num);
  msg_count = ntohs(msg_count);

  if (msg_count == 0) return;  // heartbeat

  if (msg_count == 0xFFFF) {
    std::cout << "[itch] End of session.\n";
    return;
  }

  if (seq_num != expected_seq) {
    std::cerr << "[itch] Gap: expected=" << expected_seq
              << " got=" << seq_num << "\n";
  }

  const uint8_t* ptr = buf + kMoldHeaderSize;
  const uint8_t* end = buf + received;

  for (uint16_t i = 0; i < msg_count && ptr + 2 <= end; ++i) {
    uint16_t msg_len = ntohs(*reinterpret_cast<const uint16_t*>(ptr));
    ptr += 2;
    if (ptr + msg_len > end) break;

    // Print the ITCH message type for visibility.
    if (msg_len >= 1) {
      char type = static_cast<char>(ptr[0]);
      std::cout << "[itch] msg type='" << type
                << "' seq=" << (seq_num + i) << "\n";
    }

    DispatchItchMessage(ptr, msg_len, stock_books);
    ptr += msg_len;
  }

  expected_seq = seq_num + msg_count;
}

// ── OUCH ack handler ─────────────────────────────────────────────────────────

static void HandleOuchPacket(OuchClient& ouch) {
  char    soup_type;
  uint8_t buf[kBufSize];
  int     len = ouch.Receive(&soup_type, buf, kBufSize);

  if (len < 0) {
    std::cerr << "[ouch] Receive error / disconnect\n";
    return;
  }
  if (soup_type != 'S') return;  // only care about sequenced data
  if (len < 1)          return;

  char ouch_type = static_cast<char>(buf[0]);

  switch (ouch_type) {
    case 'A': {
      if (len < static_cast<int>(sizeof(OuchOrderAccepted))) break;
      const auto* a = reinterpret_cast<const OuchOrderAccepted*>(buf);
      std::cout << "[ouch] OrderAccepted: user_ref=" << ntohl(a->user_ref_num)
                << " sym=" << std::string(a->symbol, 8)
                << " side=" << a->side
                << " qty=" << ntohl(a->quantity)
                << " state=" << a->order_state << "\n";
      break;
    }
    case 'E': {
      if (len < static_cast<int>(sizeof(OuchOrderExecuted))) break;
      const auto* e = reinterpret_cast<const OuchOrderExecuted*>(buf);
      std::cout << "[ouch] OrderExecuted: user_ref=" << ntohl(e->user_ref_num)
                << " shares=" << ntohl(e->executed_shares)
                << " price=" << __builtin_bswap64(e->execution_price)
                << " liquidity=" << e->liquidity_flag
                << " match=" << __builtin_bswap64(e->match_number) << "\n";
      break;
    }
    case 'C': {
      if (len < static_cast<int>(sizeof(OuchOrderCanceled))) break;
      const auto* c = reinterpret_cast<const OuchOrderCanceled*>(buf);
      std::cout << "[ouch] OrderCanceled: user_ref=" << ntohl(c->user_ref_num)
                << " removed=" << ntohl(c->decrement)
                << " reason=" << c->reason << "\n";
      break;
    }
    case 'J': {
      if (len < static_cast<int>(sizeof(OuchRejected))) break;
      const auto* j = reinterpret_cast<const OuchRejected*>(buf);
      std::cout << "[ouch] Rejected: user_ref=" << ntohl(j->user_ref_num)
                << " reason=" << j->reason << "\n";
      break;
    }
    default:
      std::cout << "[ouch] Unknown message type '" << ouch_type << "'\n";
      break;
  }
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
  ItchClient itch;

  std::cout << "[trader] Connecting to exchange OUCH on 127.0.0.1:9001...\n";
  OuchClient ouch("127.0.0.1", 9001, "TRADER01", "PASSWORD1");
  std::cout << "[trader] Connected. Session: \"" << ouch.session() << "\"\n";

  // ── Send test orders ───────────────────────────────────────────────────────
  //
  // Buy 100 AAPL at $150.0000 (price = 1500000, implied 4 decimal places).
  // This will rest on the exchange book.
  //
  // Then sell 100 AAPL at $149.0000 — crosses the resting buy, triggering a
  // fill at $150.0000 (the resting order's price).

  static constexpr uint64_t kBuyPrice  = 1'500'000;  // $150.00
  static constexpr uint64_t kSellPrice = 1'490'000;  // $149.00

  std::cout << "[trader] Sending Buy 100 AAPL @ $150.00 (user_ref=1)\n";
  ouch.EnterOrder(/*user_ref_num=*/1,
                  /*side=*/'B',
                  /*quantity=*/100,
                  /*symbol=*/"AAPL",
                  /*price=*/kBuyPrice);

  std::cout << "[trader] Sending Sell 100 AAPL @ $149.00 (user_ref=2)\n";
  ouch.EnterOrder(/*user_ref_num=*/2,
                  /*side=*/'S',
                  /*quantity=*/100,
                  /*symbol=*/"AAPL",
                  /*price=*/kSellPrice);

  // ── Multiplex ITCH UDP + OUCH TCP with select() ────────────────────────────

  std::unordered_map<uint16_t, OrderBook> stock_books;
  uint64_t expected_seq = 1;

  uint8_t buf[kBufSize];

  // Watch both sockets. Exit after 3 OUCH messages:
  //   OrderAccepted (buy) + OrderAccepted (sell) + OrderExecuted (sell/taker).
  // The buy-side execution ack would go to a different session in production;
  // since both orders share one session here the exchange skips the maker ack.
  int ouch_acks_remaining = 3;

  std::cout << "[trader] Listening for ITCH + OUCH messages "
            << "(expecting " << ouch_acks_remaining << " OUCH messages)...\n";

  while (ouch_acks_remaining > 0) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(itch.socket_, &read_fds);
    FD_SET(ouch.TcpFd(), &read_fds);
    int max_fd = std::max(itch.socket_, ouch.TcpFd()) + 1;

    struct timeval timeout{};
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;

    int ready = select(max_fd, &read_fds, nullptr, nullptr, &timeout);
    if (ready == 0) {
      std::cout << "[trader] Timeout waiting for messages.\n";
      break;
    }
    if (ready < 0) {
      std::cerr << "[trader] select() error\n";
      break;
    }

    if (FD_ISSET(itch.socket_, &read_fds)) {
      ssize_t received = itch.Receive(buf, kBufSize);
      if (received > 0) {
        HandleItchPacket(buf, received, expected_seq, stock_books);
      }
    }

    if (FD_ISSET(ouch.TcpFd(), &read_fds)) {
      HandleOuchPacket(ouch);
      --ouch_acks_remaining;
    }
  }

  // Print final order book state for any registered symbols.
  std::cout << "\n[trader] Final order book state:\n";
  for (auto& [locate, book] : stock_books) {
    if (!book.IsInitialised()) continue;
    auto snap = book.GetSnapshot(5);
    std::cout << "  " << book.GetName() << " (locate=" << locate << ")\n";
    for (size_t i = 0; i < snap.ask_prices.size(); ++i) {
      std::cout << "    ask " << snap.ask_prices[i]
                << " x " << snap.ask_shares[i] << "\n";
    }
    for (size_t i = 0; i < snap.bid_prices.size(); ++i) {
      std::cout << "    bid " << snap.bid_prices[i]
                << " x " << snap.bid_shares[i] << "\n";
    }
  }

  return 0;
}
