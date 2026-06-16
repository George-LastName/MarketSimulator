#ifndef NS_TRADER_TRADER_H_
#define NS_TRADER_TRADER_H_

#include <sys/socket.h>
#include <netinet/in.h>
/* The Trader has 4 purposes.
 * 1. Listen to UDP stream
 * 2. Update Order Book with stream messages.
 * 3. Signal Generation on when & how to trade.
 * 4. Submitting Orders to the exchange.
 *
 * Will implement this to work with Nasdaq ITCH and OUCH,
 * but should aim for possible extensions to other types.
 */

class ItchClient{
public:
    int socket_;
    sockaddr_in destination_{};
    ItchClient();
    ~ItchClient();
    ssize_t Receive(void* buffer, size_t length);
};

class Signals{

};


#endif //NS_TRADER_TRADER_H_
