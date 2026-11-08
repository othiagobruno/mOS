// mOS — OpenCoreGraphicsShims do SwiftUI.framework (variante com interop ObjC): os tipos CG são os do CoreGraphics de
// verdade (como com OPENCOREGRAPHICS_COREGRAPHICS no upstream). Como o OpenCoreGraphics da variante nativa, reexporta
// também o Foundation (e o que o Foundation do mOS tinha a mais: MOSFoundationExtras).
@_exported import CoreGraphics
@_exported import Foundation
@_exported import MOSFoundationExtras
