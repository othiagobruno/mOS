// mOS — overlay Swift do UIKit (Catalyst), terceira leva: as configurações por valor da Apple — UIButton.Configuration,
// UIListContentConfiguration, UIBackgroundConfiguration, UICellConfigurationState, UICellAccessory,
// UICollectionLayoutListConfiguration, UIListSeparatorConfiguration — e os acessores das células. Guardam o estado com os
// nomes e as assinaturas da Apple; o desenho de células e botões ainda é o simples do UIKit.framework do mOS.
import UIKit
import Foundation
import CoreGraphics
import ObjectiveC

/// caixa para guardar um valor Swift num objeto ObjC (objc_setAssociatedObject)
internal final class _UIBox<T>: NSObject {
    var value: T
    init(_ v: T) { value = v; super.init() }
}
internal func _uiGet<T>(_ o: AnyObject, _ key: UnsafeRawPointer) -> T? { (objc_getAssociatedObject(o, key) as? _UIBox<T>)?.value }
internal func _uiSet<T>(_ o: AnyObject, _ key: UnsafeRawPointer, _ v: T?) {
    objc_setAssociatedObject(o, key, v.map { _UIBox($0) }, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
}
internal let _uiKeys = UnsafeMutableRawPointer.allocate(byteCount: 16, alignment: 8)
internal var _kButtonConfig: UnsafeRawPointer { UnsafeRawPointer(_uiKeys) }
internal var _kContentConfig: UnsafeRawPointer { UnsafeRawPointer(_uiKeys + 1) }
internal var _kBackgroundConfig: UnsafeRawPointer { UnsafeRawPointer(_uiKeys + 2) }
internal var _kUpdateHandler: UnsafeRawPointer { UnsafeRawPointer(_uiKeys + 3) }
internal var _kAccessories: UnsafeRawPointer { UnsafeRawPointer(_uiKeys + 4) }

internal func _uiInsetsEqual(_ a: NSDirectionalEdgeInsets, _ b: NSDirectionalEdgeInsets) -> Bool {
    a.top == b.top && a.leading == b.leading && a.bottom == b.bottom && a.trailing == b.trailing
}

// MARK: - estado de configuração da célula

public struct UICellConfigurationState: UIConfigurationState, Hashable {
    public var traitCollection: UITraitCollection
    public var isDisabled = false
    public var isHighlighted = false
    public var isSelected = false
    public var isFocused = false
    public var isEditing = false
    public var isSwiped = false
    public var isExpanded = false
    public var isReordering = false
    public var cellDropState = 0

    public init(traitCollection: UITraitCollection) { self.traitCollection = traitCollection }

    public static func == (a: Self, b: Self) -> Bool {
        a.isDisabled == b.isDisabled && a.isHighlighted == b.isHighlighted && a.isSelected == b.isSelected &&
            a.isFocused == b.isFocused && a.isEditing == b.isEditing && a.isExpanded == b.isExpanded
    }
    public func hash(into h: inout Hasher) { h.combine(isSelected); h.combine(isHighlighted); h.combine(isExpanded) }
}

extension UICellConfigurationState: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> __UICellConfigurationState { __UICellConfigurationState() }
    public static func _forceBridgeFromObjectiveC(_ x: __UICellConfigurationState, result: inout UICellConfigurationState?) {
        result = UICellConfigurationState(traitCollection: UITraitCollection())
    }
    public static func _conditionallyBridgeFromObjectiveC(_ x: __UICellConfigurationState, result: inout UICellConfigurationState?) -> Bool {
        result = UICellConfigurationState(traitCollection: UITraitCollection()); return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: __UICellConfigurationState?) -> UICellConfigurationState {
        UICellConfigurationState(traitCollection: UITraitCollection())
    }
}

// MARK: - UIBackgroundConfiguration

public struct UIBackgroundConfiguration: Equatable {
    public var customView: UIView?
    public var cornerRadius: CGFloat = 0
    public var backgroundInsets = NSDirectionalEdgeInsets()
    public var edgesAddingLayoutMarginsToBackgroundInsets: NSDirectionalRectEdge = []
    public var backgroundColor: UIColor?
    public var backgroundColorTransformer: UIConfigurationColorTransformer?
    public var visualEffect: NSObject?
    public var image: UIImage?
    public var strokeColor: UIColor?
    public var strokeColorTransformer: UIConfigurationColorTransformer?
    public var strokeWidth: CGFloat = 0
    public var strokeOutset: CGFloat = 0

    public init() {}
    public static func clear() -> UIBackgroundConfiguration { UIBackgroundConfiguration() }
    public static func listPlainCell() -> UIBackgroundConfiguration { UIBackgroundConfiguration() }
    public static func listGroupedCell() -> UIBackgroundConfiguration { var c = UIBackgroundConfiguration(); c.cornerRadius = 10; return c }
    public static func listSidebarCell() -> UIBackgroundConfiguration { UIBackgroundConfiguration() }
    public static func listPlainHeaderFooter() -> UIBackgroundConfiguration { UIBackgroundConfiguration() }
    public static func listGroupedHeaderFooter() -> UIBackgroundConfiguration { UIBackgroundConfiguration() }
    public func updated(for state: UIConfigurationState) -> UIBackgroundConfiguration { self }
    public func resolvedBackgroundColor(for tintColor: UIColor) -> UIColor { backgroundColor ?? tintColor }

    public static func == (a: Self, b: Self) -> Bool {
        a.customView === b.customView && a.cornerRadius == b.cornerRadius && _uiInsetsEqual(a.backgroundInsets, b.backgroundInsets) &&
            a.backgroundColor === b.backgroundColor && a.strokeColor === b.strokeColor && a.strokeWidth == b.strokeWidth && a.image === b.image
    }
}

// MARK: - UIListContentConfiguration

public struct UIListContentConfiguration: UIContentConfiguration, Hashable {
    public struct TextProperties: Hashable {
        public var font: UIFont = UIFont.systemFont(ofSize: 13)
        public var color: UIColor = UIColor()
        public var colorTransformer: UIConfigurationColorTransformer?
        public var numberOfLines = 0
        public var lineBreakMode: NSLineBreakMode = .byTruncatingTail
        public var adjustsFontSizeToFitWidth = false
        public var minimumScaleFactor: CGFloat = 0
        public var allowsDefaultTighteningForTruncation = true
        public static func == (a: Self, b: Self) -> Bool { a.font === b.font && a.color === b.color && a.numberOfLines == b.numberOfLines }
        public func hash(into h: inout Hasher) { h.combine(numberOfLines) }
    }
    public struct ImageProperties: Hashable {
        public var preferredSymbolConfiguration: UIImageSymbolConfiguration?
        public var tintColor: UIColor?
        public var tintColorTransformer: UIConfigurationColorTransformer?
        public var cornerRadius: CGFloat = 0
        public var maximumSize = CGSize(width: 0, height: 0)
        public var reservedLayoutSize = CGSize(width: 0, height: 0)
        public var accessibilityIgnoresInvertColors = false
        public static func == (a: Self, b: Self) -> Bool { a.tintColor === b.tintColor && a.cornerRadius == b.cornerRadius }
        public func hash(into h: inout Hasher) { h.combine(cornerRadius) }
    }

    public var image: UIImage?
    public var imageProperties = ImageProperties()
    public var text: String?
    public var attributedText: NSAttributedString?
    public var textProperties = TextProperties()
    public var secondaryText: String?
    public var secondaryAttributedText: NSAttributedString?
    public var secondaryTextProperties = TextProperties()
    public var axesPreservingSuperviewLayoutMargins: UIAxis = [.horizontal, .vertical]
    public var directionalLayoutMargins = NSDirectionalEdgeInsets(top: 11, leading: 16, bottom: 11, trailing: 16)
    public var prefersSideBySideTextAndSecondaryText = false
    public var imageToTextPadding: CGFloat = 8
    public var textToSecondaryTextHorizontalPadding: CGFloat = 8
    public var textToSecondaryTextVerticalPadding: CGFloat = 2

    public init() {}
    public static func cell() -> UIListContentConfiguration { UIListContentConfiguration() }
    public static func subtitleCell() -> UIListContentConfiguration { UIListContentConfiguration() }
    public static func valueCell() -> UIListContentConfiguration { var c = UIListContentConfiguration(); c.prefersSideBySideTextAndSecondaryText = true; return c }
    public static func plainHeader() -> UIListContentConfiguration { UIListContentConfiguration() }
    public static func groupedHeader() -> UIListContentConfiguration { UIListContentConfiguration() }
    public static func sidebarCell() -> UIListContentConfiguration { UIListContentConfiguration() }
    public static func sidebarHeader() -> UIListContentConfiguration { UIListContentConfiguration() }

    public func makeContentView() -> UIView & UIContentView { _UIListContentView(configuration: self) }
    public func updated(for state: UIConfigurationState) -> UIListContentConfiguration { self }

    public static func == (a: Self, b: Self) -> Bool {
        a.text == b.text && a.secondaryText == b.secondaryText && a.image === b.image && a.textProperties == b.textProperties
    }
    public func hash(into h: inout Hasher) { h.combine(text); h.combine(secondaryText) }
}

/// a view de conteúdo de uma célula de lista: um rótulo com o texto (e o secundário embaixo)
internal final class _UIListContentView: UIView, UIContentView {
    var configuration: UIContentConfiguration {
        didSet { _apply() }
    }
    init(configuration: UIListContentConfiguration) {
        self.configuration = configuration
        super.init(frame: CGRect())
        _apply()
    }
    override init?(coder: NSCoder) { nil }
    private func _apply() {}
}

// MARK: - acessórios, separadores, layout de lista

public struct UICellAccessory {
    public enum DisplayedState: Hashable { case always, whenEditing, whenNotEditing }
    public enum LayoutDimension: Hashable { case actual, standard, custom(CGFloat) }
    public enum Placement {
        case leading(displayed: DisplayedState = .always, at: ([UICellAccessory]) -> Int = { _ in 0 })
        case trailing(displayed: DisplayedState = .always, at: ([UICellAccessory]) -> Int = { $0.count })
    }
    public struct OutlineDisclosureOptions {
        public enum Style: Hashable { case automatic, header, cell }
        public var style: Style
        public var isHidden: Bool?
        public var reservedLayoutWidth: LayoutDimension?
        public var tintColor: UIColor?
        public init(style: Style? = nil, isHidden: Bool? = nil, reservedLayoutWidth: LayoutDimension? = nil, tintColor: UIColor? = nil) {
            self.style = style ?? .automatic; self.isHidden = isHidden; self.reservedLayoutWidth = reservedLayoutWidth; self.tintColor = tintColor
        }
    }
    public struct CustomViewConfiguration {
        public var customView: UIView
        public var placement: Placement
        public var isHidden: Bool?
        public var reservedLayoutWidth: LayoutDimension?
        public var tintColor: UIColor?
        public var maintainsFixedSize: Bool?
        public init(customView: UIView, placement: Placement, isHidden: Bool? = nil, reservedLayoutWidth: LayoutDimension? = nil,
                    tintColor: UIColor? = nil, maintainsFixedSize: Bool? = nil) {
            self.customView = customView; self.placement = placement; self.isHidden = isHidden
            self.reservedLayoutWidth = reservedLayoutWidth; self.tintColor = tintColor; self.maintainsFixedSize = maintainsFixedSize
        }
    }

    internal enum _Kind { case outlineDisclosure(DisplayedState, OutlineDisclosureOptions, (() -> Void)?), customView(CustomViewConfiguration) }
    internal var _kind: _Kind

    public static func outlineDisclosure(displayed: DisplayedState = .always, options: OutlineDisclosureOptions = OutlineDisclosureOptions(),
                                         actionHandler: (() -> Void)? = nil) -> UICellAccessory {
        UICellAccessory(_kind: .outlineDisclosure(displayed, options, actionHandler))
    }
    public static func customView(configuration: CustomViewConfiguration) -> UICellAccessory {
        UICellAccessory(_kind: .customView(configuration))
    }
}

public struct UIListSeparatorConfiguration: Hashable {
    public enum Visibility: Hashable { case automatic, visible, hidden }
    public var topSeparatorVisibility: Visibility = .automatic
    public var bottomSeparatorVisibility: Visibility = .automatic
    public var topSeparatorInsets = NSDirectionalEdgeInsets()
    public var bottomSeparatorInsets = NSDirectionalEdgeInsets()
    public var color: UIColor?
    public var multipleSelectionColor: UIColor?
    public init(listAppearance: UICollectionLayoutListConfiguration.Appearance) {}
    public static func == (a: Self, b: Self) -> Bool {
        a.topSeparatorVisibility == b.topSeparatorVisibility && a.bottomSeparatorVisibility == b.bottomSeparatorVisibility &&
            _uiInsetsEqual(a.topSeparatorInsets, b.topSeparatorInsets) && _uiInsetsEqual(a.bottomSeparatorInsets, b.bottomSeparatorInsets)
    }
    public func hash(into h: inout Hasher) { h.combine(topSeparatorVisibility); h.combine(bottomSeparatorVisibility) }
}

public struct UICollectionLayoutListConfiguration {
    public enum Appearance: Hashable { case plain, grouped, insetGrouped, sidebar, sidebarPlain }
    public enum HeaderMode: Hashable { case none, supplementary, firstItemInSection }
    public enum FooterMode: Hashable { case none, supplementary }
    public var appearance: Appearance
    public var showsSeparators = true
    public var separatorConfiguration: UIListSeparatorConfiguration
    public var itemSeparatorHandler: ((IndexPath, UIListSeparatorConfiguration) -> UIListSeparatorConfiguration)?
    public var backgroundColor: UIColor?
    public var headerMode: HeaderMode = .none
    public var footerMode: FooterMode = .none
    public var headerTopPadding: CGFloat?
    public init(appearance: Appearance) {
        self.appearance = appearance
        separatorConfiguration = UIListSeparatorConfiguration(listAppearance: appearance)
    }
}

extension NSCollectionLayoutSection {
    public static func list(using configuration: UICollectionLayoutListConfiguration,
                            layoutEnvironment: NSCollectionLayoutEnvironment) -> NSCollectionLayoutSection {
        let s = NSCollectionLayoutSection()!
        _uiSet(s, _kContentConfig, configuration)
        return s
    }
}

// MARK: - UIButton.Configuration

extension UIButton {
    public struct Configuration: Equatable {
        public enum CornerStyle: Hashable { case fixed, dynamic, small, medium, large, capsule }
        public enum Size: Hashable { case mini, small, medium, large }
        public enum TitleAlignment: Hashable { case automatic, leading, center, trailing }
        public enum MacIdiomStyle: Hashable { case automatic, bordered, borderless, borderlessTinted }

        public var background = UIBackgroundConfiguration()
        public var cornerStyle: CornerStyle = .dynamic
        public var buttonSize: Size = .medium
        public var macIdiomStyle: MacIdiomStyle = .automatic
        public var baseForegroundColor: UIColor?
        public var baseBackgroundColor: UIColor?
        public var image: UIImage?
        public var imageColorTransformer: UIConfigurationColorTransformer?
        public var preferredSymbolConfigurationForImage: UIImageSymbolConfiguration?
        public var showsActivityIndicator = false
        public var title: String?
        public var attributedTitle: AttributedString?
        public var titleTextAttributesTransformer: UIConfigurationTextAttributesTransformer?
        public var subtitle: String?
        public var attributedSubtitle: AttributedString?
        public var subtitleTextAttributesTransformer: UIConfigurationTextAttributesTransformer?
        public var titleLineBreakMode: NSLineBreakMode = .byTruncatingTail
        public var contentInsets = NSDirectionalEdgeInsets(top: 6, leading: 10, bottom: 6, trailing: 10)
        public var imagePlacement: NSDirectionalRectEdge = .leading
        public var imagePadding: CGFloat = 0
        public var titlePadding: CGFloat = 0
        public var titleAlignment: TitleAlignment = .automatic
        public var automaticallyUpdateForSelection = true

        public static func plain() -> Configuration { Configuration() }
        public static func tinted() -> Configuration { Configuration() }
        public static func gray() -> Configuration { Configuration() }
        public static func filled() -> Configuration { Configuration() }
        public static func borderless() -> Configuration { Configuration() }
        public static func bordered() -> Configuration { Configuration() }
        public static func borderedTinted() -> Configuration { Configuration() }
        public static func borderedProminent() -> Configuration { Configuration() }

        public func updated(for button: UIButton) -> Configuration { self }

        public static func == (a: Configuration, b: Configuration) -> Bool {
            a.title == b.title && a.subtitle == b.subtitle && a.attributedTitle == b.attributedTitle && a.image === b.image &&
                a.cornerStyle == b.cornerStyle && a.buttonSize == b.buttonSize && a.titleAlignment == b.titleAlignment &&
                a.baseForegroundColor === b.baseForegroundColor && a.baseBackgroundColor === b.baseBackgroundColor &&
                a.showsActivityIndicator == b.showsActivityIndicator && _uiInsetsEqual(a.contentInsets, b.contentInsets) &&
                a.imagePadding == b.imagePadding && a.imagePlacement == b.imagePlacement && a.background == b.background
        }
    }

    public var configuration: Configuration? {
        get { _uiGet(self, _kButtonConfig) }
        set {
            _uiSet(self, _kButtonConfig, newValue)
            if let c = newValue {
                let t = c.title ?? c.attributedTitle.map { String($0.characters) }
                setTitle(t.map { _uiNS($0) }, forState: 0)
                if let i = c.image { setImage(i, forState: 0) }
            }
        }
    }

    public convenience init(configuration: Configuration, primaryAction: UIAction? = nil) {
        self.init(type: .plain, primaryAction: primaryAction)
        self.configuration = configuration
    }

    public func setNeedsUpdateConfiguration() {}
    public func updateConfiguration() {}
}

// MARK: - acessores das células

extension UITableViewCell {
    public func defaultContentConfiguration() -> UIListContentConfiguration { .cell() }
    public var contentConfiguration: UIContentConfiguration? {
        get { _uiGet(self, _kContentConfig) }
        set {
            _uiSet(self, _kContentConfig, newValue)
            if let l = newValue as? UIListContentConfiguration { textLabel?.text = l.text.map { _uiNS($0) } }   /* o desenho é o do textLabel */
        }
    }
    public var backgroundConfiguration: UIBackgroundConfiguration? {
        get { _uiGet(self, _kBackgroundConfig) }
        set { _uiSet(self, _kBackgroundConfig, newValue) }
    }
    public typealias ConfigurationUpdateHandler = (UITableViewCell, UICellConfigurationState) -> Void
    public var configurationUpdateHandler: ConfigurationUpdateHandler? {
        get { _uiGet(self, _kUpdateHandler) }
        set { _uiSet(self, _kUpdateHandler, newValue) }
    }
}

extension UICollectionViewCell {
    public var contentConfiguration: UIContentConfiguration? {
        get { _uiGet(self, _kContentConfig) }
        set {
            _uiSet(self, _kContentConfig, newValue)
            if let l = newValue as? UIListContentConfiguration { _mosTextLabel().text = l.text.map { _uiNS($0) } }
        }
    }
    public var backgroundConfiguration: UIBackgroundConfiguration? {
        get { _uiGet(self, _kBackgroundConfig) }
        set { _uiSet(self, _kBackgroundConfig, newValue) }
    }
    public typealias ConfigurationUpdateHandler = (UICollectionViewCell, UICellConfigurationState) -> Void
    public var configurationUpdateHandler: ConfigurationUpdateHandler? {
        get { _uiGet(self, _kUpdateHandler) }
        set { _uiSet(self, _kUpdateHandler, newValue) }
    }
}

extension UICollectionViewListCell {
    public var accessories: [UICellAccessory] {
        get { _uiGet(self, _kAccessories) ?? [] }
        set { _uiSet(self, _kAccessories, newValue) }
    }
}

extension UITableViewHeaderFooterView {
    public var backgroundConfiguration: UIBackgroundConfiguration? {
        get { _uiGet(self, _kBackgroundConfig) }
        set { _uiSet(self, _kBackgroundConfig, newValue) }
    }
}
