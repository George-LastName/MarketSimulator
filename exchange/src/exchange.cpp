#include "exchange.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <cassert>
#include <format>



ItchServer::ItchServer(/*const ItchConfig& config*/){
    int address_family = AF_INET;
    int protocol = 0;
    const char* multicast_group = "234.255.255.255";
    int port = 21001;
    socket_ = socket(address_family, SOCK_DGRAM, protocol);
    if (socket_ < 0) { throw std::runtime_error("Failed to create Itch Server socket.\n"); }

    destination_.sin_family = address_family;
    destination_.sin_port   = htons(port);


    int outcome = inet_pton(address_family, multicast_group, &destination_.sin_addr);
    if(outcome < 1){
        throw std::runtime_error(std::format("Failed to convert network address -> {}\n0: invalid network address provided.\n-1:No valid address family\n", outcome));
    }

    if(bind(socket_, (struct sockaddr*)&destination_, sizeof(destination_)) < 0) {
        throw std::runtime_error("Faild to Bind Itch Server\n");
    }

}

ItchServer::~ItchServer(){
    assert(("The socket has closed before deconstruction" && socket_ > 0));
    if (socket_ > 0){
        close(socket_);
    }
}
