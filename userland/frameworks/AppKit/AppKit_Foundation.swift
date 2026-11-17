// mOS — parte do overlay Swift do AppKit que usa o overlay Foundation (String, IndexPath): NSEvent.specialKey,
// NSImage(imageLiteralResourceName:), IndexPath.item/section e as fontes de dados "diffable" do NSCollectionView.

import Foundation

// Fora do módulo Foundation o importador do mOS não faz a ponte nos tipos importados (NSString, NSIndexPath): as
// conversões aqui são explícitas, como no CloudKit e no UIKit.

// MARK: - NSEvent.specialKey

extension NSEvent {
    /// A tecla especial do evento de teclado (keyDown 10, keyUp 11), pelo primeiro caractere sem modificadores.
    public var specialKey: SpecialKey? {
        guard type == 10 || type == 11, let s = charactersIgnoringModifiers, let u = (s as String).utf16.first else { return nil }
        let c = Int(u)
        return SpecialKey._isSpecial(c) ? SpecialKey(rawValue: c) : nil
    }
}

// MARK: - NSImage literal

extension NSImage {
    /// #imageLiteral(resourceName:): a imagem com esse nome no app; sem ela, uma imagem vazia (não nil, como no AppKit).
    public convenience init(imageLiteralResourceName name: String) {
        if NSImage(named: name as NSString) != nil { self.init(named: name as NSString)! } else { self.init(size: CGSize(width: 0, height: 0)) }
    }
}

// MARK: - IndexPath (coleções)

extension IndexPath {
    public init(item: Int, section: Int) { self.init(indexes: [section, item]) }
    public var item: Int {
        get { self[1] }
        set { self[1] = newValue }
    }
    public var section: Int {
        get { self[0] }
        set { self[0] = newValue }
    }
}

// MARK: - NSDiffableDataSourceSnapshot

/// Seções e itens identificados por valor; aplicado a uma NSCollectionViewDiffableDataSource.
public struct NSDiffableDataSourceSnapshot<SectionIdentifierType: Hashable, ItemIdentifierType: Hashable> {
    internal var _sections: [SectionIdentifierType] = []
    internal var _items: [[ItemIdentifierType]] = []
    internal var _reloaded: Set<ItemIdentifierType> = []
    internal var _reloadedSections: Set<SectionIdentifierType> = []

    public init() {}

    public var numberOfItems: Int { _items.reduce(0) { $0 + $1.count } }
    public var numberOfSections: Int { _sections.count }
    public var sectionIdentifiers: [SectionIdentifierType] { _sections }
    public var itemIdentifiers: [ItemIdentifierType] { _items.flatMap { $0 } }
    public var reloadedItemIdentifiers: [ItemIdentifierType] { Array(_reloaded) }
    public var reloadedSectionIdentifiers: [SectionIdentifierType] { Array(_reloadedSections) }

    public func numberOfItems(inSection identifier: SectionIdentifierType) -> Int {
        guard let s = _sections.firstIndex(of: identifier) else { return 0 }
        return _items[s].count
    }
    public func itemIdentifiers(inSection identifier: SectionIdentifierType) -> [ItemIdentifierType] {
        guard let s = _sections.firstIndex(of: identifier) else { return [] }
        return _items[s]
    }
    public func sectionIdentifier(containingItem identifier: ItemIdentifierType) -> SectionIdentifierType? {
        for (i, items) in _items.enumerated() where items.contains(identifier) { return _sections[i] }
        return nil
    }
    public func indexOfItem(_ identifier: ItemIdentifierType) -> Int? { itemIdentifiers.firstIndex(of: identifier) }
    public func indexOfSection(_ identifier: SectionIdentifierType) -> Int? { _sections.firstIndex(of: identifier) }

    internal func _locate(_ item: ItemIdentifierType) -> (Int, Int)? {
        for (s, items) in _items.enumerated() { if let i = items.firstIndex(of: item) { return (s, i) } }
        return nil
    }

    public mutating func appendItems(_ identifiers: [ItemIdentifierType], toSection sectionIdentifier: SectionIdentifierType? = nil) {
        let s: Int
        if let id = sectionIdentifier {
            guard let i = _sections.firstIndex(of: id) else { fatalError("NSDiffableDataSourceSnapshot: seção inexistente") }
            s = i
        } else {
            guard !_sections.isEmpty else { fatalError("NSDiffableDataSourceSnapshot: sem seções para receber itens") }
            s = _sections.count - 1
        }
        _items[s].append(contentsOf: identifiers)
    }
    public mutating func insertItems(_ identifiers: [ItemIdentifierType], beforeItem beforeIdentifier: ItemIdentifierType) {
        guard let (s, i) = _locate(beforeIdentifier) else { fatalError("NSDiffableDataSourceSnapshot: item inexistente") }
        _items[s].insert(contentsOf: identifiers, at: i)
    }
    public mutating func insertItems(_ identifiers: [ItemIdentifierType], afterItem afterIdentifier: ItemIdentifierType) {
        guard let (s, i) = _locate(afterIdentifier) else { fatalError("NSDiffableDataSourceSnapshot: item inexistente") }
        _items[s].insert(contentsOf: identifiers, at: i + 1)
    }
    public mutating func deleteItems(_ identifiers: [ItemIdentifierType]) {
        let gone = Set(identifiers)
        for s in _items.indices { _items[s].removeAll { gone.contains($0) } }
    }
    public mutating func deleteAllItems() { _sections = []; _items = []; _reloaded = []; _reloadedSections = [] }
    public mutating func moveItem(_ identifier: ItemIdentifierType, beforeItem toIdentifier: ItemIdentifierType) {
        guard identifier != toIdentifier, let (s, i) = _locate(identifier) else { return }
        _items[s].remove(at: i)
        guard let (ts, ti) = _locate(toIdentifier) else { return }
        _items[ts].insert(identifier, at: ti)
    }
    public mutating func moveItem(_ identifier: ItemIdentifierType, afterItem toIdentifier: ItemIdentifierType) {
        guard identifier != toIdentifier, let (s, i) = _locate(identifier) else { return }
        _items[s].remove(at: i)
        guard let (ts, ti) = _locate(toIdentifier) else { return }
        _items[ts].insert(identifier, at: ti + 1)
    }
    public mutating func reloadItems(_ identifiers: [ItemIdentifierType]) { _reloaded.formUnion(identifiers) }
    public mutating func reconfigureItems(_ identifiers: [ItemIdentifierType]) { _reloaded.formUnion(identifiers) }
    public mutating func appendSections(_ identifiers: [SectionIdentifierType]) {
        for id in identifiers { _sections.append(id); _items.append([]) }
    }
    public mutating func insertSections(_ identifiers: [SectionIdentifierType], beforeSection toIdentifier: SectionIdentifierType) {
        guard let i = _sections.firstIndex(of: toIdentifier) else { fatalError("NSDiffableDataSourceSnapshot: seção inexistente") }
        _sections.insert(contentsOf: identifiers, at: i)
        _items.insert(contentsOf: identifiers.map { _ in [] }, at: i)
    }
    public mutating func insertSections(_ identifiers: [SectionIdentifierType], afterSection toIdentifier: SectionIdentifierType) {
        guard let i = _sections.firstIndex(of: toIdentifier) else { fatalError("NSDiffableDataSourceSnapshot: seção inexistente") }
        _sections.insert(contentsOf: identifiers, at: i + 1)
        _items.insert(contentsOf: identifiers.map { _ in [] }, at: i + 1)
    }
    public mutating func deleteSections(_ identifiers: [SectionIdentifierType]) {
        for id in identifiers { if let i = _sections.firstIndex(of: id) { _sections.remove(at: i); _items.remove(at: i) } }
    }
    public mutating func moveSection(_ identifier: SectionIdentifierType, beforeSection toIdentifier: SectionIdentifierType) {
        guard identifier != toIdentifier, let i = _sections.firstIndex(of: identifier) else { return }
        let items = _items.remove(at: i); _sections.remove(at: i)
        guard let t = _sections.firstIndex(of: toIdentifier) else { return }
        _sections.insert(identifier, at: t); _items.insert(items, at: t)
    }
    public mutating func moveSection(_ identifier: SectionIdentifierType, afterSection toIdentifier: SectionIdentifierType) {
        guard identifier != toIdentifier, let i = _sections.firstIndex(of: identifier) else { return }
        let items = _items.remove(at: i); _sections.remove(at: i)
        guard let t = _sections.firstIndex(of: toIdentifier) else { return }
        _sections.insert(identifier, at: t + 1); _items.insert(items, at: t + 1)
    }
    public mutating func reloadSections(_ identifiers: [SectionIdentifierType]) { _reloadedSections.formUnion(identifiers) }
}

// MARK: - NSCollectionViewDiffableDataSource

/// Fonte de dados de um NSCollectionView a partir de snapshots: o item de cada posição sai do itemProvider.
open class NSCollectionViewDiffableDataSource<SectionIdentifierType: Hashable, ItemIdentifierType: Hashable>: NSObject, NSCollectionViewDataSource {
    public typealias ItemProvider = (NSCollectionView, IndexPath, ItemIdentifierType) -> NSCollectionViewItem?
    public typealias SupplementaryViewProvider = (NSCollectionView, String, IndexPath) -> (NSView & NSCollectionViewElement)?

    /// Cabeçalhos e rodapés das seções (viewForSupplementaryElementOfKind:atIndexPath:).
    open var supplementaryViewProvider: SupplementaryViewProvider?

    internal weak var _collectionView: NSCollectionView?
    internal let _provider: ItemProvider
    internal var _snapshot = NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>()

    public init(collectionView: NSCollectionView, itemProvider: @escaping ItemProvider) {
        _collectionView = collectionView
        _provider = itemProvider
        super.init()
        collectionView.dataSource = self
    }

    open func snapshot() -> NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType> { _snapshot }

    open func apply(_ snapshot: NSDiffableDataSourceSnapshot<SectionIdentifierType, ItemIdentifierType>, animatingDifferences: Bool = true, completion: (() -> Void)? = nil) {
        _snapshot = snapshot
        _snapshot._reloaded = []
        _snapshot._reloadedSections = []
        _collectionView?.reloadData()
        completion?()
    }

    open func itemIdentifier(for indexPath: IndexPath) -> ItemIdentifierType? {
        guard indexPath.count >= 2, indexPath.section >= 0, indexPath.section < _snapshot._items.count else { return nil }
        let items = _snapshot._items[indexPath.section]
        return indexPath.item >= 0 && indexPath.item < items.count ? items[indexPath.item] : nil
    }
    open func indexPath(for itemIdentifier: ItemIdentifierType) -> IndexPath? {
        guard let (s, i) = _snapshot._locate(itemIdentifier) else { return nil }
        return IndexPath(item: i, section: s)
    }
    open func sectionIdentifier(for index: Int) -> SectionIdentifierType? {
        index >= 0 && index < _snapshot._sections.count ? _snapshot._sections[index] : nil
    }
    open func index(for sectionIdentifier: SectionIdentifierType) -> Int? { _snapshot._sections.firstIndex(of: sectionIdentifier) }

    // NSCollectionViewDataSource
    open func numberOfSections(in collectionView: NSCollectionView) -> Int { _snapshot._sections.count }
    open func collectionView(_ collectionView: NSCollectionView, numberOfItemsInSection section: Int) -> Int {
        section >= 0 && section < _snapshot._items.count ? _snapshot._items[section].count : 0
    }
    // o importador do mOS não faz a ponte NSIndexPath ↔ IndexPath fora do módulo Foundation: conversão explícita
    open func collectionView(_ collectionView: NSCollectionView, viewForSupplementaryElementOfKind kind: NSString, at nsIndexPath: NSIndexPath) -> NSView & NSCollectionViewElement {
        var idx: [Int] = []
        for p in 0..<Int(nsIndexPath.length) { idx.append(Int(nsIndexPath.index(atPosition: p))) }
        guard let provider = supplementaryViewProvider, let v = provider(collectionView, kind as String, IndexPath(indexes: idx)) else {
            fatalError("NSCollectionViewDiffableDataSource: sem supplementaryViewProvider para \(kind as String)")
        }
        return v
    }
    open func collectionView(_ collectionView: NSCollectionView, itemForRepresentedObjectAt nsIndexPath: NSIndexPath) -> NSCollectionViewItem {
        var idx: [Int] = []
        for p in 0..<Int(nsIndexPath.length) { idx.append(Int(nsIndexPath.index(atPosition: p))) }
        let indexPath = IndexPath(indexes: idx)
        guard let id = itemIdentifier(for: indexPath), let item = _provider(collectionView, indexPath, id) else {
            fatalError("NSCollectionViewDiffableDataSource: o itemProvider não devolveu item para \(indexPath)")
        }
        return item
    }
}
