// mOS — overlay Swift do UIKit (Catalyst), terceira leva: os data sources "diffable" (UITableViewDiffableDataSource,
// UICollectionViewDiffableDataSource), os registros de célula da UICollectionView, ponteiro (UIPointerEffect/Shape/
// Style), AttributeScopes.UIKitAttributes e miudezas (UIFocusSystem, UITextDirection, UILayoutPriority). Nomes e
// assinaturas da Apple. As tabelas e coleções do UIKit.framework do mOS ainda não desenham células: os data sources
// guardam o snapshot e respondem às perguntas de dados (UITableViewDataSource) como na Apple.
import UIKit
import Foundation
import CoreGraphics
import ObjectiveC

// MARK: - UITableViewDiffableDataSource

open class UITableViewDiffableDataSource<SectionIdentifierType: Hashable, ItemIdentifierType: Hashable>: NSObject, UITableViewDataSource {
    public typealias CellProvider = (UITableView, IndexPath, ItemIdentifierType) -> UITableViewCell?

    internal let _cellProvider: CellProvider
    internal weak var _tableView: UITableView?
    internal var _snapshot = NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>()

    open var defaultRowAnimation: UITableViewRowAnimation = .automatic

    public init(tableView: UITableView, cellProvider: @escaping CellProvider) {
        _cellProvider = cellProvider
        _tableView = tableView
        super.init()
        tableView.dataSource = self
    }

    open func apply(_ snapshot: NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>,
                    animatingDifferences: Bool = true, completion: (() -> Void)? = nil) {
        _snapshot = snapshot
        _tableView?.reloadData()
        completion?()
    }

    open func apply(_ snapshot: NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>,
                    animatingDifferences: Bool = true) async {
        apply(snapshot, animatingDifferences: animatingDifferences, completion: nil)
    }

    open func applySnapshotUsingReloadData(_ snapshot: NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>,
                                           completion: (() -> Void)? = nil) {
        apply(snapshot, animatingDifferences: false, completion: completion)
    }

    open func applySnapshotUsingReloadData(_ snapshot: NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>) async {
        apply(snapshot, animatingDifferences: false, completion: nil)
    }

    open func snapshot() -> NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType> { _snapshot }

    open func itemIdentifier(for indexPath: IndexPath) -> ItemIdentifierType? {
        guard let s = sectionIdentifier(for: indexPath.section) else { return nil }
        let items = _snapshot.itemIdentifiers(inSection: s)
        return indexPath.row < items.count ? items[indexPath.row] : nil
    }

    open func indexPath(for itemIdentifier: ItemIdentifierType) -> IndexPath? {
        guard let s = _snapshot.sectionIdentifier(containingItem: itemIdentifier), let si = _snapshot.indexOfSection(s),
              let r = _snapshot.itemIdentifiers(inSection: s).firstIndex(of: itemIdentifier) else { return nil }
        return IndexPath(row: r, section: si)
    }

    open func sectionIdentifier(for index: Int) -> SectionIdentifierType? {
        let s = _snapshot.sectionIdentifiers
        return index >= 0 && index < s.count ? s[index] : nil
    }

    // UITableViewDataSource
    @objc open func numberOfSections(in tableView: UITableView) -> Int { _snapshot.numberOfSections }
    @objc open func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        sectionIdentifier(for: section).map { _snapshot.numberOfItems(inSection: $0) } ?? 0
    }
    @objc open func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        if let item = itemIdentifier(for: indexPath), let c = _cellProvider(tableView, indexPath, item) { return c }
        return UITableViewCell(frame: CGRect())
    }
    @objc open func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? { nil }
    @objc open func tableView(_ tableView: UITableView, titleForFooterInSection section: Int) -> String? { nil }
    @objc open func tableView(_ tableView: UITableView, canEditRowAt indexPath: IndexPath) -> Bool { false }
    @objc open func sectionIndexTitles(for tableView: UITableView) -> [String]? { nil }
}

// MARK: - UICollectionViewDiffableDataSource

open class UICollectionViewDiffableDataSource<SectionIdentifierType: Hashable, ItemIdentifierType: Hashable>: NSObject, UICollectionViewDataSource {
    public typealias CellProvider = (UICollectionView, IndexPath, ItemIdentifierType) -> UICollectionViewCell?
    public typealias SupplementaryViewProvider = (UICollectionView, String, IndexPath) -> UICollectionReusableView?

    internal let _cellProvider: CellProvider
    internal weak var _collectionView: UICollectionView?
    internal var _snapshot = NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>()
    internal var _sectionSnapshots: [SectionIdentifierType: NSDiffableDataSourceSectionSnapshot<ItemIdentifierType>] = [:]

    open var supplementaryViewProvider: SupplementaryViewProvider?

    public init(collectionView: UICollectionView, cellProvider: @escaping CellProvider) {
        _cellProvider = cellProvider
        _collectionView = collectionView
        super.init()
        collectionView.dataSource = self
    }

    open func apply(_ snapshot: NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>,
                    animatingDifferences: Bool = true, completion: (() -> Void)? = nil) {
        _snapshot = snapshot
        _collectionView?.reloadData()
        completion?()
    }

    open func apply(_ snapshot: NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>,
                    animatingDifferences: Bool = true) async {
        apply(snapshot, animatingDifferences: animatingDifferences, completion: nil)
    }

    open func applySnapshotUsingReloadData(_ snapshot: NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>,
                                           completion: (() -> Void)? = nil) {
        apply(snapshot, animatingDifferences: false, completion: completion)
    }

    open func applySnapshotUsingReloadData(_ snapshot: NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>) async {
        apply(snapshot, animatingDifferences: false, completion: nil)
    }

    open func snapshot() -> NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType> { _snapshot }

    open func itemIdentifier(for indexPath: IndexPath) -> ItemIdentifierType? {
        guard let s = sectionIdentifier(for: indexPath.section) else { return nil }
        let items = _snapshot.itemIdentifiers(inSection: s)
        return indexPath.item < items.count ? items[indexPath.item] : nil
    }

    open func indexPath(for itemIdentifier: ItemIdentifierType) -> IndexPath? {
        guard let s = _snapshot.sectionIdentifier(containingItem: itemIdentifier), let si = _snapshot.indexOfSection(s),
              let r = _snapshot.itemIdentifiers(inSection: s).firstIndex(of: itemIdentifier) else { return nil }
        return IndexPath(item: r, section: si)
    }

    open func sectionIdentifier(for index: Int) -> SectionIdentifierType? {
        let s = _snapshot.sectionIdentifiers
        return index >= 0 && index < s.count ? s[index] : nil
    }
}

extension UICollectionViewDiffableDataSource {
    /// snapshot de uma seção (listas expansíveis): substitui os itens da seção pelos visíveis da árvore
    public func apply(_ snapshot: NSDiffableDataSourceSectionSnapshot<ItemIdentifierType>, to section: SectionIdentifierType,
                      animatingDifferences: Bool = true, completion: (() -> Void)? = nil) {
        _sectionSnapshots[section] = snapshot
        var s = _snapshot
        if !s.sectionIdentifiers.contains(section) { s.appendSections([section]) }
        s.deleteItems(s.itemIdentifiers(inSection: section))
        s.appendItems(snapshot.visibleItems, toSection: section)
        apply(s, animatingDifferences: animatingDifferences, completion: completion)
    }

    public func snapshot(for section: SectionIdentifierType) -> NSDiffableDataSourceSectionSnapshot<ItemIdentifierType> {
        if let s = _sectionSnapshots[section] { return s }
        var s = NSDiffableDataSourceSectionSnapshot<ItemIdentifierType>()
        s.append(_snapshot.itemIdentifiers(inSection: section))
        return s
    }
}

// MARK: - registros de célula

extension UICollectionView {
    public struct CellRegistration<Cell: UICollectionViewCell, Item> {
        public typealias Handler = (Cell, IndexPath, Item) -> Void
        internal let _handler: Handler
        public init(handler: @escaping Handler) { _handler = handler }
    }

    public struct SupplementaryRegistration<Supplementary: UICollectionReusableView> {
        public typealias Handler = (Supplementary, String, IndexPath) -> Void
        public let elementKind: String
        internal let _handler: Handler
        public init(elementKind: String, handler: @escaping Handler) { self.elementKind = elementKind; _handler = handler }
    }

    /// cria a célula da classe do registro (+new do ObjC) e a configura pelo handler
    public func dequeueConfiguredReusableCell<Cell, Item>(using registration: CellRegistration<Cell, Item>, for indexPath: IndexPath,
                                                          item: Item?) -> Cell {
        let cell = _uiNew(Cell.self) as! Cell
        if let item { registration._handler(cell, indexPath, item) }
        return cell
    }

    public func dequeueConfiguredReusableSupplementary<Supplementary>(using registration: SupplementaryRegistration<Supplementary>,
                                                                      for indexPath: IndexPath) -> Supplementary {
        let v = _uiNew(Supplementary.self) as! Supplementary
        registration._handler(v, registration.elementKind, indexPath)
        return v
    }
}

/// +[cls new] de uma classe ObjC (o init designado de cada subclasse do app)
internal func _uiNew(_ cls: AnyClass) -> AnyObject {
    let o = (cls as AnyObject).perform(Selector(("new")))!
    return o.takeRetainedValue()
}

// MARK: - ponteiro

public enum UIPointerEffect {
    public enum TintMode { case none, overlay, underlay }
    case automatic(UITargetedPreview)
    case highlight(UITargetedPreview)
    case lift(UITargetedPreview)
    case hover(UITargetedPreview, preferredTintMode: TintMode = .overlay, prefersShadow: Bool = false, prefersScaledContent: Bool = true)
}

public enum UIPointerShape {
    case path(UIBezierPath)
    case roundedRect(CGRect, radius: CGFloat = 8)
    case verticalBeam(length: CGFloat)
    case horizontalBeam(length: CGFloat)
}

internal var _kPointer: UnsafeRawPointer { UnsafeRawPointer(_uiKeys + 5) }

extension UIPointerStyle {
    public convenience init(effect: UIPointerEffect, shape: UIPointerShape? = nil) {
        self.init()
        _uiSet(self, _kPointer, (effect, shape))
    }
}

// MARK: - miudezas

extension UIFocusSystem {
    /// o mOS ainda não tem sistema de foco (navegação por teclado entre views)
    public static func focusSystem(for environment: UIFocusEnvironment) -> UIFocusSystem? { nil }
}

extension UITextDirection {
    public static func layout(_ direction: UITextLayoutDirection) -> UITextDirection { UITextDirection(rawValue: direction.rawValue) }
}

extension UILayoutPriority: _UIKitNumericRawRepresentable {}

// MARK: - atributos de texto do UIKit (AttributedString)

extension AttributeScopes {
    public var uiKit: UIKitAttributes.Type { UIKitAttributes.self }

    public struct UIKitAttributes: AttributeScope {
        public let font: FontAttribute
        public let foregroundColor: ForegroundColorAttribute
        public let backgroundColor: BackgroundColorAttribute
        public let kern: KernAttribute
        public let foundation: FoundationAttributes

        public enum FontAttribute: AttributedStringKey {
            public typealias Value = UIFont
            public static var name: String { "NSFont" }
        }
        public enum ForegroundColorAttribute: AttributedStringKey {
            public typealias Value = UIColor
            public static var name: String { "NSColor" }
        }
        public enum BackgroundColorAttribute: AttributedStringKey {
            public typealias Value = UIColor
            public static var name: String { "NSBackgroundColor" }
        }
        public enum KernAttribute: AttributedStringKey {
            public typealias Value = CGFloat
            public static var name: String { "NSKern" }
        }
    }
}

extension AttributeDynamicLookup {
    public subscript<T: AttributedStringKey>(dynamicMember keyPath: KeyPath<AttributeScopes.UIKitAttributes, T>) -> T {
        self[T.self]
    }
}
