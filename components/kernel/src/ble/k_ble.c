#include "k_ble.h"
#include "k_proto.h"
#include "k_nav.h"
#include "k_route.h"
#include "k_storage.h"
#include "k_settings.h"
#include "k_events.h"

#include <string.h>
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "k_ble";

/* 5a1b0001-7c0d-4f2e-9b7a-3c6d8e1f2a40, bytes little-endian; [12] = 16-bit id */
#define UUID_BASE(id) BLE_UUID128_INIT( \
    0x40, 0x2a, 0x1f, 0x8e, 0x6d, 0x3c, 0x7a, 0x9b, \
    0x2e, 0x4f, 0x0d, 0x7c, (id), 0x00, 0x1b, 0x5a)

static const ble_uuid128_t UUID_SVC    = UUID_BASE(0x01);
static const ble_uuid128_t UUID_POS    = UUID_BASE(0x02);
static const ble_uuid128_t UUID_ROUTE  = UUID_BASE(0x03);
static const ble_uuid128_t UUID_NAV    = UUID_BASE(0x04);
static const ble_uuid128_t UUID_STATUS = UUID_BASE(0x05);

#define PREFERRED_MTU 185

static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_status_handle;
static uint8_t  s_own_addr_type;
static bool     s_enabled = true;
static bool     s_synced;

static int gap_event(struct ble_gap_event *ev, void *arg);

/* ---- STATUS notifications ---------------------------------------------- */
static void notify_status(const uint8_t *data, size_t len)
{
    if (s_conn == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, (uint16_t)len);
    if (om) {
        ble_gatts_notify_custom(s_conn, s_status_handle, om);
    }
}

static void send_hello(void)
{
    uint8_t b[3];
    int zmin = k_packs_zoom_min();
    int zmax = k_packs_zoom_max();
    kp_encode_hello(b, (uint8_t)(zmin < 0 ? 0 : zmin), (uint8_t)(zmax < 0 ? 0 : zmax));
    notify_status(b, sizeof(b));
}

static void send_ack(uint16_t received, bool ok)
{
    uint8_t b[4];
    kp_encode_route_ack(b, received, ok);
    notify_status(b, sizeof(b));
}

/* ---- Writes from the phone --------------------------------------------- */
static uint32_t s_up_dist, s_up_dur;

static void handle_route(const uint8_t *b, size_t len)
{
    uint8_t op;
    if (kp_route_opcode(b, len, &op) != KP_OK) {
        return;
    }
    switch (op) {
    case KP_ROUTE_BEGIN: {
        kp_route_begin_t rb;
        if (kp_decode_route_begin(b, len, &rb) == KP_OK && k_route_begin(rb.points)) {
            s_up_dist = rb.distance_m;
            s_up_dur = rb.duration_s;
        } else {
            k_route_clear();
        }
        break;
    }
    case KP_ROUTE_POINTS: {
        kp_pt_t pts[KP_POINTS_PER_WRITE * 2];     /* tolerate bigger chunks if MTU allows */
        uint16_t start;
        size_t n;
        if (kp_decode_route_points(b, len, &start, pts, sizeof(pts) / sizeof(pts[0]), &n) == KP_OK) {
            k_route_put(start, pts, n);
        }
        break;
    }
    case KP_ROUTE_END: {
        uint16_t got = 0;
        bool ok = k_route_end(&got);
        k_nav_set_route(ok, ok ? s_up_dist : 0, ok ? s_up_dur : 0);
        send_ack(got, ok);
        break;
    }
    case KP_ROUTE_CLEAR:
        k_route_clear();                          /* NAV-05 */
        k_nav_set_route(false, 0, 0);
        k_nav_clear_turn();
        break;
    }
}

static int chr_write(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn; (void)attr;
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    uint8_t buf[256];
    uint16_t len = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &len) != 0) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    switch ((uintptr_t)arg) {
    case 0x02: {
        kp_pos_t p;
        if (kp_decode_pos(buf, len, &p) == KP_OK) {
            k_nav_set_pos(&p);
        }
        break;
    }
    case 0x03:
        handle_route(buf, len);
        break;
    case 0x04: {
        kp_nav_t n;
        if (kp_decode_nav(buf, len, &n) == KP_OK) {
            k_nav_set_turn(&n);
        }
        break;
    }
    }
    return 0;
}

static int chr_status_access(uint16_t c, uint16_t a, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)c; (void)a; (void)ctxt; (void)arg;
    return 0;   /* notify-only */
}

static const struct ble_gatt_svc_def s_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_SVC.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            { .uuid = &UUID_POS.u,   .access_cb = chr_write, .arg = (void *)0x02,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP },
            { .uuid = &UUID_ROUTE.u, .access_cb = chr_write, .arg = (void *)0x03,
              .flags = BLE_GATT_CHR_F_WRITE },       /* with response: ordered, flow-controlled */
            { .uuid = &UUID_NAV.u,   .access_cb = chr_write, .arg = (void *)0x04,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP },
            { .uuid = &UUID_STATUS.u, .access_cb = chr_status_access,
              .val_handle = &s_status_handle, .flags = BLE_GATT_CHR_F_NOTIFY },
            { 0 },
        },
    },
    { 0 },
};

/* ---- Advertising ------------------------------------------------------- */
static void advertise(void)
{
    if (!s_enabled || !s_synced) {
        return;
    }
    struct ble_hs_adv_fields adv = { 0 };
    adv.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv.uuids128 = (ble_uuid128_t *)&UUID_SVC;
    adv.num_uuids128 = 1;
    adv.uuids128_is_complete = 1;               /* BLE-01: service UUID in the advert */
    ble_gap_adv_set_fields(&adv);

    const char *name = k_settings()->device_name;
    struct ble_hs_adv_fields rsp = { 0 };
    rsp.name = (uint8_t *)name;
    rsp.name_len = (uint8_t)strlen(name);
    rsp.name_is_complete = 1;                   /* BLE-01: name in the scan response */
    ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params ap = { 0 };
    ap.conn_mode = BLE_GAP_CONN_MODE_UND;
    ap.disc_mode = BLE_GAP_DISC_MODE_GEN;
    int rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &ap, gap_event, NULL);
    if (rc == 0 || rc == BLE_HS_EALREADY) {
        k_nav_set_link(K_LINK_ADVERTISING);
    } else {
        ESP_LOGE(TAG, "adv start rc=%d", rc);
    }
}

static int gap_event(struct ble_gap_event *ev, void *arg)
{
    (void)arg;
    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (ev->connect.status == 0) {
            s_conn = ev->connect.conn_handle;
            k_nav_set_link(K_LINK_CONNECTED);
            ESP_LOGI(TAG, "connected");
        } else {
            advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        ESP_LOGI(TAG, "disconnected, reason %d", ev->disconnect.reason);
        advertise();                            /* BLE-05 */
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (ev->subscribe.attr_handle == s_status_handle && ev->subscribe.cur_notify) {
            send_hello();                       /* BLE-03 */
        }
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU %u", ev->mtu.value);
        break;
    }
    return 0;
}

/* ---- Host lifecycle ---------------------------------------------------- */
static void on_sync(void)
{
    ble_hs_id_infer_auto(0, &s_own_addr_type);
    s_synced = true;
    advertise();
}

static void on_reset(int reason)
{
    s_synced = false;
    ESP_LOGW(TAG, "host reset, reason %d", reason);
}

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void k_ble_start(void)
{
    s_enabled = k_settings()->ble_on;

    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ESP_ERROR_CHECK(ble_gatts_count_cfg(s_svcs));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(s_svcs));
    ble_svc_gap_device_name_set(k_settings()->device_name);

    ble_att_set_preferred_mtu(PREFERRED_MTU);   /* BLE-04 */

    /* TODO(BLE-06): passkey pairing and bonded-phone list.
     * Configure ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY, sm_bonding = 1,
     * sm_mitm = 1 and require encryption on the write characteristics. */

    nimble_port_freertos_init(host_task);
    if (!s_enabled) {
        k_nav_set_link(K_LINK_OFF);
    }
}

void k_ble_set_enabled(bool on)
{
    s_enabled = on;
    if (on) {
        advertise();
    } else {
        ble_gap_adv_stop();
        if (s_conn != BLE_HS_CONN_HANDLE_NONE) {
            ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        }
        k_nav_set_link(K_LINK_OFF);
    }
}

bool k_ble_enabled(void)
{
    return s_enabled;
}
