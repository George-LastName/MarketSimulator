// mold_server.cpp
//
// Reads a SoupBinTCP-framed ITCH file and replays it as MoldUDP64 over multicast.
//
// Usage: ./mold_server <itch_file> [speed_multiplier]
//   speed_multiplier: 0.0 = as fast as possible (default)
//                     1.0 = real-time replay using ITCH timestamps
//                     2.0 = 2x real-time, etc.
//
// Compile: g++ -std=c++20 -O2 -o mold_server mold_server.cpp

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>

// ─────────────────────────────────────────────────────────────────────────────
// Network config — must match client
// ─────────────────────────────────────────────────────────────────────────────
static constexpr const char* kMcastGroup        = "239.1.2.3";
static constexpr uint16_t    kPort              = 21001;
static constexpr uint16_t    kMaxPayload        = 1400;  // under ethernet MTU
static constexpr int         kHeartbeatMs       = 1000;  // 1 heartbeat/sec

// ─────────────────────────────────────────────────────────────────────────────
// MoldUDP64 downstream packet layout (all multi-byte fields are big-endian)
//
//  Offset  Len  Field
//  ------  ---  -----
//   0       10  Session          (ASCII, space-padded)
//  10        8  Sequence Number  (uint64, first message in this packet)
//  18        2  Message Count    (uint16, 0=heartbeat, 0xFFFF=end-of-session)
//  20+       -  Message Blocks:
//               [ 2 bytes: message length ][ N bytes: ITCH message data ]
//               ... repeated Message Count times
// ─────────────────────────────────────────────────────────────────────────────
static constexpr size_t kMoldHeaderSize = 20;

struct [[gnu::packed]] MoldHeader {
    char     session[10];
    uint64_t seq_num;       // big-endian
    uint16_t msg_count;     // big-endian
};

// ─────────────────────────────────────────────────────────────────────────────
// ITCH SoupBinTCP file framing
//   Each record in the file: [ 2-byte big-endian length ][ N bytes ITCH msg ]
// ─────────────────────────────────────────────────────────────────────────────

// Read the 6-byte ITCH timestamp (nanoseconds from midnight) from message body.
// The timestamp sits at bytes 3..8 of the ITCH message (after type + locate + tracknum).
static uint64_t ReadItchTimestamp(const uint8_t* itch_msg) {
    uint64_t ts = 0;
    // 6 bytes starting at offset 3 (type=0, locate=1-2, tracknum=3-4, timestamp=5-10)
    std::memcpy(reinterpret_cast<uint8_t*>(&ts) + 2, itch_msg + 5, 6);
    return __builtin_bswap64(ts);
}

// ─────────────────────────────────────────────────────────────────────────────
// Packet builder
//   Accumulates ITCH messages into a MoldUDP64 packet buffer and sends when
//   either the buffer would overflow or the caller forces a flush.
// ─────────────────────────────────────────────────────────────────────────────
class PacketBuilder {
public:
    PacketBuilder(int sock, const sockaddr_in& dest, const char* session,
                  uint64_t start_seq)
    : sock_(sock), dest_(dest), seq_(start_seq) {
        std::memcpy(header()->session, session, 10);
        Reset();
    }

    // Try to add one ITCH message (msg_len bytes at msg_data).
    // Returns false if the message won't fit — caller should Flush() first.
    bool Add(const uint8_t* msg_data, uint16_t msg_len) {
        // Space needed: 2-byte length prefix + message body
        if (write_pos_ + 2 + msg_len > kMaxPayload) return false;

        uint16_t len_be = htons(msg_len);
        std::memcpy(buf_ + write_pos_, &len_be, 2);
        std::memcpy(buf_ + write_pos_ + 2, msg_data, msg_len);
        write_pos_ += 2 + msg_len;
        msg_count_++;
        return true;
    }

    // Send the current buffer and advance the sequence number.
    void Flush() {
        if (msg_count_ == 0) return;
        WriteHeader(seq_, msg_count_);
        sendto(sock_, buf_, write_pos_, 0,
               reinterpret_cast<const sockaddr*>(&dest_), sizeof(dest_));
        seq_ += msg_count_;
        Reset();
    }

    // Send a heartbeat (Message Count = 0, carries next expected seq).
    void SendHeartbeat() {
        WriteHeader(seq_, 0);
        sendto(sock_, buf_, kMoldHeaderSize, 0,
               reinterpret_cast<const sockaddr*>(&dest_), sizeof(dest_));
    }

    // Send End-of-Session (Message Count = 0xFFFF, carries next expected seq).
    void SendEndOfSession() {
        WriteHeader(seq_, 0xFFFF);
        sendto(sock_, buf_, kMoldHeaderSize, 0,
               reinterpret_cast<const sockaddr*>(&dest_), sizeof(dest_));
        std::cout << "[server] End of session sent. Final seq=" << seq_ << "\n";
    }

    uint64_t NextSeq() const { return seq_; }

private:
    MoldHeader* header() { return reinterpret_cast<MoldHeader*>(buf_); }

    void WriteHeader(uint64_t seq, uint16_t count) {
        header()->seq_num   = __builtin_bswap64(seq);
        header()->msg_count = htons(count);
    }

    void Reset() {
        write_pos_ = kMoldHeaderSize;  // reserve space for header at front
        msg_count_ = 0;
    }

    int            sock_;
    sockaddr_in    dest_;
    uint64_t       seq_;
    uint8_t        buf_[kMaxPayload];
    size_t         write_pos_ = kMoldHeaderSize;
    uint16_t       msg_count_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <itch_file> [speed_multiplier]\n";
        return 1;
    }

    const char* filepath       = argv[1];
    const double speed         = (argc >= 3) ? std::stod(argv[2]) : 0.0;
    const bool   realtime_mode = (speed > 0.0);

    // ── Open and mmap the ITCH file ──────────────────────────────────────────
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) { perror("open"); return 1; }

    struct stat st;
    fstat(fd, &st);
    const off_t file_size = st.st_size;

    auto* mapped = reinterpret_cast<uint8_t*>(
        mmap(nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0));
    if (mapped == MAP_FAILED) { perror("mmap"); return 1; }
    close(fd);

    // ── Create UDP socket ────────────────────────────────────────────────────
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    // Set multicast TTL — 1 keeps traffic on the local subnet.
    // Increase if routing across subnets.
    uint8_t ttl = 1;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    // Disable loopback so the sender doesn't receive its own packets.
    // Remove this line if server and client run on the same machine for testing.
    // uint8_t loop = 0;
    // setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(kPort);
    inet_pton(AF_INET, kMcastGroup, &dest.sin_addr);

    // ── Build session string from filename (date-like, 10 bytes) ────────────
    char session[10];
    std::memset(session, ' ', 10);
    // Use the filename as the session identifier, truncated/padded to 10 bytes.
    const char* name = std::strrchr(filepath, '/');
    name = name ? name + 1 : filepath;
    std::memcpy(session, name, std::min(std::strlen(name), size_t(10)));

    std::cout << "[server] Session: \"" << std::string(session, 10) << "\"\n";
    std::cout << "[server] Multicast: " << kMcastGroup << ":" << kPort << "\n";
    std::cout << "[server] Speed: "
    << (realtime_mode ? std::to_string(speed) + "x" : "max") << "\n";

    // ── Replay loop ──────────────────────────────────────────────────────────
    PacketBuilder builder(sock, dest, session, 1 /*seq starts at 1*/);

    const uint8_t* ptr      = mapped;
    const uint8_t* file_end = mapped + file_size;

    // For real-time pacing
    uint64_t first_itch_ts = 0;
    auto     wall_start    = std::chrono::steady_clock::now();
    auto     last_heartbeat= wall_start;

    while (ptr < file_end) {
        // SoupBinTCP framing: 2-byte big-endian length then message
        if (ptr + 2 > file_end) break;
        const uint16_t msg_len = ntohs(*reinterpret_cast<const uint16_t*>(ptr));
        ptr += 2;
        if (ptr + msg_len > file_end) break;

        const uint8_t* msg = ptr;
        ptr += msg_len;

        // ── Real-time pacing ─────────────────────────────────────────────────
        if (realtime_mode) {
            uint64_t ts = ReadItchTimestamp(msg);
            if (first_itch_ts == 0) first_itch_ts = ts;

            // How many ns into the session should we be sending this?
            uint64_t itch_elapsed_ns = ts - first_itch_ts;
            auto target_wall = wall_start +
            std::chrono::nanoseconds(
                static_cast<uint64_t>(itch_elapsed_ns / speed));

            auto now = std::chrono::steady_clock::now();

            // Send a heartbeat if we're waiting and it's been > 1 second
            if (now < target_wall) {
                auto since_hb = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_heartbeat).count();
                    if (since_hb >= kHeartbeatMs) {
                        builder.Flush();
                        builder.SendHeartbeat();
                        last_heartbeat = now;
                    }
                    std::this_thread::sleep_until(target_wall);
            }
        }

        // ── Pack message into current UDP packet ─────────────────────────────
        if (!builder.Add(msg, msg_len)) {
            // Doesn't fit — flush the current packet first
            builder.Flush();
            builder.Add(msg, msg_len);  // guaranteed to fit in a fresh packet
        }

        // Send heartbeat if real-time and enough time has passed since last one
        if (realtime_mode) {
            auto now = std::chrono::steady_clock::now();
            auto since_hb = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_heartbeat).count();
                if (since_hb >= kHeartbeatMs) {
                    builder.Flush();
                    builder.SendHeartbeat();
                    last_heartbeat = now;
                }
        }
    }

    // Flush any remaining messages
    builder.Flush();

    // Send End of Session several times (like production does)
    for (int i = 0; i < 5; ++i) {
        builder.SendEndOfSession();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    munmap(mapped, file_size);
    close(sock);

    std::cout << "[server] Done. Total messages sent: "
    << (builder.NextSeq() - 1) << "\n";
    return 0;
}
