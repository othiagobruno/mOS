// mOS — NSViewRepresentable e NSViewControllerRepresentable do SwiftUI.framework (variante com interop ObjC).
// Protocolos, contextos e implementações padrão com os nomes e assinaturas da Apple (os mesmos do upstream
// OpenSwiftUI/Integration/Representable/AppKit, que dependem do PlatformViewHost do hosting da Apple). O hosting do
// mOS é o StdoutRendererHost de NSHosting.swift, então a ponte é própria:
//  - `_makeView` leva o grafo para `_MOSRepresentableView`, que guarda a NSView (ou o NSViewController) num @State;
//  - o tamanho sai de um Layout que pergunta à representable (`sizeThatFits`), depois ao `intrinsicContentSize`
//    e por fim usa a proposta, como na Apple; `_overrideSizeThatFits` ajusta o resultado;
//  - um GeometryReader recebe o frame final e a NSView vira subview do NSHostingView nesse frame;
//  - quando a view sai da árvore o @State é solto: `dismantle...` e `removeFromSuperview`.
public import AppKit
public import Foundation
public import OpenSwiftUICore

// MARK: - NSViewRepresentable

@available(macOS 10.15, *)
@preconcurrency
@MainActor
public protocol NSViewRepresentable: View where Body == Never {
    associatedtype NSViewType: NSView
    func makeNSView(context: Context) -> NSViewType
    func updateNSView(_ nsView: NSViewType, context: Context)
    @available(macOS 12.0, *)
    func _resetNSView(_ nsView: NSViewType, coordinator: Coordinator, destroy: () -> Void)
    static func dismantleNSView(_ nsView: NSViewType, coordinator: Coordinator)
    associatedtype Coordinator = Void
    func makeCoordinator() -> Coordinator
    func _identifiedViewTree(in nsView: NSViewType) -> _IdentifiedViewTree
    @available(macOS 13.0, *)
    func sizeThatFits(_ proposal: ProposedViewSize, nsView: NSViewType, context: Context) -> CGSize?
    func _overrideSizeThatFits(_ size: inout CGSize, in proposedSize: _ProposedSize, nsView: NSViewType)
    func _overrideLayoutTraits(_ layoutTraits: inout _LayoutTraits, for nsView: NSViewType)
    @available(macOS 11.3, *)
    static func _modifyBridgedViewInputs(_ inputs: inout _ViewInputs)
    @available(macOS 13.0, *)
    static var _invalidatesSizeOnConstraintChanges: Bool { get }
    @available(macOS 13.0, *)
    static func _layoutOptions(_ provider: NSViewType) -> LayoutOptions
    typealias Context = NSViewRepresentableContext<Self>
    @available(macOS 13.0, *)
    typealias LayoutOptions = _PlatformViewRepresentableLayoutOptions
}

@available(macOS 10.15, *)
extension NSViewRepresentable where Coordinator == () {
    public func makeCoordinator() -> Coordinator { () }
}

@available(macOS 10.15, *)
extension NSViewRepresentable {
    @available(macOS 12.0, *)
    public func _resetNSView(_ nsView: NSViewType, coordinator: Coordinator, destroy: () -> Void) { destroy() }
    public static func dismantleNSView(_ nsView: NSViewType, coordinator: Coordinator) {}

    nonisolated public static func _makeView(view: _GraphValue<Self>, inputs: _ViewInputs) -> _ViewOutputs {
        _MOSRepresentableView<_MOSViewBridge<Self>>._makeView(view: view[\._mosBridged], inputs: inputs)
    }
    nonisolated public static func _makeViewList(view: _GraphValue<Self>, inputs: _ViewListInputs) -> _ViewListOutputs {
        _MOSRepresentableView<_MOSViewBridge<Self>>._makeViewList(view: view[\._mosBridged], inputs: inputs)
    }
    public func _identifiedViewTree(in nsView: NSViewType) -> _IdentifiedViewTree { .empty }
    @available(macOS 13.0, *)
    public func sizeThatFits(_ proposal: ProposedViewSize, nsView: NSViewType, context: Context) -> CGSize? { nil }
    public func _overrideSizeThatFits(_ size: inout CGSize, in proposedSize: _ProposedSize, nsView: NSViewType) {}
    public func _overrideLayoutTraits(_ layoutTraits: inout _LayoutTraits, for nsView: NSViewType) {}
    @available(macOS 11.3, *)
    public static func _modifyBridgedViewInputs(_ inputs: inout _ViewInputs) {}
    @available(macOS 13.0, *)
    public static var _invalidatesSizeOnConstraintChanges: Bool { true }
    @available(macOS 13.0, *)
    public static func _layoutOptions(_ provider: NSViewType) -> LayoutOptions { .init(rawValue: 1) }
    public var body: Never { fatalError("body de uma NSViewRepresentable não existe: \(Self.self)") }

    nonisolated var _mosBridged: _MOSRepresentableView<_MOSViewBridge<Self>> { .init(bridge: _MOSViewBridge(base: self)) }
}

@available(macOS 10.15, *)
@preconcurrency
@MainActor
public struct NSViewRepresentableContext<View> where View: NSViewRepresentable {
    var values: RepresentableContextValues
    public let coordinator: View.Coordinator
    public var transaction: Transaction { values.transaction }
    public var environment: EnvironmentValues { values.environment }

    init(coordinator: View.Coordinator, transaction: Transaction, environment: EnvironmentValues) {
        values = RepresentableContextValues(preferenceBridge: nil, transaction: transaction, environmentStorage: .eager(environment))
        self.coordinator = coordinator
    }
}

@available(macOS 15.0, *)
extension NSViewRepresentableContext {
    /// Sem animação na transação: aplica e completa na hora (o mOS anima a árvore SwiftUI, não a NSView).
    public func animate(changes: () -> Void, completion: (() -> Void)? = nil) {
        changes()
        completion?()
    }
}

// MARK: - NSViewControllerRepresentable

@available(macOS 10.15, *)
@preconcurrency
@MainActor
public protocol NSViewControllerRepresentable: View where Body == Never {
    associatedtype NSViewControllerType: NSViewController
    func makeNSViewController(context: Context) -> NSViewControllerType
    func updateNSViewController(_ nsViewController: NSViewControllerType, context: Context)
    @available(macOS 12.0, *)
    func _resetNSViewController(_ nsViewController: NSViewControllerType, coordinator: Coordinator, destroy: () -> Void)
    static func dismantleNSViewController(_ nsViewController: NSViewControllerType, coordinator: Coordinator)
    associatedtype Coordinator = Void
    func makeCoordinator() -> Coordinator
    func _identifiedViewTree(in nsViewController: NSViewControllerType) -> _IdentifiedViewTree
    @available(macOS 13.0, *)
    func sizeThatFits(_ proposal: ProposedViewSize, nsViewController: NSViewControllerType, context: Context) -> CGSize?
    @available(macOS 13.0, *)
    static var _invalidatesSizeOnConstraintChanges: Bool { get }
    @available(macOS 13.0, *)
    static func _layoutOptions(_ provider: NSViewControllerType) -> LayoutOptions
    typealias Context = NSViewControllerRepresentableContext<Self>
    @available(macOS 13.0, *)
    typealias LayoutOptions = _PlatformViewRepresentableLayoutOptions
}

@available(macOS 10.15, *)
extension NSViewControllerRepresentable where Coordinator == () {
    public func makeCoordinator() -> Coordinator { () }
}

@available(macOS 10.15, *)
extension NSViewControllerRepresentable {
    @available(macOS 12.0, *)
    public func _resetNSViewController(_ nsViewController: NSViewControllerType, coordinator: Coordinator, destroy: () -> Void) { destroy() }
    public static func dismantleNSViewController(_ nsViewController: NSViewControllerType, coordinator: Coordinator) {}

    nonisolated public static func _makeView(view: _GraphValue<Self>, inputs: _ViewInputs) -> _ViewOutputs {
        _MOSRepresentableView<_MOSViewControllerBridge<Self>>._makeView(view: view[\._mosBridged], inputs: inputs)
    }
    nonisolated public static func _makeViewList(view: _GraphValue<Self>, inputs: _ViewListInputs) -> _ViewListOutputs {
        _MOSRepresentableView<_MOSViewControllerBridge<Self>>._makeViewList(view: view[\._mosBridged], inputs: inputs)
    }
    public func _identifiedViewTree(in nsViewController: NSViewControllerType) -> _IdentifiedViewTree { .empty }
    @available(macOS 13.0, *)
    public func sizeThatFits(_ proposal: ProposedViewSize, nsViewController: NSViewControllerType, context: Context) -> CGSize? { nil }
    @available(macOS 13.0, *)
    public static var _invalidatesSizeOnConstraintChanges: Bool { true }
    @available(macOS 13.0, *)
    public static func _layoutOptions(_ provider: NSViewControllerType) -> LayoutOptions { .init(rawValue: 1) }
    public var body: Never { fatalError("body de uma NSViewControllerRepresentable não existe: \(Self.self)") }

    nonisolated var _mosBridged: _MOSRepresentableView<_MOSViewControllerBridge<Self>> { .init(bridge: _MOSViewControllerBridge(base: self)) }
}

@available(macOS 10.15, *)
@preconcurrency
@MainActor
public struct NSViewControllerRepresentableContext<ViewController> where ViewController: NSViewControllerRepresentable {
    var values: RepresentableContextValues
    public let coordinator: ViewController.Coordinator
    public var transaction: Transaction { values.transaction }
    public var environment: EnvironmentValues { values.environment }

    init(coordinator: ViewController.Coordinator, transaction: Transaction, environment: EnvironmentValues) {
        values = RepresentableContextValues(preferenceBridge: nil, transaction: transaction, environmentStorage: .eager(environment))
        self.coordinator = coordinator
    }
}

@available(macOS 15.0, *)
extension NSViewControllerRepresentableContext {
    public func animate(changes: () -> Void, completion: (() -> Void)? = nil) {
        changes()
        completion?()
    }
}

// MARK: - ponte comum

/// O que a ponte precisa de uma representable, seja de NSView ou de NSViewController.
protocol _MOSBridge {
    associatedtype Provider: AnyObject
    associatedtype Coordinator
    func makeCoordinator() -> Coordinator
    func make(_ coordinator: Coordinator, _ transaction: Transaction, _ environment: EnvironmentValues) -> Provider
    func update(_ provider: Provider, _ coordinator: Coordinator, _ transaction: Transaction, _ environment: EnvironmentValues)
    static func dismantle(_ provider: Provider, _ coordinator: Coordinator)
    func view(of provider: Provider) -> NSView
    func sizeThatFits(_ proposal: ProposedViewSize, _ provider: Provider, _ coordinator: Coordinator,
                      _ environment: EnvironmentValues) -> CGSize?
    func overrideSize(_ size: inout CGSize, _ proposal: _ProposedSize, _ provider: Provider)
}

struct _MOSViewBridge<R: NSViewRepresentable>: _MOSBridge {
    let base: R
    func makeCoordinator() -> R.Coordinator { base.makeCoordinator() }
    func make(_ c: R.Coordinator, _ t: Transaction, _ e: EnvironmentValues) -> R.NSViewType {
        base.makeNSView(context: .init(coordinator: c, transaction: t, environment: e))
    }
    func update(_ p: R.NSViewType, _ c: R.Coordinator, _ t: Transaction, _ e: EnvironmentValues) {
        base.updateNSView(p, context: .init(coordinator: c, transaction: t, environment: e))
    }
    static func dismantle(_ p: R.NSViewType, _ c: R.Coordinator) { R.dismantleNSView(p, coordinator: c) }
    func view(of p: R.NSViewType) -> NSView { p }
    func sizeThatFits(_ proposal: ProposedViewSize, _ p: R.NSViewType, _ c: R.Coordinator, _ e: EnvironmentValues) -> CGSize? {
        base.sizeThatFits(proposal, nsView: p, context: .init(coordinator: c, transaction: Transaction.current, environment: e))
    }
    func overrideSize(_ size: inout CGSize, _ proposal: _ProposedSize, _ p: R.NSViewType) {
        base._overrideSizeThatFits(&size, in: proposal, nsView: p)
    }
}

struct _MOSViewControllerBridge<R: NSViewControllerRepresentable>: _MOSBridge {
    let base: R
    func makeCoordinator() -> R.Coordinator { base.makeCoordinator() }
    func make(_ c: R.Coordinator, _ t: Transaction, _ e: EnvironmentValues) -> R.NSViewControllerType {
        base.makeNSViewController(context: .init(coordinator: c, transaction: t, environment: e))
    }
    func update(_ p: R.NSViewControllerType, _ c: R.Coordinator, _ t: Transaction, _ e: EnvironmentValues) {
        base.updateNSViewController(p, context: .init(coordinator: c, transaction: t, environment: e))
    }
    static func dismantle(_ p: R.NSViewControllerType, _ c: R.Coordinator) { R.dismantleNSViewController(p, coordinator: c) }
    func view(of p: R.NSViewControllerType) -> NSView { p.view }
    func sizeThatFits(_ proposal: ProposedViewSize, _ p: R.NSViewControllerType, _ c: R.Coordinator, _ e: EnvironmentValues) -> CGSize? {
        base.sizeThatFits(proposal, nsViewController: p, context: .init(coordinator: c, transaction: Transaction.current, environment: e))
    }
    func overrideSize(_ size: inout CGSize, _ proposal: _ProposedSize, _ p: R.NSViewControllerType) {}
}

/// Estado de uma representable na árvore: a NSView (ou controller), o coordinator e onde ela está.
final class _MOSRepresentableBox<B: _MOSBridge> {
    var provider: B.Provider?
    var coordinator: B.Coordinator?
    var bridge: B?
    var environment = EnvironmentValues()
    weak var host: NSView?

    /// cria na primeira vez, atualiza nas outras (é o corpo da view que chama, a cada mudança de entrada)
    func update(_ b: B, _ env: EnvironmentValues) {
        bridge = b
        environment = env
        let t = Transaction.current
        if let p = provider, let c = coordinator {
            b.update(p, c, t, env)
        } else {
            let c = b.makeCoordinator()
            coordinator = c
            let p = b.make(c, t, env)
            provider = p
            b.update(p, c, t, env)
        }
    }

    /// tamanho para uma proposta: a representable, depois o tamanho intrínseco, depois a proposta
    func size(for proposal: ProposedViewSize) -> CGSize {
        guard let b = bridge, let p = provider, let c = coordinator else {
            return CGSize(width: proposal.width ?? 10, height: proposal.height ?? 10)
        }
        if let s = b.sizeThatFits(proposal, p, c, environment) { return s }
        let intrinsic = b.view(of: p).intrinsicContentSize
        var s = CGSize(width: intrinsic.width >= 0 ? intrinsic.width : (proposal.width ?? 10),
                       height: intrinsic.height >= 0 ? intrinsic.height : (proposal.height ?? 10))
        b.overrideSize(&s, _ProposedSize(width: proposal.width, height: proposal.height), p)
        return s
    }

    /// põe a NSView no NSHostingView que está renderizando, no frame dado (coordenadas do hosting, viradas)
    func place(_ frame: CGRect) {
        guard let b = bridge, let p = provider, let hostView = _MOSHostingState.renderingView else { return }
        let v = b.view(of: p)
        if host !== hostView || v.superview !== hostView {
            v.removeFromSuperview()
            hostView.addSubview(v)
            host = hostView
        }
        if v.frame != frame { v.frame = frame }
    }

    deinit {
        guard let b = bridge, let p = provider, let c = coordinator else { return }
        let v = b.view(of: p)
        B.dismantle(p, c)
        v.removeFromSuperview()
    }
}

/// O tamanho de uma representable no layout do SwiftUI.
struct _MOSRepresentableLayout<B: _MOSBridge>: Layout {
    let box: _MOSRepresentableBox<B>
    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        box.size(for: proposal)
    }
    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        for s in subviews { s.place(at: bounds.origin, anchor: .topLeading, proposal: ProposedViewSize(bounds.size)) }
    }
}

struct _MOSRepresentableView<B: _MOSBridge>: View {
    let bridge: B
    @State private var box = _MOSRepresentableBox<B>()
    @Environment(\.self) private var environment

    var body: some View {
        let _ = box.update(bridge, environment)
        _MOSRepresentableLayout(box: box) {
            GeometryReader { g in
                let _ = box.place(g.frame(in: .global))
                Color.clear
            }
        }
    }
}
