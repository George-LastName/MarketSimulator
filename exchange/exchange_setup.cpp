#include "src/exchange.h"

#include <cstring>
#include <iostream>

int main(){
    Exchange ex = Exchange();
    std::cout << "EXCHANGE\n";
    char const* mess = "HELLO WORLD";
    ex.itch_server_.Send(mess, strlen(mess));
    return 1;
}
