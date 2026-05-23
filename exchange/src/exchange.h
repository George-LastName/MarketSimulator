#ifndef NS_EX_EXCHANGE_H_
#define NS_EX_EXCHANGE_H_

#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>

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

struct ItchConfig {
	char* multicast_group;
	uint16_t port;
	int address_family;
	int socket_type;
	int protocol;
};

struct OuchConfig {

};



class ItchServer{
private:
	struct {
		char* session[10];
		uint64_t sequence_number;
		uint16_t message_count;
	} PacketHeader_;

	// broadcaster
	int socket_;
	sockaddr_in destination_{};

public:
	ItchServer(/*const ItchConfig& config*/);
	~ItchServer();

	void Send(const void * message, size_t message_length);
	// stop
};

class OuchServer{
private:
	// server for incoming trades.
public:
	OuchServer(/*const OuchConfig& config*/){};
	// start
	// stop
};

class Exchange{
public:
	ItchServer itch_server_;
	OuchServer ouch_server_;
	// Exchange(const ItchConfig& itch_config, const OuchConfig& ouch_config)
	// 	: itch_server_(itch_config), ouch_server_(ouch_config) {}
	Exchange() : itch_server_(), ouch_server_() {};
};
#endif //NS_EX_CHANGE_H_
