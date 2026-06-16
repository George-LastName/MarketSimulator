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
	char* multicast_ipv4;
	uint16_t port;
	int protocol;
};

struct OuchConfig {

};



class ItchServer{
private:
	// Max UDP 1500 (IPv4 + Ethernet) - 20 IPv4 Header - 8 UDP Header = 1472
	static constexpr size_t MAX_MESSAGE_SIZE = 1472;
	char* session_[10];
	uint64_t sequence_number_ = 1;
	uint16_t message_count_  = 0;

	// broadcaster
	int socket_;
	sockaddr_in destination_{};

	uint8_t message_[MAX_MESSAGE_SIZE];
	size_t message_length_ = 20;

public:
	ItchServer(/*const ItchConfig& config*/);
	~ItchServer();

	void Send();
	void Send(const void* message, size_t message_length);
	void AppendMessage(const void*message_to_append, uint16_t additional_length);
	// stop
};

class Exchange {
public:
	ItchServer itch_server_;
	Exchange() : itch_server_() {}
};
#endif  // NS_EX_CHANGE_H_
