import CoreLocation
import Foundation

/// GPS from the phone. Keeps running in the background while a trip is active.
final class LocationService: NSObject, ObservableObject, CLLocationManagerDelegate {
    @Published private(set) var last: CLLocation?
    @Published private(set) var permission = "Location permission needed"

    var onLocation: ((CLLocation) -> Void)?
    private let manager = CLLocationManager()

    override init() {
        super.init()
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyBestForNavigation
        manager.activityType = .automotiveNavigation
        manager.pausesLocationUpdatesAutomatically = false
        manager.distanceFilter = kCLDistanceFilterNone

        // Turning this on without the "Location updates" background mode crashes the app, so check first.
        let modes = Bundle.main.object(forInfoDictionaryKey: "UIBackgroundModes") as? [String] ?? []
        if modes.contains("location") {
            manager.allowsBackgroundLocationUpdates = true
            manager.showsBackgroundLocationIndicator = true
        }
    }

    func start() {
        manager.requestWhenInUseAuthorization()
        updatePermission()
        manager.startUpdatingLocation()
    }

    private func updatePermission() {
        switch manager.authorizationStatus {
        case .authorizedAlways, .authorizedWhenInUse: permission = "GPS on"
        case .denied, .restricted: permission = "Location denied: allow it in Settings > Privacy"
        default: permission = "Waiting for location permission"
        }
    }

    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        updatePermission()
        if manager.authorizationStatus == .authorizedAlways || manager.authorizationStatus == .authorizedWhenInUse {
            manager.startUpdatingLocation()
        }
    }

    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        guard let loc = locations.last, loc.horizontalAccuracy >= 0 else { return }
        last = loc
        onLocation?(loc)
    }

    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        permission = "GPS error: \(error.localizedDescription)"
    }
}
