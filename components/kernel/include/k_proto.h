/**
 * @file k_proto.h
 * @brief BLE wire format: encode/decode of POS, ROUTE, NAV and STATUS messages.
 *
 * PORTABLE: no ESP-IDF includes, so it builds and is unit-tested on the host.
 * All multi-byte numbers are little-endian (SRS section 5, Figure 11).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Limits ------------------------------------------------------------ */
#define KP_MAX_ROUTE_POINTS   12000
#define KP_POINTS_PER_WRITE   12
#define KP_NAV_TEXT_MAX       47        /* + NUL; keeps NAV within a 185-byte MTU */

/* ---- Opcodes ----------------------------------------------------------- */
enum {
    KP_ROUTE_BEGIN  = 0x01,
    KP_ROUTE_POINTS = 0x02,
    KP_ROUTE_END    = 0x03,
    KP_ROUTE_CLEAR  = 0x04,
};
enum {
    KP_STATUS_HELLO     = 0x01,
    KP_STATUS_ROUTE_ACK = 0x02,
};

/* ---- Maneuver codes (0..13) drawn as arrows by the UI ------------------- */
typedef enum {
    KP_MAN_STRAIGHT = 0,
    KP_MAN_SLIGHT_RIGHT,
    KP_MAN_RIGHT,
    KP_MAN_SHARP_RIGHT,
    KP_MAN_UTURN,
    KP_MAN_SHARP_LEFT,
    KP_MAN_LEFT,
    KP_MAN_SLIGHT_LEFT,
    KP_MAN_ROUNDABOUT_RIGHT,
    KP_MAN_ROUNDABOUT_LEFT,
    KP_MAN_MERGE,
    KP_MAN_FORK,
    KP_MAN_ARRIVE,
    KP_MAN_DEPART,
    KP_MAN_COUNT
} kp_maneuver_t;

/* NAV flags */
#define KP_NAVF_ARRIVED   0x01
#define KP_NAVF_OFFROUTE  0x02
#define KP_NAVF_REROUTING 0x04

/* ---- Decoded messages -------------------------------------------------- */
typedef struct {
    int32_t  lat_e6;
    int32_t  lon_e6;
    uint16_t heading_d10;   /* 0.1 deg */
    uint16_t speed_d10;     /* 0.1 m/s */
    uint16_t accuracy_m;
} kp_pos_t;

typedef struct {
    int32_t lat_e6;
    int32_t lon_e6;
} kp_pt_t;

typedef struct {
    uint16_t points;
    uint32_t distance_m;
    uint32_t duration_s;
} kp_route_begin_t;

typedef struct {
    uint8_t  maneuver;
    uint8_t  flags;
    uint32_t to_turn_m;
    uint32_t remain_m;
    uint32_t remain_s;
    char     text[KP_NAV_TEXT_MAX + 1];
} kp_nav_t;

typedef enum {
    KP_OK = 0,
    KP_ERR_SHORT,       /* buffer shorter than the fixed part */
    KP_ERR_OPCODE,
    KP_ERR_RANGE,       /* value outside limits */
} kp_err_t;

/* ---- Decode (phone -> device) ------------------------------------------ */
kp_err_t kp_decode_pos(const uint8_t *b, size_t len, kp_pos_t *out);
kp_err_t kp_decode_nav(const uint8_t *b, size_t len, kp_nav_t *out);

/** Returns the opcode in @p op; payload decoders below check it. */
kp_err_t kp_route_opcode(const uint8_t *b, size_t len, uint8_t *op);
kp_err_t kp_decode_route_begin(const uint8_t *b, size_t len, kp_route_begin_t *out);

/**
 * Decode a POINTS chunk. Writes up to @p cap pairs into @p pts and returns the
 * pair count in @p n. @p start receives the index of the first pair.
 */
kp_err_t kp_decode_route_points(const uint8_t *b, size_t len, uint16_t *start,
                                kp_pt_t *pts, size_t cap, size_t *n);

/* ---- Encode (device -> phone, STATUS characteristic) -------------------- */
size_t kp_encode_hello(uint8_t out[3], uint8_t min_zoom, uint8_t max_zoom);
size_t kp_encode_route_ack(uint8_t out[4], uint16_t points_received, bool ok);

/* ---- Encode (phone side, used by tests and the reference page) ---------- */
size_t kp_encode_pos(uint8_t out[14], const kp_pos_t *p);
size_t kp_encode_route_begin(uint8_t out[11], const kp_route_begin_t *b);
size_t kp_encode_route_points(uint8_t *out, size_t cap, uint16_t start,
                              const kp_pt_t *pts, size_t n);
size_t kp_encode_nav(uint8_t *out, size_t cap, const kp_nav_t *nav);

#ifdef __cplusplus
}
#endif
