#include "src/exchange.h"

// #include <cstring>
#include <iostream>

int main(){
    Exchange ex = Exchange();
    std::cout << "EXCHANGE\n";
    // char const* mess = "HELLO WORLD";
    int i = 0;
    while(i++<10){
        ex.itch_server_.Send(&i, sizeof(i));
        std::cout << "Sent: " << i << "\n";
    }
    return 1;
}
