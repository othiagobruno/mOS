// mOS SDK — Glibc: o OpenSwiftUI só conhece dois caminhos de libc, Darwin e Glibc. O módulo Darwin ligaria os caminhos da
// Apple (ObjC, frameworks privados), então o mOS se apresenta como "Glibc": este módulo reexporta a libc do macOS
// (MOSDarwin, com as sobrecargas Float do tgmath), a libm mínima do SDK (MOSLibm) e o CGFloat.
// Só entra no -I do OpenSwiftUI (as deps não testam canImport(Glibc)).

@_exported import MOSDarwin
@_exported import MOSLibm
@_exported import CoreGraphics