//
//  PlatformSliderStyle.swift
//  OpenSwiftUI
//
//  Audited for 6.5.4
//  Status: Complete
//  ID: D73865907660DC2CCEED4B77967EA955 (SwiftUI)

import OpenSwiftUICore

// MARK: - PlatformSliderCoordinator

protocol PlatformSliderCoordinator: PlatformViewCoordinator {}

// MARK: - PlatformSliderStyle

#if !canImport(Darwin)
/// mOS: controle deslizante desenhado com formas do OpenSwiftUI (trilho em cápsula, parte preenchida na
/// cor de acento, botão branco) e arrastado com DragGesture; `value` é a fração 0...1 da configuração.
struct PlatformSliderStyle: SliderStyle {
    func makeBody(configuration: Configuration) -> some View {
        MOSSlider(value: configuration.$value, onEditingChanged: configuration.onEditingChanged)
    }
}

private struct MOSSlider: View {
    @Binding var value: Double
    var onEditingChanged: (Bool) -> Void

    @State private var isEditing = false

    @Environment(\.isEnabled)
    private var isEnabled: Bool

    @Environment(\.tintColor)
    private var tint: Color?

    private let thumb: CGFloat = 18

    var body: some View {
        GeometryReader { proxy in
            let track = max(proxy.size.width - thumb, 1)
            let x = CGFloat(min(max(value, 0), 1)) * track
            let accent = tint ?? .accentColor
            ZStack(alignment: .leading) {
                Capsule()
                    .fill(Color.gray.opacity(0.3))
                    .frame(width: track, height: 4)
                    .offset(x: thumb / 2)
                Capsule()
                    .fill(isEnabled ? accent : accent.opacity(0.4))
                    .frame(width: x, height: 4)
                    .offset(x: thumb / 2)
                ZStack {
                    Circle().fill(Color.white)
                    Circle().strokeBorder(Color.gray.opacity(0.45), lineWidth: 0.5)
                }
                .frame(width: thumb, height: thumb)
                .offset(x: x)
            }
            .frame(width: proxy.size.width, height: proxy.size.height, alignment: .leading)
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { drag in
                        guard isEnabled else { return }
                        if !isEditing {
                            isEditing = true
                            onEditingChanged(true)
                        }
                        value = Double(min(max((drag.location.x - thumb / 2) / track, 0), 1))
                    }
                    .onEnded { _ in
                        guard isEditing else { return }
                        isEditing = false
                        onEditingChanged(false)
                    }
            )
        }
        .frame(height: 22)
    }
}
#endif

// MARK: - PlatformSliderStyle + SymbolEffectState

extension PlatformSliderStyle {
    struct SymbolEffectState {
        var minimumValueLabelSeed: UInt32 = 0

        var maximumValueLabelSeed: UInt32 = 0
    }
}

// MARK: - PlatformSliderStyle + ValueLabel

extension PlatformSliderStyle {
    struct ValueLabel<Base>: View where Base: View {
        let base: Base

        var body: some View {
            #if os(macOS)
            base
                .imageScale(.small)
                .symbolVariant(.fill)
                .font(.footnote)
            #else
            base
            #endif
        }
    }

    #if os(macOS)
    // MARK: - PlatformSliderStyle + InteractiveValueLabel

    struct InteractiveValueLabel<Base>: View where Base: View {
        let base: Base

        let configuration: SliderStyleConfiguration

        var body: some View {
            base.map { element in
                Button {
                    configuration.onEditingChanged(true)
                    action()
                    configuration.onEditingChanged(false)
                } label: {
                    ValueLabel(base: element.content)
                }
                .buttonStyle(.borderless)
                .buttonRepeatBehavior(.enabled)
                .environment(\.buttonRepeatTiming, .macOSStepper)
                .accessibilityIgnoreViewResponders()
                .accessibilityAddTraits(.isLabel)
            }
        }

        private func action() {
            if base is SliderStyleConfiguration.MinimumValueLabel {
                configuration.value = max(configuration.value - configuration.skipDistance, 0)
            } else if base is SliderStyleConfiguration.MaximumValueLabel {
                configuration.value = min(configuration.value + configuration.skipDistance, 1)
            }
        }
    }
    #endif
}
