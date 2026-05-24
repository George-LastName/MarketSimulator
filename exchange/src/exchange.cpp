#include "exchange.h"

#include <sys/socket.h>
// #include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <cassert>
#include <format>
#include <iostream>
#include <cstring>



ItchServer::ItchServer(/*const ItchConfig& config*/){
    int address_family = AF_INET;
    int protocol = 0;
    const char* multicast_group = "234.255.255.255";
    int port = 21001;
    socket_ = socket(address_family, SOCK_DGRAM, protocol);
    if (socket_ < 0) { throw std::runtime_error("Failed to create Itch Server socket.\n"); }

    destination_.sin_family = address_family;
    destination_.sin_port   = htons(port);


    // For testing on one machine
    int loop = 1;
    setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));


    // Set when testing trader and exchange on one machine.
#ifdef LOCAL_TESTING
    int reuse = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        throw std::runtime_error("Failed to set SO_REUSEADDR on Itch Server.\n");
    }

    struct in_addr local_interface{};
    local_interface.s_addr = inet_addr("127.0.0.1");
    if(setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF, &local_interface, sizeof(local_interface)) < 0){
        throw std::runtime_error("Failed to set IP_MULTICAST_IF\n");
    }
#endif //LOCAL_TESTING

    if (bind(socket_, (struct sockaddr*)&destination_, sizeof(destination_)) < 0) {
        throw std::runtime_error(std::format("Failed to Bind Itch Server.\nErrno={}\n{}", errno, strerror(errno)));
    }
    int outcome = inet_pton(address_family, multicast_group, &destination_.sin_addr);
    if(outcome < 1){
        throw std::runtime_error(std::format("Failed to convert network address -> {}\n0: invalid network address provided.\n-1:No valid address family\n", outcome));
    }
}

ItchServer::~ItchServer(){
    assert(("The socket has closed before deconstruction" && socket_ > 0));
    if (socket_ > 0){
        close(socket_);
    }
}

void ItchServer::Send(const void * message, size_t message_length){
    ssize_t sent = sendto(socket_, message, message_length, 0, (struct sockaddr*)&destination_, sizeof(destination_));

    if(sent < 0){
        int err = errno;
        std::clog << "sento Error: " << strerror(err) << " | errno=" << err << "\n";
    } else if (static_cast<size_t>(sent) != message_length) {
        std::clog << "sendto did not send whole message.\nLength: " << message_length << "\nSent  : " << sent << "\n";
    }
}
