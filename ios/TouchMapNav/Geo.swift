import CoreLocation
import MapKit

/// Small flat-earth geometry helpers; accurate enough over the few hundred metres of one route step.
enum Geo {
    static let earthRadius = 6_371_000.0

    /// Metres east / north of `origin`.
    static func xy(_ c: CLLocationCoordinate2D, around origin: CLLocationCoordinate2D) -> (x: Double, y: Double) {
        let x = (c.longitude - origin.longitude) * .pi / 180 * cos(origin.latitude * .pi / 180) * earthRadius
        let y = (c.latitude - origin.latitude) * .pi / 180 * earthRadius
        return (x, y)
    }

    static func distance(_ a: CLLocationCoordinate2D, _ b: CLLocationCoordinate2D) -> Double {
        let d = xy(b, around: a)
        return hypot(d.x, d.y)
    }

    /// Compass bearing 0..360 from a to b.
    static func bearing(from a: CLLocationCoordinate2D, to b: CLLocationCoordinate2D) -> Double {
        let lat1 = a.latitude * .pi / 180, lat2 = b.latitude * .pi / 180
        let dLon = (b.longitude - a.longitude) * .pi / 180
        let y = sin(dLon) * cos(lat2)
        let x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon)
        return (atan2(y, x) * 180 / .pi + 360).truncatingRemainder(dividingBy: 360)
    }

    static func length(_ pts: [CLLocationCoordinate2D]) -> Double {
        guard pts.count > 1 else { return 0 }
        return (1 ..< pts.count).reduce(0) { $0 + distance(pts[$1 - 1], pts[$1]) }
    }

    /// Shortest distance from `p` to the polyline, and how far along the polyline (metres from its start)
    /// the closest point lies.
    static func project(_ p: CLLocationCoordinate2D, onto pts: [CLLocationCoordinate2D])
        -> (dist: Double, along: Double) {
        guard pts.count > 1 else {
            return (pts.first.map { distance(p, $0) } ?? .infinity, 0)
        }
        var best = Double.infinity, bestAlong = 0.0, walked = 0.0
        for i in 1 ..< pts.count {
            let a = xy(pts[i - 1], around: p), b = xy(pts[i], around: p)      // p is the origin (0, 0)
            let dx = b.x - a.x, dy = b.y - a.y
            let seg2 = dx * dx + dy * dy
            let segLen = sqrt(seg2)
            var t = seg2 > 0 ? ((0 - a.x) * dx + (0 - a.y) * dy) / seg2 : 0
            t = min(1, max(0, t))
            let d = hypot(a.x + t * dx, a.y + t * dy)
            if d < best {
                best = d
                bestAlong = walked + t * segLen
            }
            walked += segLen
        }
        return (best, bestAlong)
    }
}

extension MKPolyline {
    var coordinates: [CLLocationCoordinate2D] {
        var out = [CLLocationCoordinate2D](repeating: CLLocationCoordinate2D(), count: pointCount)
        getCoordinates(&out, range: NSRange(location: 0, length: pointCount))
        return out
    }
}
