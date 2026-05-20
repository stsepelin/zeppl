#include "ble_peripheral.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "phone.h"
#include "phone_data.h"
#include "phone_protocol.h"

static const char *TAG = "ble_peripheral";

// Nordic UART Service-shaped UUIDs. Pulling these straight from the
// well-known NUS layout (rather than rolling our own) means nRF
// Connect / LightBlue / Web BLE all label our characteristics
// "RX/TX" for free during early validation. The wire payload is our
// own TLV — we just borrow the address space.
//
// NimBLE stores 128-bit UUIDs little-endian, so the bytes look reversed
// vs. the conventional 6E400001-... string form. Don't reorder.
#define NUS_UUID_BYTES(low_lsb, low_msb) \
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, \
    0x93, 0xf3, 0xa3, 0xb5, low_lsb, low_msb, 0x40, 0x6e

static const ble_uuid128_t SVC_UUID = BLE_UUID128_INIT(NUS_UUID_BYTES(0x01, 0x00));
static const ble_uuid128_t RX_UUID  = BLE_UUID128_INIT(NUS_UUID_BYTES(0x02, 0x00));
static const ble_uuid128_t TX_UUID  = BLE_UUID128_INIT(NUS_UUID_BYTES(0x03, 0x00));

static uint8_t  s_own_addr_type;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_tx_attr_handle;

static int gap_event_cb(struct ble_gap_event *event, void *arg);
static void start_advertising(void);

// --- RX (phone → cluster) -------------------------------------------------

static int access_rx_cb(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle; (void)attr_handle; (void)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return 0;

    // One TLV per write packet for v1. The cluster's max field
    // (NOTIF_MSG_MAX=128) plus headers fits well under a single 247-byte
    // LE-2M MTU, so reassembly isn't pulling its weight yet — add it
    // when a real payload pushes past one ATT_MTU.
    static uint8_t buf[256];
    uint16_t       len = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &len);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_hs_mbuf_to_flat rc=%d", rc);
        return BLE_ATT_ERR_UNLIKELY;
    }

    size_t        consumed = 0;
    phone_event_t evt;
    phone_parse_result_t pr = phone_protocol_parse(buf, len, &consumed, &evt);
    if (pr == PHONE_PARSE_OK) {
        phone_data_apply(&evt);
    } else {
        ESP_LOGW(TAG, "phone_protocol_parse pr=%d consumed=%u len=%u",
                 (int)pr, (unsigned)consumed, (unsigned)len);
    }
    return 0;
}

// NimBLE's ble_gatts_chr_is_sane() rejects entries with access_cb == NULL
// (table validation, not runtime gate), so even notify-only TX needs a
// non-NULL stub. The central can't actually read or write TX — the flag
// set has neither READ nor WRITE — so this is never invoked in practice;
// notifications go out via ble_gatts_notify_custom() instead.
static int access_tx_cb(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle; (void)attr_handle; (void)ctxt; (void)arg;
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

// --- GATT service table ---------------------------------------------------

static const struct ble_gatt_chr_def chrs[] = {
    {
        .uuid      = &RX_UUID.u,
        .access_cb = access_rx_cb,
        .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    {
        // TX (cluster → phone). Notify-only; the command channel that
        // delivers CALL_ACCEPT etc. plugs in here once #33 lands.
        .uuid       = &TX_UUID.u,
        .access_cb  = access_tx_cb,
        .flags      = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &s_tx_attr_handle,
    },
    { 0 },
};

static const struct ble_gatt_svc_def svcs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &SVC_UUID.u,
        .characteristics = chrs,
    },
    { 0 },
};

// --- GAP / advertising ----------------------------------------------------

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = { 0 };
    fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name                  = (uint8_t *)ble_svc_gap_device_name();
    fields.name_len              = (uint8_t)strlen((const char *)fields.name);
    fields.name_is_complete      = 1;
    fields.uuids128              = (ble_uuid128_t *)&SVC_UUID;
    fields.num_uuids128          = 1;
    fields.uuids128_is_complete  = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(TAG, "ble_gap_adv_set_fields rc=%d", rc); return; }

    struct ble_gap_adv_params params = { 0 };
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &params,
                           gap_event_cb, NULL);
    if (rc != 0) { ESP_LOGE(TAG, "ble_gap_adv_start rc=%d", rc); return; }

    ESP_LOGI(TAG, "advertising as 'V-Rod Cluster'");
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "central connected; handle=%u", s_conn_handle);
        } else {
            ESP_LOGW(TAG, "connect failed; status=%d", event->connect.status);
            start_advertising();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect; reason=%d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        start_advertising();
        return 0;
    case BLE_GAP_EVENT_SUBSCRIBE:
        // Logging the subscribe makes early bring-up much easier: it
        // tells us a central actually opened the TX notify channel
        // rather than just touching the RX write characteristic.
        ESP_LOGI(TAG, "subscribe attr=%u notify=%d indicate=%d",
                 event->subscribe.attr_handle,
                 event->subscribe.cur_notify, event->subscribe.cur_indicate);
        return 0;
    default:
        return 0;
    }
}

// --- Host lifecycle -------------------------------------------------------

static void on_host_reset(int reason)
{
    ESP_LOGW(TAG, "host reset; reason=%d", reason);
}

static void on_host_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) { ESP_LOGE(TAG, "ble_hs_id_infer_auto rc=%d", rc); return; }
    start_advertising();
}

static void nimble_host_task(void *arg)
{
    (void)arg;
    nimble_port_run();           // blocks until nimble_port_stop()
    nimble_port_freertos_deinit();
}

void ble_peripheral_init(void)
{
    // NimBLE expects NVS available for bond / IRK storage before init.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    int rc = nimble_port_init();
    if (rc != 0) { ESP_LOGE(TAG, "nimble_port_init rc=%d", rc); return; }

    ble_hs_cfg.reset_cb = on_host_reset;
    ble_hs_cfg.sync_cb  = on_host_sync;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(svcs);
    if (rc != 0) { ESP_LOGE(TAG, "ble_gatts_count_cfg rc=%d", rc); return; }
    rc = ble_gatts_add_svcs(svcs);
    if (rc != 0) { ESP_LOGE(TAG, "ble_gatts_add_svcs rc=%d", rc); return; }

    ble_svc_gap_device_name_set("V-Rod Cluster");

    nimble_port_freertos_init(nimble_host_task);
}
