#ifndef NS_EXCHANGE_OUCH_MESSAGES_H_
#define NS_EXCHANGE_OUCH_MESSAGES_H_

// OUCH 5.0 message structs (exchange / server perspective).
//
// All multi-byte numeric fields are big-endian.
// Alpha fields are left-justified, space-padded on the right.
// Price fields have an implied 4 decimal places.
// The Type byte at offset 0 is included in each struct for direct serialisation.

#include <cstdint>

// ── Inbound (Trader → Exchange) ───────────────────────────────────────────────

struct [[gnu::packed]] OuchEnterOrder {
  char     type;             // 'O'
  uint32_t user_ref_num;
  char     side;             // 'B' 'S' 'T' 'E'
  uint32_t quantity;
  char     symbol[8];
  uint64_t price;            // implied 4 decimal places, 8 bytes big-endian
  char     time_in_force;   // '0'=day '3'=IOC
  char     display;          // 'Y' 'N' 'A'
  char     capacity;         // 'A' 'P' 'R' 'O'
  char     iso_eligibility;  // 'Y' 'N'
  char     cross_type;       // 'N' 'O' 'C' etc.
  char     cl_ord_id[14];
  uint16_t appendage_length;
};

struct [[gnu::packed]] OuchCancelOrder {
  char     type;             // 'X'
  uint32_t user_ref_num;
  uint32_t quantity;         // 0 = cancel all
  uint16_t appendage_length;
};

// ── Outbound (Exchange → Trader) ──────────────────────────────────────────────

struct [[gnu::packed]] OuchSystemEvent {
  char     type;       // 'S'
  uint64_t timestamp;  // nanoseconds since midnight
  char     event_code;
};

struct [[gnu::packed]] OuchOrderAccepted {
  char     type;             // 'A'
  uint64_t timestamp;
  uint32_t user_ref_num;
  char     side;
  uint32_t quantity;
  char     symbol[8];
  uint64_t price;
  char     time_in_force;
  char     display;
  uint64_t order_ref_num;    // exchange-assigned
  char     capacity;
  char     iso_eligibility;
  char     cross_type;
  char     order_state;      // 'L'=live 'D'=dead
  char     cl_ord_id[14];
  uint16_t appendage_length;
};

struct [[gnu::packed]] OuchOrderExecuted {
  char     type;             // 'E'
  uint64_t timestamp;
  uint32_t user_ref_num;
  uint32_t executed_shares;
  uint64_t execution_price;
  char     liquidity_flag;
  uint64_t match_number;
};

struct [[gnu::packed]] OuchOrderCanceled {
  char     type;             // 'C'
  uint64_t timestamp;
  uint32_t user_ref_num;
  uint32_t decrement;
  char     reason;
  uint16_t appendage_length;
};

struct [[gnu::packed]] OuchRejected {
  char     type;             // 'J'
  uint64_t timestamp;
  uint32_t user_ref_num;
  char     reason;
};

#endif  // NS_EXCHANGE_OUCH_MESSAGES_H_
