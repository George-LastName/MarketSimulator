#include "trader.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <stdexcept>
#include <arpa/inet.h>
#include <format>
#include <cassert>
#include <unistd.h>

ItchClient::ItchClient(){
    int address_family = AF_INET;
    int protocol = 0;
    const char* multicast_group = "234.255.255.255";
    int port = 21001;

    socket_ = socket(address_family, SOCK_DGRAM, protocol);
    if (socket_ < 0) { throw std::runtime_error("Failed to create Itch Client socket.\n"); }

    int reuse = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        throw std::runtime_error("Failed to set SO_REUSEADDR on Itch Client.\n");
    }

    sockaddr_in local{};
    local.sin_family = address_family;
    local.sin_port   = htons(port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_, (struct sockaddr*)&local, sizeof(local)) < 0) {
        throw std::runtime_error("Failed to Bind Itch Client.\n");
    }

    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_group);
    // mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    mreq.imr_interface.s_addr = inet_addr("127.0.0.1");

    if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        throw std::runtime_error("Failed to join multicast group on Itch Client.\n");
    }

    // Store the multicast address for reference (e.g. for future recvfrom)
    destination_.sin_family = address_family;
    destination_.sin_port   = htons(port);
    int outcome = inet_pton(address_family, multicast_group, &destination_.sin_addr);
    if(outcome < 1){
        throw std::runtime_error(std::format("Failed to convert network address -> {}\n0: invalid network address provided.\n-1:No valid address family\n", outcome));
    }
}

ItchClient::~ItchClient(){
    assert(("The socket has closed before deconstruction" && socket_ > 0));
    close(socket_);
}

ssize_t ItchClient::Receive(void* buffer, size_t length){
    sockaddr_in sender{};
    socklen_t sender_len = sizeof(sender);
    return recvfrom(socket_, buffer, length, 0, (struct sockaddr*)&sender, &sender_len);
}

