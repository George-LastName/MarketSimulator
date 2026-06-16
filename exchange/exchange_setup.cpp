#include "src/exchange.h"
#include "src/ouch_server.h"

#include <iostream>

static constexpr uint16_t kOuchPort    = 9001;
static constexpr const char* kSession  = "SIMDAY001 ";  // 10 chars

int main() {
  Exchange ex;
  OuchServer ouch(kOuchPort, kSession, &ex.itch_server_);

  std::cout << "[exchange] Ready. ITCH multicast on 239.1.2.3:21001, "
            << "OUCH TCP on port " << kOuchPort << "\n";

  // Accept and serve traders one at a time. Extend to threading for multiple
  // concurrent traders.
  while (true) {
    ouch.AcceptAndServe();
  }

  return 0;
}
