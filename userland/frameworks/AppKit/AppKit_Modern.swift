// mOS — APIs Swift do AppKit do macOS 14 a 26 que os apps do corpus usam: invalidação por property wrapper
// (NSView.Invalidating), carregamento da view (NSViewController.ViewLoading), regiões de layout (NSView.LayoutRegion,
// edgeInsets(for:)), escopo AppKit do AttributedString, recursos de catálogo (NSImage/NSColor(resource:)),
// NSTableViewDiffableDataSource, NSPasteboard.detectedValues(for:), NSGradient(colorsAndLocations:),
// NSStoryboard.instantiateInitialController(creator:), NSMenuItem.sectionHeader(title:), literal de cor,
// URLResourceValues.effectiveIcon/labelColor e NSWindowLevel numérico.
// Fora do módulo Foundation o importador do mOS não faz a ponte nos tipos ObjC (NSString, NSArray): as conversões aqui
// são explícitas.

import Foundation
import DeveloperToolsSupport

// MARK: - NSWindowLevel

extension NSWindowLevel: _AppKitKitNumericRawRepresentable {}

// MARK: - literal de cor e recursos de catálogo

extension NSColor {
    /// #colorLiteral(red:green:blue:alpha:), no espaço sRGB.
    public convenience init(_colorLiteralRed red: Float, green: Float, blue: Float, alpha: Float) {
        self.init(srgbRed: CGFloat(red), green: CGFloat(green), blue: CGFloat(blue), alpha: CGFloat(alpha))
    }

    /// A cor de um catálogo de recursos (ColorResource gerado pelo Xcode); sem ela, transparente.
    public convenience init(resource: ColorResource) {
        let name = resource.name as NSString
        if NSColor(named: name, bundle: resource.bundle) != nil {
            self.init(named: name, bundle: resource.bundle)!
        } else {
            self.init(srgbRed: 0, green: 0, blue: 0, alpha: 0)
        }
    }
}

extension NSImage {
    /// A imagem de um catálogo de recursos (ImageResource gerado pelo Xcode): do catálogo do app, de um arquivo do
    /// bundle ou, sem nenhum dos dois, uma imagem vazia (o init da Apple também não falha).
    public convenience init(resource: ImageResource) {
        let name = resource.name as NSString
        if resource.bundle == Bundle.main, NSImage(named: name) != nil {
            self.init(named: name)!
        } else if let path = resource.bundle.path(forImageResource: name), NSImage(contentsOfFile: path) != nil {
            self.init(contentsOfFile: path)!
        } else if let image = resource.bundle.image(forResource: name), let data = image.tiffRepresentation, NSImage(data: data) != nil {
            self.init(data: data)!
        } else {
            self.init(size: CGSize(width: 0, height: 0))
        }
    }
}

// MARK: - NSView.Invalidating

/// O que fazer na view quando uma propriedade marcada com @Invalidating muda.
public protocol NSViewInvalidating {
    func invalidate(view: NSView)
}

extension NSView {
    /// Property wrapper que invalida a view (desenho, layout, restrições...) quando o valor muda.
    @propertyWrapper
    public struct Invalidating<Value, InvalidationType> where Value: Equatable, InvalidationType: NSViewInvalidating {
        internal var _value: Value
        internal let _invalidation: InvalidationType

        public init(wrappedValue: Value, _ invalidations: InvalidationType) {
            _value = wrappedValue
            _invalidation = invalidations
        }

        @available(*, unavailable, message: "@Invalidating só vale em propriedades de uma NSView")
        public var wrappedValue: Value {
            get { fatalError("@Invalidating só vale em propriedades de uma NSView") }
            set { fatalError("@Invalidating só vale em propriedades de uma NSView") }
        }

        public static subscript<EnclosingSelf: NSView>(
            _enclosingInstance observed: EnclosingSelf,
            wrapped wrappedKeyPath: ReferenceWritableKeyPath<EnclosingSelf, Value>,
            storage storageKeyPath: ReferenceWritableKeyPath<EnclosingSelf, Invalidating<Value, InvalidationType>>
        ) -> Value {
            get { observed[keyPath: storageKeyPath]._value }
            set {
                let old = observed[keyPath: storageKeyPath]._value
                observed[keyPath: storageKeyPath]._value = newValue
                if old != newValue { observed[keyPath: storageKeyPath]._invalidation.invalidate(view: observed) }
            }
        }
    }

    /// As invalidações prontas.
    public enum Invalidations {
        public struct Display: NSViewInvalidating {
            public init() {}
            public func invalidate(view: NSView) { view.needsDisplay = true }
        }
        public struct Layout: NSViewInvalidating {
            public init() {}
            public func invalidate(view: NSView) { view.needsLayout = true }
        }
        public struct Constraints: NSViewInvalidating {
            public init() {}
            public func invalidate(view: NSView) { view.needsUpdateConstraints = true }
        }
        public struct IntrinsicContentSize: NSViewInvalidating {
            public init() {}
            public func invalidate(view: NSView) { view.invalidateIntrinsicContentSize() }
        }
        public struct RestorableState: NSViewInvalidating {
            public init() {}
            public func invalidate(view: NSView) { view.invalidateRestorableState() }
        }
    }
}

extension NSViewInvalidating where Self == NSView.Invalidations.Display {
    public static var display: NSView.Invalidations.Display { .init() }
}
extension NSViewInvalidating where Self == NSView.Invalidations.Layout {
    public static var layout: NSView.Invalidations.Layout { .init() }
}
extension NSViewInvalidating where Self == NSView.Invalidations.Constraints {
    public static var constraints: NSView.Invalidations.Constraints { .init() }
}
extension NSViewInvalidating where Self == NSView.Invalidations.IntrinsicContentSize {
    public static var intrinsicContentSize: NSView.Invalidations.IntrinsicContentSize { .init() }
}
extension NSViewInvalidating where Self == NSView.Invalidations.RestorableState {
    public static var restorableState: NSView.Invalidations.RestorableState { .init() }
}

// MARK: - NSViewController.ViewLoading

extension NSViewController {
    /// Property wrapper para outlets e valores que só existem depois de a view carregar: ler carrega a view.
    @propertyWrapper
    public struct ViewLoading<Value> {
        internal var _stored: Value?

        public init() { _stored = nil }

        @available(*, unavailable, message: "@ViewLoading só vale em propriedades de um NSViewController")
        public var wrappedValue: Value {
            get { fatalError("@ViewLoading só vale em propriedades de um NSViewController") }
            set { fatalError("@ViewLoading só vale em propriedades de um NSViewController") }
        }

        public static subscript<EnclosingSelf: NSViewController>(
            _enclosingInstance observed: EnclosingSelf,
            wrapped wrappedKeyPath: ReferenceWritableKeyPath<EnclosingSelf, Value>,
            storage storageKeyPath: ReferenceWritableKeyPath<EnclosingSelf, ViewLoading<Value>>
        ) -> Value {
            get {
                observed.loadViewIfNeeded()
                guard let value = observed[keyPath: storageKeyPath]._stored else {
                    fatalError("@ViewLoading: o valor não foi definido ao carregar a view")
                }
                return value
            }
            set { observed[keyPath: storageKeyPath]._stored = newValue }
        }
    }
}

// MARK: - NSView.LayoutRegion

extension NSView {
    /// Uma região de layout da view: a área segura ou as margens, com adaptação aos cantos arredondados da janela.
    public struct LayoutRegion: Sendable {
        public enum AdaptivityAxis: Sendable {
            case horizontal
            case vertical
        }
        internal var _margins: Bool
        internal var _cornerAdaptation: AdaptivityAxis?

        public static func safeArea(cornerAdaptation: AdaptivityAxis? = nil) -> LayoutRegion {
            LayoutRegion(_margins: false, _cornerAdaptation: cornerAdaptation)
        }
        public static func margins(cornerAdaptation: AdaptivityAxis? = nil) -> LayoutRegion {
            LayoutRegion(_margins: true, _cornerAdaptation: cornerAdaptation)
        }
    }

    /// As distâncias das bordas da view até a região: a área segura (safeAreaInsets) e, nas margens, o espaçamento
    /// padrão de 20 pontos. A adaptação aos cantos acrescenta o raio do canto da janela no eixo pedido (8 no mOS).
    public func edgeInsets(for region: LayoutRegion) -> NSEdgeInsets {
        var e = safeAreaInsets
        if region._margins { e.top += 20; e.left += 20; e.bottom += 20; e.right += 20 }
        switch region._cornerAdaptation {
        case .horizontal?: e.left += 8; e.right += 8
        case .vertical?: e.top += 8; e.bottom += 8
        case nil: break
        }
        return e
    }
}

// MARK: - NSMenuItem.sectionHeader, NSGradient, NSStoryboard

extension NSMenuItem {
    /// Um item de título de seção (não escolhível), como os do macOS 14.
    public static func sectionHeader(title: String) -> NSMenuItem { __sectionHeader(withTitle: title as NSString) }
}

extension NSGradient {
    /// Um degradê com pares (cor, posição): NSGradient(colorsAndLocations: (.red, 0), (.blue, 1)).
    public convenience init?(colorsAndLocations objects: (NSColor, CGFloat)...) {
        let colors = objects.map { $0.0 } as NSArray
        var locations = objects.map { $0.1 }
        self.init(__colors: colors, atLocations: &locations, colorSpace: NSColorSpace.sRGB())
    }
}

extension NSStoryboard {
    /// O controlador inicial, criado pelo bloco (injeção de dependências) quando ele é dado.
    public func instantiateInitialController<Controller: NSViewController>(creator: ((NSCoder) -> Controller?)? = nil) -> Controller? {
        if let creator { return __instantiateInitialController(creator: { coder in creator(coder) }) as? Controller }
        return __instantiateInitialController() as? Controller
    }

    public func instantiateInitialController<Controller: NSWindowController>(creator: ((NSCoder) -> Controller?)? = nil) -> Controller? {
        if let creator { return __instantiateInitialController(creator: { coder in creator(coder) }) as? Controller }
        return __instantiateInitialController() as? Controller
    }
}

// MARK: - NSPasteboard.detectedValues (macOS 15)

extension NSPasteboard {
    /// Os valores achados no texto da área de transferência.
    public struct DetectedValues {
        public var links: [DDMatchLink] = []
        public init() {}
    }

    /// Procura no texto da área de transferência os valores pedidos (hoje, links: http, https, mailto e www.).
    public func detectedValues(for keyPaths: Set<PartialKeyPath<DetectedValues>>) async throws -> DetectedValues {
        var result = DetectedValues()
        guard keyPaths.contains(\DetectedValues.links),
              let text = string(forType: "public.utf8-plain-text" as NSString) else { return result }
        let s = text as String
        let utf16 = Array(s.utf16)
        var i = 0
        func startsWith(_ p: String, at k: Int) -> Bool {
            let pu = Array(p.utf16)
            guard k + pu.count <= utf16.count else { return false }
            for (j, c) in pu.enumerated() {
                var u = utf16[k + j]
                if u >= 65 && u <= 90 { u += 32 }
                if u != c { return false }
            }
            return true
        }
        while i < utf16.count {
            let atWordStart = i == 0 || utf16[i - 1] == 32 || utf16[i - 1] == 10 || utf16[i - 1] == 9 || utf16[i - 1] == 40 || utf16[i - 1] == 60
            if atWordStart && (startsWith("http://", at: i) || startsWith("https://", at: i) || startsWith("mailto:", at: i) || startsWith("www.", at: i)) {
                var j = i
                while j < utf16.count, utf16[j] > 32, utf16[j] != 62, utf16[j] != 34 { j += 1 }
                while j > i, [46, 44, 41, 59, 58, 33, 63].contains(utf16[j - 1]) { j -= 1 }   /* pontuação no fim fica fora */
                let piece = String(decoding: utf16[i..<j], as: UTF16.self)
                let urlText = piece.lowercased().hasPrefix("www.") ? "http://" + piece : piece
                if let url = URL(string: urlText) {
                    let m = cx_ddmatch_new(1, piece as NSString, NSRange(location: i, length: j - i), url as NSURL)
                    if let link = m as? DDMatchLink { result.links.append(link) }
                }
                i = j
            } else {
                i += 1
            }
        }
        return result
    }
}

// MARK: - URLResourceValues

extension URLResourceValues {
    /// O ícone que o sistema mostra para o item (NSImage).
    public var effectiveIcon: AnyObject? { allValues[URLResourceKey(rawValue: "NSURLEffectiveIconKey" as NSString)] as AnyObject? }
    /// O ícone próprio do item, se houver.
    public var customIcon: NSImage? { allValues[URLResourceKey(rawValue: "NSURLCustomIconKey" as NSString)] as? NSImage }
    /// A cor da etiqueta do Finder (o mOS não tem etiquetas: nil).
    public var labelColor: NSColor? { allValues[URLResourceKey(rawValue: "NSURLLabelColorKey" as NSString)] as? NSColor }
}

// MARK: - AttributedString: escopo AppKit

extension AttributeScopes {
    public var appKit: AppKitAttributes.Type { AppKitAttributes.self }

    /// Os atributos de texto do AppKit para AttributedString (attributedString.appKit.foregroundColor = .red).
    public struct AppKitAttributes: AttributeScope {
        public let foregroundColor: ForegroundColorAttribute
        public let backgroundColor: BackgroundColorAttribute
        public let paragraphStyle: ParagraphStyleAttribute
        public let foundation: FoundationAttributes

        public enum ForegroundColorAttribute: AttributedStringKey {
            public typealias Value = NSColor
            public static let name = "NSColor"
        }
        public enum BackgroundColorAttribute: AttributedStringKey {
            public typealias Value = NSColor
            public static let name = "NSBackgroundColor"
        }
        public enum ParagraphStyleAttribute: AttributedStringKey {
            public typealias Value = NSParagraphStyle
            public static let name = "NSParagraphStyle"
        }
    }
}

extension AttributeDynamicLookup {
    public subscript<T: AttributedStringKey>(dynamicMember keyPath: KeyPath<AttributeScopes.AppKitAttributes, T>) -> T {
        self[T.self]
    }
}

// MARK: - NSTableViewDiffableDataSource

/// Fonte de dados de uma NSTableView a partir de snapshots: as linhas são os itens das seções, em ordem (e uma linha
/// de cabeçalho por seção quando há sectionHeaderViewProvider).
open class NSTableViewDiffableDataSource<SectionIdentifierType: Hashable, ItemIdentifierType: Hashable>: NSObject, NSTableViewDataSource {
    public typealias CellProvider = (NSTableView, NSTableColumn, Int, ItemIdentifierType) -> NSView
    public typealias RowProvider = (NSTableView, Int, ItemIdentifierType) -> NSTableRowView
    public typealias SectionHeaderViewProvider = (NSTableView, Int, SectionIdentifierType) -> NSView

    internal weak var _tableView: NSTableView?
    internal let _cellProvider: CellProvider
    internal var _snapshot = NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>()

    open var rowViewProvider: RowProvider?
    open var sectionHeaderViewProvider: SectionHeaderViewProvider?

    public init(tableView: NSTableView, cellProvider: @escaping CellProvider) {
        _tableView = tableView
        _cellProvider = cellProvider
        super.init()
        tableView.dataSource = self
    }

    open func apply(_ snapshot: NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>, animatingDifferences: Bool = true,
                    completion: (() -> Void)? = nil) {
        _snapshot = snapshot
        _snapshot._reloaded = []
        _snapshot._reloadedSections = []
        _tableView?.reloadData()
        completion?()
    }
    open func snapshot() -> NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType> { _snapshot }

    /// Linha → (seção, item ou nil para o cabeçalho).
    internal func _locate(row: Int) -> (Int, Int?)? {
        var r = row
        let headers = sectionHeaderViewProvider != nil
        for (s, items) in _snapshot._items.enumerated() {
            if headers {
                if r == 0 { return (s, nil) }
                r -= 1
            }
            if r < items.count { return (s, r) }
            r -= items.count
        }
        return nil
    }
    open func itemIdentifier(forRow row: Int) -> ItemIdentifierType? {
        guard let (s, i) = _locate(row: row), let i else { return nil }
        return _snapshot._items[s][i]
    }
    open func row(forItemIdentifier identifier: ItemIdentifierType) -> Int? {
        var row = 0
        let headers = sectionHeaderViewProvider != nil
        for items in _snapshot._items {
            if headers { row += 1 }
            if let i = items.firstIndex(of: identifier) { return row + i }
            row += items.count
        }
        return nil
    }
    open func sectionIdentifier(forRow row: Int) -> SectionIdentifierType? {
        guard let (s, _) = _locate(row: row) else { return nil }
        return _snapshot._sections[s]
    }
    open func row(forSectionIdentifier identifier: SectionIdentifierType) -> Int? {
        guard let s = _snapshot._sections.firstIndex(of: identifier) else { return nil }
        let headers = sectionHeaderViewProvider != nil
        var row = 0
        for k in 0..<s { row += _snapshot._items[k].count + (headers ? 1 : 0) }
        return row
    }

    // NSTableViewDataSource
    open func numberOfRows(in tableView: NSTableView) -> Int {
        _snapshot._items.reduce(0) { $0 + $1.count } + (sectionHeaderViewProvider != nil ? _snapshot._sections.count : 0)
    }
    open func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        guard let (s, i) = _locate(row: row) else { return nil }
        guard let i else { return sectionHeaderViewProvider?(tableView, row, _snapshot._sections[s]) }
        guard let column = tableColumn else { return nil }
        return _cellProvider(tableView, column, row, _snapshot._items[s][i])
    }
}
