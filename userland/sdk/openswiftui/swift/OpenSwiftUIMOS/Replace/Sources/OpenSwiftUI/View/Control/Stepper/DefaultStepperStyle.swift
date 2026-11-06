//
//  DefaultStepperStyle.swift
//  OpenSwiftUI
//
//  Audited for 6.5.4
//  Status: Complete

import OpenSwiftUICore

// MARK: - DefaultStepperStyle

struct DefaultStepperStyle: StepperStyle {
    func makeBody(configuration: Configuration) -> some View {
        #if canImport(Darwin)
        Stepper(configuration)
            .modifier(
                StepperStyleModifier(style: MenuStepperStyle())
                    .requiring(MenuStyleContext.self)
            )
            #if os(macOS)
            .modifier(StepperStyleModifier(style: ArrowStepperStyle()))
            #else
            .modifier(StepperStyleModifier(style: SegmentedButtonStepperStyle()))
            #endif
        #else
        // mOS: rótulo e um par de botões − / + sem borda sobre uma cápsula (sem NSStepper)
        HStack(spacing: 8) {
            configuration.label
            HStack(spacing: 0) {
                Button {
                    Self.step(configuration.onDecrement, configuration.onEditingChanged)
                } label: {
                    Text(verbatim: "\u{2212}").frame(width: 22, height: 20)
                }
                .disabled(configuration.onDecrement == nil)
                Button {
                    Self.step(configuration.onIncrement, configuration.onEditingChanged)
                } label: {
                    Text(verbatim: "+").frame(width: 22, height: 20)
                }
                .disabled(configuration.onIncrement == nil)
            }
            .buttonStyle(.borderless)
            .background(Capsule().fill(Color.gray.opacity(0.18)))
        }
        #endif
    }

    #if !canImport(Darwin)
    private static func step(_ action: (() -> Void)?, _ onEditingChanged: (Bool) -> Void) {
        guard let action else { return }
        onEditingChanged(true)
        action()
        onEditingChanged(false)
    }
    #endif
}
