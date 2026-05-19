#ifndef NS_EX_EXCHANGE_H_
#define NS_EX_EXCHANGE_H_

/* The exchange will have 3 purposes.
 * 1. Maintain Authority Order Book.
 * 2. Handle incoming orders.
 * 3. Broadcast changes to the order book (Impact of valid orders).
 *
 * To test this, we add one more purpose.
 * - Reading from a historical file.
 *   This involves maintaining the order book from the read messages like
 *   a trader would.
 *
 * This exchange will be mirroring the NASDAQ protocols of ITCH (MoldUDP64)
 * for outbound and OUCH (SoupbinTCP?) for inbound.
 */

#endif //NS_EX_CHANGE_H_
