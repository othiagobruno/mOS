// mOS — overlay Swift do AppKit (/usr/lib/swift/libswiftAppKit.dylib, como no macOS). Código próprio do mOS: só os
// tipos e extensões que os apps importam do overlay; o AppKit ObjC em si vem do libSystem.mos.
// tools/build-frameworks-objc.sh (swift-overlay-path, overlay-only). As partes que dependem do overlay Foundation
// (String, IndexPath) ficam em AppKit_Foundation.swift.

@_exported import AppKit
import CoreGraphics

// MARK: - NSEvent.SpecialKey

extension NSEvent {
    /// Teclas especiais: os caracteres de função do OpenStep (0xF700…) e alguns de controle.
    public struct SpecialKey: RawRepresentable, Equatable, Hashable, Sendable {
        public let rawValue: Int
        public init(rawValue: Int) { self.rawValue = rawValue }
        public var unicodeScalar: Unicode.Scalar { Unicode.Scalar(UInt32(truncatingIfNeeded: rawValue)) ?? "\u{FFFD}" }

        public static var upArrow: SpecialKey { SpecialKey(rawValue: 0xF700) }
        public static var downArrow: SpecialKey { SpecialKey(rawValue: 0xF701) }
        public static var leftArrow: SpecialKey { SpecialKey(rawValue: 0xF702) }
        public static var rightArrow: SpecialKey { SpecialKey(rawValue: 0xF703) }
        public static var f1: SpecialKey { SpecialKey(rawValue: 0xF704) }
        public static var f2: SpecialKey { SpecialKey(rawValue: 0xF705) }
        public static var f3: SpecialKey { SpecialKey(rawValue: 0xF706) }
        public static var f4: SpecialKey { SpecialKey(rawValue: 0xF707) }
        public static var f5: SpecialKey { SpecialKey(rawValue: 0xF708) }
        public static var f6: SpecialKey { SpecialKey(rawValue: 0xF709) }
        public static var f7: SpecialKey { SpecialKey(rawValue: 0xF70A) }
        public static var f8: SpecialKey { SpecialKey(rawValue: 0xF70B) }
        public static var f9: SpecialKey { SpecialKey(rawValue: 0xF70C) }
        public static var f10: SpecialKey { SpecialKey(rawValue: 0xF70D) }
        public static var f11: SpecialKey { SpecialKey(rawValue: 0xF70E) }
        public static var f12: SpecialKey { SpecialKey(rawValue: 0xF70F) }
        public static var f13: SpecialKey { SpecialKey(rawValue: 0xF710) }
        public static var f14: SpecialKey { SpecialKey(rawValue: 0xF711) }
        public static var f15: SpecialKey { SpecialKey(rawValue: 0xF712) }
        public static var f16: SpecialKey { SpecialKey(rawValue: 0xF713) }
        public static var f17: SpecialKey { SpecialKey(rawValue: 0xF714) }
        public static var f18: SpecialKey { SpecialKey(rawValue: 0xF715) }
        public static var f19: SpecialKey { SpecialKey(rawValue: 0xF716) }
        public static var f20: SpecialKey { SpecialKey(rawValue: 0xF717) }
        public static var f21: SpecialKey { SpecialKey(rawValue: 0xF718) }
        public static var f22: SpecialKey { SpecialKey(rawValue: 0xF719) }
        public static var f23: SpecialKey { SpecialKey(rawValue: 0xF71A) }
        public static var f24: SpecialKey { SpecialKey(rawValue: 0xF71B) }
        public static var f25: SpecialKey { SpecialKey(rawValue: 0xF71C) }
        public static var f26: SpecialKey { SpecialKey(rawValue: 0xF71D) }
        public static var f27: SpecialKey { SpecialKey(rawValue: 0xF71E) }
        public static var f28: SpecialKey { SpecialKey(rawValue: 0xF71F) }
        public static var f29: SpecialKey { SpecialKey(rawValue: 0xF720) }
        public static var f30: SpecialKey { SpecialKey(rawValue: 0xF721) }
        public static var f31: SpecialKey { SpecialKey(rawValue: 0xF722) }
        public static var f32: SpecialKey { SpecialKey(rawValue: 0xF723) }
        public static var f33: SpecialKey { SpecialKey(rawValue: 0xF724) }
        public static var f34: SpecialKey { SpecialKey(rawValue: 0xF725) }
        public static var f35: SpecialKey { SpecialKey(rawValue: 0xF726) }
        public static var insert: SpecialKey { SpecialKey(rawValue: 0xF727) }
        public static var deleteForward: SpecialKey { SpecialKey(rawValue: 0xF728) }
        public static var home: SpecialKey { SpecialKey(rawValue: 0xF729) }
        public static var begin: SpecialKey { SpecialKey(rawValue: 0xF72A) }
        public static var end: SpecialKey { SpecialKey(rawValue: 0xF72B) }
        public static var pageUp: SpecialKey { SpecialKey(rawValue: 0xF72C) }
        public static var pageDown: SpecialKey { SpecialKey(rawValue: 0xF72D) }
        public static var printScreen: SpecialKey { SpecialKey(rawValue: 0xF72E) }
        public static var scrollLock: SpecialKey { SpecialKey(rawValue: 0xF72F) }
        public static var pause: SpecialKey { SpecialKey(rawValue: 0xF730) }
        public static var sysReq: SpecialKey { SpecialKey(rawValue: 0xF731) }
        public static var `break`: SpecialKey { SpecialKey(rawValue: 0xF732) }
        public static var reset: SpecialKey { SpecialKey(rawValue: 0xF733) }
        public static var stop: SpecialKey { SpecialKey(rawValue: 0xF734) }
        public static var menu: SpecialKey { SpecialKey(rawValue: 0xF735) }
        public static var user: SpecialKey { SpecialKey(rawValue: 0xF736) }
        public static var system: SpecialKey { SpecialKey(rawValue: 0xF737) }
        public static var print: SpecialKey { SpecialKey(rawValue: 0xF738) }
        public static var clearLine: SpecialKey { SpecialKey(rawValue: 0xF739) }
        public static var clearDisplay: SpecialKey { SpecialKey(rawValue: 0xF73A) }
        public static var insertLine: SpecialKey { SpecialKey(rawValue: 0xF73B) }
        public static var deleteLine: SpecialKey { SpecialKey(rawValue: 0xF73C) }
        public static var insertCharacter: SpecialKey { SpecialKey(rawValue: 0xF73D) }
        public static var deleteCharacter: SpecialKey { SpecialKey(rawValue: 0xF73E) }
        public static var prev: SpecialKey { SpecialKey(rawValue: 0xF73F) }
        public static var next: SpecialKey { SpecialKey(rawValue: 0xF740) }
        public static var select: SpecialKey { SpecialKey(rawValue: 0xF741) }
        public static var execute: SpecialKey { SpecialKey(rawValue: 0xF742) }
        public static var undo: SpecialKey { SpecialKey(rawValue: 0xF743) }
        public static var redo: SpecialKey { SpecialKey(rawValue: 0xF744) }
        public static var find: SpecialKey { SpecialKey(rawValue: 0xF745) }
        public static var help: SpecialKey { SpecialKey(rawValue: 0xF746) }
        public static var modeSwitch: SpecialKey { SpecialKey(rawValue: 0xF747) }
        public static var enter: SpecialKey { SpecialKey(rawValue: 0x0003) }
        public static var backspace: SpecialKey { SpecialKey(rawValue: 0x0008) }
        public static var tab: SpecialKey { SpecialKey(rawValue: 0x0009) }
        public static var newline: SpecialKey { SpecialKey(rawValue: 0x000A) }
        public static var formFeed: SpecialKey { SpecialKey(rawValue: 0x000C) }
        public static var carriageReturn: SpecialKey { SpecialKey(rawValue: 0x000D) }
        public static var backTab: SpecialKey { SpecialKey(rawValue: 0x0019) }
        public static var delete: SpecialKey { SpecialKey(rawValue: 0x007F) }
        public static var lineSeparator: SpecialKey { SpecialKey(rawValue: 0x2028) }
        public static var paragraphSeparator: SpecialKey { SpecialKey(rawValue: 0x2029) }

        /// O valor bruto é uma tecla especial (função 0xF700–0xF8FF ou um dos controles acima)?
        internal static func _isSpecial(_ c: Int) -> Bool {
            switch c {
            case 0xF700...0xF8FF, 0x03, 0x08, 0x09, 0x0A, 0x0C, 0x0D, 0x19, 0x7F, 0x2028, 0x2029: return true
            default: return false
            }
        }
    }
}

// MARK: - _AppKitKitNumericRawRepresentable (NSLayoutPriority + 1, - 1)

public protocol _AppKitKitNumericRawRepresentable: RawRepresentable, Comparable where RawValue: Comparable & Numeric {}

extension _AppKitKitNumericRawRepresentable {
    public static func < (lhs: Self, rhs: Self) -> Bool { lhs.rawValue < rhs.rawValue }
    public static func + (lhs: Self, rhs: RawValue) -> Self { Self(rawValue: lhs.rawValue + rhs)! }
    public static func - (lhs: Self, rhs: RawValue) -> Self { Self(rawValue: lhs.rawValue - rhs)! }
}

extension NSLayoutPriority: _AppKitKitNumericRawRepresentable {}

extension NSLayoutPriority {
    public static var required: NSLayoutPriority { NSLayoutPriority(rawValue: 1000) }
    public static var defaultHigh: NSLayoutPriority { NSLayoutPriority(rawValue: 750) }
    public static var dragThatCanResizeWindow: NSLayoutPriority { NSLayoutPriority(rawValue: 510) }
    public static var windowSizeStayPut: NSLayoutPriority { NSLayoutPriority(rawValue: 500) }
    public static var dragThatCannotResizeWindow: NSLayoutPriority { NSLayoutPriority(rawValue: 490) }
    public static var defaultLow: NSLayoutPriority { NSLayoutPriority(rawValue: 250) }
    public static var fittingSizeCompression: NSLayoutPriority { NSLayoutPriority(rawValue: 50) }
}
