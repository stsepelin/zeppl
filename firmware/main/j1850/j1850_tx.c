#include "j1850_tx.h"
#include "j1850_tx_logic.h"
#include "j1850_vpw.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_sig_map.h"
#include <stdio.h>
#include <string.h>

#if CONFIG_VROD_J1850_TX_SELFTEST
#include "j1850_sniffer.h"
#endif

static const char *TAG = "j1850tx";

#define TX_GPIO      ((gpio_num_t)CONFIG_VROD_J1850_TX_GPIO)
#define RMT_RES_HZ   1000000  // 1 tick = 1 us; VPW symbols are 64..280 us
#define WD_SAMPLE_US 50       // watchdog poll period

// SOF + 8 bits/byte + a trailing recessive EOF pulse, then packed two
// pulses per RMT symbol.
#define MAX_PULSES  (1 + 8 * J1850_MAX_FRAME + 1)
#define MAX_SYMBOLS ((MAX_PULSES + 1) / 2)

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_encoder;
static gptimer_handle_t     s_wd;
static bool                 s_ready;

// Shared with the watchdog ISR.
static volatile uint32_t s_dom_us;
static volatile bool     s_faulted;

#if CONFIG_VROD_J1850_TX_WD_DEBUG
// Per-transmit pad-sample trace, frozen on a watchdog trip so the replay task
// can dump what the readback actually saw. Each entry is one 50 us sample.
#define WD_TRACE_N 160
static volatile uint8_t  s_wd_trace[WD_TRACE_N];
static volatile uint16_t s_wd_trace_n;      // samples recorded this transmit
static volatile uint32_t s_wd_send_seq;     // transmit counter
static volatile uint32_t s_wd_dom_at_trip;  // s_dom_us when it latched
static volatile uint16_t s_wd_trip_idx;     // sample index at the trip (0 = none)
#endif

// Watchdog: sampled every WD_SAMPLE_US off the (input-enabled) TX pad. A
// dominant (HIGH) that outlasts the longest valid symbol is a stuck bus.
// The release is deliberately independent of RMT and the TX task: detach
// the pad from the RMT matrix, hand it to the GPIO peripheral, and drive
// it LOW (recessive) — all ISR-safe register writes. Reverse polarity of
// the old inversion-era note: on standard VPW a stuck HIGH is the jam.
static bool IRAM_ATTR wd_on_alarm(gptimer_handle_t t, const gptimer_alarm_event_data_t *ed,
                                  void *arg)
{
    (void)t;
    (void)ed;
    (void)arg;
    int lvl = gpio_get_level(TX_GPIO);
#if CONFIG_VROD_J1850_TX_WD_DEBUG
    if (s_wd_trace_n < WD_TRACE_N)
        s_wd_trace[s_wd_trace_n++] = (uint8_t)lvl;
#endif
    if (lvl) {
        s_dom_us += WD_SAMPLE_US;
        if (s_dom_us > J1850_TX_DOMINANT_MAX_US && !s_faulted) {
#if CONFIG_VROD_J1850_TX_WD_DEBUG
            s_wd_dom_at_trip = s_dom_us;
            s_wd_trip_idx    = s_wd_trace_n;
#endif
            gpio_set_level(TX_GPIO, 0);
            esp_rom_gpio_connect_out_signal(TX_GPIO, SIG_GPIO_OUT_IDX, false, false);
            s_faulted = true;
        }
    } else {
        s_dom_us = 0;
    }
    return false;
}

void j1850_tx_init(void)
{
    // Pad LOW (recessive) before anything else drives it.
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << TX_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(TX_GPIO, 0);

    const rmt_tx_channel_config_t cfg = {
        .gpio_num          = TX_GPIO,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = RMT_RES_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags             = {.invert_out = 0, .init_level = 0},  // idle LOW = recessive
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&cfg, &s_chan));

    const rmt_copy_encoder_config_t enc = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&enc, &s_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_chan));

    // Input buffer on the pad so the watchdog can read back the driven
    // level; and pre-zero the GPIO out register so a fault-time signal
    // detach lands the pad LOW with no transient.
    gpio_input_enable(TX_GPIO);
    gpio_set_level(TX_GPIO, 0);

    const gptimer_config_t wt = {
        .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
        .direction     = GPTIMER_COUNT_UP,
        .resolution_hz = RMT_RES_HZ,  // 1 us tick
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&wt, &s_wd));
    const gptimer_event_callbacks_t cbs = {.on_alarm = wd_on_alarm};
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_wd, &cbs, NULL));
    const gptimer_alarm_config_t al = {
        .alarm_count  = WD_SAMPLE_US,
        .reload_count = 0,
        .flags        = {.auto_reload_on_alarm = true},
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(s_wd, &al));
    ESP_ERROR_CHECK(gptimer_enable(s_wd));

    s_faulted = false;
    s_ready   = true;
    ESP_LOGI(TAG, "TX ready on GPIO%d (high-side, HIGH=dominant); watchdog cutoff %d us", TX_GPIO,
             J1850_TX_DOMINANT_MAX_US);
}

bool j1850_tx_faulted(void)
{
    return s_faulted;
}

bool j1850_tx_reset(void)
{
    if (!s_ready)
        return false;
    if (s_faulted) {
        // A trip handed the pad to the GPIO peripheral (already driven LOW);
        // give it back to RMT (which idles LOW), then re-enable the watchdog
        // readback. rmt_tx_switch_gpio() rejects an enabled channel with
        // ESP_ERR_INVALID_STATE, so the channel must be disabled around the
        // switch. The pad stays LOW (recessive) throughout: the ISR left the
        // GPIO output LOW, and the disabled/re-enabled RMT channel idles LOW
        // (init_level = 0) -- no transient dominant on the bus.
        gpio_set_level(TX_GPIO, 0);

        if (rmt_disable(s_chan) != ESP_OK)
            return false;
        if (rmt_tx_switch_gpio(s_chan, TX_GPIO, false) != ESP_OK) {
            (void)rmt_enable(s_chan);  // best-effort: leave the channel usable
            return false;
        }
        if (rmt_enable(s_chan) != ESP_OK)
            return false;
        gpio_input_enable(TX_GPIO);
        gpio_set_level(TX_GPIO, 0);
    }
    s_dom_us  = 0;
    s_faulted = false;
    return true;
}

// Pack the alternating-level pulse stream into RMT symbols (two pulses
// per symbol). n is even here — the caller appended the recessive EOF.
static size_t pack_symbols(const j1850_pulse_t *p, size_t n, rmt_symbol_word_t *sym)
{
    size_t s = 0;
    for (size_t i = 0; i + 1 < n; i += 2) {
        sym[s].duration0 = p[i].dur_us;
        sym[s].level0    = p[i].active ? 1 : 0;
        sym[s].duration1 = p[i + 1].dur_us;
        sym[s].level1    = p[i + 1].active ? 1 : 0;
        s++;
    }
    return s;
}

bool j1850_tx_send(const uint8_t *payload, size_t n)
{
    if (!s_ready || s_faulted)
        return false;

    uint8_t frame[J1850_MAX_FRAME];
    size_t  flen = 0;
    if (!j1850_tx_build_frame(payload, n, frame, sizeof(frame), &flen))
        return false;

    j1850_pulse_t pulses[MAX_PULSES];
    size_t        np = j1850_vpw_encode(frame, flen, pulses, MAX_PULSES - 1);
    if (np == 0)
        return false;
    // Trailing recessive closes the last (active) bit with an edge and
    // carries EOD/EOF in-band, so the receiver frames without waiting for
    // an idle-timeout flush. Keeps np even for clean symbol packing.
    pulses[np++] = (j1850_pulse_t){.active = false, .dur_us = J1850_TX_EOF_US};

    // Never key a stream that could hold the bus dominant too long.
    if (!j1850_tx_stream_within_limits(pulses, np)) {
        ESP_LOGE(TAG, "refusing over-long dominant in stream");
        return false;
    }

    rmt_symbol_word_t symbols[MAX_SYMBOLS];
    size_t            ns = pack_symbols(pulses, np, symbols);

    // A TX-side error must never panic the cluster (ESP_ERROR_CHECK
    // aborts): log, leave the bus recessive, and let the caller decide.
    // Refusing to key without the watchdog armed is the safe default.
    s_dom_us = 0;
#if CONFIG_VROD_J1850_TX_WD_DEBUG
    s_wd_trace_n     = 0;
    s_wd_trip_idx    = 0;
    s_wd_dom_at_trip = 0;
    s_wd_send_seq++;
#endif
    if (gptimer_start(s_wd) != ESP_OK) {
        ESP_LOGE(TAG, "watchdog arm failed; refusing TX this frame");
        return false;
    }

    const rmt_transmit_config_t tx = {.loop_count = 0, .flags = {.eot_level = 0}};
    esp_err_t err = rmt_transmit(s_chan, s_encoder, symbols, ns * sizeof(symbols[0]), &tx);
    if (err == ESP_OK)
        err = rmt_tx_wait_all_done(s_chan, 100);
    (void)gptimer_stop(s_wd);  // best-effort; eot_level already idles the bus LOW

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "transmit failed (%s); bus forced recessive", esp_err_to_name(err));
        s_faulted = true;
        return false;
    }
    return !s_faulted;
}

#if CONFIG_VROD_J1850_TX_SELFTEST || CONFIG_VROD_J1850_TX_BIKE_REPLAY

// IM keep-alive payloads (WITHOUT CRC — the driver appends it), the set
// identified in the Stage 2 capture. Doubles as the bike replay set.
static const struct {
    uint8_t bytes[4];
    size_t  len;
} SELFTEST_FRAMES[] = {
    {{0x68, 0xFF, 0x40, 0x03}, 4},
    {{0x68, 0xFF, 0x60, 0x03}, 4},
    {{0x29, 0xFE, 0x40, 0x01}, 4},
    {{0x29, 0xFE, 0x60, 0x01}, 4},
};
#define SELFTEST_N (sizeof(SELFTEST_FRAMES) / sizeof(SELFTEST_FRAMES[0]))

// Positive watchdog test: the self-sniff pass only proves the watchdog
// does not MISFIRE. This proves it FIRES. Both software guards are
// bypassed on purpose — drive the pad dominant (HIGH) straight from the
// GPIO peripheral, off RMT and past the pre-transmit stream guard — and
// confirm layer 2 (the gptimer ISR) detaches the pad and pulls it LOW
// past the 300 us cutoff. Destructive: it latches a fault, so afterwards
// re-bind RMT and clear the latch for the self-sniff run. Needed because
// the pad readback (gpio_input_enable) is unproven on P4 silicon — this
// is its acceptance test.
static bool wd_selftest(void)
{
    s_dom_us  = 0;
    s_faulted = false;

    gpio_set_level(TX_GPIO, 1);
    esp_rom_gpio_connect_out_signal(TX_GPIO, SIG_GPIO_OUT_IDX, false, false);

    ESP_ERROR_CHECK(gptimer_start(s_wd));
    vTaskDelay(pdMS_TO_TICKS(5));  // >> 300 us; the watchdog must trip
    ESP_ERROR_CHECK(gptimer_stop(s_wd));

    bool tripped = s_faulted && gpio_get_level(TX_GPIO) == 0;
    ESP_LOGI(TAG, "watchdog trigger test: %s (faulted=%d, line=%s)", tripped ? "PASS" : "FAIL",
             s_faulted, gpio_get_level(TX_GPIO) ? "HIGH" : "LOW");

    // Recover through the normal fault-reset path (re-bind RMT + clear).
    j1850_tx_reset();
    return tripped;
}
#endif  // SELFTEST || BIKE_REPLAY — shared keep-alive set + watchdog trigger test

#if CONFIG_VROD_J1850_TX_SELFTEST
static void selftest_task(void *arg)
{
    (void)arg;
    // Let the RX sniffer settle after boot.
    vTaskDelay(pdMS_TO_TICKS(500));
    wd_selftest();
    uint32_t pass = 0, fail = 0;

    for (;;) {
        for (size_t i = 0; i < SELFTEST_N; i++) {
            uint8_t expect[J1850_MAX_FRAME];
            size_t  elen = 0;
            j1850_tx_build_frame(SELFTEST_FRAMES[i].bytes, SELFTEST_FRAMES[i].len, expect,
                                 sizeof(expect), &elen);

            j1850_sniffer_stats_t before;
            j1850_sniffer_get_stats(&before);

            bool sent = j1850_tx_send(SELFTEST_FRAMES[i].bytes, SELFTEST_FRAMES[i].len);
            // Wait for the sniffer's idle-flush + decode of our frame.
            vTaskDelay(pdMS_TO_TICKS(30));

            j1850_sniffer_stats_t after;
            j1850_sniffer_get_stats(&after);

            bool decoded = after.frames != before.frames;
            bool match   = decoded && after.last_len == elen && after.last_crc_ok &&
                           memcmp(after.last_frame, expect, elen) == 0;

            char hex[48];
            int  pos = 0;
            for (size_t b = 0; b < after.last_len && pos < (int)sizeof(hex) - 4; b++)
                pos += snprintf(hex + pos, sizeof(hex) - (size_t)pos, "%02X ", after.last_frame[b]);
            if (after.last_len == 0)
                snprintf(hex, sizeof(hex), "(none)");

            if (sent && match) {
                pass++;
                ESP_LOGI(TAG, "self-sniff PASS: rx [%s] CRC %s", hex,
                         after.last_crc_ok ? "OK" : "BAD");
            } else {
                fail++;
                ESP_LOGW(TAG, "self-sniff FAIL: sent=%d decoded=%d rx [%s] CRC %s%s", sent, decoded,
                         hex, after.last_crc_ok ? "OK" : "BAD",
                         j1850_tx_faulted() ? " [TX FAULT]" : "");
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        ESP_LOGI(TAG, "self-sniff tally: %lu pass, %lu fail", (unsigned long)pass,
                 (unsigned long)fail);
    }
}

void j1850_tx_selftest_start(void)
{
    xTaskCreatePinnedToCore(selftest_task, "j1850_tx_st", 4096, NULL, 6, NULL, 0);
}
#endif

#if CONFIG_VROD_J1850_TX_BIKE_REPLAY
// Live-bus replay for the Stage-4 on-bike step (3): emit the IM keep-alive set
// at ~2 s cadence WITHOUT the bench self-sniff compare — a live bus carries the
// stock IM's traffic, which would flap that verdict. The RX sniffer logs the
// whole bus separately; judge from its per-frame CRC log + stats, not from a
// PASS/FAIL line here.
#if CONFIG_VROD_J1850_TX_WD_DEBUG
// Dump the frozen pad-sample trace of the faulting transmit as run-lengths
// (each unit = 50 us). A run like "H8" = 400 us stuck HIGH is the trip cause;
// compare it to a scope on the TX GPIO to tell real electrical from a lying
// readback.
static void wd_trace_dump(void)
{
    char     buf[220];
    int      pos = 0;
    uint16_t n   = s_wd_trace_n;
    uint16_t i   = 0;
    while (i < n && pos < (int)sizeof(buf) - 12) {
        uint8_t  v   = s_wd_trace[i];
        uint16_t run = 0;
        while (i < n && s_wd_trace[i] == v) {
            i++;
            run++;
        }
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%c%u ", v ? 'H' : 'L', run);
    }
    ESP_LOGW(TAG, "wd-trace send#%lu n=%u trip@%u dom=%luus | %s", (unsigned long)s_wd_send_seq, n,
             s_wd_trip_idx, (unsigned long)s_wd_dom_at_trip, buf);
}
#endif

static void bike_replay_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    wd_selftest();  // prove the watchdog once before keying a live bus
    uint32_t n = 0, ok = 0, faults = 0;
    for (;;) {
        for (size_t i = 0; i < SELFTEST_N; i++) {
            bool sent = j1850_tx_send(SELFTEST_FRAMES[i].bytes, SELFTEST_FRAMES[i].len);
            ++n;
            if (sent) {
                ++ok;
            } else if (j1850_tx_faulted()) {
                // DEBUG: recover and count, so we can see fault RATE and whether
                // a reset re-arms (persistent noise -> reset fails / re-faults
                // instantly; intermittent collision -> recovers and runs on).
                ++faults;
#if CONFIG_VROD_J1850_TX_WD_DEBUG
                wd_trace_dump();  // what the pad readback saw during this transmit
#endif
                bool rec = j1850_tx_reset();
                ESP_LOGW(TAG, "replay: frame %lu TX FAULT (#%lu, %lu ok) -> reset %s",
                         (unsigned long)n, (unsigned long)faults, (unsigned long)ok,
                         rec ? "OK re-armed" : "FAILED");
            }
            vTaskDelay(pdMS_TO_TICKS(150));  // inter-frame gap within the set
        }
        ESP_LOGI(TAG, "replay tally: %lu ok, %lu faults (of %lu)", (unsigned long)ok,
                 (unsigned long)faults, (unsigned long)n);
        vTaskDelay(pdMS_TO_TICKS(2000));  // ~2 s IM keep-alive cadence (Stage 2)
    }
}

void j1850_tx_bike_replay_start(void)
{
    xTaskCreatePinnedToCore(bike_replay_task, "j1850_tx_rp", 4096, NULL, 6, NULL, 0);
}
#endif
