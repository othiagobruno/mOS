// mOS — o CoreColor da Apple resolve uma cor de plataforma (CGColor/NSColor) por trás de
// canImport(Darwin). O renderizador de stdout chama esse inicializador; aqui ele não produz cor.
#if !OPENSWIFTUI_SWIFTUI_RENDERER
extension Color.Resolved {
    package init?(platformColor: AnyObject) {
        _ = platformColor
        return nil
    }
}
#endif
