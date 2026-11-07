// mOS — overlay Foundation (fase 5, marco B): IndexSet, com a API e os nomes da Apple, sobre faixas ordenadas e
// disjuntas (Range<Int>). Ponte com o NSIndexSet do mOS (que guarda índices um a um).

public struct IndexSet: Equatable, Hashable, BidirectionalCollection, SetAlgebra, Sendable {
    public typealias Element = Int
    public typealias ArrayLiteralElement = Int

    /// faixas ordenadas, disjuntas e não adjacentes
    internal var _ranges: [Range<Int>]

    public struct Index: Comparable, Sendable {
        internal var value: Int
        internal var rangeIndex: Int
        public static func < (a: Index, b: Index) -> Bool { a.value < b.value }
        public static func == (a: Index, b: Index) -> Bool { a.value == b.value && a.rangeIndex == b.rangeIndex }
    }

    public init() { _ranges = [] }
    public init(integer: Int) { _ranges = [integer..<(integer + 1)] }
    public init(integersIn range: Range<Int>) { _ranges = range.isEmpty ? [] : [range] }
    public init<S: Sequence>(_ s: S) where S.Element == Int { self.init(); for i in s { insert(i) } }

    public var count: Int { _ranges.reduce(0) { $0 + $1.count } }
    public var isEmpty: Bool { _ranges.isEmpty }
    public var first: Int? { _ranges.first?.lowerBound }
    public var last: Int? { _ranges.last.map { $0.upperBound - 1 } }

    public var startIndex: Index { _ranges.isEmpty ? endIndex : Index(value: _ranges[0].lowerBound, rangeIndex: 0) }
    public var endIndex: Index { Index(value: _ranges.last?.upperBound ?? 0, rangeIndex: _ranges.count) }
    public subscript(i: Index) -> Int { i.value }
    public func index(after i: Index) -> Index {
        let r = _ranges[i.rangeIndex]
        if i.value + 1 < r.upperBound { return Index(value: i.value + 1, rangeIndex: i.rangeIndex) }
        if i.rangeIndex + 1 < _ranges.count { return Index(value: _ranges[i.rangeIndex + 1].lowerBound, rangeIndex: i.rangeIndex + 1) }
        return endIndex
    }
    public func index(before i: Index) -> Index {
        if i.rangeIndex < _ranges.count, i.value > _ranges[i.rangeIndex].lowerBound { return Index(value: i.value - 1, rangeIndex: i.rangeIndex) }
        let ri = i.rangeIndex - 1
        return Index(value: _ranges[ri].upperBound - 1, rangeIndex: ri)
    }

    public func contains(_ v: Int) -> Bool { _ranges.contains { $0.contains(v) } }
    public func contains(integersIn r: Range<Int>) -> Bool { r.isEmpty || _ranges.contains { $0.lowerBound <= r.lowerBound && r.upperBound <= $0.upperBound } }
    public var rangeView: [Range<Int>] { _ranges }

    internal mutating func _add(_ r: Range<Int>) {
        if r.isEmpty { return }
        var lo = r.lowerBound, hi = r.upperBound
        var out: [Range<Int>] = []
        for x in _ranges {
            if x.upperBound < lo || x.lowerBound > hi { out.append(x) } else { lo = Swift.min(lo, x.lowerBound); hi = Swift.max(hi, x.upperBound) }
        }
        out.append(lo..<hi)
        _ranges = out.sorted { $0.lowerBound < $1.lowerBound }
    }
    internal mutating func _remove(_ r: Range<Int>) {
        if r.isEmpty { return }
        var out: [Range<Int>] = []
        for x in _ranges {
            if x.upperBound <= r.lowerBound || x.lowerBound >= r.upperBound { out.append(x); continue }
            if x.lowerBound < r.lowerBound { out.append(x.lowerBound..<r.lowerBound) }
            if x.upperBound > r.upperBound { out.append(r.upperBound..<x.upperBound) }
        }
        _ranges = out
    }

    public mutating func insert(integersIn r: Range<Int>) { _add(r) }
    public mutating func remove(integersIn r: Range<Int>) { _remove(r) }

    // SetAlgebra
    @discardableResult
    public mutating func insert(_ v: Int) -> (inserted: Bool, memberAfterInsert: Int) {
        if contains(v) { return (false, v) }
        _add(v..<(v + 1)); return (true, v)
    }
    @discardableResult public mutating func remove(_ v: Int) -> Int? { guard contains(v) else { return nil }; _remove(v..<(v + 1)); return v }
    @discardableResult public mutating func update(with v: Int) -> Int? { insert(v).inserted ? nil : v }
    public func union(_ o: IndexSet) -> IndexSet { var r = self; r.formUnion(o); return r }
    public func intersection(_ o: IndexSet) -> IndexSet { var r = self; r.formIntersection(o); return r }
    public func symmetricDifference(_ o: IndexSet) -> IndexSet { var r = self; r.formSymmetricDifference(o); return r }
    public mutating func formUnion(_ o: IndexSet) { for r in o._ranges { _add(r) } }
    public mutating func formIntersection(_ o: IndexSet) { self = IndexSet(filter { o.contains($0) }) }
    public mutating func formSymmetricDifference(_ o: IndexSet) {
        let a = self.subtracting(o), b = o.subtracting(self)
        self = a.union(b)
    }
    public func subtracting(_ o: IndexSet) -> IndexSet { var r = self; for x in o._ranges { r._remove(x) }; return r }

    public static func == (a: IndexSet, b: IndexSet) -> Bool { a._ranges == b._ranges }
    public func hash(into h: inout Hasher) { for r in _ranges { h.combine(r.lowerBound); h.combine(r.upperBound) } }
}

extension IndexSet: ExpressibleByArrayLiteral {
    public init(arrayLiteral elements: Int...) { self.init(elements) }
}

extension IndexSet: CustomStringConvertible {
    public var description: String { "\(count) indexes" }
}

extension IndexSet: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSIndexSet {
        let m = NSMutableIndexSet()
        for i in self { m.add(UInt(i)) }
        return m
    }

    internal static func _mosIndexSet(_ ns: NSIndexSet) -> IndexSet {
        // o NSIndexSet do mOS não tem enumeração; percorre de firstIndex a lastIndex (o mutável sabe dizer)
        var r = IndexSet()
        let first = ns.firstIndex
        if first == UInt.max { return r }
        if let m = ns as? NSMutableIndexSet {
            let last = m.lastIndex
            var i = first
            while i <= last { if m.contains(i) { r.insert(Int(i)) }; i += 1 }
        } else {
            r.insert(Int(first))
        }
        return r
    }

    public static func _forceBridgeFromObjectiveC(_ x: NSIndexSet, result: inout IndexSet?) { result = _mosIndexSet(x) }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSIndexSet, result: inout IndexSet?) -> Bool { result = _mosIndexSet(x); return true }
    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSIndexSet?) -> IndexSet { source.map(_mosIndexSet) ?? IndexSet() }
}
