#ifndef NS_TRADER_ITCH_MESSAGE_TYPES_H_
#define NS_TRADER_ITCH_MESSAGE_TYPES_H_

#include <cstdint>
#include <chrono>
#include <cstring>
#include <string_view>
#include <arpa/inet.h>

struct [[gnu::packed]] ItchHeader {
    char type;
    std::uint16_t locate;
    std::uint16_t track_num;
    std::uint8_t timestamp[6];

    std::uint64_t GetTimestamp() const {
        std::uint64_t ts = 0;
        std::memcpy(&ts, timestamp, 8);
        ts = __builtin_bswap64(ts);
        return ts >> 16;
    }

    std::chrono::hh_mm_ss<std::chrono::nanoseconds> GetTimeFromMid() const {
        std::chrono::nanoseconds duration(GetTimestamp());
        return std::chrono::hh_mm_ss<std::chrono::nanoseconds>{duration};
    }

    std::uint16_t GetLocate()   const { return ntohs(locate); }
    std::uint16_t GetTrackNum() const { return ntohs(track_num); }
};

struct [[gnu::packed]] SysEvent {
    char event_code;
    /* O=Start of Messages, S=Start System Hours, Q=Market Open,
     * M=Market Close, E=End System Hours, C=End of Messages */
};

struct [[gnu::packed]] StockDir {
    char stock[8];
    char market_cat;
    char fin_stat_ind;
    std::uint32_t rnd_lot_size;
    char rnd_lots_only;
    char issue_class;
    char issue_sub[2];
    char auth;
    char srt_thr_ind;
    char ipo_flg;
    char luld_ref;
    char etp_flg;
    std::uint32_t etp_lev;
    char inv_ind;

    std::string_view GetStock() const { return std::string_view(stock, 8); }
};

struct [[gnu::packed]] StockTradingAction {
    char stock[8];
    char trading_state; // 'H'=Halted, 'P'=Paused, 'Q'=Quotation, 'T'=Trading
    char reserved;
    char reason[4];
};

struct [[gnu::packed]] RegShoRestriction {
    char stock[8];
    char reg_sho_action;
};

struct [[gnu::packed]] MarketParticipantPosition {
    char mpid[4];
    char stock[8];
    char primary_market_maker;
    char market_maker_mode;
    char market_participant_state;
};

struct [[gnu::packed]] MwcbDeclineLevel {
    std::uint64_t level_1;
    std::uint64_t level_2;
    std::uint64_t level_3;
};

struct [[gnu::packed]] MwcbStatus {
    char breached_level;
};

struct [[gnu::packed]] QuotingPeriodUpdate {
    char stock[8];
    std::uint32_t ipo_quot_release_time;
    char ipo_quot_release_qualifier;
    std::uint32_t ipo_price;
};

struct [[gnu::packed]] LuldAuctionCollar {
    char stock[8];
    std::uint32_t auction_collar_ref_price;
    std::uint32_t upper_auction_collar_price;
    std::uint32_t lower_auction_collar_price;
    std::uint32_t auction_collar_ext;
};

struct [[gnu::packed]] OperationalHalt {
    char stock[8];
    char market_code;
    char halt_action;
};

struct [[gnu::packed]] AddOrderNoMpid {
    std::uint64_t order_reference_number;
    char buy_sell_indicator; // 'B'=Buy, 'S'=Sell
    std::uint32_t shares;
    char stock[8];
    std::uint32_t price; // implied 4 decimal places
};

struct [[gnu::packed]] AddOrderMpid {
    std::uint64_t order_reference_number;
    char buy_sell_indicator;
    std::uint32_t shares;
    char stock[8];
    std::uint32_t price;
    char attribution[4];
};

struct [[gnu::packed]] OrderExecuted {
    std::uint64_t order_reference_number;
    std::uint32_t executed_shares;
    std::uint64_t match_number;
};

struct [[gnu::packed]] OrderExecutedWithPrice {
    std::uint64_t order_reference_number;
    std::uint32_t executed_shares;
    std::uint64_t match_number;
    char printable;
    std::uint32_t execution_price;
};

struct [[gnu::packed]] OrderCancel {
    std::uint64_t order_reference_number;
    std::uint32_t canceled_shares;
};

struct [[gnu::packed]] OrderDelete {
    std::uint64_t order_reference_number;
};

struct [[gnu::packed]] OrderReplace {
    std::uint64_t original_order_reference_number;
    std::uint64_t new_order_reference_number;
    std::uint32_t shares;
    std::uint32_t price;
};

struct [[gnu::packed]] TradeNonCross {
    std::uint64_t order_reference_number;
    char buy_sell_indicator;
    std::uint32_t shares;
    char stock[8];
    std::uint32_t price;
    std::uint64_t match_number;
};

struct [[gnu::packed]] CrossTrade {
    std::uint64_t shares;
    char stock[8];
    std::uint32_t cross_price;
    std::uint64_t match_number;
    char cross_type;
};

struct [[gnu::packed]] BrokenTrade {
    std::uint64_t match_number;
};

struct [[gnu::packed]] Noii {
    std::uint64_t paired_shares;
    std::uint64_t imbalance_shares;
    char imbalance_direction;
    char stock[8];
    std::uint32_t far_price;
    std::uint32_t near_price;
    std::uint32_t current_reference_price;
    char cross_type;
    char price_variation_indicator;
};

struct [[gnu::packed]] Rpii {
    char stock[8];
    char interest_flag;
};

struct [[gnu::packed]] DlcrPriceDiscovery {
    char stock[8];
    char open_eligibility_status;
    std::uint32_t min_allow_price;
    std::uint32_t max_allow_price;
    std::uint32_t near_exe_price;
    std::uint64_t near_exe_time;
    std::uint32_t lower_price_range_collar;
    std::uint32_t upper_price_range_collar;
};

#endif // NS_TRADER_ITCH_MESSAGE_TYPES_H_
