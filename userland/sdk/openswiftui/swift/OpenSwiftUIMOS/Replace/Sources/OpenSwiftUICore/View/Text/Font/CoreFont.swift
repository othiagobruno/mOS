//
//  CoreFont.swift
//  OpenSwiftUICore
//
//  Audited for 6.5.4
//  Status: Complete
//
//  mOS: cópia do upstream; limitedFontHeight sem o SPI Objective-C (CoreFontGetLineHeight) e CTFont por
//  CFTypeID + bitcast (sem ObjC o cast dinâmico para tipo CF falha).

package import Foundation
import OpenSwiftUI_SPI

#if canImport(CoreText)
package import CoreText
package import CoreText_Private
#endif
package typealias CoreFont = CTFont

extension NSAttributedString {
    package func kitFont() -> NSObject? {
        attribute(.kitFont, at: 0, effectiveRange: nil) as? NSObject
    }

    package func limitedFontHeight(by lineLimit: Int) -> CGFloat? {
        #if canImport(CoreText) && !canImport(Darwin)
        // mOS: o CoreFontGetLineHeight do SPI chama -[NSFont lineHeight] por objc_msgSend; aqui a altura
        // da linha sai das métricas do CTFont do CoreText do mOS (ascent + descent + leading)
        guard length > 0, let font = mosCTFont(attribute(.kitFont, at: 0, effectiveRange: nil)) else {
            return nil
        }
        return CoreFont.limitedHeight(
            by: lineLimit,
            lineHeight: font.ascender + font.descender + font.leading,
            leading: font.leading
        )
        #elseif canImport(CoreText)
        guard let font = kitFont() else {
            return nil
        }
        return CoreFont.limitedHeight(
            by: lineLimit,
            lineHeight: CoreFontGetLineHeight(.default, font),
            leading: CoreFontGetLeading(.default, font)
        )
        #else
        return nil
        #endif
    }
}

extension CTFont {
    package static func make(platformFont: AnyObject) -> Self? {
        #if canImport(CoreText)
        // mOS também: CFGetTypeID/CTFontGetTypeID vêm do CF em C e do CoreText do mOS
        guard CFGetTypeID(platformFont) == CTFontGetTypeID() else {
            return nil
        }
        #if canImport(Darwin)
        let coreFont: Self = platformFont as! Self
        #else
        // mOS: sem ObjC o cast dinâmico para tipo CF falha; o tipo já foi conferido acima
        let coreFont: Self = unsafeBitCast(platformFont, to: Self.self)
        #endif
        return coreFont
        #else
        _openSwiftUIPlatformUnimplementedWarning()
        return nil
        #endif
    }

    /// Calculates the height occupied by a limited number of lines.
    ///
    /// Each line contributes `lineHeight`, while every line after the first
    /// contributes an additional `leading`:
    ///
    ///     height = lineHeight * lineLimit + leading * (lineLimit - 1)
    ///
    /// For example, three lines with a line height of `20` and leading of `2`
    /// occupy `64` points: `(20 * 3) + (2 * 2)`.
    ///
    /// - Parameters:
    ///   - lineLimit: The number of lines to include.
    ///   - lineHeight: The height of each line.
    ///   - leading: The spacing between adjacent lines.
    /// - Returns: The calculated height, or zero when `lineLimit` is less than
    ///   one.
    package static func limitedHeight(
        by lineLimit: Int,
        lineHeight: CGFloat,
        leading: CGFloat
    ) -> CGFloat {
        guard lineLimit >= 1 else {
            return 0
        }
        return CGFloat(lineLimit) * lineHeight + CGFloat(lineLimit - 1) * leading
    }
}
