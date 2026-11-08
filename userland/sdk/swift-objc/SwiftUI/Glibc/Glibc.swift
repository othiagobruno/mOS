// mOS — Glibc do SwiftUI.framework (variante com interop ObjC). O OpenSwiftUI só conhece dois caminhos de libc,
// Darwin e Glibc; o caminho Darwin dele exige o SDK privado da Apple. Como na variante nativa
// (userland/sdk/openswiftui/swift/Glibc), o mOS se apresenta como "Glibc", aqui reexportando o Darwin e o
// CoreGraphics (CGFloat) da variante.
@_exported import Darwin
@_exported import CoreGraphics
