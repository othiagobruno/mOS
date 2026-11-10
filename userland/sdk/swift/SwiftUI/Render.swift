// mOS SDK — SwiftUI: layout, reconciliação com o AppKit e ciclo de vida do app
import MOSBridge

// MARK: - Layout (proposta → tamanho → posição, como o SwiftUI)

func cstr(_ s: String, _ f: (UnsafePointer<CChar>) -> Void) { s.withCString { f($0) } }

func measureText(_ s: String, _ font: Font) -> (Double, Double) {
    var w = 0.0, h = 0.0
    s.withCString { mos_measure($0, font.size, Int32(font.weight), &w, &h) }
    return (w, h)
}

/// Flexível no eixo: ocupa o espaço que sobrar.
func flexible(_ n: Node, vertical: Bool) -> Bool {
    switch n.kind {
    case .spacer: return true
    case .field, .secure, .slider, .progress: return !vertical
    case .divider: return false
    case .editor: return true
    case .frame: if vertical ? n.maxH : n.maxW { return true }; if vertical ? n.fixedH >= 0 : n.fixedW >= 0 { return false }
    default: break
    }
    if n.kind == .background && n.children.isEmpty { return vertical ? n.maxH : n.maxW }   // Color como view
    if n.kind == .zstack { if let f = n.children.first { return flexible(f, vertical: vertical) }; return false }
    if (n.kind == .stack || n.kind == .scroll) && (vertical ? n.maxH : n.maxW) { return true }
    for c in n.children {
        // um Spacer só expande no eixo da pilha que o contém
        if c.kind == .spacer { if n.kind == .stack && n.axisVertical == vertical { return true }; continue }
        if flexible(c, vertical: vertical) { return true }
    }
    return false
}

/// Tamanho ideal de um nó para a proposta (pw, ph); -1 = sem limite.
func size(_ n: Node, _ pw: Double, _ ph: Double) -> (Double, Double) {
    switch n.kind {
    case .text:
        let (w, h) = measureText(n.text, n.font)
        return (pw >= 0 ? min(w, pw) : w, h)
    case .button:
        let (w, _) = measureText(n.text, n.font)
        return (max(w + 28, 70), 24)
    case .field, .secure: return (pw >= 0 ? pw : 200, 24)
    case .editor: return (pw >= 0 ? pw : 300, ph >= 0 ? ph : 160)
    case .toggle, .radio: let (w, _) = measureText(n.text, n.font); return (w + 24, 18)
    case .tap: return (0, 0)
    case .zstack:
        var (w, h) = (0.0, 0.0)
        for c in n.children { let s = size(c, pw, ph); w = max(w, s.0); h = max(h, s.1) }
        return (w, h)
    case .slider: return (pw >= 0 ? pw : 160, 22)
    case .progress: return (pw >= 0 ? pw : 160, 12)
    case .spacer: return (n.minLength, n.minLength)
    case .divider: return (1, 1)
    case .padding, .background:
        let i = n.insets
        var (w, h) = (0.0, 0.0)
        for c in n.children { let s = size(c, pw >= 0 ? max(0, pw - i.leading - i.trailing) : -1, ph >= 0 ? max(0, ph - i.top - i.bottom) : -1); w = max(w, s.0); h = max(h, s.1) }
        return (w + i.leading + i.trailing, h + i.top + i.bottom)
    case .frame:
        let cw = n.fixedW >= 0 ? n.fixedW : (n.maxW && pw >= 0 ? pw : pw)
        let ch = n.fixedH >= 0 ? n.fixedH : (n.maxH && ph >= 0 ? ph : ph)
        var (w, h) = (0.0, 0.0)
        for c in n.children { let s = size(c, cw, ch); w = max(w, s.0); h = max(h, s.1) }
        if n.fixedW >= 0 { w = n.fixedW } else if n.maxW && pw >= 0 { w = pw }
        if n.fixedH >= 0 { h = n.fixedH } else if n.maxH && ph >= 0 { h = ph }
        return (w, h)
    case .scroll:
        // Eixo aberto: o mínimo é zero para o pai entregar o que sobrar. O tamanho do
        // conteúdo é medido de novo em placeScroll, que é quem aplica o deslocamento.
        if pw >= 0 && ph >= 0 { return (pw, ph) }
        let content = stackSize(n, pw, ph)
        return (pw >= 0 ? pw : content.0, ph >= 0 ? ph : 0)
    case .stack, .image:
        return stackSize(n, pw, ph)
    }
}

func stackSize(_ n: Node, _ pw: Double, _ ph: Double) -> (Double, Double) {
    let v = n.axisVertical
    let kids = n.children
    if kids.isEmpty { return (0, 0) }
    let gaps = n.spacing * Double(kids.count - 1)
    var main = gaps, cross = 0.0
    var flex = 0
    for c in kids {
        if flexible(c, vertical: v) { flex += 1 }
        let s = v ? size(c, pw, -1) : size(c, -1, ph)
        if !flexible(c, vertical: v) || c.kind != .spacer { main += v ? s.1 : s.0 }
        cross = max(cross, v ? s.0 : s.1)
    }
    let pm = v ? ph : pw
    if flex > 0 && pm >= 0 { main = max(main, pm) }
    if (v ? n.maxW : n.maxH) && (v ? pw : ph) >= 0 { cross = v ? pw : ph }
    if v && n.maxH && ph >= 0 { main = ph }
    return v ? (cross, main) : (main, cross)
}

/// Posiciona o nó (e os filhos) no retângulo dado.
func place(_ n: Node, _ x: Double, _ y: Double, _ w: Double, _ h: Double) {
    n.x = x; n.y = y; n.w = w; n.h = h
    switch n.kind {
    case .zstack:
        for c in n.children {
            if c.kind == .tap { place(c, x, y, w, h); continue }   // área clicável cobre o conjunto
            let s = size(c, w, h)
            let cw = flexible(c, vertical: false) ? w : min(s.0, w), ch = flexible(c, vertical: true) ? h : min(s.1, h)
            place(c, x + (w - cw) / 2, y + (h - ch) / 2, cw, ch)
        }
    case .padding, .background:
        let i = n.insets
        for c in n.children { place(c, x + i.leading, y + i.top, max(0, w - i.leading - i.trailing), max(0, h - i.top - i.bottom)) }
    case .frame:
        for c in n.children {
            let s = size(c, w, h)
            let cw = flexible(c, vertical: false) ? w : min(s.0, w), ch = flexible(c, vertical: true) ? h : min(s.1, h)
            let cx = n.align < 0 ? x : n.align > 0 ? x + w - cw : x + (w - cw) / 2
            place(c, cx, y + (h - ch) / 2, cw, ch)
        }
    case .scroll:
        placeScroll(n, x, y, w, h)
    case .stack, .image:
        let v = n.axisVertical
        let kids = n.children
        var fixedMain = n.spacing * Double(max(0, kids.count - 1))
        var flex: [Bool] = []
        var sizes: [(Double, Double)] = []
        for c in kids {
            let f = c.kind == .spacer ? true : flexible(c, vertical: v)
            flex.append(f)
            let s = v ? size(c, w, -1) : size(c, -1, h)
            sizes.append(s)
            if !f { fixedMain += v ? s.1 : s.0 } else if c.kind != .spacer { fixedMain += 0 }
        }
        let nflex = flex.filter { $0 }.count
        let total = v ? h : w
        let extra = nflex > 0 ? max(0, total - fixedMain) / Double(nflex) : 0
        var pos = v ? y : x
        if nflex == 0 { pos += v ? (h - fixedMain) / 2 : (w - fixedMain) / 2 }    // conteúdo centralizado
        for (i, c) in kids.enumerated() {
            var cw: Double, ch: Double
            if v {
                ch = flex[i] ? max(extra, c.kind == .spacer ? c.minLength : sizes[i].1) : sizes[i].1
                cw = flexible(c, vertical: false) || c.kind == .divider ? w : min(sizes[i].0, w)
                let cx = n.align < 0 ? x : n.align > 0 ? x + w - cw : x + (w - cw) / 2
                place(c, cx, pos, cw, ch)
                pos += ch + n.spacing
            } else {
                cw = flex[i] ? max(extra, c.kind == .spacer ? c.minLength : sizes[i].0) : sizes[i].0
                ch = flexible(c, vertical: true) || c.kind == .divider ? h : min(sizes[i].1, h)
                let cy = n.align < 0 ? y : n.align > 0 ? y + h - ch : y + (h - ch) / 2
                place(c, pos, cy, cw, ch)
                pos += cw + n.spacing
            }
        }
    default: break
    }
}

/// ScrollView: o conteúdo guarda o tamanho intrínseco e anda com `n.value` (deslocamento).
/// O valor já limitado volta pelo `onDouble`, para o próximo quadro não acumular além do fim.
func placeScroll(_ n: Node, _ x: Double, _ y: Double, _ w: Double, _ h: Double) {
    n.x = x; n.y = y; n.w = w; n.h = h
    let v = n.axisVertical
    let kids = n.children
    var sizes: [(Double, Double)] = []
    var contentMain = n.spacing * Double(max(0, kids.count - 1))
    for c in kids {
        let s = v ? size(c, w, -1) : size(c, -1, h)
        sizes.append(s)
        contentMain += v ? s.1 : s.0
    }
    let viewport = v ? h : w
    let off = mos_sui_clamp_offset(n.value, 0, viewport, contentMain)
    n.value = off
    n.upper = contentMain
    n.onDouble?(off)
    if n.placeholder.hasPrefix("scroll:") {
        _ScrollHits.all.append(_ScrollHit(x: x, y: y, w: w, h: h, content: contentMain, viewport: viewport, key: n.placeholder, vertical: v))
    }
    var pos = (v ? y : x) - off
    for (i, c) in kids.enumerated() {
        if v {
            let ch = sizes[i].1
            let cw = min(max(sizes[i].0, 0), w)
            let cx = n.align < 0 ? x : n.align > 0 ? x + max(0, w - cw) : x + max(0, w - cw) / 2
            place(c, cx, pos, cw > 0 ? cw : w, ch)
            pos += ch + n.spacing
        } else {
            let cw = sizes[i].0
            let ch = min(max(sizes[i].1, 0), h)
            place(c, pos, y, cw, ch > 0 ? ch : h)
            pos += cw + n.spacing
        }
    }
}

func markClipOut(_ n: Node) {
    n.clipOut = true
    n.w = 0
    n.h = 0
    for c in n.children { markClipOut(c) }
}

func applyClip(_ nodes: [Node], _ x: Double, _ y: Double, _ w: Double, _ h: Double) {
    for c in nodes {
        var ix = 0.0, iy = 0.0, iw = 0.0, ih = 0.0
        if mos_sui_intersect(c.x, c.y, c.w, c.h, x, y, w, h, &ix, &iy, &iw, &ih) == 0 {
            markClipOut(c)
            continue
        }
        c.x = ix; c.y = iy; c.w = iw; c.h = ih
        c.clipOut = false
        applyClip(c.children, x, y, w, h)
    }
}

func clipScrolls(_ n: Node) {
    if n.kind == .scroll { applyClip(n.children, n.x, n.y, n.w, n.h) }
    for c in n.children { clipScrolls(c) }
}

struct _ScrollHit {
    var x, y, w, h, content, viewport: Double
    var key: String
    var vertical: Bool
}

/// Rolagens vivas no último layout. A roda do _CXSwiftHost cai aqui.
enum _ScrollHits {
    static var all: [_ScrollHit] = []

    static func wheel(_ x: Double, _ y: Double, _ dx: Double, _ dy: Double) {
        let inside = all.filter { x >= $0.x && y >= $0.y && x < $0.x + $0.w && y < $0.y + $0.h }
        let hit = inside.min { $0.w * $0.h < $1.w * $1.h } ?? (all.count == 1 ? all[0] : nil)
        guard let hit else { return }
        let box: _Box<Double> = _Runtime.shared.boxAt(hit.key, 0.0)
        // scrollingDeltaY é o oposto do notch (positivo = para cima). Um notch anda 40 pt, como o NSScrollView.
        let delta = (hit.vertical ? -dy : -dx) * 40
        let next = mos_sui_clamp_offset(box.value, delta, hit.viewport, hit.content)
        if next == box.value { return }
        box.value = next
        _Runtime.shared.invalidate()
    }
}

// MARK: - Reconciliação: uma NSView por nó primitivo, reaproveitada pelo caminho

final class _Handle {
    var view: UnsafeMutableRawPointer
    var kind: _Kind
    var target: UnsafeMutableRawPointer? = nil
    var node: Node
    var lastText = ""
    var seen = true
    var fresh = true
    var leaving = false
    var key = ""
    init(view: UnsafeMutableRawPointer, kind: _Kind, node: Node) { self.view = view; self.kind = kind; self.node = node }
}

final class _Renderer {
    func dropHandle(_ k: String) { handles[k] = nil }
    var handles: [String: _Handle] = [:]
    var build: ((_Builder) -> Void)? = nil
    var content: UnsafeMutableRawPointer? = nil
    var height: Double = 0

    func render(_ content: UnsafeMutableRawPointer, _ w: Double, _ h: Double) {
        self.content = content
        height = h
        let rt = _Runtime.shared
        var pass = 0
        repeat {
            rt.dirty = false
            rt.rendering = true
            _ScrollHits.all.removeAll()
            let root = Node(.stack)
            root.axisVertical = true; root.spacing = 0
            let b = _Builder(root: root)
            build?(b)
            // conteúdo da janela: ocupa tudo se for flexível, senão centralizado
            let s = size(root, w, h)
            let fw = flexible(root, vertical: false) ? w : min(s.0, w), fh = flexible(root, vertical: true) ? h : min(s.1, h)
            place(root, (w - fw) / 2, (h - fh) / 2, fw, fh)
            clipScrolls(root)
            for (_, hd) in handles { hd.seen = false }
            sync(root)
            for (k, hd) in handles where !hd.seen && !hd.leaving {
                if _animator.remove(k, hd, h) { mos_remove_from_superview(hd.view); handles[k] = nil } else { hd.leaving = true }
            }
            rt.implicitAnimation = nil
            rt.rendering = false
            let pend = rt.pending
            rt.pending = []
            for a in pend { a() }
            pass += 1
        } while rt.dirty && pass < 4
    }

    func sync(_ n: Node) {
        if n.clipOut {
            let key = n.path + ":" + String(n.kind.code)
            if let hd = handles[key] {
                hd.seen = true
                mos_send_i(hd.view, "setHidden:", 1)
            }
            if n.kind == .scroll, let thumb = handles[n.path + ":thumb"] {
                thumb.seen = true
                mos_send_i(thumb.view, "setHidden:", 1)
            }
            for c in n.children { sync(c) }
            return
        }
        switch n.kind {
        case .stack, .padding, .frame, .spacer, .image, .zstack: for c in n.children { sync(c) }; return
        case .scroll:
            for c in n.children { sync(c) }
            syncScrollIndicator(n)
            return
        case .background:
            // fundo: uma caixa colorida criada antes (atrás) do conteúdo
            let hd = handle(n, "NSBox")
            mos_send_i(hd.view, "setHidden:", 0)
            mos_send_i(hd.view, "setBoxType:", 4)
            mos_send_d(hd.view, "setCornerRadius:", n.radius)
            if let c = n.color { mos_send_p(hd.view, "setFillColor:", mos_color(c.r, c.g, c.b, c.a)) }
            _animator.place(hd.key, hd.view, n, height, isNew: hd.fresh)
            hd.fresh = false
            for c in n.children { sync(c) }
            return
        default: break
        }
        let cls: String
        switch n.kind {
        case .text: cls = "NSTextField"
        case .button, .toggle, .radio, .tap: cls = "NSButton"
        case .field, .secure: cls = "NSTextField"
        case .slider: cls = "NSSlider"
        case .progress: cls = "NSProgressIndicator"
        case .divider: cls = "NSBox"
        case .editor: cls = "NSScrollView"
        default: cls = "NSView"
        }
        let hd = handle(n, cls)
        let v = hd.view
        mos_send_i(v, "setHidden:", 0)
        hd.node = n
        switch n.kind {
        case .text:
            if hd.lastText != n.text { cstr(n.text) { mos_set_string(v, "setStringValue:", $0) }; hd.lastText = n.text }
            mos_send_p(v, "setFont:", mos_font(n.font.size, Int32(n.font.weight)))
            let c = n.color ?? Color.primary
            mos_send_p(v, "setTextColor:", mos_color(c.r, c.g, c.b, c.a))
        case .button:
            if hd.lastText != n.text { cstr(n.text) { mos_set_string(v, "setTitle:", $0) }; hd.lastText = n.text }
            mos_send_i(v, "setEnabled:", n.disabled ? 0 : 1)
        case .tap:
            break
        case .radio:
            if hd.lastText != n.text { cstr(n.text) { mos_set_string(v, "setTitle:", $0) }; hd.lastText = n.text }
            mos_send_i(v, "setState:", n.flag ? 1 : 0)
        case .toggle:
            if hd.lastText != n.text { cstr(n.text) { mos_set_string(v, "setTitle:", $0) }; hd.lastText = n.text }
            mos_send_i(v, "setState:", n.flag ? 1 : 0)
            mos_send_i(v, "setEnabled:", n.disabled ? 0 : 1)
        case .field, .secure:
            let cur = String(cString: mos_get_string(v, "stringValue"))
            if cur != n.text { cstr(n.text) { mos_set_string(v, "setStringValue:", $0) } }
            cstr(n.placeholder) { mos_set_string(v, "setPlaceholderString:", $0) }
            mos_send_i(v, "setEnabled:", n.disabled ? 0 : 1)
        case .slider:
            mos_send_d(v, "setMinValue:", n.lower); mos_send_d(v, "setMaxValue:", n.upper)
            if mos_send_ret_d(v, "doubleValue") != n.value { mos_send_d(v, "setDoubleValue:", n.value) }
        case .progress:
            mos_send_d(v, "setMinValue:", 0); mos_send_d(v, "setMaxValue:", n.upper); mos_send_d(v, "setDoubleValue:", n.value)
        case .editor:
            if let tv = mos_send(v, "documentView") {
                let cur = String(cString: mos_get_string(tv, "string"))
                if cur != n.text { cstr(n.text) { mos_set_string(tv, "setString:", $0) } }
            }
        default: break
        }
        // coordenadas: o conteúdo da janela usa origem embaixo à esquerda (com animação, interpola)
        _animator.place(hd.key, v, n, height, isNew: hd.fresh)
        hd.fresh = false
    }

    func handle(_ n: Node, _ cls: String) -> _Handle {
        let key = n.path + ":" + String(n.kind.code)
        if let hd = handles[key] {
            hd.seen = true
            if hd.leaving { hd.leaving = false; _animator.tweens[key] = nil }   // voltou antes de terminar de sair
            return hd
        }
        var view: UnsafeMutableRawPointer
        if n.kind == .text {
            view = n.text.withCString { mos_send_p(mos_class("NSTextField"), "labelWithString:", mos_str($0))! }
            mos_retain(view)
        } else if n.kind == .editor {
            view = mos_send(mos_class("NSTextView"), "scrollableTextView")!
            mos_retain(view)
        } else if n.kind == .divider {
            view = mos_new_view("NSBox", 0, 0, 1, 1)!
            mos_send_i(view, "setBoxType:", 2)
        } else {
            view = cls.withCString { mos_new_view($0, 0, 0, 10, 10)! }
        }
        mos_add_subview(content, view)
        let hd = _Handle(view: view, kind: n.kind, node: n)
        hd.key = key
        handles[key] = hd
        let ctx = Unmanaged.passUnretained(hd).toOpaque()
        switch n.kind {
        case .button, .toggle, .slider, .radio, .tap:
            if n.kind == .toggle { mos_send_i(view, "setButtonType:", 3) }
            if n.kind == .radio { mos_send_i(view, "setButtonType:", 4) }
            if n.kind == .tap { mos_send_i(view, "setBordered:", 0); mos_set_string(view, "setTitle:", "") }
            hd.target = mos_on_action(view, { p in
                let hd = Unmanaged<_Handle>.fromOpaque(p!).takeUnretainedValue()
                switch hd.kind {
                case .button, .radio, .tap: hd.node.action?()
                case .toggle: hd.node.onBool?(mos_send_ret_i(hd.view, "state") != 0)
                case .slider: hd.node.onDouble?(mos_send_ret_d(hd.view, "doubleValue"))
                default: break
                }
                _Runtime.shared.invalidate()
            }, ctx)
        case .field, .secure:
            hd.target = mos_on_text(view, { p, t in
                let hd = Unmanaged<_Handle>.fromOpaque(p!).takeUnretainedValue()
                hd.node.onText?(String(cString: t!))
            }, ctx)
        case .editor:
            if let tv = mos_send(view, "documentView") {
                hd.target = mos_on_text(tv, { p, t in
                    let hd = Unmanaged<_Handle>.fromOpaque(p!).takeUnretainedValue()
                    hd.node.onText?(String(cString: t!))
                }, ctx)
            }
        default: break
        }
        return hd
    }

    /// Polegar à direita (ou embaixo). Some quando o conteúdo cabe ou flag está desligada.
    func syncScrollIndicator(_ n: Node) {
        var x = 0.0, y = 0.0, w = 0.0, h = 0.0
        let vertical = n.axisVertical
        let viewport = vertical ? n.h : n.w
        let on = mos_sui_indicator(n.value, viewport, n.upper, Int32(n.flag ? 1 : 0), n.x, n.y, n.w, n.h, Int32(vertical ? 1 : 0), &x, &y, &w, &h)
        let key = n.path + ":thumb"
        if on == 0 || w < 1 || h < 1 {
            if let hd = handles[key] {
                hd.seen = true
                mos_send_i(hd.view, "setHidden:", 1)
            }
            return
        }
        let hd: _Handle
        if let existing = handles[key] {
            existing.seen = true
            hd = existing
        } else if let parent = content, let view = mos_new_view("NSBox", 0, 0, w, h) {
            mos_send_i(view, "setBoxType:", 4)
            mos_send_d(view, "setCornerRadius:", 2.5)
            mos_send_p(view, "setFillColor:", mos_color(80.0 / 255.0, 80.0 / 255.0, 88.0 / 255.0, 1))
            mos_add_subview(parent, view)
            let node = Node(.background)
            node.path = key
            let created = _Handle(view: view, kind: .background, node: node)
            created.key = key
            handles[key] = created
            hd = created
        } else {
            return
        }
        mos_send_i(hd.view, "setHidden:", 0)
        if let parent = content { mos_add_subview(parent, hd.view) }
        let thumb = hd.node
        thumb.x = x
        thumb.y = y
        thumb.w = w
        thumb.h = h
        _animator.place(key, hd.view, thumb, height, isNew: hd.fresh)
        hd.fresh = false
    }
}

extension _Kind {
    var code: Int {
        switch self {
        case .text: return 0; case .button: return 1; case .field: return 2; case .secure: return 3; case .toggle: return 4
        case .slider: return 5; case .progress: return 6; case .stack: return 7; case .spacer: return 8; case .divider: return 9
        case .padding: return 10; case .frame: return 11; case .scroll: return 12; case .editor: return 13; case .image: return 14
        case .background: return 15; case .radio: return 16; case .tap: return 17; case .zstack: return 18
        }
    }
}

// MARK: - App e Scene

public protocol Scene {
    func _start()
}
public protocol App {
    associatedtype Body: Scene
    var body: Body { get }
    init()
}
extension App {
    public static func main() {
        let app = Self()
        _Runtime.shared.beginScope("app")
        let scene = app.body
        _Runtime.shared.endScope()
        scene._start()
    }
}

nonisolated(unsafe) let _renderer = _Renderer()

public struct WindowGroup<Content: View>: Scene {
    let title: String
    let content: () -> Content
    var width: Double = 480, height: Double = 360
    public init(_ title: String = "", @ViewBuilder content: @escaping () -> Content) { self.title = title; self.content = content }
    public func defaultSize(width: Double, height: Double) -> WindowGroup<Content> { var s = self; s.width = width; s.height = height; return s }
    public func _start() {
        let make = content
        let host = NSHostingView(rootView: make())
        host.rebuild = make
        _currentHost = host
        let t = title.isEmpty ? _Runtime.shared.title : title
        t.withCString { tp in
            mos_run_app(tp, width, height, { _, content, w, h in
                _currentHost?.embed(content!, w, h)
            }, nil)
        }
    }
}
