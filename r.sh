clean() {
    rm -rf build
}

build() {
    cmake -S . -B build
    cmake --build build -j32
}

build_local() {
    cmake -S . -B build -DLOCAL_TESTING=ON
    cmake --build build -j32
}

run_exchange() {
    ./build/exchange/Nasdaq_Exchange
}

run_trader() {
    ./build/trader/Nasdaq_Trader
}

case $1 in
    "c")
        clean
        ;;
    "b")
        build
        ;;
    "bl")
        build_local
        ;;
    "rex")
        run_exchange
        ;;
    "rtr")
        run_trader
        ;;

    *)
        echo "Usage: ./r.sh <command>"
        echo ""
        echo "  c   Clean the Build Folder"
        echo "  b   Build"
        echo "  bl  Build for Exchange & Trader on same machine"
        echo "  rex Run the Exchange executable"
        echo "  rtr Run the Trader executable"
        ;;
esac
