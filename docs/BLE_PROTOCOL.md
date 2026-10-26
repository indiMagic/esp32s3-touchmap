# BLE protocol (for the phone page)

Implemented in `components/kernel/src/proto/k_proto.c` and verified by `test/host/test_main.c`.
All numbers are **little-endian**. One GATT service; the phone writes, the board notifies.

Base UUID `5a1b0000-7c0d-4f2e-9b7a-3c6d8e1f2a40`; the 16-bit part replaces `0000`.

| Characteristic | UUID | Access | Content |
|---|---|---|---|
| Service | `5a1b0001-7c0d-4f2e-9b7a-3c6d8e1f2a40` | | Navigator service |
| POS | `5a1b0002-...` | write / write without response | 14 bytes |
| ROUTE | `5a1b0003-...` | write (with response) | BEGIN, POINTS, END, CLEAR |
| NAV | `5a1b0004-...` | write / write without response | 14 bytes + text |
| STATUS | `5a1b0005-...` | notify | HELLO, ROUTE_ACK |

Advertising: service UUID in the advert, device name in the scan response (default `ESP32-Maps`).
The board asks for MTU 185, so a 12-point route chunk (99 bytes) fits in one write.

## Messages

```
POS (14 B)            i32 lat_e6 | i32 lon_e6 | u16 heading (0.1 deg) | u16 speed (0.1 m/s) | u16 accuracy m
ROUTE BEGIN (11 B)    u8 0x01 | u16 points | u32 distance m | u32 duration s
ROUTE POINTS (3+8N)   u8 0x02 | u16 start index | N x { i32 lat_e6, i32 lon_e6 }     (N <= 12 per write)
ROUTE END (1 B)       u8 0x03
ROUTE CLEAR (1 B)     u8 0x04
NAV (14 B + text)     u8 maneuver | u8 flags | u32 to turn m | u32 remaining m | u32 remaining s | text (<= 47 chars)
STATUS HELLO (3 B)    u8 0x01 | u8 min zoom | u8 max zoom        sent when the phone subscribes
STATUS ROUTE_ACK (4)  u8 0x02 | u16 points received | u8 ok      sent after ROUTE END
```

Constraints enforced by the firmware: at most 12,000 route points; latitude within +/-90 and longitude within +/-180;
POINTS must arrive in order starting at index 0 (`start` equals the number of points already received);
maneuver code 0..13; NAV text is truncated at 47 characters.

### Maneuver codes

| Code | Meaning | | Code | Meaning |
|---|---|---|---|---|
| 0 | straight | | 7 | slight left |
| 1 | slight right | | 8 | roundabout (right-hand) |
| 2 | right | | 9 | roundabout (left-hand) |
| 3 | sharp right | | 10 | merge |
| 4 | U-turn | | 11 | fork |
| 5 | sharp left | | 12 | arrive |
| 6 | left | | 13 | depart |

### NAV flags

`0x01` arrived, `0x02` off route, `0x04` rerouting.

## Session

1. Connect, subscribe to STATUS, read HELLO (zoom range available on the card).
2. Upload: `BEGIN`, `POINTS` x N, `END`; wait for `ROUTE_ACK` with `ok = 1`.
   If `ok = 0`, resend from `BEGIN`.
3. Loop at about 1 Hz: write `POS`, then `NAV`.
4. On arrival or user request: `CLEAR`. The board advertises again after a disconnect (test T-11 expects within 2 s).

If no `POS` arrives for 10 s the board shows the position as unknown (NAV-04).
