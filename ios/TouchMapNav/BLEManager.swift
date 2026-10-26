import CoreBluetooth
import CoreLocation
import Foundation

/// Finds the board ("ESP32-Maps"), connects, and sends POS / NAV / ROUTE messages.
/// Everything runs on the main queue.
final class BLEManager: NSObject, ObservableObject {
    @Published private(set) var status = "Starting Bluetooth..."
    @Published private(set) var isReady = false                 // connected and all characteristics found
    @Published private(set) var cardZoom: String = "-"          // from the board's HELLO message
    @Published private(set) var routeState = ""

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var posChar: CBCharacteristic?
    private var routeChar: CBCharacteristic?
    private var navChar: CBCharacteristic?
    private var statusChar: CBCharacteristic?

    private var routeQueue: [Data] = []
    private var lastRoute: (coords: [CLLocationCoordinate2D], distance: Double, duration: Double)?
    private var retriedUpload = false

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    // MARK: - Sending

    func sendPos(_ data: Data) {
        guard let p = peripheral, let c = posChar, isReady, p.canSendWriteWithoutResponse else { return }
        p.writeValue(data, for: c, type: .withoutResponse)
    }

    func sendNav(_ data: Data) {
        guard let p = peripheral, let c = navChar, isReady, p.canSendWriteWithoutResponse else { return }
        p.writeValue(data, for: c, type: .withoutResponse)
    }

    /// BEGIN, POINTS x N, END; the board answers with ROUTE_ACK on the STATUS characteristic.
    func uploadRoute(_ coords: [CLLocationCoordinate2D], distance: Double, duration: Double) {
        guard let p = peripheral, isReady, routeChar != nil else {
            routeState = "Board not connected: route not sent"
            return
        }
        lastRoute = (coords, distance, duration)
        let pts = Self.thin(coords, maxPoints: Proto.maxRoutePoints)
        let perWrite = max(1, min(12, p.maximumWriteValueLength(for: .withResponse) / 8))

        var queue = [Proto.routeBegin(points: pts.count, distanceM: distance, durationS: duration)]
        var i = 0
        while i < pts.count {
            let n = min(perWrite, pts.count - i)
            queue.append(Proto.routePoints(start: i, pts[i ..< i + n]))
            i += n
        }
        queue.append(Proto.routeEnd)
        routeQueue = queue
        routeState = "Sending route (\(pts.count) points)..."
        writeNextRouteChunk()
    }

    func clearRoute() {
        guard isReady, routeChar != nil else { return }
        lastRoute = nil
        routeQueue = [Proto.routeClear]
        writeNextRouteChunk()
        routeState = ""
    }

    private func writeNextRouteChunk() {
        guard let p = peripheral, let c = routeChar, !routeQueue.isEmpty else { return }
        p.writeValue(routeQueue.removeFirst(), for: c, type: .withResponse)
    }

    /// Keep at most `maxPoints` points, always including the first and the last.
    private static func thin(_ pts: [CLLocationCoordinate2D], maxPoints: Int) -> [CLLocationCoordinate2D] {
        guard pts.count > maxPoints else { return pts }
        let stride = Double(pts.count - 1) / Double(maxPoints - 1)
        return (0 ..< maxPoints).map { pts[min(pts.count - 1, Int((Double($0) * stride).rounded()))] }
    }

    // MARK: - Scanning

    private func startScan() {
        guard central.state == .poweredOn else { return }
        status = "Searching for ESP32-Maps..."
        central.scanForPeripherals(withServices: [BoardUUID.service], options: nil)
    }

    private func resetLink() {
        isReady = false
        posChar = nil; routeChar = nil; navChar = nil; statusChar = nil
        routeQueue.removeAll()
    }
}

// MARK: - CBCentralManagerDelegate

extension BLEManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:    startScan()
        case .poweredOff:   status = "Bluetooth is off: turn it on in Settings"
        case .unauthorized: status = "Bluetooth permission denied: allow it in Settings > Privacy"
        case .unsupported:  status = "Bluetooth LE is not supported on this device"
        default:            status = "Bluetooth not ready"
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        central.stopScan()
        self.peripheral = peripheral
        peripheral.delegate = self
        status = "Connecting to \(peripheral.name ?? "board")..."
        central.connect(peripheral, options: nil)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        status = "Connected, setting up..."
        peripheral.discoverServices([BoardUUID.service])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        resetLink()
        startScan()
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        resetLink()
        status = "Board disconnected, searching..."
        startScan()                       // the board advertises again within 2 s
    }
}

// MARK: - CBPeripheralDelegate

extension BLEManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let service = peripheral.services?.first(where: { $0.uuid == BoardUUID.service }) else {
            status = "Board service not found"
            return
        }
        peripheral.discoverCharacteristics([BoardUUID.pos, BoardUUID.route, BoardUUID.nav, BoardUUID.status],
                                           for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        for c in service.characteristics ?? [] {
            switch c.uuid {
            case BoardUUID.pos:    posChar = c
            case BoardUUID.route:  routeChar = c
            case BoardUUID.nav:    navChar = c
            case BoardUUID.status: statusChar = c; peripheral.setNotifyValue(true, for: c)
            default: break
            }
        }
        if posChar != nil, routeChar != nil, navChar != nil, statusChar != nil {
            isReady = true
            status = "Connected to \(peripheral.name ?? "board")"
        } else {
            status = "Board is missing a characteristic"
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == BoardUUID.status, let b = characteristic.value, let op = b.first else { return }
        switch op {
        case 0x01 where b.count >= 3:                       // HELLO: min zoom, max zoom on the SD card
            cardZoom = "z\(b[1]) to z\(b[2])"
        case 0x02 where b.count >= 4:                       // ROUTE_ACK: points received, ok
            let points = Int(b[1]) | (Int(b[2]) << 8)
            if b[3] == 1 {
                routeState = "Route is on the board (\(points) points)"
                retriedUpload = false
            } else if !retriedUpload, let r = lastRoute {
                retriedUpload = true
                routeState = "Board rejected the route, retrying..."
                uploadRoute(r.coords, distance: r.distance, duration: r.duration)
            } else {
                routeState = "Board rejected the route"
            }
        default: break
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == BoardUUID.route else { return }
        if let error = error {
            routeQueue.removeAll()
            routeState = "Route send failed: \(error.localizedDescription)"
            return
        }
        writeNextRouteChunk()
    }
}
