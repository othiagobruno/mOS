// mOS — renderer do OpenSwiftUI para o mOS (entra no módulo OpenSwiftUICore).
// O StdoutRenderer (caminho não-Darwin) entrega o DisplayList pronto ao destino registrado em
// _mosDisplayListSink (ver Replace/.../DisplayListStdoutRenderer.swift); aqui ele é desenhado na view
// de desenho do mOS pela ponte C (MOSBridge): cores, formas (Path), texto e listas aninhadas, com os
// efeitos de opacidade, translação/transformação afim e recorte.
#if !OPENSWIFTUI_SWIFTUI_RENDERER
package import Foundation
package import OpenCoreGraphicsShims
import MOSBridge

/// Destino do DisplayList renderizado (registrado pelo host do mOS).
nonisolated(unsafe) package var _mosDisplayListSink: ((DisplayList, DisplayList.Version) -> Void)? = nil

// Formas, traço e recorte (sessão de Path/DisplayList): preenchimentos e recortes passam pela ponte de
// caminhos em C (MOSBridge_path.h, userland/libsystem/swiftbridge_path.c), que achata as curvas, aplica
// a regra não-zero/par-ímpar e mantém a pilha de recorte (retângulo ou caminho, com máscara AA). O traço
// chega como forma preenchida: _StrokedShape usa Path.strokedPath, cujo contorno vem do mesmo C.
// Efeitos tratados: opacidade, transformações (afim; projeção e rotação 3D pela parte afim), clip (por
// retângulo, caminho, par-ímpar e invertido), máscara (por alfa do conteúdo da máscara), sombra
// (filtro .shadow e conteúdo .shadow) e filtros de cor sobre cores planas (colorMultiply, brilho,
// contraste, saturação, tons de cinza, matiz, inversão, matriz de cor, luminância para alfa,
// monocromático). Sem leitura do quadro já desenhado, blendMode, blur e backdrop desenham sem o efeito.
package enum MOSDisplayListDrawing {
    /// Desenha a lista inteira (chamado dentro do drawRect: do canvas do mOS; origem no topo).
    package static func draw(_ list: DisplayList) {
        mos_clip_reset()
        var v = Visitor()
        v.append(list: list, transform: .identity, opacity: 1)
        mos_clip_reset()
    }

    struct Visitor {
        /// Filtros de cor dos efeitos ancestrais (do mais externo ao mais interno).
        var colorFilters: [GraphicsFilter] = []
        /// Dentro de uma captura de máscara/sombra: só formas e cores entram; texto e imagem são pulados.
        var capturing = 0

        mutating func append(list: DisplayList, transform: CGAffineTransform, opacity: Float) {
            for item in list.items { append(item: item, transform: transform, opacity: opacity) }
        }

        mutating func append(item: DisplayList.Item, transform: CGAffineTransform, opacity: Float) {
            switch item.value {
            case let .content(content):
                draw(content, frame: item.frame, transform: transform, opacity: opacity)
            case let .effect(effect, list):
                apply(effect, list: list, frame: item.frame, transform: transform, opacity: opacity)
            case let .states(states):
                if let first = states.first { append(list: first.1, transform: transform, opacity: opacity) }
            case .empty:
                break
            }
        }

        mutating func apply(_ effect: DisplayList.Effect, list: DisplayList, frame: CGRect, transform: CGAffineTransform, opacity: Float) {
            // o conteúdo do efeito é relativo à origem do item
            let local = CGAffineTransform(translationX: frame.minX, y: frame.minY).concatenating(transform)
            switch effect {
            case let .opacity(alpha):
                append(list: list, transform: local, opacity: opacity * alpha)
            case let .transform(t):
                if let affine = t.affineTransform {
                    append(list: list, transform: affine.concatenating(local), opacity: opacity)
                } else if let projection = t.projectionTransform {
                    append(list: list, transform: projection.mosAffine.concatenating(local), opacity: opacity)
                } else {
                    append(list: list, transform: local, opacity: opacity)
                }
            case let .clip(path, style, options):
                pushClip(path, style: style, inverse: options.contains(.inverse), transform: local)
                append(list: list, transform: local, opacity: opacity)
                mos_clip_pop()
            case let .mask(mask, options):
                // o conteúdo da máscara (no mesmo espaço do item) vira cobertura por alfa
                mos_mask_begin()
                capturing += 1
                append(list: mask, transform: local, opacity: 1)
                capturing -= 1
                mos_mask_end_clip(options.contains(.inverse) ? 1 : 0)
                append(list: list, transform: local, opacity: opacity)
                mos_clip_pop()
            case let .filter(filter):
                apply(filter, list: list, transform: local, opacity: opacity)
            default:
                // identity, geometryGroup, compositingGroup, archive, properties, animation, contentTransition,
                // view, accessibility, platform, state, interpolator*: só passam o conteúdo adiante.
                // blendMode e backdropGroup precisariam ler o que já foi desenhado: desenho normal.
                append(list: list, transform: local, opacity: opacity)
            }
        }

        mutating func apply(_ filter: GraphicsFilter, list: DisplayList, transform: CGAffineTransform, opacity: Float) {
            switch filter {
            case let .shadow(style) where !style.kind.contains(.inner):
                // sombra do conteúdo: captura a cobertura dos filhos, desenha desfocada e deslocada, depois os filhos
                mos_mask_begin()
                capturing += 1
                append(list: list, transform: transform, opacity: opacity)
                capturing -= 1
                let c = resolve(style.color)
                let off = style.offset.applying(transform)
                mos_mask_end_shadow(Double(style.radius * transform.mosScale), Double(off.width), Double(off.height),
                                    Double(c.red), Double(c.green), Double(c.blue), Double(c.opacity * opacity))
                append(list: list, transform: transform, opacity: opacity)
            case let .projection(projection):
                append(list: list, transform: projection.mosAffine.concatenating(transform), opacity: opacity)
            case .colorMultiply, .brightness, .contrast, .saturation, .grayscale, .hueRotation, .colorInvert,
                 .colorMatrix, .luminanceToAlpha, .colorMonochrome:
                colorFilters.append(filter)
                append(list: list, transform: transform, opacity: opacity)
                colorFilters.removeLast()
            default:
                // blur, variableBlur, averageColor, curvas, shader, alphaThreshold, vibrantColorMatrix e sombra
                // interna: sem leitura do quadro, desenha sem o filtro
                append(list: list, transform: transform, opacity: opacity)
            }
        }

        /// Cor com os filtros de cor herdados (o mais interno primeiro).
        func resolve(_ color: Color.Resolved) -> Color.Resolved {
            var c = color
            for f in colorFilters.reversed() { c = c.mosApplying(f) }
            return c
        }

        func pushClip(_ path: Path, style: FillStyle, inverse: Bool, transform t: CGAffineTransform) {
            if !inverse, let r = path.rect(), t.b == 0, t.c == 0 {
                let v = r.applying(t)
                mos_clip_push_rect(Double(v.minX), Double(v.minY), Double(v.width), Double(v.height))
                return
            }
            let pushed = MOSPathBridge.with(path, transform: t) { mos_clip_push_path($0, style.isEOFilled ? 1 : 0, inverse ? 1 : 0) }
            if pushed == nil { mos_clip_push_rect(0, 0, 0, 0) }   // mantém o pareamento com mos_clip_pop
        }

        mutating func draw(_ content: DisplayList.Content, frame: CGRect, transform: CGAffineTransform, opacity: Float) {
            let rect = frame.applying(transform)
            // o Path das formas é relativo ao quadro do item
            let toItem = CGAffineTransform(translationX: frame.minX, y: frame.minY).concatenating(transform)
            if capturing > 0 {
                switch content.value {
                case .text, .image: return   // não entram na máscara (e não podem aparecer na tela)
                default: break
                }
            }
            switch content.value {
            case let .color(color):
                fill(CGRect(origin: .zero, size: frame.size), toItem, resolve(color), opacity)
            case let .chameleonColor(fallback, _):
                fill(CGRect(origin: .zero, size: frame.size), toItem, resolve(fallback), opacity)
            case let .backdrop(effect):
                // material: sem desfoque do fundo, só a tinta
                fill(CGRect(origin: .zero, size: frame.size), toItem, resolve(effect.color), opacity)
            case let .shape(path, paint, fillStyle):
                guard let color = paint.mosResolvedColor else { return }
                fill(path, fillStyle, toItem, resolve(color), opacity)
            case let .shadow(path, style):
                guard !style.kind.contains(.inner) else { return }
                let c = resolve(style.color)
                let off = style.offset.applying(toItem)
                _ = MOSPathBridge.with(path, transform: toItem) {
                    mos_path_fill_shadow($0, 0, Double(style.radius * toItem.mosScale), Double(off.width), Double(off.height),
                                         Double(c.red), Double(c.green), Double(c.blue), Double(c.opacity * opacity))
                }
            case let .text(view, size):
                if view.text.isEmpty { return }
                #if !canImport(Darwin)
                // como o DisplayListViewPlatform: o ResolvedStyledText desenha no quadro do item, aplicando as
                // margens; fontes, cores e quebra vêm dos atributos (MOSTextAttributes.swift)
                MOSTextAttributes.withOpacity(Double(opacity)) {
                    view.text.draw(
                        in: CGRect(origin: rect.origin, size: frame.size),
                        with: size,
                        applyingMarginOffsets: true,
                        containsResolvable: view.text.isDynamic,
                        context: .shared,
                        renderer: view.renderer
                    )
                }
                #else
                let s = view.mosString
                let font = view.mosFontSize
                let c = view.mosColor ?? Color.Resolved(red: 0, green: 0, blue: 0, opacity: 0.85)
                s.withCString { mos_draw_text($0, Double(rect.minX), Double(rect.minY), font, 0,
                                              Double(c.red), Double(c.green), Double(c.blue), Double(c.opacity * opacity)) }
                #endif
            case let .flattened(list, offset, _):
                append(list: list, transform: CGAffineTransform(translationX: frame.minX + offset.x, y: frame.minY + offset.y).concatenating(transform), opacity: opacity)
            case let .image(image):
                // imagens: arquivo do bundle ou símbolo do sistema (ver MOSImage.swift)
                MOSImage.draw(image, frame: frame, transform: transform, opacity: opacity)
            default:
                // .drawing (lista do RenderBox), .platformView/.platformLayer (views/camadas nativas),
                // .view e .placeholder: não há equivalente no mOS
                break
            }
        }

        /// Retângulo (local) com a afim `t`: GC_RRECT quando é só translação/escala, senão caminho.
        func fill(_ r: CGRect, _ t: CGAffineTransform, _ c: Color.Resolved, _ opacity: Float, radius: CGFloat = 0) {
            let a = Double(c.opacity * opacity)
            guard a > 0, !r.isNull, !r.isEmpty else { return }
            if t.b == 0, t.c == 0, t.a > 0, t.d > 0, radius == 0 || t.a == t.d {
                let v = r.applying(t)
                mos_fill_rect(Double(v.minX), Double(v.minY), Double(v.width), Double(v.height), Double(radius * t.a),
                              Double(c.red), Double(c.green), Double(c.blue), a)
                return
            }
            let path = radius > 0 ? Path(roundedRect: r, cornerRadius: radius, style: .circular) : Path(r)
            _ = MOSPathBridge.with(path, transform: t) { mos_path_fill($0, 0, Double(c.red), Double(c.green), Double(c.blue), a) }
        }

        /// Forma preenchida (não-zero ou par-ímpar); retângulos e retângulos de canto circular uniforme
        /// vão direto para o retângulo arredondado do kernel.
        func fill(_ path: Path, _ style: FillStyle, _ t: CGAffineTransform, _ c: Color.Resolved, _ opacity: Float) {
            switch path.storage {
            case let .rect(r):
                fill(r, t, c, opacity)
                return
            case let .roundedRect(rr) where rr.isUniform && !rr.needsContinuousCorners:
                fill(rr.rect, t, c, opacity, radius: rr.clampedCornerRadius)
                return
            default:
                break
            }
            let a = Double(c.opacity * opacity)
            guard a > 0 else { return }
            _ = MOSPathBridge.with(path, transform: t) {
                mos_path_fill($0, style.isEOFilled ? 1 : 0, Double(c.red), Double(c.green), Double(c.blue), a)
            }
        }
    }
}

extension CGAffineTransform {
    /// Escala média (raiz do determinante): raios de sombra e espessuras em pixels.
    var mosScale: CGFloat { sqrt(abs(a * d - b * c)) }
}

extension ProjectionTransform {
    /// Parte afim da projeção (a perspectiva não é desenhada no mOS).
    var mosAffine: CGAffineTransform {
        let w = m33 != 0 ? m33 : 1
        return CGAffineTransform(a: m11 / w, b: m12 / w, c: m21 / w, d: m22 / w, tx: m31 / w, ty: m32 / w)
    }
}

extension Color.Resolved {
    /// Filtro de cor aplicado a uma cor plana (componentes sRGB, como no CoreImage/SwiftUI para cores).
    func mosApplying(_ filter: GraphicsFilter) -> Color.Resolved {
        var r = red, g = green, b = blue, a = opacity
        func lum() -> Float { 0.2126 * r + 0.7152 * g + 0.0722 * b }
        func clamp(_ v: Float) -> Float { min(max(v, 0), 1) }
        switch filter {
        case let .colorMultiply(m):
            r *= m.red; g *= m.green; b *= m.blue; a *= m.opacity
        case let .brightness(amount):
            let k = Float(amount)
            r += k; g += k; b += k
        case let .contrast(amount):
            let k = Float(amount)
            r = (r - 0.5) * k + 0.5; g = (g - 0.5) * k + 0.5; b = (b - 0.5) * k + 0.5
        case let .saturation(amount):
            let k = Float(amount), l = lum()
            r = l + (r - l) * k; g = l + (g - l) * k; b = l + (b - l) * k
        case let .grayscale(amount):
            let k = Float(amount), l = lum()
            r += (l - r) * k; g += (l - g) * k; b += (l - b) * k
        case let .hueRotation(angle):
            let cs = Float(cos(angle.radians)), sn = Float(sin(angle.radians))
            let nr = r * (0.213 + cs * 0.787 - sn * 0.213) + g * (0.715 - cs * 0.715 - sn * 0.715) + b * (0.072 - cs * 0.072 + sn * 0.928)
            let ng = r * (0.213 - cs * 0.213 + sn * 0.143) + g * (0.715 + cs * 0.285 + sn * 0.140) + b * (0.072 - cs * 0.072 - sn * 0.283)
            let nb = r * (0.213 - cs * 0.213 - sn * 0.787) + g * (0.715 - cs * 0.715 + sn * 0.715) + b * (0.072 + cs * 0.928 + sn * 0.072)
            r = nr; g = ng; b = nb
        case .colorInvert:
            r = 1 - r; g = 1 - g; b = 1 - b
        case let .colorMatrix(m, _):
            let nr = m.m11 * r + m.m12 * g + m.m13 * b + m.m14 * a + m.m15
            let ng = m.m21 * r + m.m22 * g + m.m23 * b + m.m24 * a + m.m25
            let nb = m.m31 * r + m.m32 * g + m.m33 * b + m.m34 * a + m.m35
            let na = m.m41 * r + m.m42 * g + m.m43 * b + m.m44 * a + m.m45
            r = nr; g = ng; b = nb; a = na
        case .luminanceToAlpha:
            a = lum(); r = 0; g = 0; b = 0
        case let .colorMonochrome(mono):
            let l = lum() + mono.bias, k = mono.amount
            r += (mono.color.red * l - r) * k; g += (mono.color.green * l - g) * k; b += (mono.color.blue * l - b) * k
        default:
            break
        }
        return Color.Resolved(red: clamp(r), green: clamp(g), blue: clamp(b), opacity: clamp(a))
    }
}

private struct MOSColorPaintVisitor: ResolvedPaintVisitor {
    var color: Color.Resolved?
    mutating func visitPaint<P>(_ paint: P) where P: ResolvedPaint { color = paint as? Color.Resolved }
}
extension AnyResolvedPaint {
    var mosResolvedColor: Color.Resolved? {
        var v = MOSColorPaintVisitor()
        visit(&v)
        return v.color
    }
}

/// União dos retângulos da lista, para o NSHostingController dimensionar a raiz.
package enum MOSDisplayListMetrics {
    package static func extent(_ list: DisplayList) -> CGSize {
        var maxX: CGFloat = 0
        var maxY: CGFloat = 0
        func walk(_ list: DisplayList) {
            for item in list.items {
                maxX = max(maxX, item.frame.maxX)
                maxY = max(maxY, item.frame.maxY)
                switch item.value {
                case let .effect(_, child):
                    walk(child)
                case let .states(states):
                    if let first = states.first { walk(first.1) }
                default:
                    break
                }
            }
        }
        walk(list)
        return CGSize(width: maxX, height: maxY)
    }
}

extension StyledTextContentView {
    /// Texto puro do conteúdo.
    var mosString: String { text.storage?.string ?? "" }
    #if !canImport(Darwin)
    /// Tamanho da fonte do primeiro caractere (atributo .kitFont, CTFont do CoreText do mOS).
    var mosFontSize: Double { Double(text.storage?.mosFirstStyle.size ?? 13) }
    /// Cor do primeiro caractere (atributo .kitForegroundColor); nil se o texto não tem cor própria.
    var mosColor: Color.Resolved? {
        guard let storage = text.storage, storage.length > 0,
              let (r, g, b, a) = MOSTextAttributes.rgba(storage.attribute(.kitForegroundColor, at: 0, effectiveRange: nil))
        else { return nil }
        return Color.Resolved(red: Float(r), green: Float(g), blue: Float(b), opacity: Float(a))
    }
    #else
    var mosFontSize: Double { 13 }
    var mosColor: Color.Resolved? { nil }
    #endif
}
#endif
