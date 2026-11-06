// mOS — entrada de eventos do OpenSwiftUI no mOS (entra no módulo OpenSwiftUI).
//
// Caminho: canvas do mOS (_CXSwiftCanvas, AppKit do mOS) → mos_canvas_events (userland/libsystem/
// swiftbridge_input.c) → MOSEventRouter.handle → MouseEvent/KeyEvent → EventBindingManager → hit testing
// pelos ViewResponders → grafo de gestos (TapGesture, ButtonGesture, DragGesture...).
//
// O host do mOS (StdoutRendererHost) não é view de plataforma; MOSEventHost faz o papel de EventGraphHost
// dele e é exposto pelo `as(_:)` do host (Replace/.../StdoutRendererHost.swift, mosInterfaceProvider), que é
// por onde ViewResponder, LayoutGesture e EventBindingManager.current o encontram.
//
// Coordenadas: o canvas ocupa a área de conteúdo da janela com origem no topo à esquerda, e o host não tem
// RootTransformProvider (transformação da raiz = identidade); a posição no canvas é a posição global.
#if !OPENSWIFTUI_SWIFTUI_RENDERER
import Foundation
@_spi(ForOpenSwiftUIOnly)
import OpenSwiftUICore
import MOSBridge

// MARK: - MOSEventHost

/// EventGraphHost do host do mOS: delega o grafo de gestos ao ViewGraph do host.
final class MOSEventHost: EventGraphHost, EventBindingManagerDelegate {
    let eventBindingManager = EventBindingManager()
    private weak var host: (any ViewRendererHost)?
    /// Chamado quando o resultado de um gesto pede um redesenho.
    var needsDisplay: () -> Void = {}

    init(host: any ViewRendererHost) {
        self.host = host
        eventBindingManager.host = self
        eventBindingManager.delegate = self
        // teclas para onKeyPress (despachadas ao respondedor focado, como no NSHostingView)
        eventBindingManager.addForwardedEventDispatcher(KeyEventDispatcher())
    }

    var responderNode: ResponderNode? { host?.responderNode }

    var focusedResponder: ResponderNode? { MOSFocusManager.shared.focusedResponder }

    var nextGestureUpdateTime: Time { host?.nextGestureUpdateTime ?? .infinity }

    func setInheritedPhase(_ phase: _GestureInputs.InheritedPhase) {
        host?.setInheritedPhase(phase)
    }

    func sendEvents(_ events: [EventID: any EventType], rootNode: ResponderNode, at time: Time) -> GesturePhase<Void> {
        guard let host else { return .failed }
        return host.sendEvents(events, rootNode: rootNode, at: time)
    }

    func resetEvents() { host?.resetEvents() }

    func gestureCategory() -> GestureCategory? { host?.gestureCategory() }

    // EventBindingManagerDelegate
    func didUpdate(phase: GesturePhase<Void>, in eventBindingManager: EventBindingManager) {
        needsDisplay()
    }
}

// MARK: - MOSEventRouter

/// Converte os eventos do canvas em eventos do OpenSwiftUI e os envia ao EventBindingManager.
final class MOSEventRouter {
    let eventHost: MOSEventHost
    /// Pede um redesenho do canvas depois de cada evento (o estado pode ter mudado).
    var needsDisplay: () -> Void = {}

    /// Sequência do clique atual por botão (MouseEvent com a mesma série = mesmo gesto).
    private var mouseSerial: [Int: Int] = [:]
    private var nextSerial = 1000
    private var keyTracker = KeyEvent.Tracker()

    init(eventHost: MOSEventHost) {
        self.eventHost = eventHost
    }

    private var manager: EventBindingManager { eventHost.eventBindingManager }

    private func newSerial() -> Int {
        nextSerial &+= 1
        return nextSerial
    }

    /// Ponto de entrada (chamado pelo handler C registrado em mos_canvas_events).
    func handle(_ ev: mos_input_event_t) {
        let location = CGPoint(x: ev.x, y: ev.y)
        let modifiers = MOSEventRouter.modifiers(UInt(ev.modifiers))
        switch Int(ev.kind) {   // enum anônimo do C chega ao Swift como constantes Int
        case MOS_INPUT_MOUSE_DOWN:
            MOSFocusManager.shared.mouseDown()
            sendMouse(button: .primary, key: 0, phase: .began, at: location, modifiers: modifiers)
        case MOS_INPUT_MOUSE_DRAGGED:
            sendMouse(button: .primary, key: 0, phase: .active, at: location, modifiers: modifiers)
        case MOS_INPUT_MOUSE_UP:
            sendMouse(button: .primary, key: 0, phase: .ended, at: location, modifiers: modifiers)
            MOSFocusManager.shared.mouseUp()
        case MOS_INPUT_RIGHT_MOUSE_DOWN:
            // o AppKit do mOS não entrega rightMouseUp: o clique secundário é um toque completo
            sendMouse(button: .secondary, key: 1, phase: .began, at: location, modifiers: modifiers)
            sendMouse(button: .secondary, key: 1, phase: .ended, at: location, modifiers: modifiers)
        case MOS_INPUT_SCROLL:
            MOSScrollInput.dispatch(MOSScrollInput.Event(
                location: location,
                delta: CGSize(width: ev.delta_x, height: ev.delta_y),
                modifiers: modifiers
            ))
        case MOS_INPUT_KEY_DOWN:
            let chars = ev.characters.map { String(cString: $0) } ?? ""
            handleKey(MOSKeyInput(keyCode: Int(ev.key_code), characters: chars, modifiers: modifiers))
        default:
            break
        }
        needsDisplay()
    }

    private func sendMouse(button: MouseEvent.Button, key: Int, phase: EventPhase, at location: CGPoint, modifiers: EventModifiers) {
        let serial: Int
        if phase == .began {
            // um clique novo enquanto outro está aberto encerra o anterior como falho
            if let open = mouseSerial[key] {
                send(MouseEvent(timestamp: .systemUptime, button: button, phase: .failed,
                                location: .zero, globalLocation: location, modifiers: modifiers), serial: open)
            }
            serial = newSerial()
            mouseSerial[key] = serial
        } else {
            guard let open = mouseSerial[key] else { return }   // arrasto/soltura sem clique nosso
            serial = open
            if phase.isTerminal { mouseSerial[key] = nil }
        }
        send(MouseEvent(timestamp: .systemUptime, button: button, phase: phase,
                        location: .zero, globalLocation: location, modifiers: modifiers), serial: serial)
    }

    @discardableResult
    private func send<E: EventType>(_ event: E, serial: Int) -> Bool {
        let id = EventID(type: E.self, serial: serial)
        return manager.send([id: event]).contains(id)
    }

    private func handleKey(_ key: MOSKeyInput) {
        // 1) controle focado do mOS (TextField, navegação por Tab)
        if MOSFocusManager.shared.handleKey(key) { return }
        // 2) onKeyPress e afins: KeyEvent de descida e subida (o AppKit do mOS não entrega keyUp)
        for phase in [EventPhase.began, .ended] {
            let event = KeyEvent(
                phase: phase,
                timestamp: .systemUptime,
                modifiers: key.modifiers,
                keys: key.characters,
                stringValue: key.characters,
                keyID: AnyHashable(key.keyCode)
            )
            send(event, serial: keyTracker.serial(for: event))
        }
    }

    /// Bits do NSEventModifierFlags do AppKit do mOS → EventModifiers.
    static func modifiers(_ flags: UInt) -> EventModifiers {
        var m: EventModifiers = []
        if flags & UInt(MOS_INPUT_MOD_CAPS) != 0 { m.insert(.capsLock) }
        if flags & UInt(MOS_INPUT_MOD_SHIFT) != 0 { m.insert(.shift) }
        if flags & UInt(MOS_INPUT_MOD_CONTROL) != 0 { m.insert(.control) }
        if flags & UInt(MOS_INPUT_MOD_OPTION) != 0 { m.insert(.option) }
        if flags & UInt(MOS_INPUT_MOD_COMMAND) != 0 { m.insert(.command) }
        return m
    }
}

// MARK: - Rolagem

/// O OpenSwiftUI não tem evento de rolagem genérico no caminho não-Darwin (no AppKit/UIKit a rolagem vai
/// para a NSScrollView/UIScrollView da plataforma). Quem rola (ScrollView do mOS, quando existir) assina aqui.
enum MOSScrollInput {
    struct Event {
        var location: CGPoint
        var delta: CGSize
        var modifiers: EventModifiers
    }

    nonisolated(unsafe) static var handlers: [(Event) -> Bool] = []

    static func dispatch(_ event: Event) {
        for handler in handlers.reversed() where handler(event) { return }
    }
}

// MARK: - ligação com o canvas

/// Liga a entrada do canvas ao roteador (o roteador precisa continuar vivo: guarde-o).
func mosAttachEvents(canvas: UnsafeMutableRawPointer, router: MOSEventRouter) {
    let ctx = Unmanaged.passUnretained(router).toOpaque()
    mos_canvas_events(canvas, { ctx, ev in
        guard let ctx, let ev else { return }
        let router = Unmanaged<MOSEventRouter>.fromOpaque(ctx).takeUnretainedValue()
        router.handle(ev.pointee)
    }, ctx)
}
#endif
