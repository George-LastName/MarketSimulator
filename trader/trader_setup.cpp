#include "src/trader.h"

#include <stdexcept>
#include <iostream>

int main(){
    ItchClient IC = ItchClient();
    int i = 0;
    while (i < 10){
        ssize_t rec = IC.Receive(&i, sizeof(i));
        if(rec < 0){
            throw std::runtime_error("Error in Itch Receive");
        }

        std::cout << "Rec: " << i << "\n";
    }

    return 1;
}
