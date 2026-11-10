// TextEditor do OpenSwiftUI no mOS: caret, seleção e edição.
//
// A métrica é uma grade de 8×18 pt (um unicode scalar por célula, linha só em "\n").
// O display list não garante a mesma largura que mos_measure, então o caret não usa a fonte.
// Os offsets são bytes UTF-8 e passam por mos_sui_ed_* (a mesma conta do hosting_test).
#if !OPENSWIFTUI_SWIFTUI_RENDERER
public import OpenSwiftUICore
import CoreGraphics
import Foundation
import MOSBridge

private enum MOSEditorMetrics {
    static let cell = 8.0
    static let line = 18.0
    static let insetX = 6.0
    static let insetY = 4.0
    static let cap = 4096
}

private struct MOSEditorGlyph: Hashable {
    var byte: Int
    var text: String
}

private struct MOSEditorRow {
    var start: Int
    var end: Int
    var glyphs: [MOSEditorGlyph]
}

private struct MOSEditorBuffer {
    var bytes = ContiguousArray<CChar>(repeating: 0, count: MOSEditorMetrics.cap)
    var len: Int32 = 0
    var caret: Int32 = 0
    var anchor: Int32 = 0

    mutating func load(_ text: String, caret: Int, anchor: Int) -> Bool {
        let utf = Array(text.utf8)
        if utf.count >= MOSEditorMetrics.cap { return false }
        for i in utf.indices { bytes[i] = CChar(bitPattern: utf[i]) }
        bytes[utf.count] = 0
        len = Int32(utf.count)
        let limit = utf.count
        self.caret = Int32(min(max(caret, 0), limit))
        self.anchor = Int32(min(max(anchor, 0), limit))
        return true
    }

    var text: String {
        bytes.withUnsafeBufferPointer { buf in
            guard let base = buf.baseAddress else { return "" }
            return String(cString: base)
        }
    }

    mutating func place(_ index: Int32, _ extend: Int32) {
        var caret = self.caret
        var anchor = self.anchor
        mos_sui_ed_place(len, &caret, &anchor, index, extend)
        self.caret = caret
        self.anchor = anchor
    }

    mutating func replace(_ ins: String) {
        var len = self.len
        var caret = self.caret
        var anchor = self.anchor
        ins.withCString { raw in
            bytes.withUnsafeMutableBufferPointer { buf in
                guard let base = buf.baseAddress else { return }
                mos_sui_ed_replace(base, Int32(MOSEditorMetrics.cap), &len, &caret, &anchor, raw)
            }
        }
        self.len = len
        self.caret = caret
        self.anchor = anchor
    }

    mutating func backspace() {
        var len = self.len
        var caret = self.caret
        var anchor = self.anchor
        bytes.withUnsafeMutableBufferPointer { buf in
            guard let base = buf.baseAddress else { return }
            mos_sui_ed_backspace(base, Int32(MOSEditorMetrics.cap), &len, &caret, &anchor)
        }
        self.len = len
        self.caret = caret
        self.anchor = anchor
    }

    mutating func arrow(_ dir: Int32, _ shift: Int32) {
        var caret = self.caret
        var anchor = self.anchor
        let len = self.len
        bytes.withUnsafeBufferPointer { buf in
            guard let base = buf.baseAddress else { return }
            mos_sui_ed_arrow(base, len, &caret, &anchor, dir, shift)
        }
        self.caret = caret
        self.anchor = anchor
    }

    mutating func vertical(_ dir: Int32, _ shift: Int32) {
        var caret = self.caret
        var anchor = self.anchor
        let len = self.len
        bytes.withUnsafeBufferPointer { buf in
            guard let base = buf.baseAddress else { return }
            mos_sui_ed_vertical(base, len, &caret, &anchor, dir, shift)
        }
        self.caret = caret
        self.anchor = anchor
    }
}

private func mosEditorRows(_ text: String) -> [MOSEditorRow] {
    var rows: [MOSEditorRow] = []
    var glyphs: [MOSEditorGlyph] = []
    var start = 0
    var byte = 0
    let scalars = Array(text.unicodeScalars)
    if scalars.isEmpty { return [MOSEditorRow(start: 0, end: 0, glyphs: [])] }
    for scalar in scalars {
        let n = scalar.utf8.count
        if scalar == "\n" {
            rows.append(MOSEditorRow(start: start, end: byte, glyphs: glyphs))
            byte += n
            start = byte
            glyphs = []
        } else {
            glyphs.append(MOSEditorGlyph(byte: byte, text: String(scalar)))
            byte += n
        }
    }
    rows.append(MOSEditorRow(start: start, end: byte, glyphs: glyphs))
    return rows
}

private func mosEditorColumn(_ row: MOSEditorRow, _ byte: Int) -> Int {
    if byte <= row.start { return 0 }
    var column = 0
    for glyph in row.glyphs {
        if byte <= glyph.byte { return column }
        column += 1
    }
    return row.glyphs.count
}

private func mosEditorHit(_ text: String, _ point: CGPoint) -> Int {
    let rows = mosEditorRows(text)
    if rows.isEmpty { return 0 }
    var line = Int((Double(point.y) - MOSEditorMetrics.insetY) / MOSEditorMetrics.line)
    if line < 0 { line = 0 }
    if line >= rows.count { return rows[rows.count - 1].end }
    let row = rows[line]
    let column = Int(mos_sui_ed_index(Double(point.x) - MOSEditorMetrics.insetX, MOSEditorMetrics.cell, Int32(row.glyphs.count)))
    if column <= 0 { return row.start }
    if column >= row.glyphs.count { return row.end }
    return row.glyphs[column].byte
}

private func mosEditorApply(_ text: Binding<String>, _ caret: Binding<Int>, _ anchor: Binding<Int>, _ change: (inout MOSEditorBuffer) -> Void) -> Bool {
    var edit = MOSEditorBuffer()
    guard edit.load(text.wrappedValue, caret: caret.wrappedValue, anchor: anchor.wrappedValue) else { return false }
    change(&edit)
    caret.wrappedValue = Int(edit.caret)
    anchor.wrappedValue = Int(edit.anchor)
    let next = edit.text
    if next != text.wrappedValue { text.wrappedValue = next }
    return true
}

private func mosEditorHandle(_ key: MOSKeyInput, _ text: Binding<String>, _ caret: Binding<Int>, _ anchor: Binding<Int>) -> Bool {
    if key.modifiers.contains(.command) || key.modifiers.contains(.control) { return false }
    let shift: Int32 = key.modifiers.contains(.shift) ? 1 : 0
    switch key.keyCode {
    case MOSKeyInput.deleteKey:
        return mosEditorApply(text, caret, anchor) { $0.backspace() }
    case MOSKeyInput.forwardDeleteKey:
        return mosEditorApply(text, caret, anchor) { edit in
            if edit.caret != edit.anchor {
                edit.backspace()
            } else {
                let before = edit.caret
                edit.arrow(1, 1)
                if edit.caret != before { edit.backspace() }
            }
        }
    case MOSKeyInput.returnKey, MOSKeyInput.enterKey:
        return mosEditorApply(text, caret, anchor) { $0.replace("\n") }
    case MOSKeyInput.leftArrowKey:
        return mosEditorApply(text, caret, anchor) { $0.arrow(-1, shift) }
    case MOSKeyInput.rightArrowKey:
        return mosEditorApply(text, caret, anchor) { $0.arrow(1, shift) }
    case 126:
        return mosEditorApply(text, caret, anchor) { $0.vertical(-1, shift) }
    case 125:
        return mosEditorApply(text, caret, anchor) { $0.vertical(1, shift) }
    case MOSKeyInput.tabKey, MOSKeyInput.escapeKey:
        return false
    default:
        let insert = key.insertableText
        if insert.isEmpty { return false }
        return mosEditorApply(text, caret, anchor) { $0.replace(insert) }
    }
}

private struct MOSEditorLine: View {
    var row: MOSEditorRow
    var index: Int
    var caret: Int
    var anchor: Int
    var focused: Bool

    var body: some View {
        let y = MOSEditorMetrics.insetY + Double(index) * MOSEditorMetrics.line
        let lo = min(caret, anchor)
        let hi = max(caret, anchor)
        let from = max(lo, row.start)
        let to = min(hi, row.end)
        return ZStack(alignment: .topLeading) {
            if to > from {
                Rectangle()
                    .fill(Color.accentColor.opacity(0.35))
                    .frame(width: Double(mosEditorColumn(row, to) - mosEditorColumn(row, from)) * MOSEditorMetrics.cell, height: 16)
                    .offset(x: MOSEditorMetrics.insetX + Double(mosEditorColumn(row, from)) * MOSEditorMetrics.cell, y: y)
            }
            HStack(spacing: 0) {
                ForEach(row.glyphs, id: \.byte) { glyph in
                    Text(verbatim: glyph.text)
                        .frame(width: MOSEditorMetrics.cell, height: 16, alignment: .leading)
                }
            }
            .offset(x: MOSEditorMetrics.insetX, y: y)
            if focused && caret == anchor && caret >= row.start && caret <= row.end {
                Rectangle()
                    .fill(Color.accentColor)
                    .frame(width: 1.5, height: 16)
                    .offset(x: MOSEditorMetrics.insetX + Double(mosEditorColumn(row, caret)) * MOSEditorMetrics.cell, y: y)
            }
        }
    }
}

@available(macOS 11.0, *)
public struct TextEditor: View {
    var text: Binding<String>
    @State private var target = MOSFocusTarget()
    @State private var isFocused = false
    @State private var caret = 0
    @State private var anchor = 0

    public init(text: Binding<String>) { self.text = text }

    public var body: some View {
        configureTarget()
        let value = text.wrappedValue
        let rows = mosEditorRows(value)
        return GeometryReader { _ in
            ZStack(alignment: .topLeading) {
                RoundedRectangle(cornerRadius: 6).fill(Color.white)
                RoundedRectangle(cornerRadius: 6)
                    .strokeBorder(isFocused ? Color.accentColor : Color.gray.opacity(0.45), lineWidth: isFocused ? 2 : 1)
                ForEach(0..<rows.count, id: \.self) { index in
                    MOSEditorLine(row: rows[index], index: index, caret: caret, anchor: anchor, focused: isFocused)
                }
            }
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { drag in
                        MOSFocusManager.shared.focus(target, byClick: true)
                        select(from: mosEditorHit(value, drag.startLocation), to: mosEditorHit(value, drag.location))
                    }
                    .onEnded { drag in
                        select(from: mosEditorHit(value, drag.startLocation), to: mosEditorHit(value, drag.location))
                    }
            )
        }
        .frame(minHeight: 96)
        .onDisappear { MOSFocusManager.shared.unregister(target) }
    }

    private func select(from start: Int, to current: Int) {
        _ = mosEditorApply(text, $caret, $anchor) { edit in
            edit.place(Int32(start), 0)
            if current != start { edit.place(Int32(current), 1) }
        }
    }

    private func configureTarget() {
        let target = target
        let text = text
        let focused = $isFocused
        let caret = $caret
        let anchor = $anchor
        target.canFocus = true
        target.focusDidChange = { focused.wrappedValue = $0 }
        target.handleKey = { mosEditorHandle($0, text, caret, anchor) }
        MOSFocusManager.shared.register(target)
    }
}
#endif
