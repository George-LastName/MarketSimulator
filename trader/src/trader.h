#ifndef NS_TRADER_TRADER_H_
#define NS_TRADER_TRADER_H_

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

};

class OrderBook{

};

class Signals{

};

class OuchClient{

};


#endif //NS_TRADER_TRADER_H_
