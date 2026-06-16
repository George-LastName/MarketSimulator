#ifndef NS_TRADER_OUCH_MESSAGES_H_
#define NS_TRADER_OUCH_MESSAGES_H_

// OUCH 5.0 message structs.
//
// All multi-byte numeric fields are big-endian.
// Alpha fields are left-justified, space-padded on the right.
// Price fields have an implied 4 decimal places (e.g. $1.0000 = 10000).
// The Type byte at offset 0 is NOT included in these structs — it is the
// SoupBinTCP payload type byte written separately by OuchClient.

#include <cstdint>

// ── Inbound (Trader → Exchange) ───────────────────────────────────────────────

// Type 'O' — Enter Order (offset 0 is the 'O' type byte, included here for
// direct binary serialisation as a complete OUCH message).
struct [[gnu::packed]] OuchEnterOrder {
  char     type;           // 'O'
  uint32_t user_ref_num;   // unsigned, strictly increasing per account per day
  char     side;           // 'B'=buy 'S'=sell 'T'=sell short 'E'=sell short exempt
  uint32_t quantity;       // shares, 1–999999
  char     symbol[8];      // left-justified, space-padded
  uint64_t price;          // implied 4 decimal places
  char     time_in_force;  // '0'=day '3'=IOC '5'=GTX '6'=GTT 'E'=after hours
  char     display;        // 'Y'=visible 'N'=hidden 'A'=attributable
  char     capacity;       // 'A'=agency 'P'=principal 'R'=riskless 'O'=other
  char     iso_eligibility;// 'Y'=eligible 'N'=not eligible
  char     cross_type;     // 'N'=continuous 'O'=open 'C'=close etc.
  char     cl_ord_id[14];  // customer order id, left-justified space-padded
  uint16_t appendage_length; // 0 = no optional appendage
  // No optional appendage in the base implementation.
};

// Type 'X' — Cancel Order Request
struct [[gnu::packed]] OuchCancelOrder {
  char     type;          // 'X'
  uint32_t user_ref_num;  // original UserRefNum from Enter Order
  uint32_t quantity;      // new intended size; 0 = cancel all
  uint16_t appendage_length; // 0
};

// ── Outbound (Exchange → Trader) ──────────────────────────────────────────────

// Type 'S' — System Event
struct [[gnu::packed]] OuchSystemEvent {
  char     type;       // 'S'
  uint64_t timestamp;  // nanoseconds since midnight
  char     event_code; // 'S'=start of day 'E'=end of day
};

// Type 'A' — Order Accepted
struct [[gnu::packed]] OuchOrderAccepted {
  char     type;                // 'A'
  uint64_t timestamp;           // nanoseconds since midnight
  uint32_t user_ref_num;
  char     side;
  uint32_t quantity;
  char     symbol[8];
  uint64_t price;
  char     time_in_force;
  char     display;
  uint64_t order_ref_num;       // exchange-assigned, day-unique
  char     capacity;
  char     iso_eligibility;
  char     cross_type;
  char     order_state;         // 'L'=live 'D'=dead (accepted then auto-canceled)
  char     cl_ord_id[14];
  uint16_t appendage_length;    // 0
};

// Type 'E' — Order Executed
struct [[gnu::packed]] OuchOrderExecuted {
  char     type;           // 'E'
  uint64_t timestamp;
  uint32_t user_ref_num;
  uint32_t executed_shares;
  uint64_t execution_price; // implied 4 decimal places
  char     liquidity_flag;  // see Appendix D of OUCH spec
  uint64_t match_number;    // exchange-assigned match id
};

// Type 'C' — Order Canceled
struct [[gnu::packed]] OuchOrderCanceled {
  char     type;            // 'C'
  uint64_t timestamp;
  uint32_t user_ref_num;
  uint32_t decrement;       // shares removed from the order
  char     reason;          // see Appendix B of OUCH spec
  uint16_t appendage_length; // 0
};

// Type 'J' — Rejected
struct [[gnu::packed]] OuchRejected {
  char     type;           // 'J'
  uint64_t timestamp;
  uint32_t user_ref_num;
  char     reason;         // see Appendix C of OUCH spec
};

#endif  // NS_TRADER_OUCH_MESSAGES_H_
