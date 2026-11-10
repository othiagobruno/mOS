// mOS — NSHostingView e NSHostingController do SwiftUI.framework (variante com interop ObjC, docs/swift-objc-runtime.md,
// fase 6). Subclasses Swift de verdade do NSView/NSViewController do AppKit do mOS (userland/libsystem/appkit.c), com os
// nomes mangled da Apple ($s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfc...). Por dentro reaproveitam o host da variante
// nativa (OpenSwiftUIMOS/App/MOSApp.swift): o grafo de views roda num StdoutRendererHost, a DisplayList chega por
// _mosDisplayListSink e é desenhada por MOSDisplayListDrawing num canvas do libSystem (mos_canvas, MOSBridge), que fica
// como subview do NSHostingView; a entrada do canvas segue para o grafo por MOSEvents.swift. Um timer (~60 Hz) só para
// todos os hosts vivos.
#if !OPENSWIFTUI_SWIFTUI_RENDERER
public import AppKit
import Foundation
@_spi(ForOpenSwiftUIOnly)
@_spi(StdoutRenderer)
import OpenSwiftUICore
import MOSBridge

/// O que não depende do tipo do conteúdo: canvas, host, última DisplayList, eventos.
final class _MOSHostingState {
    var canvas: UnsafeMutableRawPointer?
    var host: AnyObject?
    var render: (() -> Void)?
    var lastList: DisplayList?
    var size = CGSize(width: 0, height: 0)
    var eventHost: MOSEventHost?
    var eventRouter: MOSEventRouter?
    /// cria o host para um tamanho (o conteúdo vai com .frame(width:height:), como no MOSApp)
    var makeHost: ((CGSize) -> Void)?

    nonisolated(unsafe) static var live: [ObjectIdentifier: _MOSHostingState] = [:]
    nonisolated(unsafe) static var timer: UnsafeMutableRawPointer?

    func attach(to view: AnyObject) {
        let me = Unmanaged.passUnretained(self).toOpaque()
        let cv = mos_canvas({ ctx, _, _ in
            guard let ctx else { return }
            let state = Unmanaged<_MOSHostingState>.fromOpaque(ctx).takeUnretainedValue()
            if let list = state.lastList { MOSDisplayListDrawing.draw(list) }
        }, me)!
        mos_add_subview(Unmanaged.passUnretained(view).toOpaque(), cv)
        canvas = cv
        _MOSHostingState.live[ObjectIdentifier(self)] = self
        if _MOSHostingState.timer == nil {
            _MOSHostingState.timer = mos_timer(0.016, { _ in
                for state in _MOSHostingState.live.values { state.renderNow() }
            }, nil)
        }
    }

    func detach() { _MOSHostingState.live[ObjectIdentifier(self)] = nil }

    static func renderAll() {
        for state in live.values { state.renderNow() }
    }

    func resize(_ s: CGSize) {
        guard s.width > 0, s.height > 0 else { return }
        if let canvas { mos_set_frame(canvas, 0, 0, Double(s.width), Double(s.height)) }
        if host == nil || abs(s.width - size.width) > 0.5 || abs(s.height - size.height) > 0.5 {
            size = s
            makeHost?(s)
        }
        renderNow()
    }

    /// renderiza este host: a DisplayList do StdoutRenderer vem pelo sink global, que aponta para este estado
    func renderNow() {
        guard let render else { return }
        _mosDisplayListSink = { [weak self] list, _ in
            guard let self else { return }
            self.lastList = list
            if let c = self.canvas { mos_set_needs_display(c) }
        }
        render()
    }

    func install<Content: View>(host: StdoutRendererHost<Content>) {
        self.host = host
        render = { [weak host] in host?.renderOnce() }
        let eventHost = MOSEventHost(host: host)
        let router = MOSEventRouter(eventHost: eventHost)
        let redraw: () -> Void = { [weak self] in if let c = self?.canvas { mos_set_needs_display(c) } }
        eventHost.needsDisplay = redraw
        router.needsDisplay = redraw
        host.mosRequestUpdate = { _ in redraw() }
        host.mosInterfaceProvider = { [weak eventHost] type in
            guard let eventHost else { return nil }
            if type == (any EventGraphHost).self { return eventHost as any EventGraphHost }
            return nil
        }
        self.eventHost = eventHost
        eventRouter = router
        if let canvas { mosAttachEvents(canvas: canvas, router: router) }
    }
}

/// Uma view do AppKit que mostra uma hierarquia de views do SwiftUI.
@available(macOS 10.15, *)
open class NSHostingView<Content>: NSView where Content: View {
    private let _mosState = _MOSHostingState()
    private var _rootView: Content

    /// A raiz da hierarquia de views do SwiftUI. Trocar a raiz recria o grafo.
    public var rootView: Content {
        get { _rootView }
        set {
            _rootView = newValue
            if _mosState.size.width > 0 { _mosState.makeHost?(_mosState.size); _mosState.renderNow() }
        }
    }

    public required init(rootView: Content) {
        _rootView = rootView
        super.init(frame: CGRect(x: 0, y: 0, width: 0, height: 0))
        _mosSetUp()
    }

    public required override init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    deinit { _mosState.detach() }

    private func _mosSetUp() {
        _mosState.makeHost = { [unowned self] size in
            Update.dispatchImmediately(reason: nil) {
                var options = _RendererConfiguration.StdoutOptions()
                options.surface = size
                let host = StdoutRendererHost(rootView: self._rootView.frame(width: size.width, height: size.height),
                                              environment: EnvironmentValues(), options: options)
                self._mosState.install(host: host)
            }
        }
        _mosState.attach(to: self)
    }

    open override var isFlipped: Bool { true }

    open override func setFrameSize(_ newSize: NSSize) {
        super.setFrameSize(newSize)
        _mosState.resize(newSize)
    }

    open override func layout() {
        super.layout()
        let s = frame.size
        if abs(s.width - _mosState.size.width) > 0.5 || abs(s.height - _mosState.size.height) > 0.5 {
            _mosState.resize(s)
        }
    }

    /// Mede a última DisplayList no tamanho proposto e devolve a união dos itens.
    func mosSizeThatFits(_ size: CGSize) -> CGSize {
        guard size.width > 0, size.height > 0 else { return size }
        _mosState.resize(size)
        guard let list = _mosState.lastList else { return size }
        let ext = MOSDisplayListMetrics.extent(list)
        if ext.width <= 1 || ext.height <= 1 { return size }
        return CGSize(width: min(size.width, ext.width), height: min(size.height, ext.height))
    }

    open override var intrinsicContentSize: NSSize { NSSize(width: -1, height: -1) }   // NSView.noIntrinsicMetric
}

/// Um view controller do AppKit que gerencia uma hierarquia de views do SwiftUI.
@available(macOS 10.15, *)
open class NSHostingController<Content>: NSViewController where Content: View {
    private let _mosHostingView: NSHostingView<Content>

    public init(rootView: Content) {
        _mosHostingView = NSHostingView(rootView: rootView)
        super.init(nibName: nil, bundle: nil)
        view = _mosHostingView
    }

    public init?(coder: NSCoder, rootView: Content) {
        _mosHostingView = NSHostingView(rootView: rootView)
        super.init(nibName: nil, bundle: nil)
        view = _mosHostingView
    }

    public required override init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    public var rootView: Content {
        get { _mosHostingView.rootView }
        set { _mosHostingView.rootView = newValue }
    }

    open override func loadView() { view = _mosHostingView }

    public func sizeThatFits(in size: CGSize) -> CGSize { _mosHostingView.mosSizeThatFits(size) }
}

#endif
