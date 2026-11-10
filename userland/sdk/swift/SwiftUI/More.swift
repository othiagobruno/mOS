// mOS SDK — SwiftUI: navegação, seleção, abas, formulários, apresentação e utilitários
import MOSBridge

// MARK: - Utilitários


extension View {
    /// .tag(_:) — usado por Picker e TabView para identificar as opções
    public func tag<V>(_ v: V) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let before = b.stack[b.stack.count - 1].children.count
            c._build(b)
            let kids = b.stack[b.stack.count - 1].children
            if kids.count > before { kids[before].tagPtr = Unmanaged.passRetained(_Box(v)).toOpaque() }
        }
    }
    /// .onChange(of:) chama a ação quando o valor muda entre duas atualizações
    public func onChange<V: Equatable>(of value: V, perform action: @escaping (V) -> Void) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let k = "change:" + b.path
            let rt = _Runtime.shared
            if let p = rt.storage[k] {
                let box = Unmanaged<_Box<V>>.fromOpaque(p).takeUnretainedValue()
                if box.value != value { box.value = value; rt.pending.append { action(value) } }
            } else { rt.storage[k] = Unmanaged.passRetained(_Box(value)).toOpaque() }
            c._build(b)
        }
    }
    public func onTapGesture(perform action: @escaping () -> Void) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let z = Node(.zstack)
            b.container(z) { bb in
                c._build(bb)
                let t = bb.add(Node(.tap))
                t.action = action
            }
        }
    }
    public func background(_ col: Color, cornerRadius r: Double) -> _Modified<Self> {
        _Modified(content: self) { b, c in let n = Node(.background); n.color = col; n.radius = r; b.container(n) { c._build($0) } }
    }
    /// Folha modal: quando `isPresented`, o conteúdo aparece sobre a janela.
    public func sheet<S: View>(isPresented: Binding<Bool>, @ViewBuilder content: @escaping () -> S) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let z = Node(.zstack)
            b.container(z) { bb in
                c._build(bb)
                if isPresented.wrappedValue {
                    let dim = Node(.background)
                    dim.color = Color(white: 0, opacity: 0.18); dim.radius = 0
                    let f = Node(.frame); f.maxW = true; f.maxH = true
                    bb.container(dim) { b2 in
                        b2.container(f) { b3 in
                            let panel = Node(.background)
                            panel.color = Color(white: 0.97); panel.radius = 10
                            panel.insets = EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)
                            b3.container(panel) { b4 in b4.child("sheet") { content()._build($0) } }
                        }
                    }
                }
            }
        }
    }
    /// Alerta: mostrado como NSAlert do mOS depois da atualização; ao fechar, isPresented volta a false.
    public func alert(_ title: String, isPresented: Binding<Bool>, message: String = "") -> _Modified<Self> {
        _Modified(content: self) { b, c in
            if isPresented.wrappedValue {
                _Runtime.shared.pending.append {
                    title.withCString { t in message.withCString { m in _ = mos_alert(t, m, "OK", nil) } }
                    isPresented.wrappedValue = false
                }
            }
            c._build(b)
        }
    }
    /// Popover desenhado por cima da view (não é uma NSPopover). O clique fora ou em "Fechar" dispensa.
    public func popover<S: View>(isPresented: Binding<Bool>, @ViewBuilder content: @escaping () -> S) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let z = Node(.zstack)
            b.container(z) { bb in
                c._build(bb)
                if isPresented.wrappedValue {
                    let t = bb.add(Node(.tap))
                    let flag = isPresented
                    t.action = { flag.wrappedValue = false }
                    let panel = Node(.background)
                    panel.color = Color(red: 0.98, green: 0.98, blue: 0.99)
                    panel.radius = 10
                    panel.insets = EdgeInsets(top: 12, leading: 12, bottom: 12, trailing: 12)
                    bb.container(panel) { b2 in
                        let col = Node(.stack)
                        col.axisVertical = true
                        col.spacing = 8
                        col.align = -1
                        b2.container(col) { b3 in
                            content()._build(b3)
                            Button("Fechar") { flag.wrappedValue = false }._build(b3)
                        }
                    }
                }
            }
        }
    }
    public func tabItem<L: View>(@ViewBuilder _ label: () -> L) -> _Modified<Self> {
        let lbl = label()
        return _Modified(content: self) { b, c in
            let tmp = Node(.stack)
            lbl._build(_Builder(root: tmp))
            let tab = Node(.stack)
            tab.axisVertical = true; tab.spacing = 0; tab.maxW = true; tab.maxH = true
            tab.placeholder = _Builder.collectText(tmp)   // título da aba
            b.container(tab) { c._build($0) }
        }
    }
    public func toolbar<C: View>(@ViewBuilder content: () -> C) -> _Modified<Self> { _Modified(content: self) { b, c in c._build(b) } }
    public func listStyle<S>(_ s: S) -> _Modified<Self> { _Modified(content: self) { b, c in c._build(b) } }
    public func pickerStyle<S>(_ s: S) -> _Modified<Self> { _Modified(content: self) { b, c in c._build(b) } }
}
public struct SegmentedPickerStyle { public init() {} }
public struct InsetListStyle { public init() {} }
public struct SidebarListStyle { public init() {} }

// MARK: - Formulários

public struct Label: _PrimitiveView {
    let title: String, icon: Image
    public init(_ title: String, systemImage: String) { self.title = title; icon = Image(systemName: systemImage) }
    public func _build(_ b: _Builder) {
        let n = Node(.stack); n.axisVertical = false; n.spacing = 6
        b.container(n) { icon._build($0); Text(title)._build($0) }
    }
}
public struct Form<Content: View>: _PrimitiveView {
    let content: Content
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
    public func _build(_ b: _Builder) {
        let n = Node(.stack); n.axisVertical = true; n.spacing = 14; n.align = -1; n.maxW = true
        b.container(n) { content._build($0) }
    }
}
public struct Section<Content: View>: _PrimitiveView {
    let header: String, content: Content
    public init(_ header: String, @ViewBuilder content: () -> Content) { self.header = header; self.content = content() }
    public init(header: Text, @ViewBuilder content: () -> Content) { self.header = header.content; self.content = content() }
    public func _build(_ b: _Builder) {
        let n = Node(.stack); n.axisVertical = true; n.spacing = 6; n.align = -1; n.maxW = true
        b.container(n) { bb in
            if !header.isEmpty { Text(header.uppercased()).font(.caption).foregroundColor(.secondary)._build(bb) }
            content._build(bb)
        }
    }
}
public struct GroupBox<Content: View>: _PrimitiveView {
    let title: String, content: Content
    public init(_ title: String = "", @ViewBuilder content: () -> Content) { self.title = title; self.content = content() }
    public func _build(_ b: _Builder) {
        let bg = Node(.background); bg.color = Color(white: 0, opacity: 0.05); bg.radius = 8
        bg.insets = EdgeInsets(top: 10, leading: 10, bottom: 10, trailing: 10)
        b.container(bg) { bb in
            let n = Node(.stack); n.axisVertical = true; n.spacing = 6; n.align = -1
            bb.container(n) { b2 in
                if !title.isEmpty { Text(title).font(.headline)._build(b2) }
                content._build(b2)
            }
        }
    }
}

/// Stepper: rótulo, "−" e "+" (como no macOS, ajusta um inteiro dentro do intervalo)
public struct Stepper: _PrimitiveView {
    let title: String, value: Binding<Int>, range: ClosedRange<Int>, step: Int
    public init(_ title: String, value: Binding<Int>, in range: ClosedRange<Int> = Int.min...Int.max, step: Int = 1) { self.title = title; self.value = value; self.range = range; self.step = step }
    public func _build(_ b: _Builder) {
        let v = value, r = range, s = step
        let n = Node(.stack); n.axisVertical = false; n.spacing = 6
        b.container(n) { bb in
            Text(title)._build(bb)
            Button("−") { if v.wrappedValue - s >= r.lowerBound { v.wrappedValue -= s } }._build(bb)
            Button("+") { if v.wrappedValue + s <= r.upperBound { v.wrappedValue += s } }._build(bb)
        }
    }
}

// MARK: - Picker (opções identificadas por .tag)

public struct Picker<SelectionValue: Equatable, Content: View>: _PrimitiveView {
    let title: String, selection: Binding<SelectionValue>, content: Content
    public init(_ title: String, selection: Binding<SelectionValue>, @ViewBuilder content: () -> Content) { self.title = title; self.selection = selection; self.content = content() }
    public func _build(_ b: _Builder) {
        let tmp = Node(.stack)
        content._build(_Builder(root: tmp))
        let sel = selection
        let row = Node(.stack); row.axisVertical = false; row.spacing = 12
        b.container(row) { bb in
            if !title.isEmpty { Text(title + ":")._build(bb) }
            for opt in tmp.children {
                guard let p = opt.tagPtr else { continue }
                let v = Unmanaged<_Box<SelectionValue>>.fromOpaque(p).takeUnretainedValue().value
                let r = bb.add(Node(.radio))
                r.text = _Builder.collectText(opt)
                r.flag = sel.wrappedValue == v
                r.action = { sel.wrappedValue = v }
            }
        }
    }
}

// MARK: - List com seleção

public struct List<Content: View>: _PrimitiveView {
    let content: Content
    let select: ((_Builder, Node) -> Void)?
    public init(@ViewBuilder content: () -> Content) { self.content = content(); select = nil }
    public func _build(_ b: _Builder) {
        ScrollView { content }._build(b)
    }
}
/// List(itens, selection:) { item in ... } — linha clicável, a selecionada fica destacada.
public struct SelectableList<Element, ID: Equatable, Row: View>: _PrimitiveView {
    let items: [Element], id: (Element) -> ID, selection: Binding<ID?>, row: (Element) -> Row
    public func _build(_ b: _Builder) {
        let n = Node(.stack); n.axisVertical = true; n.spacing = 2; n.align = -1; n.maxW = true
        let sel = selection
        b.container(n) { bb in
            for e in items {
                let key = id(e)
                let selected = sel.wrappedValue == key
                bb.child("row") { b2 in
                    let z = Node(.zstack)
                    b2.container(z) { b3 in
                        let bg = Node(.background)
                        bg.color = selected ? Color.accentColor.opacity(0.22) : Color.clear
                        bg.radius = 5
                        bg.insets = EdgeInsets(top: 4, leading: 8, bottom: 4, trailing: 8)
                        let f = Node(.frame); f.maxW = true; f.align = -1
                        b3.container(bg) { b4 in b4.container(f) { row(e)._build($0) } }
                        let t = b3.add(Node(.tap))
                        t.action = { sel.wrappedValue = key }
                    }
                }
            }
        }
    }
}
extension List {
    public init<Element, ID: Equatable, Row: View>(_ data: [Element], id: @escaping (Element) -> ID, selection: Binding<ID?>, @ViewBuilder rowContent: @escaping (Element) -> Row) where Content == SelectableList<Element, ID, Row> {
        content = SelectableList(items: data, id: id, selection: selection, row: rowContent)
        select = nil
    }
}

// MARK: - TabView

public struct TabView<Content: View>: _PrimitiveView {
    let content: Content
    let selection: Binding<Int>?
    public init(@ViewBuilder content: () -> Content) { self.content = content(); selection = nil }
    public init(selection: Binding<Int>, @ViewBuilder content: () -> Content) { self.content = content(); self.selection = selection }
    public func _build(_ b: _Builder) {
        let own = _Runtime.shared.boxAt("tab:" + b.path, 0)
        let sel = selection ?? Binding(get: { own.value }, set: { own.value = $0; _Runtime.shared.invalidate() })
        let col = Node(.stack); col.axisVertical = true; col.spacing = 10; col.maxW = true; col.maxH = true
        b.container(col) { bb in
            // constrói todas as abas (para ler os títulos) e mostra só a selecionada
            let holder = Node(.stack)
            let hb = _Builder(root: holder)
            hb.paths = [bb.path]
            content._build(hb)
            let tabs = holder.children
            let bar = Node(.stack); bar.axisVertical = false; bar.spacing = 4
            bb.container(bar) { b2 in
                for (i, t) in tabs.enumerated() {
                    let r = b2.add(Node(.radio))
                    r.text = t.placeholder.isEmpty ? "Aba " + String(i + 1) : t.placeholder
                    r.flag = sel.wrappedValue == i
                    r.action = { sel.wrappedValue = i }
                }
            }
            Divider()._build(bb)
            let i = sel.wrappedValue
            if i >= 0 && i < tabs.count { bb.stack[bb.stack.count - 1].children.append(tabs[i]) }
        }
    }
}

// MARK: - Navegação

final class _NavState {
    var stack: [(String, (_Builder) -> Void)] = []
}
nonisolated(unsafe) var _navCurrent: [_NavState] = []

public struct NavigationStack<Content: View>: _PrimitiveView {
    let content: Content
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
    public func _build(_ b: _Builder) {
        let nav = _Runtime.shared.boxAt("nav:" + b.path, _NavState()).value
        let col = Node(.stack); col.axisVertical = true; col.spacing = 10; col.align = -1; col.maxW = true; col.maxH = true
        _navCurrent.append(nav)
        b.container(col) { bb in
            if let top = nav.stack.last {
                let bar = Node(.stack); bar.axisVertical = false; bar.spacing = 10
                bb.container(bar) { b2 in
                    Button("‹ Voltar") { _ = nav.stack.popLast(); _Runtime.shared.invalidate() }._build(b2)
                    Text(top.0).font(.headline)._build(b2)
                }
                Divider()._build(bb)
                bb.child("dest" + String(nav.stack.count)) { top.1($0) }
            } else {
                content._build(bb)
            }
        }
        _navCurrent.removeLast()
    }
}
public typealias NavigationView = NavigationStack

public struct NavigationLink<Label: View, Destination: View>: _PrimitiveView {
    let label: Label, destination: () -> Destination, title: String
    public init(destination: @escaping @autoclosure () -> Destination, @ViewBuilder label: () -> Label) { self.label = label(); self.destination = destination; title = "" }
    public init(@ViewBuilder destination: @escaping () -> Destination, @ViewBuilder label: () -> Label) { self.label = label(); self.destination = destination; title = "" }
    public func _build(_ b: _Builder) {
        let nav = _navCurrent.last
        let tmp = Node(.stack)
        label._build(_Builder(root: tmp))
        let t = _Builder.collectText(tmp)
        let dest = destination
        let n = b.add(Node(.button))
        n.text = t + " ›"
        n.action = {
            guard let nav else { return }
            nav.stack.append((t, { bb in dest()._build(bb) }))
        }
    }
}
extension NavigationLink where Label == Text {
    public init(_ title: String, destination: @escaping @autoclosure () -> Destination) { label = Text(title); self.destination = destination; self.title = title }
}

/// NavigationSplitView: barra lateral à esquerda e detalhe à direita (como no macOS)
public struct NavigationSplitView<Sidebar: View, Detail: View>: _PrimitiveView {
    let sidebar: Sidebar, detail: Detail
    public init(@ViewBuilder sidebar: () -> Sidebar, @ViewBuilder detail: () -> Detail) { self.sidebar = sidebar(); self.detail = detail() }
    public func _build(_ b: _Builder) {
        let row = Node(.stack); row.axisVertical = false; row.spacing = 0; row.align = -1; row.maxW = true; row.maxH = true
        b.container(row) { bb in
            let side = Node(.background); side.color = Color(white: 0, opacity: 0.04); side.radius = 0
            side.insets = EdgeInsets(top: 12, leading: 10, bottom: 12, trailing: 10)
            let f = Node(.frame); f.fixedW = 200; f.maxH = true; f.align = -1
            bb.container(f) { b2 in b2.container(side) { b3 in
                let col = Node(.stack); col.axisVertical = true; col.align = -1; col.maxW = true; col.maxH = true; col.spacing = 4
                b3.container(col) { sidebar._build($0); Spacer()._build($0) }
            } }
            let d = Node(.stack); d.axisVertical = true; d.align = -1; d.maxW = true; d.maxH = true
            d.insets = EdgeInsets(top: 0, leading: 0, bottom: 0, trailing: 0)
            let pad = Node(.padding); pad.insets = EdgeInsets(top: 16, leading: 16, bottom: 16, trailing: 16)
            bb.container(pad) { b2 in b2.container(d) { detail._build($0); Spacer()._build($0) } }
        }
    }
}

// MARK: - glassEffect
//
// macOS 26 pede um material que amostra o fundo. Aqui o fundo não é amostrado:
// o preenchimento é o branco translúcido de mos_sui_glass_fallback (fallback=1, samples_backdrop=0).

public struct Glass {
    public enum Style { case regular, clear, identity }
    public var style: Style
    public var tint: Color?
    public var interactive: Bool
    public init(_ style: Style = .regular) { self.style = style; tint = nil; interactive = true }
    public func tint(_ color: Color) -> Glass { var g = self; g.tint = color; return g }
    public func interactive(_ on: Bool = true) -> Glass { var g = self; g.interactive = on; return g }
    public static var regular: Glass { Glass(.regular) }
    public static var clear: Glass { Glass(.clear) }
    public static var identity: Glass { Glass(.identity) }
}

extension View {
    public func glassEffect(_ glass: Glass = .regular) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            if glass.style == .identity { c._build(b); return }
            var r = 0.0, g = 0.0, bl = 0.0, a = 0.0, rad = 0.0
            var fb: Int32 = 0, samp: Int32 = 0
            mos_sui_glass_fallback(&r, &g, &bl, &a, &rad, &fb, &samp)
            let bg = Node(.background)
            if glass.style == .clear { bg.color = Color(white: 1, opacity: 0.28) }
            else if let tint = glass.tint { bg.color = tint.opacity(0.55) }
            else { bg.color = Color(red: r, green: g, blue: bl, opacity: a) }
            bg.radius = rad
            bg.placeholder = "glass-fallback"
            b.container(bg) { c._build($0) }
        }
    }
    public func glassEffect<S>(_ glass: Glass = .regular, in shape: S) -> _Modified<Self> { glassEffect(glass) }
}

/// O espaçamento é guardado. Não há amostragem compartilhada entre os filhos.
public struct GlassEffectContainer<Content: View>: _PrimitiveView {
    let content: Content
    let spacing: Double
    public init(spacing: Double = 0, @ViewBuilder content: () -> Content) { self.spacing = spacing; self.content = content() }
    public func _build(_ b: _Builder) { content._build(b) }
}
