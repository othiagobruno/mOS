// mOS — imagens do SwiftUI no mOS (entra no módulo OpenSwiftUICore).
//
// Sem CoreUI/CoreGraphics não há CUICatalog nem CGImage de verdade. No mOS:
//  - Image("nome") procura o arquivo nos recursos do bundle (e em Assets.xcassets/<nome>.imageset)
//    pela ponte C (mos_image_lookup, userland/libsystem/swiftbridge_image.c);
//  - Image(systemName:) usa o conjunto básico de símbolos de MOSImageSymbols.swift.
// Nos dois casos o GraphicsImage carrega .named(.bitmap(chave)): a chave guarda o nome e o local
// (bundle ou sistema), e o renderer (MOSDisplayListRenderer → MOSImage.draw) desenha a partir dela.
// O tamanho vem do arquivo (pixels / escala @2x/@3x) ou, nos símbolos, de um tamanho de ponto fixo
// ajustado por .imageScale; .resizable() e .aspectRatio() funcionam pelo layout normal do OpenSwiftUI.
#if !OPENSWIFTUI_SWIFTUI_RENDERER
package import Foundation
package import OpenCoreGraphicsShims
import MOSBridge

package enum MOSImage {
    /// Tamanho base (em pontos) de um símbolo do sistema com .imageScale(.medium).
    package static let symbolPointSize: CGFloat = 16

    // MARK: resolução (Image → Image.Resolved)

    #if !OPENSWIFTUI_LINK_COREUI
    /// Resolve um Image(nome)/Image(systemName:) no mOS; nil se não achou (o chamador registra o erro).
    package static func resolve(_ provider: Image.NamedImageProvider, in context: ImageResolutionContext) -> Image.Resolved? {
        if let r = resolve(provider, at: provider.location, in: context) { return r }
        if let backup = provider.backupLocation { return resolve(provider, at: backup, in: context) }
        return nil
    }

    private static func resolve(_ provider: Image.NamedImageProvider, at location: Image.Location,
                                in context: ImageResolutionContext) -> Image.Resolved? {
        let environment = context.environment
        switch location {
        case let .bundle(bundle):
            guard let found = lookup(provider.name, in: bundle) else { return nil }
            let key = NamedImage.BitmapKey(name: provider.name, location: location, in: environment)
            var image = GraphicsImage(
                contents: .named(.bitmap(key)),
                scale: CGFloat(found.scale),
                unrotatedPixelSize: CGSize(width: found.width, height: found.height),
                orientation: .up,
                isTemplate: environment.imageIsTemplate()
            )
            if environment.shouldRedactContent { image.redact(in: environment) }
            return Image.Resolved(image: image, decorative: provider.decorative, label: provider.label)
        case .system, .privateSystem:
            // variantes (.fill, .circle, .slash...) pelo mesmo algoritmo de nomes do upstream
            let name: String? = location.findName(environment.symbolVariants, base: provider.name) { candidate in
                MOSSymbols.has(candidate) ? candidate : nil
            } ?? (MOSSymbols.has(provider.name) ? provider.name : nil)
            guard var symbol = name else { return nil }
            if environment.shouldRedactSymbolImages { symbol = "circle.fill" }
            let key = NamedImage.BitmapKey(name: symbol, location: .system, in: environment)
            let side = symbolPointSize * scaleFactor(environment.imageScale)
            var image = GraphicsImage(
                contents: .named(.bitmap(key)),
                scale: 1,
                unrotatedPixelSize: CGSize(width: side, height: side),
                orientation: .up,
                isTemplate: true          // símbolos seguem a cor de primeiro plano
            )
            if environment.shouldRedactContent { image.redact(in: environment) }
            return Image.Resolved(image: image, decorative: provider.decorative, label: provider.label)
        }
    }

    private static func scaleFactor(_ scale: Image.Scale) -> CGFloat {
        switch scale {
        case .small: return 0.8
        case .large: return 1.3
        default: return 1
        }
    }
    #endif

    // MARK: arquivos

    package struct Found { var handle: UnsafeMutableRawPointer; var width: Double; var height: Double; var scale: Double }

    /// Diretório de busca do bundle (nil = recursos do bundle principal, resolvido no C).
    static func directory(for bundle: Bundle) -> String? {
        let path = bundle.bundlePath
        return path == Bundle.main.bundlePath ? nil : path
    }

    package static func lookup(_ name: String, in bundle: Bundle) -> Found? {
        lookup(name, directory: directory(for: bundle))
    }

    package static func lookup(_ name: String, directory: String?) -> Found? {
        // [largura, altura, escala] devolvidos pela ponte (ponteiros explícitos: sem conversão &var implícita)
        var out: [Double] = [0, 0, 1]
        let handle: UnsafeMutableRawPointer? = out.withUnsafeMutableBufferPointer { o in
            let w = o.baseAddress!, h = w + 1, s = w + 2
            return name.withCString { cname in
                if let directory {
                    return directory.withCString { mos_image_lookup($0, cname, w, h, s) }
                }
                return mos_image_lookup(nil, cname, w, h, s)
            }
        }
        guard let handle, out[0] > 0, out[1] > 0 else { return nil }
        return Found(handle: handle, width: out[0], height: out[1], scale: out[2] > 0 ? out[2] : 1)
    }

    // MARK: desenho (DisplayList.Content.image)

    /// Desenha um GraphicsImage no quadro `frame` (coordenadas do item) com a transformação acumulada.
    package static func draw(_ image: GraphicsImage, frame: CGRect, transform: CGAffineTransform, opacity: Float) {
        guard opacity > 0, frame.width > 0, frame.height > 0 else { return }
        switch image.contents {
        case let .named(.bitmap(key)):
            switch key.location {
            case let .bundle(bundle):
                guard let found = lookup(key.name, in: bundle) else { return }
                let r = frame.applying(transform)
                mos_image_draw(found.handle, Double(r.minX), Double(r.minY), Double(r.width), Double(r.height), Double(opacity))
            case .system, .privateSystem:
                drawSymbol(key.name, image: image, frame: frame, transform: transform, opacity: opacity)
            }
        case let .color(color):
            // imagem redigida (placeholder) ou cor sólida
            let r = frame.applying(transform)
            mos_draw_rect(Double(r.minX), Double(r.minY), Double(r.width), Double(r.height), 0,
                          Double(color.red), Double(color.green), Double(color.blue), Double(color.opacity * opacity), 1, 1)
        default:
            // .cgImage/.ioSurface/.vectorGlyph/.vectorLayer não existem no mOS (sem CoreGraphics/CoreUI)
            break
        }
    }

    static func drawSymbol(_ name: String, image: GraphicsImage, frame: CGRect, transform t: CGAffineTransform, opacity: Float) {
        let c = image.maskColor ?? Color.Resolved(red: 0, green: 0, blue: 0, opacity: 0.85)
        let sx = frame.width / 24, sy = frame.height / 24
        let det = abs(t.a * t.d - t.b * t.c)
        let unit = Double((sx * sy).squareRoot() * det.squareRoot())
        MOSSymbols.draw(name, r: Double(c.red), g: Double(c.green), b: Double(c.blue), a: Double(c.opacity * opacity),
                        unit: unit) { gx, gy in
            let p = CGPoint(x: frame.minX + CGFloat(gx) * sx, y: frame.minY + CGFloat(gy) * sy).applying(t)
            return (Double(p.x), Double(p.y))
        }
    }
}
#endif
