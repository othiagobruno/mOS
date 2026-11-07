// mOS — overlay Swift do UIKit do Mac Catalyst (libswiftUIKit.dylib em /System/iOSSupport/usr/lib/swift), com os nomes e
// as assinaturas da Apple, sobre o módulo clang UIKit (userland/sdk/swift-objc/sdk/UIKit) e o UIKit.framework do mOS.
// tools/build-swift-uikit-objc.ps1. Primeira leva: IndexPath, UIContentSizeCategory, UIWindowLevel,
// UIAccessibilityTraits e os snapshots "diffable" (Swift puro). Ver docs/uikit-catalyst.md.
@_exported import UIKit
@_exported import Foundation

// MARK: - IndexPath (linha/item e seção)

extension IndexPath {
    public init(row: Int, section: Int) { self.init(indexes: [section, row]) }
    public init(item: Int, section: Int) { self.init(indexes: [section, item]) }
    public var section: Int {
        get { self[0] }
        set { self[0] = newValue }
    }
    public var row: Int {
        get { self[1] }
        set { self[1] = newValue }
    }
    public var item: Int {
        get { self[1] }
        set { self[1] = newValue }
    }
}

// MARK: - UIContentSizeCategory

extension UIContentSizeCategory {
    public var isAccessibilityCategory: Bool { __UIContentSizeCategoryIsAccessibilityCategory(self) }
    public static func < (left: UIContentSizeCategory, right: UIContentSizeCategory) -> Bool {
        __UIContentSizeCategoryCompareToCategory(left, right) == .orderedAscending
    }
    public static func <= (left: UIContentSizeCategory, right: UIContentSizeCategory) -> Bool {
        __UIContentSizeCategoryCompareToCategory(left, right) != .orderedDescending
    }
    public static func > (left: UIContentSizeCategory, right: UIContentSizeCategory) -> Bool {
        __UIContentSizeCategoryCompareToCategory(left, right) == .orderedDescending
    }
    public static func >= (left: UIContentSizeCategory, right: UIContentSizeCategory) -> Bool {
        __UIContentSizeCategoryCompareToCategory(left, right) != .orderedAscending
    }
}

// MARK: - níveis de janela (aritmética sobre o valor bruto)

public protocol _UIKitNumericRawRepresentable: RawRepresentable, Comparable where RawValue: Comparable & Numeric {}

extension _UIKitNumericRawRepresentable {
    public static func < (lhs: Self, rhs: Self) -> Bool { lhs.rawValue < rhs.rawValue }
    public static func + (lhs: Self, rhs: RawValue) -> Self { Self(rawValue: lhs.rawValue + rhs)! }
    public static func + (lhs: RawValue, rhs: Self) -> Self { Self(rawValue: lhs + rhs.rawValue)! }
    public static func - (lhs: Self, rhs: RawValue) -> Self { Self(rawValue: lhs.rawValue - rhs)! }
    public static func += (lhs: inout Self, rhs: RawValue) { lhs = lhs + rhs }
    public static func -= (lhs: inout Self, rhs: RawValue) { lhs = lhs - rhs }
}

extension UIWindowLevel: _UIKitNumericRawRepresentable {}

// MARK: - traços de acessibilidade como OptionSet

extension UIAccessibilityTraits: OptionSet {}

// MARK: - snapshots "diffable"

public struct NSDiffableDataSourceSnapshot<SectionIdentifierType: Hashable, ItemIdentifierType: Hashable> {
    internal var _sections: [SectionIdentifierType] = []
    internal var _items: [SectionIdentifierType: [ItemIdentifierType]] = [:]
    internal var _reloaded: Set<ItemIdentifierType> = []

    public init() {}

    public var numberOfItems: Int { _sections.reduce(0) { $0 + (_items[$1]?.count ?? 0) } }
    public var numberOfSections: Int { _sections.count }
    public var sectionIdentifiers: [SectionIdentifierType] { _sections }
    public var itemIdentifiers: [ItemIdentifierType] { _sections.flatMap { _items[$0] ?? [] } }
    public var reloadedSectionIdentifiers: [SectionIdentifierType] { [] }
    public var reloadedItemIdentifiers: [ItemIdentifierType] { Array(_reloaded) }
    public var reconfiguredItemIdentifiers: [ItemIdentifierType] { [] }

    public func numberOfItems(inSection identifier: SectionIdentifierType) -> Int { _items[identifier]?.count ?? 0 }
    public func itemIdentifiers(inSection identifier: SectionIdentifierType) -> [ItemIdentifierType] { _items[identifier] ?? [] }
    public func sectionIdentifier(containingItem identifier: ItemIdentifierType) -> SectionIdentifierType? {
        _sections.first { _items[$0]?.contains(identifier) == true }
    }
    public func indexOfItem(_ identifier: ItemIdentifierType) -> Int? { itemIdentifiers.firstIndex(of: identifier) }
    public func indexOfSection(_ identifier: SectionIdentifierType) -> Int? { _sections.firstIndex(of: identifier) }

    public mutating func appendItems(_ identifiers: [ItemIdentifierType], toSection sectionIdentifier: SectionIdentifierType? = nil) {
        guard let s = sectionIdentifier ?? _sections.last else { return }
        _items[s, default: []].append(contentsOf: identifiers)
    }
    public mutating func insertItems(_ identifiers: [ItemIdentifierType], beforeItem beforeIdentifier: ItemIdentifierType) {
        guard let s = self.sectionIdentifier(containingItem: beforeIdentifier), let i = _items[s]?.firstIndex(of: beforeIdentifier) else { return }
        _items[s]!.insert(contentsOf: identifiers, at: i)
    }
    public mutating func insertItems(_ identifiers: [ItemIdentifierType], afterItem afterIdentifier: ItemIdentifierType) {
        guard let s = self.sectionIdentifier(containingItem: afterIdentifier), let i = _items[s]?.firstIndex(of: afterIdentifier) else { return }
        _items[s]!.insert(contentsOf: identifiers, at: i + 1)
    }
    public mutating func deleteItems(_ identifiers: [ItemIdentifierType]) {
        let del = Set(identifiers)
        for s in _sections { _items[s]?.removeAll { del.contains($0) } }
    }
    public mutating func deleteAllItems() { _sections = []; _items = [:] }
    public mutating func moveItem(_ identifier: ItemIdentifierType, beforeItem toIdentifier: ItemIdentifierType) {
        deleteItems([identifier]); insertItems([identifier], beforeItem: toIdentifier)
    }
    public mutating func moveItem(_ identifier: ItemIdentifierType, afterItem toIdentifier: ItemIdentifierType) {
        deleteItems([identifier]); insertItems([identifier], afterItem: toIdentifier)
    }
    public mutating func reloadItems(_ identifiers: [ItemIdentifierType]) { _reloaded.formUnion(identifiers) }
    public mutating func reconfigureItems(_ identifiers: [ItemIdentifierType]) { _reloaded.formUnion(identifiers) }

    public mutating func appendSections(_ identifiers: [SectionIdentifierType]) {
        for s in identifiers where !_sections.contains(s) { _sections.append(s); _items[s] = _items[s] ?? [] }
    }
    public mutating func insertSections(_ identifiers: [SectionIdentifierType], beforeSection toIdentifier: SectionIdentifierType) {
        guard let i = _sections.firstIndex(of: toIdentifier) else { return }
        _sections.insert(contentsOf: identifiers.filter { !_sections.contains($0) }, at: i)
    }
    public mutating func insertSections(_ identifiers: [SectionIdentifierType], afterSection toIdentifier: SectionIdentifierType) {
        guard let i = _sections.firstIndex(of: toIdentifier) else { return }
        _sections.insert(contentsOf: identifiers.filter { !_sections.contains($0) }, at: i + 1)
    }
    public mutating func deleteSections(_ identifiers: [SectionIdentifierType]) {
        let del = Set(identifiers)
        _sections.removeAll { del.contains($0) }
        for s in identifiers { _items[s] = nil }
    }
    public mutating func moveSection(_ identifier: SectionIdentifierType, beforeSection toIdentifier: SectionIdentifierType) {
        guard _sections.contains(identifier) else { return }
        _sections.removeAll { $0 == identifier }
        if let i = _sections.firstIndex(of: toIdentifier) { _sections.insert(identifier, at: i) }
    }
    public mutating func moveSection(_ identifier: SectionIdentifierType, afterSection toIdentifier: SectionIdentifierType) {
        guard _sections.contains(identifier) else { return }
        _sections.removeAll { $0 == identifier }
        if let i = _sections.firstIndex(of: toIdentifier) { _sections.insert(identifier, at: i + 1) }
    }
    public mutating func reloadSections(_ identifiers: [SectionIdentifierType]) {}
    public mutating func reconfigureSections(_ identifiers: [SectionIdentifierType]) {}
}

/// árvore de itens de uma seção (listas com expansão)
public struct NSDiffableDataSourceSectionSnapshot<ItemIdentifierType: Hashable> {
    internal var _order: [ItemIdentifierType] = []
    internal var _parent: [ItemIdentifierType: ItemIdentifierType] = [:]
    internal var _expanded: Set<ItemIdentifierType> = []

    public init() {}

    public var items: [ItemIdentifierType] { _order }
    public var rootItems: [ItemIdentifierType] { _order.filter { _parent[$0] == nil } }
    public var visibleItems: [ItemIdentifierType] { _order.filter { isVisible($0) } }

    public mutating func append(_ items: [ItemIdentifierType], to parent: ItemIdentifierType? = nil) {
        var at = _order.endIndex
        if let p = parent, let pi = _order.firstIndex(of: p) {
            at = pi + 1
            while at < _order.count, _isDescendant(_order[at], of: p) { at += 1 }
            for i in items { _parent[i] = p }
        }
        _order.insert(contentsOf: items, at: at)
    }
    public mutating func expand(_ items: [ItemIdentifierType]) { _expanded.formUnion(items) }
    public mutating func collapse(_ items: [ItemIdentifierType]) { _expanded.subtract(items) }
    public func isExpanded(_ item: ItemIdentifierType) -> Bool { _expanded.contains(item) }
    public func parent(of child: ItemIdentifierType) -> ItemIdentifierType? { _parent[child] }
    public func contains(_ item: ItemIdentifierType) -> Bool { _order.contains(item) }
    public func isVisible(_ item: ItemIdentifierType) -> Bool {
        var p = _parent[item]
        while let q = p { if !_expanded.contains(q) { return false }; p = _parent[q] }
        return true
    }
    public mutating func deleteAll() { _order = []; _parent = [:]; _expanded = [] }

    internal func _isDescendant(_ x: ItemIdentifierType, of p: ItemIdentifierType) -> Bool {
        var q = _parent[x]
        while let r = q { if r == p { return true }; q = _parent[r] }
        return false
    }
}
