// I2C Error Rate Test - Compares direct CNTR read vs ODR latch read
// Tests the frequency of corrupt I2C reads on the LS7866 counter chips.
// Phase 1: Direct CNTR read (register 0x07) - the old/broken method
// Phase 2: ODR latch read (write TPR LODC, read ODR 0x06) - the correct method per datasheet
//
// Also tests whether errors correlate with counter incrementing vs stationary,
// and whether there's a periodic pattern (e.g. from Waveshare touchscreen on same bus).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <signal.h>

static volatile bool running = true;

void signal_handler(int) {
    running = false;
}

struct ErrorEvent {
    int64_t time_ms;
    int counter_id;          // 1 or 2
    uint32_t raw_value;
    uint32_t expected_value;
    int32_t delta;
    uint8_t raw_bytes[4];
    bool is_moving;
};

struct PhaseStats {
    const char* name;
    int64_t total_reads;
    int64_t errors_c1, errors_c2;
    int64_t backward_c1, backward_c2;
    int64_t forward_jump_c1, forward_jump_c2;
    int64_t ioctl_failures_c1, ioctl_failures_c2;
    int64_t errors_while_moving, errors_while_stationary;
    int64_t reads_while_moving, reads_while_stationary;
    std::vector<ErrorEvent> errors;
    double elapsed_sec;
};

class I2CReader {
    int fd;
    int addr;
public:
    I2CReader(int bus, int address) : addr(address) {
        char filename[20];
        snprintf(filename, sizeof(filename), "/dev/i2c-%d", bus);
        fd = open(filename, O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "ERROR: Cannot open /dev/i2c-%d\n", bus);
            throw -1;
        }
        if (ioctl(fd, I2C_SLAVE, address) < 0) {
            close(fd);
            fprintf(stderr, "ERROR: Cannot set I2C slave address 0x%02x\n", address);
            throw -1;
        }
    }
    ~I2CReader() { if (fd >= 0) close(fd); }

    // Direct register read (returns true on success)
    bool read_register(uint8_t reg, uint32_t& value, uint8_t raw_bytes[4]) {
        struct i2c_msg messages[2];
        struct i2c_rdwr_ioctl_data ioctl_data;
        uint8_t reg_addr = reg;

        messages[0].addr = addr;
        messages[0].flags = 0;
        messages[0].len = 1;
        messages[0].buf = &reg_addr;

        messages[1].addr = addr;
        messages[1].flags = I2C_M_RD;
        messages[1].len = 4;
        messages[1].buf = raw_bytes;

        ioctl_data.msgs = messages;
        ioctl_data.nmsgs = 2;

        if (ioctl(fd, I2C_RDWR, &ioctl_data) < 0) {
            return false;
        }

        value = (static_cast<uint32_t>(raw_bytes[0]) << 24) |
                (static_cast<uint32_t>(raw_bytes[1]) << 16) |
                (static_cast<uint32_t>(raw_bytes[2]) << 8) |
                static_cast<uint32_t>(raw_bytes[3]);
        return true;
    }

    // Write a single byte to a register
    bool write_register(uint8_t reg, uint8_t value) {
        struct i2c_msg msg;
        struct i2c_rdwr_ioctl_data ioctl_data;
        uint8_t buf[2] = { reg, value };

        msg.addr = addr;
        msg.flags = 0;
        msg.len = 2;
        msg.buf = buf;

        ioctl_data.msgs = &msg;
        ioctl_data.nmsgs = 1;

        return ioctl(fd, I2C_RDWR, &ioctl_data) >= 0;
    }

    // Latched read: write LODC to TPR, then read ODR
    bool read_counter_latched(uint32_t& value, uint8_t raw_bytes[4]) {
        static constexpr uint8_t REG_TPR = 0x05;
        static constexpr uint8_t REG_ODR = 0x06;
        static constexpr uint8_t TPR_LODC = 0x08;

        if (!write_register(REG_TPR, TPR_LODC)) {
            return false;
        }
        return read_register(REG_ODR, value, raw_bytes);
    }
};

static int64_t now_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

typedef bool (I2CReader::*ReadFunc)(uint32_t&, uint8_t[4]);

// Adapter for direct CNTR read to match the latched signature
class DirectReadAdapter {
public:
    static bool read_direct(I2CReader& reader, uint32_t& value, uint8_t raw_bytes[4]) {
        return reader.read_register(0x07, value, raw_bytes);
    }
    static bool read_latched(I2CReader& reader, uint32_t& value, uint8_t raw_bytes[4]) {
        return reader.read_counter_latched(value, raw_bytes);
    }
};

PhaseStats run_phase(const char* name, I2CReader& cntr1, I2CReader& cntr2,
                     bool use_latched, int poll_interval_us, int duration_sec) {
    PhaseStats stats = {};
    stats.name = name;

    // Establish baseline
    uint32_t last_good_c1 = 0, last_good_c2 = 0;
    uint8_t b1[4], b2[4];
    bool ok;
    if (use_latched) {
        ok = cntr1.read_counter_latched(last_good_c1, b1) &&
             cntr2.read_counter_latched(last_good_c2, b2);
    } else {
        ok = cntr1.read_register(0x07, last_good_c1, b1) &&
             cntr2.read_register(0x07, last_good_c2, b2);
    }
    if (!ok) {
        printf("  ERROR: Cannot read baseline for %s!\n", name);
        return stats;
    }
    printf("  Baseline: c1=%u (0x%08X) c2=%u (0x%08X)\n", last_good_c1, last_good_c1, last_good_c2, last_good_c2);

    uint32_t movement_check_c1 = last_good_c1;
    int64_t movement_check_time = now_ms();
    bool is_moving = false;

    int64_t start_time = now_ms();
    int64_t end_time = start_time + (duration_sec * 1000LL);
    int64_t last_report = start_time;

    printf("  %-8s %-10s %-8s %-8s %-8s %-8s %-7s\n",
           "Time(s)", "Reads", "ErrC1", "ErrC2", "IOF1", "IOF2", "Moving");

    while (running && now_ms() < end_time) {
        stats.total_reads++;

        uint32_t v1 = 0, v2 = 0;
        uint8_t b1[4] = {}, b2[4] = {};
        bool ok1, ok2;

        if (use_latched) {
            ok1 = cntr1.read_counter_latched(v1, b1);
            ok2 = cntr2.read_counter_latched(v2, b2);
        } else {
            ok1 = cntr1.read_register(0x07, v1, b1);
            ok2 = cntr2.read_register(0x07, v2, b2);
        }

        if (!ok1) stats.ioctl_failures_c1++;
        if (!ok2) stats.ioctl_failures_c2++;

        int64_t t = now_ms();
        if (t - movement_check_time >= 500) {
            is_moving = (last_good_c1 != movement_check_c1);
            movement_check_c1 = last_good_c1;
            movement_check_time = t;
        }

        if (is_moving) stats.reads_while_moving++;
        else stats.reads_while_stationary++;

        // Validate counter 1
        if (ok1) {
            bool bad = false;
            int32_t delta = static_cast<int32_t>(v1) - static_cast<int32_t>(last_good_c1);

            if (v1 < last_good_c1) {
                bad = true;
                stats.backward_c1++;
            } else if ((v1 - last_good_c1) > 1000) {
                bad = true;
                stats.forward_jump_c1++;
            }

            if (bad) {
                stats.errors_c1++;
                if (is_moving) stats.errors_while_moving++;
                else stats.errors_while_stationary++;

                ErrorEvent e;
                e.time_ms = t - start_time;
                e.counter_id = 1;
                e.raw_value = v1;
                e.expected_value = last_good_c1;
                e.delta = delta;
                e.is_moving = is_moving;
                memcpy(e.raw_bytes, b1, 4);
                stats.errors.push_back(e);

                fprintf(stderr, "  ERR c1 t=%.3fs raw=%u(0x%08X) expected=%u delta=%d [%02X %02X %02X %02X]%s\n",
                        e.time_ms / 1000.0, v1, v1, last_good_c1, delta,
                        b1[0], b1[1], b1[2], b1[3],
                        is_moving ? " MOVING" : " STAT");
            } else {
                last_good_c1 = v1;
            }
        }

        // Validate counter 2
        if (ok2) {
            bool bad = false;
            int32_t delta = static_cast<int32_t>(v2) - static_cast<int32_t>(last_good_c2);

            if (v2 < last_good_c2) {
                bad = true;
                stats.backward_c2++;
            } else if ((v2 - last_good_c2) > 1000) {
                bad = true;
                stats.forward_jump_c2++;
            }

            if (bad) {
                stats.errors_c2++;
                if (is_moving) stats.errors_while_moving++;
                else stats.errors_while_stationary++;

                ErrorEvent e;
                e.time_ms = t - start_time;
                e.counter_id = 2;
                e.raw_value = v2;
                e.expected_value = last_good_c2;
                e.delta = delta;
                e.is_moving = is_moving;
                memcpy(e.raw_bytes, b2, 4);
                stats.errors.push_back(e);

                fprintf(stderr, "  ERR c2 t=%.3fs raw=%u(0x%08X) expected=%u delta=%d [%02X %02X %02X %02X]%s\n",
                        e.time_ms / 1000.0, v2, v2, last_good_c2, delta,
                        b2[0], b2[1], b2[2], b2[3],
                        is_moving ? " MOVING" : " STAT");
            } else {
                last_good_c2 = v2;
            }
        }

        // Progress every 10 seconds
        if (t - last_report >= 10000) {
            double elapsed = (t - start_time) / 1000.0;
            printf("  %-8.0f %-10ld %-8ld %-8ld %-8ld %-8ld %-7s\n",
                   elapsed, (long)stats.total_reads,
                   (long)stats.errors_c1, (long)stats.errors_c2,
                   (long)stats.ioctl_failures_c1, (long)stats.ioctl_failures_c2,
                   is_moving ? "YES" : "no");
            last_report = t;
        }

        if (poll_interval_us > 0) {
            usleep(poll_interval_us);
        }
    }

    stats.elapsed_sec = (now_ms() - start_time) / 1000.0;
    return stats;
}

void print_phase_report(const PhaseStats& s) {
    int64_t total_errors = s.errors_c1 + s.errors_c2;
    double error_rate = s.total_reads > 0 ? (double)total_errors / s.total_reads * 100.0 : 0;
    double reads_per_sec = s.total_reads / s.elapsed_sec;

    printf("  Duration:      %.1f seconds\n", s.elapsed_sec);
    printf("  Total reads:   %ld (%.0f reads/sec)\n", (long)s.total_reads, reads_per_sec);
    printf("  C1 errors:     %ld (backward: %ld, forward: %ld)\n",
           (long)s.errors_c1, (long)s.backward_c1, (long)s.forward_jump_c1);
    printf("  C2 errors:     %ld (backward: %ld, forward: %ld)\n",
           (long)s.errors_c2, (long)s.backward_c2, (long)s.forward_jump_c2);
    printf("  Total errors:  %ld (%.4f%% error rate)\n", (long)total_errors, error_rate);
    printf("  I/O failures:  c1=%ld c2=%ld\n", (long)s.ioctl_failures_c1, (long)s.ioctl_failures_c2);

    if (s.reads_while_moving > 0) {
        printf("  Moving:        %ld reads, %ld errors (%.4f%%)\n",
               (long)s.reads_while_moving, (long)s.errors_while_moving,
               (double)s.errors_while_moving / s.reads_while_moving * 100.0);
    }
    if (s.reads_while_stationary > 0) {
        printf("  Stationary:    %ld reads, %ld errors (%.4f%%)\n",
               (long)s.reads_while_stationary, (long)s.errors_while_stationary,
               (double)s.errors_while_stationary / s.reads_while_stationary * 100.0);
    }

    if (!s.errors.empty()) {
        // Error type breakdown
        int all_ones_16 = 0, all_ones_24 = 0, all_ones_32 = 0, backward = 0, other = 0;
        for (const auto& e : s.errors) {
            if (e.raw_value == 0xFFFF) all_ones_16++;
            else if (e.raw_value == 0xFFFFFF) all_ones_24++;
            else if (e.raw_value == 0xFFFFFFFF) all_ones_32++;
            else if (e.delta < 0) backward++;
            else other++;
        }
        printf("  Error types:   ");
        bool first = true;
        if (all_ones_16) { printf("%s0xFFFF=%d", first?"":", ", all_ones_16); first = false; }
        if (all_ones_24) { printf("%s0xFFFFFF=%d", first?"":", ", all_ones_24); first = false; }
        if (all_ones_32) { printf("%s0xFFFFFFFF=%d", first?"":", ", all_ones_32); first = false; }
        if (backward) { printf("%sbackward=%d", first?"":", ", backward); first = false; }
        if (other) { printf("%sother=%d", first?"":", ", other); first = false; }
        printf("\n");

        // First few errors
        size_t show = std::min(s.errors.size(), (size_t)10);
        printf("  First %zu errors:\n", show);
        for (size_t i = 0; i < show; i++) {
            const auto& e = s.errors[i];
            printf("    t=%7.3fs c%d: 0x%08X expected=0x%08X delta=%+d [%02X %02X %02X %02X]\n",
                   e.time_ms / 1000.0, e.counter_id,
                   e.raw_value, e.expected_value, e.delta,
                   e.raw_bytes[0], e.raw_bytes[1], e.raw_bytes[2], e.raw_bytes[3]);
        }
    }
}

int main(int argc, char* argv[]) {
    int poll_interval_us = 1000;  // default 1ms
    int duration_sec = 30;        // default 30 seconds per phase

    if (argc >= 2) poll_interval_us = atoi(argv[1]);
    if (argc >= 3) duration_sec = atoi(argv[2]);

    printf("=== I2C Error Rate Test: Direct CNTR vs ODR Latch ===\n");
    printf("Bus: /dev/i2c-1\n");
    printf("Counter 1: 0x70, Counter 2: 0x71\n");
    printf("Poll interval: %d us (%d Hz)\n", poll_interval_us, 1000000 / poll_interval_us);
    printf("Duration: %d seconds per phase (%d total)\n", duration_sec, duration_sec * 2);
    printf("Press Ctrl+C to stop early\n\n");

    signal(SIGINT, signal_handler);

    I2CReader cntr1(1, 0x70);
    I2CReader cntr2(1, 0x71);

    // Phase 1: Direct CNTR read (the old broken method)
    printf("====== PHASE 1: Direct CNTR Read (register 0x07) ======\n");
    PhaseStats direct = run_phase("Direct CNTR", cntr1, cntr2, false, poll_interval_us, duration_sec);

    if (!running) {
        printf("\n[Interrupted]\n");
    }

    // Phase 2: ODR latch read (the correct method)
    if (running) {
        printf("\n====== PHASE 2: ODR Latch Read (TPR LODC + read ODR 0x06) ======\n");
        PhaseStats latched = run_phase("ODR Latch", cntr1, cntr2, true, poll_interval_us, duration_sec);

        // Comparison
        printf("\n\n========================================\n");
        printf("=== COMPARISON: Direct CNTR vs ODR Latch ===\n");
        printf("========================================\n\n");

        printf("--- Direct CNTR Read (old method) ---\n");
        print_phase_report(direct);

        printf("\n--- ODR Latch Read (correct method per datasheet) ---\n");
        print_phase_report(latched);

        int64_t direct_total = direct.errors_c1 + direct.errors_c2;
        int64_t latched_total = latched.errors_c1 + latched.errors_c2;

        printf("\n--- Summary ---\n");
        printf("Direct CNTR:  %ld errors in %ld reads (%.4f%%)\n",
               (long)direct_total, (long)direct.total_reads,
               direct.total_reads > 0 ? (double)direct_total / direct.total_reads * 100.0 : 0.0);
        printf("ODR Latch:    %ld errors in %ld reads (%.4f%%)\n",
               (long)latched_total, (long)latched.total_reads,
               latched.total_reads > 0 ? (double)latched_total / latched.total_reads * 100.0 : 0.0);

        if (direct_total > 0 && latched_total == 0) {
            printf("\nODR latch completely eliminated errors!\n");
        } else if (direct_total > 0 && latched_total > 0) {
            double improvement = (1.0 - (double)latched_total / direct_total) * 100.0;
            printf("\nODR latch reduced errors by %.1f%%\n", improvement);
        } else if (direct_total == 0) {
            printf("\nNo errors in either phase (try longer duration or moving counters)\n");
        }
    } else {
        printf("\n--- Direct CNTR Read Results ---\n");
        print_phase_report(direct);
    }

    printf("\n=== TEST COMPLETE ===\n");
    return 0;
}
