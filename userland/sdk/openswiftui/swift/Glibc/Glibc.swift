// mOS SDK — Glibc: o OpenSwiftUI só conhece dois caminhos de libc, Darwin e Glibc. O módulo Darwin ligaria os caminhos da
// Apple (ObjC, frameworks privados), então o mOS se apresenta como "Glibc": este módulo reexporta a libm/stdio mínimas do
// SDK (MOSLibm) e o CGFloat, e traz as sobrecargas genéricas que o overlay Glibc do Swift oferece (tgmath).
// Só entra no -I do OpenSwiftUI (as deps não testam canImport(Glibc)).

@_exported import MOSLibm
@_exported import CoreGraphics

@_transparent public func round(_ x: Float) -> Float { roundf(x) }
@_transparent public func round(_ x: Double) -> Double { MOSLibm.round(x) }
// round(CGFloat) vem do CoreGraphics (reexportado)
