// Stub do Path.Element do OpenSwiftUICore, para testar MOSPathGeometry.swift fora do módulo
// (tools/test-openswiftui-path.ps1 compila os dois juntos).
import Foundation
import OpenCoreGraphics

package struct Path {
    package enum Element: Equatable {
        case move(to: CGPoint)
        case line(to: CGPoint)
        case quadCurve(to: CGPoint, control: CGPoint)
        case curve(to: CGPoint, control1: CGPoint, control2: CGPoint)
        case closeSubpath
    }
}
