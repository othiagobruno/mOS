// mOS SDK — SwiftUI: grids, Layout, GeometryReader, Environment.
// As grades viram pilhas (o layout já sabe medir HStack/VStack). O GeometryReader
// lê a proposta do NSHostingView. Environment e EnvironmentObject valem durante o body.
import MOSBridge

// MARK: - Environment

public protocol EnvironmentKey {
    associatedtype Value
    static var defaultValue: Value { get }
}

public final class EnvironmentValues {
    var slots: [ObjectIdentifier: UnsafeMutableRawPointer] = [:]
    public init() {}

    public func value<K: EnvironmentKey>(_ key: K.Type) -> K.Value {
        let id = ObjectIdentifier(K.self)
        if let p = slots[id] {
            return Unmanaged<_Box<K.Value>>.fromOpaque(p).takeUnretainedValue().value
        }
        return K.defaultValue
    }

    public func set<K: EnvironmentKey>(_ key: K.Type, _ newValue: K.Value) {
        let id = ObjectIdentifier(K.self)
        if let p = slots[id] {
            Unmanaged<_Box<K.Value>>.fromOpaque(p).takeUnretainedValue().value = newValue
        } else {
            slots[id] = Unmanaged.passRetained(_Box(newValue)).toOpaque()
        }
    }
}

public enum ColorScheme: Equatable { case light, dark }

public struct ColorSchemeKey: EnvironmentKey {
    public static var defaultValue: ColorScheme { .light }
}

extension EnvironmentValues {
    public var colorScheme: ColorScheme {
        get { value(ColorSchemeKey.self) }
        set { set(ColorSchemeKey.self, newValue) }
    }
}

@propertyWrapper
public struct Environment<Value> {
    let keyPath: KeyPath<EnvironmentValues, Value>
    public init(_ keyPath: KeyPath<EnvironmentValues, Value>) { self.keyPath = keyPath }
    public var wrappedValue: Value { _Runtime.shared.env[keyPath: keyPath] }
}

@propertyWrapper
public struct EnvironmentObject<O: ObservableObject> {
    public init() {}
    public var wrappedValue: O {
        let id = ObjectIdentifier(O.self)
        if let p = _Runtime.shared.objects[id] {
            return Unmanaged<O>.fromOpaque(p).takeUnretainedValue()
        }
        fatalError("EnvironmentObject ausente")
    }
}

extension View {
    public func environment<V>(_ keyPath: WritableKeyPath<EnvironmentValues, V>, _ value: V) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let rt = _Runtime.shared
            let old = rt.env[keyPath: keyPath]
            rt.env[keyPath: keyPath] = value
            c._build(b)
            rt.env[keyPath: keyPath] = old
        }
    }

    public func environmentObject<O: ObservableObject>(_ object: O) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let rt = _Runtime.shared
            let id = ObjectIdentifier(O.self)
            let prev = rt.objects[id]
            rt.objects[id] = Unmanaged.passRetained(object).toOpaque()
            c._build(b)
            if let p = rt.objects[id] { Unmanaged<O>.fromOpaque(p).release() }
            rt.objects[id] = prev
        }
    }

    public func opacity(_ opacity: Double) -> _Modified<Self> {
        _Modified(content: self) { b, c in c._build(b) }
    }

    public func offset(x: Double = 0, y: Double = 0) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let n = Node(.padding)
            n.insets = EdgeInsets(top: max(0, y), leading: max(0, x), bottom: 0, trailing: 0)
            b.container(n) { c._build($0) }
        }
    }

    public func overlay<S: View>(alignment: Alignment = .center, @ViewBuilder content: () -> S) -> _Modified<Self> {
        let extra = content()
        return _Modified(content: self) { b, c in
            let z = Node(.zstack)
            b.container(z) { bb in
                c._build(bb)
                extra._build(bb)
            }
        }
    }

    public func border(_ color: Color, width: Double = 1) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let n = Node(.background)
            n.color = color
            n.radius = 0
            n.insets = EdgeInsets(top: width, leading: width, bottom: width, trailing: width)
            b.container(n) { c._build($0) }
        }
    }

    public func clipped() -> _Modified<Self> {
        _Modified(content: self) { b, c in c._build(b) }
    }

    public func id<I: Hashable>(_ identity: I) -> _Modified<Self> {
        _Modified(content: self) { b, c in b.child("id") { c._build($0) } }
    }
}

// MARK: - GeometryReader

public enum CoordinateSpace { case local, global, named(String) }

public struct GeometryProxy {
    public var sizeWidth: Double
    public var sizeHeight: Double
    public var size: (width: Double, height: Double) { (sizeWidth, sizeHeight) }
    public func frame(in space: CoordinateSpace) -> (x: Double, y: Double, width: Double, height: Double) {
        (0, 0, sizeWidth, sizeHeight)
    }
}

public struct GeometryReader<Content: View>: _PrimitiveView {
    let content: (GeometryProxy) -> Content
    public init(@ViewBuilder content: @escaping (GeometryProxy) -> Content) { self.content = content }
    public func _build(_ b: _Builder) {
        let rt = _Runtime.shared
        let proxy = GeometryProxy(sizeWidth: rt.hostW, sizeHeight: rt.hostH)
        let n = Node(.frame)
        n.maxW = true
        n.maxH = true
        b.container(n) { content(proxy)._build($0) }
    }
}

// MARK: - Grids

public struct GridItem {
    public enum Size { case fixed(Double), flexible, adaptive(minimum: Double) }
    public var size: Size
    public var spacing: Double
    public init(_ size: Size = .flexible, spacing: Double? = nil) {
        self.size = size
        self.spacing = spacing ?? 8
    }
}

func _gridColumns(_ items: [GridItem]) -> Int { max(1, items.count) }

public struct LazyVGrid<Content: View>: _PrimitiveView {
    let columns: [GridItem]
    let spacing: Double
    let content: Content
    public init(columns: [GridItem], spacing: Double? = nil, @ViewBuilder content: () -> Content) {
        self.columns = columns
        self.spacing = spacing ?? 8
        self.content = content()
    }
    public func _build(_ b: _Builder) {
        let holder = Node(.stack)
        content._build(_Builder(root: holder))
        let cols = _gridColumns(columns)
        let gap = columns.first?.spacing ?? spacing
        let outer = Node(.stack)
        outer.axisVertical = true
        outer.spacing = spacing
        outer.align = -1
        b.container(outer) { bb in
            var i = 0
            while i < holder.children.count {
                let row = Node(.stack)
                row.axisVertical = false
                row.spacing = gap
                row.align = -1
                bb.container(row) { rb in
                    let parent = rb.stack[rb.stack.count - 1]
                    var c = 0
                    while c < cols && i < holder.children.count {
                        parent.children.append(holder.children[i])
                        i += 1
                        c += 1
                    }
                }
            }
        }
    }
}

public struct LazyHGrid<Content: View>: _PrimitiveView {
    let rows: [GridItem]
    let spacing: Double
    let content: Content
    public init(rows: [GridItem], spacing: Double? = nil, @ViewBuilder content: () -> Content) {
        self.rows = rows
        self.spacing = spacing ?? 8
        self.content = content()
    }
    public func _build(_ b: _Builder) {
        let holder = Node(.stack)
        content._build(_Builder(root: holder))
        let n = _gridColumns(rows)
        let outer = Node(.stack)
        outer.axisVertical = false
        outer.spacing = spacing
        outer.align = -1
        b.container(outer) { bb in
            var i = 0
            while i < holder.children.count {
                let col = Node(.stack)
                col.axisVertical = true
                col.spacing = rows.first?.spacing ?? spacing
                col.align = -1
                bb.container(col) { cb in
                    let parent = cb.stack[cb.stack.count - 1]
                    var r = 0
                    while r < n && i < holder.children.count {
                        parent.children.append(holder.children[i])
                        i += 1
                        r += 1
                    }
                }
            }
        }
    }
}

public struct GridRow<Content: View>: _PrimitiveView {
    let content: Content
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
    public func _build(_ b: _Builder) {
        let n = Node(.stack)
        n.axisVertical = false
        n.spacing = 8
        n.align = -1
        n.placeholder = "gridrow"
        b.container(n) { content._build($0) }
    }
}

public struct Grid<Content: View>: _PrimitiveView {
    let content: Content
    let hSpacing: Double
    let vSpacing: Double
    public init(alignment: Alignment = .center, horizontalSpacing: Double = 8, verticalSpacing: Double = 8, @ViewBuilder content: () -> Content) {
        self.content = content()
        self.hSpacing = horizontalSpacing
        self.vSpacing = verticalSpacing
    }
    public func _build(_ b: _Builder) {
        let n = Node(.stack)
        n.axisVertical = true
        n.spacing = vSpacing
        n.align = -1
        b.container(n) { content._build($0) }
    }
}

// MARK: - Layout

public struct ProposedViewSize {
    public var width: Double?
    public var height: Double?
    public static let unspecified = ProposedViewSize(width: nil, height: nil)
    public static let infinity = ProposedViewSize(width: 1.0e9, height: 1.0e9)
}

public struct LayoutSubviews {
    public var count: Int
    public init(count: Int) { self.count = count }
}

public protocol Layout {
    func sizeThatFits(proposal: ProposedViewSize, subviews: LayoutSubviews) -> (width: Double, height: Double)
    func placeSubviews(in width: Double, _ height: Double, proposal: ProposedViewSize, subviews: LayoutSubviews)
}

extension Layout {
    public func sizeThatFits(proposal: ProposedViewSize, subviews: LayoutSubviews) -> (width: Double, height: Double) {
        (proposal.width ?? 0, proposal.height ?? 0)
    }
    public func placeSubviews(in width: Double, _ height: Double, proposal: ProposedViewSize, subviews: LayoutSubviews) {}
}

public struct _LayoutHost<L: Layout, Content: View>: _PrimitiveView {
    let layout: L
    let content: Content
    public func _build(_ b: _Builder) {
        let holder = Node(.stack)
        content._build(_Builder(root: holder))
        let proposal = ProposedViewSize(width: _Runtime.shared.hostW, height: _Runtime.shared.hostH)
        let subs = LayoutSubviews(count: holder.children.count)
        let fitted = layout.sizeThatFits(proposal: proposal, subviews: subs)
        layout.placeSubviews(in: fitted.width, fitted.height, proposal: proposal, subviews: subs)
        let n = Node(.stack)
        n.axisVertical = true
        n.spacing = 0
        n.placeholder = "layout"
        if fitted.width > 0 { n.maxW = false }
        b.container(n) { rb in
            let parent = rb.stack[rb.stack.count - 1]
            parent.children.append(contentsOf: holder.children)
        }
    }
}

extension Layout {
    public func callAsFunction<Content: View>(@ViewBuilder content: () -> Content) -> _LayoutHost<Self, Content> {
        _LayoutHost(layout: self, content: content())
    }
}

public struct NavigationPath {
    var items: [String] = []
    public init() {}
    public var count: Int { items.count }
    public var isEmpty: Bool { items.isEmpty }
    public mutating func append<V>(_ value: V) { items.append("item") }
    public mutating func removeLast(_ k: Int = 1) {
        var n = k
        while n > 0 && !items.isEmpty { items.removeLast(); n -= 1 }
    }
}
