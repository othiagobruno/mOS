// NSHostingView / NSHostingController do runtime embutido.
//
// A subclasse real de NSView/NSViewController está em
// userland/sdk/swift-objc/SwiftUI/UI/NSHosting.swift (SwiftUI.framework, com interop ObjC).
// Embedded Swift não declara classes ObjC, então esta view é dona de uma NSView
// criada por mos_new_view e hospeda a árvore SwiftUI dentro dela.
// Não declare outra classe ObjC com o nome NSHostingView no libSystem: o nome
// ObjC da classe Swift do framework já é NSHostingView.

import MOSBridge

protocol _HostBox: AnyObject {
    func embed(_ content: UnsafeMutableRawPointer, _ w: Double, _ h: Double)
}

nonisolated(unsafe) var _currentHost: _HostBox?

public final class NSHostingView<Content: View>: _HostBox {
    public var rootView: Content
    public var rebuild: (() -> Content)?
    var view: UnsafeMutableRawPointer?
    var embedded = false
    var width: Double = 0
    var height: Double = 0

    public init(rootView: Content) { self.rootView = rootView }

    public func embed(_ content: UnsafeMutableRawPointer, _ w: Double, _ h: Double) {
        width = w
        height = h
        _Runtime.shared.hostW = w
        _Runtime.shared.hostH = h
        if view == nil {
            view = mos_host_view(0, 0, w, h)
            if let view {
                mos_view_on_scroll(view, { _, x, y, dx, dy in
                    _ScrollHits.wheel(x, y, dx, dy)
                }, UnsafeMutableRawPointer(bitPattern: 1))
            }
        }
        guard let view else { return }
        if !embedded {
            mos_add_subview(content, view)
            embedded = true
        }
        mos_set_frame(view, 0, 0, w, h)
        let make = rebuild
        let root = rootView
        _renderer.build = { b in
            _Runtime.shared.beginScope("host")
            let body = make?() ?? root
            _Runtime.shared.endScope()
            body._build(b)
        }
        let host = self
        _Runtime.shared.hostRelayout = {
            guard let view = host.view else { return }
            _renderer.render(view, host.width, host.height)
        }
        _renderer.render(view, w, h)
    }
}

/// No runtime embutido o controlador só segura o hosting view. A subclasse de
/// NSViewController está no SwiftUI.framework.
public final class NSHostingController<Content: View> {
    public let hosting: NSHostingView<Content>
    public var rootView: Content {
        get { hosting.rootView }
        set { hosting.rootView = newValue }
    }
    public init(rootView: Content) { hosting = NSHostingView(rootView: rootView) }
}

/// Roda a mesma cena do teste C (layout, pixels, clique e scroll) e escreve o veredito no log.
public func mosHostingSelfTest() -> Int32 {
    var buf = [CChar](repeating: 0, count: 8192)
    let n = buf.withUnsafeMutableBufferPointer { p in
        mos_sui_selftest(p.baseAddress, Int32(p.count))
    }
    buf.withUnsafeBufferPointer { p in
        if let s = p.baseAddress { mos_log(s) }
    }
    mos_log(n == 0 ? "HOSTING PASS" : "HOSTING FAIL")
    return n
}
