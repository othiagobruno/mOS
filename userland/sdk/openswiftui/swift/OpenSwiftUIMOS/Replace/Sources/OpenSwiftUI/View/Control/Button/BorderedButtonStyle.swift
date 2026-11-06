//
//  BorderedButtonStyle.swift
//  OpenSwiftUI
//
//  Audited for 6.5.4
//  Status: Complete

public import OpenSwiftUICore

// MARK: - PrimitiveButtonStyle + BorderedButtonStyle

@available(OpenSwiftUI_v3_0, *)
extension PrimitiveButtonStyle where Self == BorderedButtonStyle {
    /// A button style that applies standard border artwork based on the
    /// button's context.
    ///
    /// To apply this style to a button, or to a view that contains buttons, use
    /// the ``View/buttonStyle(_:)`` modifier.
    @_alwaysEmitIntoClient
    @MainActor
    @preconcurrency
    public static var bordered: BorderedButtonStyle {
        .init()
    }
}

// MARK: - BorderedButtonStyle

/// A button style that applies standard border artwork based on the button's
/// context.
///
/// You can also use ``PrimitiveButtonStyle/bordered`` to construct this style.
@available(OpenSwiftUI_v3_0, *)
public struct BorderedButtonStyle: PrimitiveButtonStyle {
    let tint: Color?
    let isProminent: Bool

    /// Creates a bordered button style.
    nonisolated public init() {
        tint = nil
        isProminent = false
    }

    /// Creates a bordered button style with a tint color.
    @available(watchOS, deprecated: 100000.0, message: "Use ``View/tint(_)`` instead.")
    @available(iOS, unavailable)
    @available(macOS, unavailable)
    @available(tvOS, unavailable)
    @available(visionOS, unavailable)
    nonisolated public init(tint: Color) {
        self.tint = tint
        self.isProminent = false
    }

    public func makeBody(configuration: Configuration) -> some View {
        #if canImport(Darwin)
        Button(configuration)
            #if os(iOS) || os(visionOS)
            .modifier(StaticIf(
                idiom: .clarityUI,
                then: ButtonStyleContainerModifier(style: PlatterButtonStyle(isProminent: isProminent)),
                else: EmptyModifier()
            ))
            #endif
            .modifier(StaticIf(
                idiom: .widget,
                then: ButtonStyleContainerModifier(style: WidgetBorderedButtonStyle()),
                else: EmptyModifier()
            ))
            #if os(macOS)
            .buttonStyle(BorderedButtonStyle_CarMac(tint: tint, isProminent: isProminent))
            #else
            .modifier(StaticIf(
                idiom: .carPlay,
                then: PrimitiveButtonStyleContainerModifier(
                    style: BorderedButtonStyle_CarMac(tint: tint, isProminent: isProminent)
                ),
                else: EmptyModifier()
            ))
            .buttonStyle(BorderedButtonStyle_Phone(tint: tint, isProminent: isProminent))
            #endif
            .input(ButtonContainerIsBorderedInput.self)
        #else
        // mOS: sem botão de plataforma; a borda é desenhada com formas do próprio OpenSwiftUI.
        Button(configuration)
            .buttonStyle(MOSBorderedButtonStyleBase(tint: tint, isProminent: isProminent))
            .input(ButtonContainerIsBorderedInput.self)
        #endif
    }
}

@available(*, unavailable)
extension BorderedButtonStyle: Sendable {}

#if !canImport(Darwin)
// MARK: - MOSBorderedButtonStyleBase

/// mOS: botão com borda no visual do macOS Tahoe, só com formas: cápsula translúcida (ou cor de acento
/// quando proeminente), texto em cor de rótulo, papel destrutivo em vermelho, mais escuro ao pressionar e
/// esmaecido quando desabilitado. Usado por `.bordered`, `.borderedProminent` e pelo estilo padrão.
struct MOSBorderedButtonStyleBase: ButtonStyle {
    var tint: Color?
    var isProminent: Bool

    @Environment(\.isEnabled)
    private var isEnabled: Bool

    @Environment(\.tintColor)
    private var controlTint: Color?

    func makeBody(configuration: Configuration) -> some View {
        let accent = tint ?? controlTint ?? .accentColor
        let isDestructive = configuration.role == .destructive
        let fill: Color = if isProminent {
            isDestructive ? .red : accent
        } else {
            Color.gray.opacity(configuration.isPressed ? 0.32 : 0.18)
        }
        let foreground: Color = if isProminent {
            .white
        } else {
            isDestructive ? .red : .primary
        }
        return configuration.label
            .foregroundColor(isEnabled ? foreground : foreground.opacity(0.4))
            .padding(.horizontal, 12)
            .padding(.vertical, 4)
            .frame(minHeight: 22)
            .background(Capsule().fill(isEnabled ? fill : fill.opacity(0.5)))
            .opacity(isProminent && configuration.isPressed ? 0.75 : 1)
            .contentShape(Capsule())
    }
}
#endif
