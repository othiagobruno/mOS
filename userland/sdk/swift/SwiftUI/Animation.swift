// mOS SDK — SwiftUI: animações e transições.
// Modelo inspirado no perfil Embedded do OpenSwiftUI (MIT): a mudança de estado dentro de withAnimation
// (ou com .animation(_:value:)) gera um novo layout; cada view interpola do quadro atual até o novo,
// com a curva da animação, num timer de ~60 Hz. Transições (.scale, .opacity, .slide, .move) animam
// a entrada e a saída das views.
import MOSBridge

public struct Animation: Equatable {
    var duration: Double
    var curve: Int          // 0 linear, 1 easeIn, 2 easeOut, 3 easeInOut
    public static let `default` = Animation(duration: 0.25, curve: 3)
    public static let linear = Animation(duration: 0.25, curve: 0)
    public static let easeIn = Animation(duration: 0.25, curve: 1)
    public static let easeOut = Animation(duration: 0.25, curve: 2)
    public static let easeInOut = Animation(duration: 0.25, curve: 3)
    public static let spring = Animation(duration: 0.4, curve: 2)
    public static let bouncy = Animation(duration: 0.45, curve: 2)
    public static let smooth = Animation(duration: 0.35, curve: 3)
    public static func linear(duration: Double) -> Animation { Animation(duration: duration, curve: 0) }
    public static func easeIn(duration: Double) -> Animation { Animation(duration: duration, curve: 1) }
    public static func easeOut(duration: Double) -> Animation { Animation(duration: duration, curve: 2) }
    public static func easeInOut(duration: Double) -> Animation { Animation(duration: duration, curve: 3) }
    public static func spring(response: Double = 0.4, dampingFraction: Double = 0.8, blendDuration: Double = 0) -> Animation { Animation(duration: response, curve: 2) }
    public func speed(_ s: Double) -> Animation { Animation(duration: duration / max(s, 0.01), curve: curve) }
    func ease(_ t: Double) -> Double {
        switch curve {
        case 1: return t * t
        case 2: return 1 - (1 - t) * (1 - t)
        case 3: return t < 0.5 ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t)
        default: return t
        }
    }
}

/// Aplica as mudanças de estado do bloco com animação.
public func withAnimation<R>(_ animation: Animation? = .default, _ body: () -> R) -> R {
    let rt = _Runtime.shared
    let outer = rt.batching
    rt.batching = true
    let r = body()
    rt.batching = outer
    if !outer {
        rt.animation = animation
        if rt.dirty { rt.dirty = false; mos_request_render() }
        rt.animation = nil
    }
    return r
}

public struct AnyTransition {
    var kind: Int       // 1 opacidade, 2 escala, 3 deslizar, 4 mover da borda
    var edge: Int = 0   // move: 0 leading, 1 trailing, 2 top, 3 bottom
    public static let opacity = AnyTransition(kind: 1)
    public static let scale = AnyTransition(kind: 2)
    public static let slide = AnyTransition(kind: 3)
    public static let identity = AnyTransition(kind: 0)
    public static func move(edge: Edge.Set) -> AnyTransition {
        AnyTransition(kind: 4, edge: edge.contains(.trailing) ? 1 : edge.contains(.top) ? 2 : edge.contains(.bottom) ? 3 : 0)
    }
    public static func scale(scale: Double) -> AnyTransition { .scale }
    public func combined(with other: AnyTransition) -> AnyTransition { kind == 1 ? other : self }
    public static func asymmetric(insertion: AnyTransition, removal: AnyTransition) -> AnyTransition { insertion }
}

extension View {
    public func transition(_ t: AnyTransition) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let parent = b.stack[b.stack.count - 1]
            let before = parent.children.count
            c._build(b)
            func mark(_ n: Node) { n.transition = t.kind; n.transitionEdge = t.edge; for k in n.children { mark(k) } }
            if parent.children.count > before { for i in before..<parent.children.count { mark(parent.children[i]) } }
        }
    }
    /// Anima as mudanças da atualização em que `value` mudou.
    public func animation<V: Equatable>(_ a: Animation?, value: V) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let k = "anim:" + b.path
            let rt = _Runtime.shared
            if let p = rt.storage[k] {
                let box = Unmanaged<_Box<V>>.fromOpaque(p).takeUnretainedValue()
                if box.value != value { box.value = value; if rt.animation == nil { rt.implicitAnimation = a } }
            } else { rt.storage[k] = Unmanaged.passRetained(_Box(value)).toOpaque() }
            c._build(b)
        }
    }
}

// MARK: - Interpolação dos quadros das NSViews

final class _Tween {
    var from: (Double, Double, Double, Double)
    var to: (Double, Double, Double, Double)
    var t: Double = 0
    var anim: Animation
    var removeAfter = false
    init(from: (Double, Double, Double, Double), to: (Double, Double, Double, Double), anim: Animation) { self.from = from; self.to = to; self.anim = anim }
}

final class _Animator {
    var tweens: [String: _Tween] = [:]
    var current: [String: (Double, Double, Double, Double)] = [:]
    var views: [String: UnsafeMutableRawPointer] = [:]
    var timer: UnsafeMutableRawPointer? = nil

    var active: Animation? { _Runtime.shared.animation ?? _Runtime.shared.implicitAnimation }

    /// Quadro "colapsado" de onde a view entra / para onde sai, conforme a transição.
    func collapsed(_ f: (Double, Double, Double, Double), _ n: Node, _ height: Double) -> (Double, Double, Double, Double) {
        switch n.transition {
        case 2: return (f.0 + f.2 / 2, f.1 + f.3 / 2, 0, 0)
        case 1: return (f.0 + f.2 / 2, f.1, 0, f.3)            // opacidade: abre a partir do centro (sem alfa no AppKit do mOS)
        case 3: return (f.0 - f.2 - 20, f.1, f.2, f.3)
        case 4:
            switch n.transitionEdge {
            case 1: return (f.0 + f.2 + 40, f.1, f.2, f.3)
            case 2: return (f.0, height, f.2, f.3)
            case 3: return (f.0, -f.3, f.2, f.3)
            default: return (-f.2, f.1, f.2, f.3)
            }
        default: return f
        }
    }

    /// Posiciona a view do nó; com animação ativa, interpola a partir do quadro atual.
    func place(_ key: String, _ view: UnsafeMutableRawPointer, _ n: Node, _ height: Double, isNew: Bool) {
        let target = (n.x, height - n.y - n.h, n.w, n.h)
        views[key] = view
        if let tw = tweens[key], !tw.removeAfter, same(tw.to, target) { return }   // já indo para lá
        let anim = active
        if let anim, anim.duration > 0 {
            let start: (Double, Double, Double, Double)
            if isNew { start = n.transition != 0 ? collapsed(target, n, height) : target }
            else { start = current[key] ?? target }
            if !same(start, target) {
                tweens[key] = _Tween(from: start, to: target, anim: anim)
                set(key, start)
                startTimer()
                return
            }
        }
        tweens[key] = nil
        set(key, target)
    }
    /// Remove a view; com animação e transição, anima a saída antes.
    func remove(_ key: String, _ hd: _Handle, _ height: Double) -> Bool {
        if let anim = active, anim.duration > 0, hd.node.transition != 0, let cur = current[key] {
            let end = collapsed(cur, hd.node, height)
            let tw = _Tween(from: cur, to: end, anim: anim)
            tw.removeAfter = true
            tweens[key] = tw
            views[key] = hd.view
            startTimer()
            return false   // ainda não remove
        }
        tweens[key] = nil
        current[key] = nil
        views[key] = nil
        return true
    }
    func same(_ a: (Double, Double, Double, Double), _ b: (Double, Double, Double, Double)) -> Bool {
        abs(a.0 - b.0) < 0.5 && abs(a.1 - b.1) < 0.5 && abs(a.2 - b.2) < 0.5 && abs(a.3 - b.3) < 0.5
    }
    func set(_ key: String, _ f: (Double, Double, Double, Double)) {
        current[key] = f
        if let v = views[key] { mos_set_frame(v, f.0, f.1, max(0, f.2), max(0, f.3)) }
    }
    func startTimer() {
        if timer != nil { return }
        timer = mos_timer(0.016, { _ in _animator.tick() }, nil)
    }
    func tick() {
        var done: [String] = []
        for (k, tw) in tweens {
            tw.t += 0.016 / max(tw.anim.duration, 0.016)
            let e = tw.anim.ease(min(1, tw.t))
            let f = (tw.from.0 + (tw.to.0 - tw.from.0) * e, tw.from.1 + (tw.to.1 - tw.from.1) * e,
                     tw.from.2 + (tw.to.2 - tw.from.2) * e, tw.from.3 + (tw.to.3 - tw.from.3) * e)
            set(k, f)
            if tw.t >= 1 { done.append(k) }
        }
        for k in done {
            if let tw = tweens[k], tw.removeAfter {
                if let v = views[k] { mos_remove_from_superview(v) }
                current[k] = nil
                views[k] = nil
                _renderer.dropHandle(k)
            }
            tweens[k] = nil
        }
        if tweens.isEmpty, let t = timer { mos_timer_stop(t); timer = nil }
    }
}
nonisolated(unsafe) let _animator = _Animator()
