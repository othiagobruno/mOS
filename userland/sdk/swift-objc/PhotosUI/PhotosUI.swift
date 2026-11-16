// mOS — overlay Swift do PhotosUI do Mac Catalyst (/System/iOSSupport/usr/lib/swift/libswiftPhotosUI.dylib), com os
// nomes da Apple: PHPickerFilter, PHPickerConfiguration (AssetRepresentationMode, Selection), PHPickerMode,
// PHPickerResult, o protocolo Swift PHPickerViewControllerDelegate e as extensões de PHPickerViewController. Cada struct
// guarda os valores em Swift e conversa com as classes ObjC do PhotosUI.framework do mOS (__PHPicker...).
import ObjectiveC
import Foundation
import UIKit

// fora do módulo Foundation o importador do mOS não faz a ponte nos tipos importados (NSArray, NSString): é explícita
@inline(__always) func _puArray<T>(_ a: NSArray) -> [T] { [T]._unconditionallyBridgeFromObjectiveC(a) }

public struct PHPickerFilter: Equatable, Hashable, CustomStringConvertible {
    let _filter: __PHPickerFilter
    init(_ f: __PHPickerFilter) { _filter = f }

    public static var images: PHPickerFilter { PHPickerFilter(__PHPickerFilter.images) }
    public static var videos: PHPickerFilter { PHPickerFilter(__PHPickerFilter.videos) }
    public static var livePhotos: PHPickerFilter { PHPickerFilter(__PHPickerFilter.livePhotos) }
    public static var depthEffectPhotos: PHPickerFilter { PHPickerFilter(__PHPickerFilter.depthEffectPhotos) }
    public static var bursts: PHPickerFilter { PHPickerFilter(__PHPickerFilter.bursts) }
    public static var panoramas: PHPickerFilter { PHPickerFilter(__PHPickerFilter.panoramas) }
    public static var screenshots: PHPickerFilter { PHPickerFilter(__PHPickerFilter.screenshots) }
    public static var screenRecordings: PHPickerFilter { PHPickerFilter(__PHPickerFilter.screenRecordings) }
    public static var cinematicVideos: PHPickerFilter { PHPickerFilter(__PHPickerFilter.cinematicVideos) }
    public static var slomoVideos: PHPickerFilter { PHPickerFilter(__PHPickerFilter.slomoVideos) }
    public static var timelapseVideos: PHPickerFilter { PHPickerFilter(__PHPickerFilter.timelapseVideos) }
    public static var spatialMedia: PHPickerFilter { PHPickerFilter(__PHPickerFilter.spatialMedia) }

    public static func any(of subfilters: [PHPickerFilter]) -> PHPickerFilter {
        PHPickerFilter(__PHPickerFilter.anyFilter(matchingSubfilters: subfilters.map { $0._filter }._bridgeToObjectiveC()))
    }
    public static func all(of subfilters: [PHPickerFilter]) -> PHPickerFilter {
        PHPickerFilter(__PHPickerFilter.allFilter(matchingSubfilters: subfilters.map { $0._filter }._bridgeToObjectiveC()))
    }
    public static func not(_ filter: PHPickerFilter) -> PHPickerFilter {
        PHPickerFilter(__PHPickerFilter.notFilter(ofSubfilter: filter._filter))
    }

    public static func == (a: PHPickerFilter, b: PHPickerFilter) -> Bool { a._filter.isEqual(b._filter) }
    public func hash(into h: inout Hasher) { h.combine(_filter.hash) }
    public var description: String { "PHPickerFilter(\(_filter))" }
}

public struct PHPickerMode: Equatable, Hashable {
    let rawValue: Int
    public static var `default`: PHPickerMode { PHPickerMode(rawValue: 0) }
    public static var compact: PHPickerMode { PHPickerMode(rawValue: 1) }
}

public struct PHPickerConfiguration: Equatable, Hashable {
    public enum AssetRepresentationMode: Int, Equatable, Hashable {
        case automatic = 0
        case current = 1
        case compatible = 2
    }
    public enum Selection: Int, Equatable, Hashable {
        case `default` = 0
        case ordered = 1
        case continuous = 2
        case continuousAndOrdered = 3
    }

    public var preferredAssetRepresentationMode: AssetRepresentationMode = .automatic
    public var selection: Selection = .default
    /// 0 = sem limite (o padrão da Apple é 1)
    public var selectionLimit: Int = 1
    public var filter: PHPickerFilter?
    public var preselectedAssetIdentifiers: [String] = []
    public var mode: PHPickerMode = .default
    public var disabledCapabilities: PHPickerCapabilities = []
    public var edgesWithoutContentMargins: NSDirectionalRectEdge = []
    let _library: PHPhotoLibrary?

    public init() { _library = nil }
    public init(photoLibrary: PHPhotoLibrary) { _library = photoLibrary }

    init(_ c: __PHPickerConfiguration) {
        _library = c._mosLibrary()
        preferredAssetRepresentationMode = AssetRepresentationMode(rawValue: c.preferredAssetRepresentationMode) ?? .automatic
        selection = Selection(rawValue: c.selection) ?? .default
        selectionLimit = c.selectionLimit
        filter = c.filter.map { PHPickerFilter($0) }
        preselectedAssetIdentifiers = _puArray(c.preselectedAssetIdentifiers)
        mode = PHPickerMode(rawValue: c.mode)
        disabledCapabilities = c.disabledCapabilities
        edgesWithoutContentMargins = c.edgesWithoutContentMargins
    }

    func _objc() -> __PHPickerConfiguration {
        let c = _library.map { __PHPickerConfiguration(photoLibrary: $0) } ?? __PHPickerConfiguration()
        c.preferredAssetRepresentationMode = preferredAssetRepresentationMode.rawValue
        c.selection = selection.rawValue
        c.selectionLimit = selectionLimit
        c.filter = filter?._filter
        c.preselectedAssetIdentifiers = preselectedAssetIdentifiers._bridgeToObjectiveC()
        c.mode = mode.rawValue
        c.disabledCapabilities = disabledCapabilities
        c.edgesWithoutContentMargins = edgesWithoutContentMargins
        return c
    }

    public static func == (a: PHPickerConfiguration, b: PHPickerConfiguration) -> Bool {
        a.preferredAssetRepresentationMode == b.preferredAssetRepresentationMode && a.selection == b.selection &&
            a.selectionLimit == b.selectionLimit && a.filter == b.filter &&
            a.preselectedAssetIdentifiers == b.preselectedAssetIdentifiers && a.mode == b.mode &&
            a.disabledCapabilities == b.disabledCapabilities && a.edgesWithoutContentMargins == b.edgesWithoutContentMargins &&
            a._library === b._library
    }
    public func hash(into h: inout Hasher) {
        h.combine(preferredAssetRepresentationMode)
        h.combine(selection)
        h.combine(selectionLimit)
        h.combine(filter)
        h.combine(preselectedAssetIdentifiers)
        h.combine(mode)
    }
}

public struct PHPickerResult: Equatable, Hashable {
    public let itemProvider: NSItemProvider
    public let assetIdentifier: String?

    init(_ r: __PHPickerResult) {
        itemProvider = r.itemProvider
        assetIdentifier = r.assetIdentifier.map { String._unconditionallyBridgeFromObjectiveC($0) }
    }
    public static func == (a: PHPickerResult, b: PHPickerResult) -> Bool {
        a.itemProvider === b.itemProvider && a.assetIdentifier == b.assetIdentifier
    }
    public func hash(into h: inout Hasher) {
        h.combine(ObjectIdentifier(itemProvider))
        h.combine(assetIdentifier)
    }
}

public protocol PHPickerViewControllerDelegate: AnyObject {
    func picker(_ picker: PHPickerViewController, didFinishPicking results: [PHPickerResult])
}

/// Ponte entre o delegate Swift e o protocolo ObjC que o PhotosUI.framework chama. O controlador a retém por objeto
/// associado; o delegate do app fica fraco, como na Apple.
final class _PHPickerDelegateBridge: NSObject, __PHPickerViewControllerDelegate {
    weak var target: PHPickerViewControllerDelegate?
    init(_ t: PHPickerViewControllerDelegate) { target = t }
    func picker(_ picker: PHPickerViewController, didFinishPicking results: NSArray) {
        let objs: [__PHPickerResult] = _puArray(results)
        target?.picker(picker, didFinishPicking: objs.map { PHPickerResult($0) })
    }
}

private var _bridgeKey: UInt8 = 0

extension PHPickerViewController {
    public var configuration: PHPickerConfiguration { PHPickerConfiguration(__configuration) }

    public convenience init(configuration: PHPickerConfiguration) {
        self.init(__configuration: configuration._objc())
    }

    public var delegate: PHPickerViewControllerDelegate? {
        get { (objc_getAssociatedObject(self, &_bridgeKey) as? _PHPickerDelegateBridge)?.target }
        set {
            let bridge = newValue.map { _PHPickerDelegateBridge($0) }
            objc_setAssociatedObject(self, &_bridgeKey, bridge, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
            __delegate = bridge
        }
    }
}
