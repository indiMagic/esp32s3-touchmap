import CoreBluetooth
import CoreLocation
import Foundation

/// BLE wire format of the ESP32 Touch Map board. Mirrors docs/BLE_PROTOCOL.md and
/// components/kernel/src/proto/k_proto.c. All numbers are little-endian.
enum BoardUUID {
    static let service = CBUUID(string: "5A1B0001-7C0D-4F2E-9B7A-3C6D8E1F2A40")
    static let pos     = CBUUID(string: "5A1B0002-7C0D-4F2E-9B7A-3C6D8E1F2A40")   // write / write w/o response
    static let route   = CBUUID(string: "5A1B0003-7C0D-4F2E-9B7A-3C6D8E1F2A40")   // write (with response)
    static let nav     = CBUUID(string: "5A1B0004-7C0D-4F2E-9B7A-3C6D8E1F2A40")   // write / write w/o response
    static let status  = CBUUID(string: "5A1B0005-7C0D-4F2E-9B7A-3C6D8E1F2A40")   // notify
}

/// Arrow shown on the board. Raw values are the maneuver codes 0..13.
enum Maneuver: UInt8 {
    case straight = 0, slightRight, right, sharpRight, uturn, sharpLeft, left, slightLeft
    case roundaboutRight, roundaboutLeft, merge, fork, arrive, depart
}

struct NavFlags: OptionSet {
    let rawValue: UInt8
    static let arrived   = NavFlags(rawValue: 0x01)
    static let offRoute  = NavFlags(rawValue: 0x02)
    static let rerouting = NavFlags(rawValue: 0x04)
}

extension Data {
    mutating func appendLE<T: FixedWidthInteger>(_ value: T) {
        var le = value.littleEndian
        Swift.withUnsafeBytes(of: &le) { self.append(contentsOf: $0) }
    }
}

enum Proto {
    static let maxRoutePoints = 12_000
    static let navTextMax = 47

    private static func e6(_ degrees: Double) -> Int32 {
        Int32((degrees * 1_000_000).rounded())
    }

    /// POS (14 bytes): lat_e6, lon_e6, heading 0.1 deg, speed 0.1 m/s, accuracy m.
    static func pos(lat: Double, lon: Double, headingDeg: Double, speedMps: Double, accuracyM: Double) -> Data {
        var d = Data()
        d.appendLE(e6(lat))
        d.appendLE(e6(lon))
        let heading = headingDeg >= 0 ? min(3599, Int((headingDeg * 10).rounded())) : 0
        d.appendLE(UInt16(heading))
        d.appendLE(UInt16(clamping: Int((max(0, speedMps) * 10).rounded())))
        d.appendLE(UInt16(clamping: Int(max(0, accuracyM).rounded())))
        return d
    }

    static func pos(_ loc: CLLocation) -> Data {
        pos(lat: loc.coordinate.latitude, lon: loc.coordinate.longitude,
            headingDeg: loc.course, speedMps: loc.speed, accuracyM: loc.horizontalAccuracy)
    }

    /// NAV (14 bytes + text, text at most 47 ASCII characters).
    static func nav(maneuver: Maneuver, flags: NavFlags, toTurnM: Double, remainM: Double,
                    remainS: Double, text: String) -> Data {
        var d = Data()
        d.append(maneuver.rawValue)
        d.append(flags.rawValue)
        d.appendLE(UInt32(clamping: Int(max(0, toTurnM).rounded())))
        d.appendLE(UInt32(clamping: Int(max(0, remainM).rounded())))
        d.appendLE(UInt32(clamping: Int(max(0, remainS).rounded())))
        d.append(sanitize(text))
        return d
    }

    /// The board font is plain ASCII: strip accents, replace anything else, cut to 47 characters.
    static func sanitize(_ text: String) -> Data {
        let folded = text.folding(options: [.diacriticInsensitive], locale: .current)
        var ascii = ""
        for u in folded.unicodeScalars {
            ascii.unicodeScalars.append((u.value >= 32 && u.value < 127) ? u : " ")
        }
        return Data(ascii.prefix(navTextMax).utf8)
    }

    // ROUTE opcodes
    static func routeBegin(points: Int, distanceM: Double, durationS: Double) -> Data {
        var d = Data([0x01])
        d.appendLE(UInt16(points))
        d.appendLE(UInt32(clamping: Int(max(0, distanceM).rounded())))
        d.appendLE(UInt32(clamping: Int(max(0, durationS).rounded())))
        return d
    }

    static func routePoints(start: Int, _ pts: ArraySlice<CLLocationCoordinate2D>) -> Data {
        var d = Data([0x02])
        d.appendLE(UInt16(start))
        for p in pts {
            d.appendLE(e6(p.latitude))
            d.appendLE(e6(p.longitude))
        }
        return d
    }

    static let routeEnd = Data([0x03])
    static let routeClear = Data([0x04])
}
