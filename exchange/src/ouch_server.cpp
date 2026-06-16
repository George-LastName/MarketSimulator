#include "ouch_server.h"

#include <arpa/inet.h>
#include <time.h>

#include <cstring>
#include <iostream>

#include "ouch_messages.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static void FillAlpha(char* dest, size_t dest_len, const char* src,
                      size_t src_len) {
  std::memset(dest, ' ', dest_len);
  std::memcpy(dest, src, std::min(src_len, dest_len));
}

// ITCH AddOrderNoMpid ('A') on-wire layout (packed, after ItchHeader).
// ItchHeader is 11 bytes (type + locate + tracknum + timestamp[6]).
struct [[gnu::packed]] ItchAddOrderBody {
  uint64_t order_reference_number;  // big-endian
  char     buy_sell_indicator;
  uint32_t shares;                  // big-endian
  char     stock[8];
  uint32_t price;                   // big-endian, 4-byte, implied 4 dec places
};

// ITCH OrderExecuted ('E') on-wire layout (after ItchHeader).
struct [[gnu::packed]] ItchOrderExecutedBody {
  uint64_t order_reference_number;  // big-endian
  uint32_t executed_shares;         // big-endian
  uint64_t match_number;            // big-endian
};

// Full ITCH message = ItchHeader + body.
struct [[gnu::packed]] ItchHeader {
  char     type;
  uint16_t locate;     // big-endian
  uint16_t track_num;  // big-endian, always 0 in our sim
  uint8_t  timestamp[6];  // 6-byte big-endian nanoseconds since midnight
};

static void WriteTimestamp(uint8_t* dest, uint64_t ns) {
  // 6-byte big-endian: take the low 48 bits of ns.
  uint64_t ns_be = __builtin_bswap64(ns);
  // ns_be is now big-endian in 8 bytes; the 6-byte field is the high 6 bytes
  // of the big-endian representation (i.e. bytes [2..7] of the 8-byte value).
  std::memcpy(dest, reinterpret_cast<uint8_t*>(&ns_be) + 2, 6);
}

// ── OuchServer ────────────────────────────────────────────────────────────────

OuchServer::OuchServer(uint16_t port, const std::string& session_id,
                       ItchServer* itch)
    : tcp_server_(port, session_id), itch_(itch) {}

uint64_t OuchServer::NowNs() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  // Nanoseconds since midnight: ts.tv_sec % 86400 * 1e9 + ts.tv_nsec
  uint64_t secs_today = static_cast<uint64_t>(ts.tv_sec) % 86400;
  return secs_today * 1'000'000'000ULL +
         static_cast<uint64_t>(ts.tv_nsec);
}

uint16_t OuchServer::LocateForSymbol(const char symbol[8]) {
  std::string key(symbol, 8);
  auto it = locate_map_.find(key);
  if (it != locate_map_.end()) return it->second;
  uint16_t code = next_locate_++;
  locate_map_[key] = code;
  return code;
}

void OuchServer::BroadcastItchAddOrder(uint16_t locate,
                                       uint64_t order_ref_num,
                                       char side,
                                       uint32_t shares,
                                       const char symbol[8],
                                       uint64_t ouch_price) {
  uint64_t now = NowNs();

  struct [[gnu::packed]] {
    ItchHeader        hdr;
    ItchAddOrderBody  body;
  } msg{};

  msg.hdr.type     = 'A';
  msg.hdr.locate   = htons(locate);
  msg.hdr.track_num = 0;
  WriteTimestamp(msg.hdr.timestamp, now);

  msg.body.order_reference_number = __builtin_bswap64(order_ref_num);
  msg.body.buy_sell_indicator     = side;
  msg.body.shares                 = htonl(shares);
  FillAlpha(msg.body.stock, 8, symbol, 8);
  // OUCH price is 8-byte; ITCH AddOrder price is 4-byte.
  // Both use implied 4 decimal places. Truncate — prices > $429496 won't fit,
  // which is fine for a simulator.
  msg.body.price = htonl(static_cast<uint32_t>(ouch_price));

  itch_->AppendMessage(&msg, sizeof(msg));
  itch_->Send();
}

void OuchServer::BroadcastItchOrderExecuted(uint16_t locate,
                                            uint64_t order_ref_num,
                                            uint32_t executed_shares,
                                            uint64_t match_number) {
  uint64_t now = NowNs();

  struct [[gnu::packed]] {
    ItchHeader           hdr;
    ItchOrderExecutedBody body;
  } msg{};

  msg.hdr.type      = 'E';
  msg.hdr.locate    = htons(locate);
  msg.hdr.track_num = 0;
  WriteTimestamp(msg.hdr.timestamp, now);

  msg.body.order_reference_number = __builtin_bswap64(order_ref_num);
  msg.body.executed_shares        = htonl(executed_shares);
  msg.body.match_number           = __builtin_bswap64(match_number);

  itch_->AppendMessage(&msg, sizeof(msg));
  itch_->Send();
}

void OuchServer::HandleEnterOrder(ClientSession* session,
                                  const uint8_t* payload,
                                  uint16_t payload_len) {
  if (payload_len < sizeof(OuchEnterOrder)) {
    std::cerr << "[ouch] EnterOrder too short\n";
    return;
  }
  const auto* req = reinterpret_cast<const OuchEnterOrder*>(payload);

  uint32_t user_ref_num = ntohl(req->user_ref_num);
  uint32_t quantity     = ntohl(req->quantity);
  uint64_t price        = __builtin_bswap64(req->price);
  char     side         = req->side;
  char     tif          = req->time_in_force;

  std::string sym_key(req->symbol, 8);
  auto& book = books_[sym_key];

  uint64_t order_ref_num = next_order_ref_num_++;
  uint16_t locate        = LocateForSymbol(req->symbol);
  uint64_t now           = NowNs();

  std::vector<Fill> fills;
  uint32_t remaining = book.AddOrder(user_ref_num, session, side, quantity,
                                     price, tif, order_ref_num, &fills);

  // Send OUCH Order Accepted back to the submitting trader.
  OuchOrderAccepted ack{};
  ack.type          = 'A';
  ack.timestamp     = __builtin_bswap64(now);
  ack.user_ref_num  = htonl(user_ref_num);
  ack.side          = side;
  ack.quantity      = htonl(quantity);
  FillAlpha(ack.symbol, 8, req->symbol, 8);
  ack.price             = req->price;  // echo back as-is (already big-endian)
  ack.time_in_force     = tif;
  ack.display           = req->display;
  ack.order_ref_num     = __builtin_bswap64(order_ref_num);
  ack.capacity          = req->capacity;
  ack.iso_eligibility   = req->iso_eligibility;
  ack.cross_type        = req->cross_type;
  ack.order_state       = (remaining > 0 || !fills.empty()) ? 'L' : 'D';
  FillAlpha(ack.cl_ord_id, 14, req->cl_ord_id, 14);
  ack.appendage_length  = htons(0);
  session->SendSequenced(&ack, sizeof(ack));

  // Broadcast ITCH AddOrder if the order rested on the book.
  if (remaining > 0 && tif == '0') {
    BroadcastItchAddOrder(locate, order_ref_num, side, remaining,
                          req->symbol, price);
  }

  // Process fills: send OUCH executions to both sides and broadcast ITCH.
  for (const Fill& fill : fills) {
    // OUCH Order Executed → incoming trader
    OuchOrderExecuted exec_in{};
    exec_in.type             = 'E';
    exec_in.timestamp        = __builtin_bswap64(now);
    exec_in.user_ref_num     = htonl(user_ref_num);
    exec_in.executed_shares  = htonl(fill.quantity);
    exec_in.execution_price  = __builtin_bswap64(fill.price);
    exec_in.liquidity_flag   = 'T';  // taker
    exec_in.match_number     = __builtin_bswap64(fill.match_number);
    session->SendSequenced(&exec_in, sizeof(exec_in));

    // OUCH Order Executed → resting trader (maker)
    if (fill.resting.session != nullptr &&
        fill.resting.session != session) {
      OuchOrderExecuted exec_rest{};
      exec_rest.type            = 'E';
      exec_rest.timestamp       = __builtin_bswap64(now);
      exec_rest.user_ref_num    = htonl(fill.resting.user_ref_num);
      exec_rest.executed_shares = htonl(fill.quantity);
      exec_rest.execution_price = __builtin_bswap64(fill.price);
      exec_rest.liquidity_flag  = 'M';  // maker
      exec_rest.match_number    = __builtin_bswap64(fill.match_number);
      fill.resting.session->SendSequenced(&exec_rest, sizeof(exec_rest));
    }

    // ITCH Order Executed broadcast
    BroadcastItchOrderExecuted(locate, fill.resting.order_ref_num,
                               fill.quantity, fill.match_number);
  }

  std::cout << "[ouch] EnterOrder user_ref=" << user_ref_num
            << " sym=" << sym_key
            << " side=" << side
            << " qty=" << quantity
            << " price=" << price
            << " fills=" << fills.size()
            << " resting=" << remaining << "\n";
}

void OuchServer::HandleCancelOrder(ClientSession* session,
                                   const uint8_t* payload,
                                   uint16_t payload_len) {
  if (payload_len < sizeof(OuchCancelOrder)) {
    std::cerr << "[ouch] CancelOrder too short\n";
    return;
  }
  const auto* req = reinterpret_cast<const OuchCancelOrder*>(payload);

  uint32_t user_ref_num = ntohl(req->user_ref_num);
  uint32_t new_qty      = ntohl(req->quantity);
  uint64_t now          = NowNs();

  // Find the book this order lives in.
  uint32_t removed = 0;
  for (auto& [sym, book] : books_) {
    removed = book.CancelOrder(user_ref_num, new_qty);
    if (removed > 0) break;
  }

  OuchOrderCanceled canceled{};
  canceled.type             = 'C';
  canceled.timestamp        = __builtin_bswap64(now);
  canceled.user_ref_num     = htonl(user_ref_num);
  canceled.decrement        = htonl(removed);
  canceled.reason           = 'U';  // user-requested
  canceled.appendage_length = htons(0);
  session->SendSequenced(&canceled, sizeof(canceled));

  std::cout << "[ouch] CancelOrder user_ref=" << user_ref_num
            << " removed=" << removed << "\n";
}

void OuchServer::HandleMessage(ClientSession* session,
                               char msg_type,
                               const uint8_t* payload,
                               uint16_t payload_len) {
  switch (msg_type) {
    case 'O':
      HandleEnterOrder(session, payload, payload_len);
      break;
    case 'X':
      HandleCancelOrder(session, payload, payload_len);
      break;
    default:
      std::cout << "[ouch] Unhandled message type '" << msg_type << "'\n";
      break;
  }
}

void OuchServer::AcceptAndServe() {
  ClientSession* session = tcp_server_.Accept();
  if (session == nullptr) return;

  static constexpr uint16_t kBufSize = 2048;
  uint8_t buf[kBufSize];

  while (true) {
    char msg_type;
    int len = session->ReceivePacket(&msg_type, buf, kBufSize);
    if (len < 0) {
      std::cout << "[ouch] Client disconnected\n";
      break;
    }
    if (msg_type == kSoupClientHeartbeat) continue;
    if (msg_type != kSoupUnsequenced) {
      std::cout << "[ouch] Unexpected SoupBinTCP type '" << msg_type << "'\n";
      continue;
    }
    if (len == 0) continue;

    // The first byte of an Unsequenced Data payload is the OUCH message type.
    char ouch_type = static_cast<char>(buf[0]);
    HandleMessage(session, ouch_type, buf, static_cast<uint16_t>(len));
  }

  delete session;
}
