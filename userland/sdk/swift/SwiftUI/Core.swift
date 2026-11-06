// mOS SDK — SwiftUI (subconjunto da API da Apple, escrito do zero em Embedded Swift sobre o AppKit do mOS).
// Modelo: cada atualização reavalia `body`, gera uma árvore de nós (`Node`), calcula o layout e
// reconcilia com as NSViews existentes (por caminho), mantendo foco e cursor dos campos de texto.
import MOSBridge

// MARK: - View

public protocol View {
    associatedtype Body: View
    @ViewBuilder var body: Body { get }
    func _build(_ b: _Builder)
}

extension View {
    public func _build(_ b: _Builder) {
        _Runtime.shared.beginScope(b.path)
        let content = body
        _Runtime.shared.endScope()
        content._build(b)
    }
}

extension Never: View {
    public typealias Body = Never
    public var body: Never { fatalError("Never.body") }
    public func _build(_ b: _Builder) {}
}

/// Views primitivas (sem body próprio) implementam `_build` diretamente.
public protocol _PrimitiveView: View where Body == Never {}
extension _PrimitiveView {
    public var body: Never { fatalError("body de view primitiva") }
}

// MARK: - Builder

@resultBuilder
public struct ViewBuilder {
    public static func buildBlock() -> EmptyView { EmptyView() }
    public static func buildBlock<C: View>(_ c: C) -> C { c }
    public static func buildBlock<C0: View, C1: View>(_ c0: C0, _ c1: C1) -> _Pair<C0, C1> { _Pair(c0, c1) }
    public static func buildBlock<C0: View, C1: View, C2: View>(_ c0: C0, _ c1: C1, _ c2: C2) -> _Pair<C0, _Pair<C1, C2>> { _Pair(c0, _Pair(c1, c2)) }
    public static func buildBlock<C0: View, C1: View, C2: View, C3: View>(_ c0: C0, _ c1: C1, _ c2: C2, _ c3: C3) -> _Pair<_Pair<C0, C1>, _Pair<C2, C3>> { _Pair(_Pair(c0, c1), _Pair(c2, c3)) }
    public static func buildBlock<C0: View, C1: View, C2: View, C3: View, C4: View>(_ c0: C0, _ c1: C1, _ c2: C2, _ c3: C3, _ c4: C4) -> _Pair<_Pair<C0, C1>, _Pair<C2, _Pair<C3, C4>>> { _Pair(_Pair(c0, c1), _Pair(c2, _Pair(c3, c4))) }
    public static func buildBlock<C0: View, C1: View, C2: View, C3: View, C4: View, C5: View>(_ c0: C0, _ c1: C1, _ c2: C2, _ c3: C3, _ c4: C4, _ c5: C5) -> _Pair<_Pair<C0, _Pair<C1, C2>>, _Pair<C3, _Pair<C4, C5>>> { _Pair(_Pair(c0, _Pair(c1, c2)), _Pair(c3, _Pair(c4, c5))) }
    public static func buildBlock<C0: View, C1: View, C2: View, C3: View, C4: View, C5: View, C6: View>(_ c0: C0, _ c1: C1, _ c2: C2, _ c3: C3, _ c4: C4, _ c5: C5, _ c6: C6) -> _Pair<_Pair<C0, _Pair<C1, C2>>, _Pair<_Pair<C3, C4>, _Pair<C5, C6>>> { _Pair(_Pair(c0, _Pair(c1, c2)), _Pair(_Pair(c3, c4), _Pair(c5, c6))) }
    public static func buildBlock<C0: View, C1: View, C2: View, C3: View, C4: View, C5: View, C6: View, C7: View>(_ c0: C0, _ c1: C1, _ c2: C2, _ c3: C3, _ c4: C4, _ c5: C5, _ c6: C6, _ c7: C7) -> _Pair<_Pair<_Pair<C0, C1>, _Pair<C2, C3>>, _Pair<_Pair<C4, C5>, _Pair<C6, C7>>> { _Pair(_Pair(_Pair(c0, c1), _Pair(c2, c3)), _Pair(_Pair(c4, c5), _Pair(c6, c7))) }
    public static func buildBlock<C0: View, C1: View, C2: View, C3: View, C4: View, C5: View, C6: View, C7: View, C8: View>(_ c0: C0, _ c1: C1, _ c2: C2, _ c3: C3, _ c4: C4, _ c5: C5, _ c6: C6, _ c7: C7, _ c8: C8) -> _Pair<_Pair<_Pair<C0, C1>, _Pair<C2, C3>>, _Pair<_Pair<C4, C5>, _Pair<C6, _Pair<C7, C8>>>> { _Pair(_Pair(_Pair(c0, c1), _Pair(c2, c3)), _Pair(_Pair(c4, c5), _Pair(c6, _Pair(c7, c8)))) }
    public static func buildBlock<C0: View, C1: View, C2: View, C3: View, C4: View, C5: View, C6: View, C7: View, C8: View, C9: View>(_ c0: C0, _ c1: C1, _ c2: C2, _ c3: C3, _ c4: C4, _ c5: C5, _ c6: C6, _ c7: C7, _ c8: C8, _ c9: C9) -> _Pair<_Pair<_Pair<C0, C1>, _Pair<C2, _Pair<C3, C4>>>, _Pair<_Pair<C5, C6>, _Pair<C7, _Pair<C8, C9>>>> { _Pair(_Pair(_Pair(c0, c1), _Pair(c2, _Pair(c3, c4))), _Pair(_Pair(c5, c6), _Pair(c7, _Pair(c8, c9)))) }
    public static func buildOptional<C: View>(_ c: C?) -> _Optional<C> { _Optional(c) }
    public static func buildEither<A: View, B: View>(first: A) -> _Either<A, B> { _Either(a: first, b: nil) }
    public static func buildEither<A: View, B: View>(second: B) -> _Either<A, B> { _Either(a: nil, b: second) }
    public static func buildLimitedAvailability<C: View>(_ c: C) -> C { c }
}

public struct EmptyView: _PrimitiveView {
    public init() {}
    public func _build(_ b: _Builder) {}
}
public struct _Pair<A: View, B: View>: _PrimitiveView {
    let a: A, b: B
    init(_ a: A, _ b: B) { self.a = a; self.b = b }
    public func _build(_ bd: _Builder) { a._build(bd); b._build(bd) }
}
public struct _Optional<C: View>: _PrimitiveView {
    let c: C?
    init(_ c: C?) { self.c = c }
    public func _build(_ b: _Builder) { if let c { b.child("o") { c._build($0) } } }
}
public struct _Either<A: View, B: View>: _PrimitiveView {
    let a: A?, b: B?
    public func _build(_ bd: _Builder) {
        if let a { bd.child("a") { a._build($0) } }
        if let b { bd.child("b") { b._build($0) } }
    }
}
public struct Group<Content: View>: _PrimitiveView {
    let content: Content
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
    public func _build(_ b: _Builder) { content._build(b) }
}

// MARK: - Árvore de nós

public enum _Kind { case text, button, field, secure, toggle, slider, progress, stack, spacer, divider, padding, frame, scroll, editor, image, background, radio, tap, zstack }

public final class Node {
    var kind: _Kind
    var path: String = ""
    var children: [Node] = []
    var text: String = ""
    var placeholder: String = ""
    var axisVertical = true
    var spacing: Double = 8
    var align: Int = 0          // pilhas: -1 início, 0 centro, 1 fim
    var font: Font = .body
    var color: Color? = nil
    var value: Double = 0
    var lower: Double = 0
    var upper: Double = 1
    var flag = false
    var disabled = false
    var insets = EdgeInsets(top: 0, leading: 0, bottom: 0, trailing: 0)
    var fixedW: Double = -1, fixedH: Double = -1, maxW = false, maxH = false
    var minLength: Double = 0
    var action: (() -> Void)? = nil
    var onText: ((String) -> Void)? = nil
    var onDouble: ((Double) -> Void)? = nil
    var onBool: ((Bool) -> Void)? = nil
    var tagPtr: UnsafeMutableRawPointer? = nil   // .tag(_:) (lido pelo Picker/TabView com o tipo certo)
    var radius: Double = 6
    var transition = 0, transitionEdge = 0
    // layout calculado (coordenadas da janela, topo-esquerda)
    var x: Double = 0, y: Double = 0, w: Double = 0, h: Double = 0
    init(_ k: _Kind) { kind = k }
}

/// Construtor da árvore: mantém o caminho (identidade) e o ambiente (fonte, cor, desabilitado).
public final class _Builder {
    var stack: [Node]
    var counters: [Int] = [0]
    var paths: [String] = ["r"]
    var fonts: [Font] = []
    var colors: [Color] = []
    var disabledDepth = 0
    var path: String { paths[paths.count - 1] + "." + String(counters[counters.count - 1]) }
    init(root: Node) { stack = [root] }

    @discardableResult
    func add(_ n: Node) -> Node {
        let i = counters[counters.count - 1]
        n.path = paths[paths.count - 1] + "/" + String(i)
        counters[counters.count - 1] = i + 1
        if let f = fonts.last { n.font = f }
        if let c = colors.last, n.color == nil { n.color = c }
        if disabledDepth > 0 { n.disabled = true }
        stack[stack.count - 1].children.append(n)
        return n
    }
    /// Nó contêiner: os filhos gerados em `body` entram nele.
    func container(_ n: Node, _ body: (_Builder) -> Void) {
        add(n)
        stack.append(n)
        paths.append(n.path)
        counters.append(0)
        body(self)
        counters.removeLast()
        paths.removeLast()
        stack.removeLast()
    }
    /// Sub-identidade (ramos de if/else, ForEach) sem nó próprio.
    func child(_ tag: String, _ body: (_Builder) -> Void) {
        let i = counters[counters.count - 1]
        counters[counters.count - 1] = i + 1
        paths.append(paths[paths.count - 1] + "/" + tag + String(i))
        counters.append(0)
        body(self)
        counters.removeLast()
        paths.removeLast()
    }
}

// MARK: - Estado

public final class _Box<Value> {
    var value: Value
    init(_ v: Value) { value = v }
}

/// Armazenamento do @State identificado pela posição de criação dentro de cada `body`
/// (como a identidade estrutural do SwiftUI).
public final class _Runtime {
    nonisolated(unsafe) public static let shared = _Runtime()
    var storage: [String: UnsafeMutableRawPointer] = [:]
    var scopes: [String] = ["app"]
    var slot: [Int] = [0]
    var dirty = false
    var rendering = false
    var batching = false
    var animation: Animation? = nil
    var implicitAnimation: Animation? = nil
    var title = ""
    var pending: [() -> Void] = []

    func beginScope(_ p: String) { scopes.append(p); slot.append(0) }
    func endScope() { scopes.removeLast(); slot.removeLast() }
    func nextKey() -> String {
        let i = slot[slot.count - 1]
        slot[slot.count - 1] = i + 1
        return scopes[scopes.count - 1] + "#" + String(i)
    }
    func box<V>(_ initial: V) -> _Box<V> {
        let k = nextKey()
        if let p = storage[k] { return Unmanaged<_Box<V>>.fromOpaque(p).takeUnretainedValue() }
        let b = _Box(initial)
        storage[k] = Unmanaged.passRetained(b).toOpaque()
        return b
    }
    /// estado interno de uma view primitiva, identificado pelo caminho na árvore
    func boxAt<V>(_ key: String, _ initial: V) -> _Box<V> {
        if let p = storage[key] { return Unmanaged<_Box<V>>.fromOpaque(p).takeUnretainedValue() }
        let b = _Box(initial)
        storage[key] = Unmanaged.passRetained(b).toOpaque()
        return b
    }
    public func invalidate() {
        if rendering || batching { dirty = true; return }
        mos_request_render()
    }
}

@propertyWrapper
public struct State<Value> {
    let box: _Box<Value>
    public init(wrappedValue: Value) { box = _Runtime.shared.box(wrappedValue) }
    public init(initialValue: Value) { box = _Runtime.shared.box(initialValue) }
    public var wrappedValue: Value {
        get { box.value }
        nonmutating set { box.value = newValue; _Runtime.shared.invalidate() }
    }
    public var projectedValue: Binding<Value> {
        let b = box
        return Binding(get: { b.value }, set: { b.value = $0; _Runtime.shared.invalidate() })
    }
}

@propertyWrapper
public struct Binding<Value> {
    let getter: () -> Value
    let setter: (Value) -> Void
    public init(get: @escaping () -> Value, set: @escaping (Value) -> Void) { getter = get; setter = set }
    public static func constant(_ v: Value) -> Binding<Value> { Binding(get: { v }, set: { _ in }) }
    public var wrappedValue: Value {
        get { getter() }
        nonmutating set { setter(newValue) }
    }
    public var projectedValue: Binding<Value> { self }
}

/// Objeto observável simples: subclasses chamam `objectWillChange()` (ou usam @Published).
open class ObservableObject {
    public init() {}
    public func objectWillChange() { _Runtime.shared.invalidate() }
}
@propertyWrapper
public struct Published<Value> {
    let box: _Box<Value>
    public init(wrappedValue: Value) { box = _Box(wrappedValue) }
    public var wrappedValue: Value {
        get { box.value }
        nonmutating set { box.value = newValue; _Runtime.shared.invalidate() }
    }
    public var projectedValue: Binding<Value> {
        let b = box
        return Binding(get: { b.value }, set: { b.value = $0; _Runtime.shared.invalidate() })
    }
}
@propertyWrapper
public struct StateObject<O: ObservableObject> {
    let box: _Box<O>
    public init(wrappedValue: @autoclosure () -> O) {
        let k = _Runtime.shared.nextKey()
        if let p = _Runtime.shared.storage[k] { box = Unmanaged<_Box<O>>.fromOpaque(p).takeUnretainedValue() }
        else { let b = _Box(wrappedValue()); _Runtime.shared.storage[k] = Unmanaged.passRetained(b).toOpaque(); box = b }
    }
    public var wrappedValue: O { box.value }
}
@propertyWrapper
public struct ObservedObject<O: ObservableObject> {
    public let wrappedValue: O
    public init(wrappedValue: O) { self.wrappedValue = wrappedValue }
}
