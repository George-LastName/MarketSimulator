#include "ouch_client.h"

#include <arpa/inet.h>

#include <cstring>

// Fills a fixed-width alpha field: left-justified, space-padded on the right.
static void FillAlpha(char* dest, size_t dest_len, const std::string& src) {
  std::memset(dest, ' ', dest_len);
  std::memcpy(dest, src.c_str(), std::min(src.size(), dest_len));
}

OuchClient::OuchClient(const std::string& host, uint16_t port,
                       const std::string& username,
                       const std::string& password)
    : tcp_(host, port, username, password) {}

void OuchClient::EnterOrder(uint32_t user_ref_num,
                            char side,
                            uint32_t quantity,
                            const std::string& symbol,
                            uint64_t price,
                            char time_in_force,
                            char display,
                            char capacity,
                            char iso_eligible,
                            char cross_type,
                            const std::string& cl_ord_id) {
  OuchEnterOrder msg{};
  msg.type             = 'O';
  msg.user_ref_num     = htonl(user_ref_num);
  msg.side             = side;
  msg.quantity         = htonl(quantity);
  FillAlpha(msg.symbol, sizeof(msg.symbol), symbol);
  // Price is 8-byte big-endian (64-bit).
  msg.price            = __builtin_bswap64(price);
  msg.time_in_force    = time_in_force;
  msg.display          = display;
  msg.capacity         = capacity;
  msg.iso_eligibility  = iso_eligible;
  msg.cross_type       = cross_type;
  FillAlpha(msg.cl_ord_id, sizeof(msg.cl_ord_id), cl_ord_id);
  msg.appendage_length = htons(0);

  tcp_.SendUnsequenced(&msg, sizeof(msg));
}

void OuchClient::CancelOrder(uint32_t user_ref_num, uint32_t quantity) {
  OuchCancelOrder msg{};
  msg.type             = 'X';
  msg.user_ref_num     = htonl(user_ref_num);
  msg.quantity         = htonl(quantity);
  msg.appendage_length = htons(0);

  tcp_.SendUnsequenced(&msg, sizeof(msg));
}

int OuchClient::Receive(char* packet_type_out, void* buf, uint16_t buf_len) {
  return tcp_.ReceivePacket(packet_type_out, buf, buf_len);
}
