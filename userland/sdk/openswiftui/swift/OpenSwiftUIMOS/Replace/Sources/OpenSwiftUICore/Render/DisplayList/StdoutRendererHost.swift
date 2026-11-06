//
//  StdoutRendererHost.swift
//  OpenSwiftUICore
//
//  mOS: cópia do upstream com respondedores de view ligados, `mosInterfaceProvider` (EventGraphHost/FocusHost
//  por `as(_:)`) e `mosRequestUpdate`. Só isso muda; o resto é o original.

#if !OPENSWIFTUI_SWIFTUI_RENDERER
import Foundation

// MARK: - StdoutRendererHost

final package class StdoutRendererHost<Content>: ViewRendererHost, ViewGraphRenderDelegate where Content: View {
    typealias RootView = ModifiedContent<Content, HitTestBindingModifier>

    package let viewGraph: ViewGraph
    package let renderer: DisplayList.ViewRenderer
    package let rootView: Content
    package let environment: EnvironmentValues
    package let options: _RendererConfiguration.StdoutOptions

    package var currentTimestamp: Time = .zero
    package var propertiesNeedingUpdate: ViewRendererHostProperties = .all
    package var renderingPhase: ViewRenderingPhase = .none
    package var externalUpdateCount: Int = .zero

    /// mOS: interfaces extras que o host expõe por `as(_:)` (EventGraphHost para eventos, FocusHost para
    /// foco...). A camada do mOS (OpenSwiftUIMOS/App/MOSEvents.swift) registra aqui um adaptador; o valor
    /// devolvido precisa ser o existencial pedido (ex.: `adaptador as any EventGraphHost`).
    package var mosInterfaceProvider: ((Any.Type) -> Any?)?

    package init(
        rootView: Content,
        environment: EnvironmentValues,
        options: _RendererConfiguration.StdoutOptions
    ) {
        self.rootView = rootView
        self.environment = environment
        self.options = options
        Update.begin()
        // mOS: além de layout e DisplayList, pede os respondedores de view (hit testing e gestos).
        viewGraph = ViewGraph(rootViewType: RootView.self, requestedOutputs: [.displayList, .layout, .viewResponders])
        renderer = DisplayList.ViewRenderer(
            platform: .init(definition: StdoutPlatformViewDefinition.self)
        )
        renderer.configuration = .stdout(options)
        renderer.host = self
        initializeViewGraph()
        Update.end()
    }

    package func renderOnce() {
        render(interval: .zero, targetTimestamp: nil)
    }

    package func updateRootView() {
        viewGraph.setRootView(Self.makeRootView(rootView))
    }

    package func updateEnvironment() {
        viewGraph.setEnvironment(environment)
    }

    package func updateTransform() {
        viewGraph.invalidateTransform()
    }

    package func updateSize() {
        viewGraph.setProposedSize(options.surface)
    }

    package func updateSafeArea() {
        viewGraph.setSafeAreaInsets(.zero)
    }

    package func updateContainerSize() {
        viewGraph.setContainerSize(.fixed(options.surface))
    }

    package func updateFocusStore() {}

    package func updateFocusedItem() {}

    package func updateFocusedValues() {}

    package func updateAccessibilityEnvironment() {}

    package func `as`<T>(_ type: T.Type) -> T? {
        if ViewGraphRenderDelegate.self == T.self {
            return unsafeBitCast(self as any ViewGraphRenderDelegate, to: T.self)
        } else if DisplayList.ViewRenderer.self == T.self {
            return unsafeBitCast(renderer, to: T.self)
        } else if let provided = mosInterfaceProvider?(T.self) {
            return provided as? T
        } else {
            return nil
        }
    }

    /// mOS: chamado quando o grafo pede outra atualização (animações, gestos com tempo); a camada do mOS
    /// liga isto ao canvas para redesenhar.
    package var mosRequestUpdate: ((Double) -> Void)?

    package func requestUpdate(after delay: Double) { mosRequestUpdate?(delay) }

    package var renderingRootView: AnyObject {
        self
    }

    package func updateRenderContext(_ context: inout ViewGraphRenderContext) {
        context.contentsScale = 1.0
        context.opaqueBackground = false
    }

    package func withMainThreadRender(wasAsync: Bool, _ body: () -> Time) -> Time {
        body()
    }
}

private final class StdoutPlatformViewDefinition: PlatformViewDefinition, @unchecked Sendable {}
#endif
