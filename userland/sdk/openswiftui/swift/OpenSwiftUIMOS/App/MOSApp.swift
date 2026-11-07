// mOS — ciclo de vida do app OpenSwiftUI no mOS (entra no módulo OpenSwiftUI).
// Abre uma janela do AppKit do mOS com uma view de desenho, monta o grafo de views do OpenSwiftUI pelo
// StdoutRendererHost (caminho não-Darwin) e desenha o DisplayList a cada atualização (timer de ~60 Hz).
// Mouse, rolagem e teclado do canvas seguem para o OpenSwiftUI por MOSEvents.swift (mosInstallEvents).
#if !OPENSWIFTUI_SWIFTUI_RENDERER
import Foundation
@_spi(ForOpenSwiftUIOnly)
@_spi(StdoutRenderer)
import OpenSwiftUICore
import MOSBridge

final class _MOSAppState {
    var host: AnyObject? = nil
    var render: (() -> Void)? = nil
    var canvas: UnsafeMutableRawPointer? = nil
    var lastList: DisplayList? = nil
    var size = CGSize(width: 640, height: 480)
    /// entrada de eventos (MOSEvents.swift): adaptador EventGraphHost do host e roteador do canvas
    var eventHost: MOSEventHost? = nil
    var eventRouter: MOSEventRouter? = nil
    var start: ((CGSize) -> Void)? = nil
}

/// Liga eventos e foco ao host recém-criado (o canvas já existe).
private func mosInstallEvents<Content: View>(on host: StdoutRendererHost<Content>) {
    let eventHost = MOSEventHost(host: host)
    let router = MOSEventRouter(eventHost: eventHost)
    let redraw: () -> Void = { if let c = _mosApp.canvas { mos_set_needs_display(c) } }
    eventHost.needsDisplay = redraw
    router.needsDisplay = redraw
    host.mosRequestUpdate = { _ in redraw() }
    MOSFocusManager.shared.onChange = redraw
    // o host passa a se apresentar como EventGraphHost (ViewResponder, LayoutGesture, EventBindingManager.current)
    host.mosInterfaceProvider = { [weak eventHost] type in
        guard let eventHost else { return nil }
        if type == (any EventGraphHost).self { return eventHost as any EventGraphHost }
        return nil
    }
    _mosApp.eventHost = eventHost
    _mosApp.eventRouter = router
    if let canvas = _mosApp.canvas { mosAttachEvents(canvas: canvas, router: router) }
}
nonisolated(unsafe) let _mosApp = _MOSAppState()

func runMOSApp<Application>(_ app: Application) -> Never where Application: App {
    "runMOSApp".withCString { mos_log($0) }
    var title = "App"
    let start: (CGSize) -> Void = { size in
        "grafo".withCString { mos_log($0) }
        Update.dispatchImmediately(reason: nil) {
            "appgraph".withCString { mos_log($0) }
            let graph = AppGraph(app: app)
            "criou".withCString { mos_log($0) }
            graph.instantiate()
            "instanciou".withCString { mos_log($0) }
            AppGraph.shared = graph
            guard let item = graph.rootSceneList?.items.first else { return }
            var options = _RendererConfiguration.StdoutOptions()
            options.surface = size
            let host = StdoutRendererHost(rootView: item.value.view.frame(width: size.width, height: size.height),
                                          environment: item.environment, options: options)
            _mosApp.host = host
            _mosApp.render = { host.renderOnce() }
            mosInstallEvents(on: host)
        }
    }
    // o StdoutRenderer entrega o DisplayList aqui em vez de imprimir
    _mosDisplayListSink = { list, _ in
        _mosApp.lastList = list
        if let c = _mosApp.canvas { mos_set_needs_display(c) }
    }
    _mosApp.start = start
    title.withCString { t in
        "mos_run_app".withCString { mos_log($0) }
        mos_run_app(t, 640, 480, { _, content, w, h in
            if _mosApp.canvas == nil {
                "canvas".withCString { mos_log($0) }
                let cv = mos_canvas({ _, _, _ in if let l = _mosApp.lastList { MOSDisplayListDrawing.draw(l) } }, nil)!
                mos_add_subview(content, cv)
                _mosApp.canvas = cv
                _mosApp.start?(CGSize(width: w, height: h))
                _ = mos_timer(0.016, { _ in _mosApp.render?() }, nil)
            }
            mos_set_frame(_mosApp.canvas, 0, 0, w, h)
            _mosApp.render?()
        }, nil)
    }
    exit(0)
}
#endif
