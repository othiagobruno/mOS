// mOS — teste do Scanner (userland/sdk/swift-objc/Foundation/Scanner.swift sobre o NSScanner de fdn_scanner.c):
// charactersToBeSkipped padrão e nil, caseSensitive, posição em UTF-16, números e o caso do OpenSwiftUI
// (Text+Localized: skip nil, scanUpToCharacters, scanCharacter, scanInt). tools/test-scanner-swift.sh.
// Imprime "scanner: N/N ok".
import Foundation

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

// padrão: pula espaços e quebras de linha
var s = Scanner(string: "  \n hello 42 3.5")
let pulo = s.charactersToBeSkipped
check(pulo?.contains(" ") == true && pulo?.contains("\n") == true && pulo?.contains("\u{3000}") == true && pulo?.contains("a") == false, "padrão é whitespacesAndNewlines")
check(s.scanString("hello") == "hello", "padrão: scanString depois de espaços")
check(s.scanInt() == 42, "padrão: scanInt")
check(s.scanDouble() == 3.5, "padrão: scanDouble")
check(s.isAtEnd, "padrão: isAtEnd")

// nil: espaço é caractere comum
s = Scanner(string: " 7")
s.charactersToBeSkipped = nil
check(s.charactersToBeSkipped == nil, "nil guardado")
check(s.scanInt() == nil, "nil: scanInt não pula o espaço")
check(s.scanLocation == 0, "nil: falha não move")
check(s.scanCharacter() == " ", "nil: scanCharacter devolve o espaço")
check(s.scanInt() == 7, "nil: scanInt depois")

// OpenSwiftUI Text+Localized
s = Scanner(string: "Olá %1$@ e %2$lld")
s.charactersToBeSkipped = nil
let especificador = CharacterSet(charactersIn: "%")
check(s.scanUpToCharacters(from: especificador) == "Olá ", "scanUpToCharacters até o %")
check(s.scanCharacter() == "%", "scanCharacter: %")
check(s.scanInt(representation: .decimal) == 1, "scanInt(representation: .decimal)")
check(s.scanString("$") == "$", "scanString $")
check(s.scanCharacter() == "@", "scanCharacter: @")

// caseSensitive
s = Scanner(string: "HeLLo")
check(s.caseSensitive == false, "caseSensitive padrão NO")
check(s.scanString("hello") == "HeLLo", "sem caixa")
s = Scanner(string: "HeLLo")
s.caseSensitive = true
check(s.scanString("hello") == nil, "com caixa falha")

// posição UTF-16 e currentIndex
s = Scanner(string: "a😀b")
check(s.scanCharacters(from: CharacterSet(charactersIn: "a😀")) == "a😀", "conjunto fora do BMP")
check(s.scanLocation == 3, "scanLocation em UTF-16")
let emoji = "a😀b"
check(s.currentIndex == String.Index(utf16Offset: 3, in: emoji), "currentIndex")
s.currentIndex = String.Index(utf16Offset: 1, in: emoji)
check(s.scanLocation == 1, "currentIndex atribuído")

// números
s = Scanner(string: "0xff -12 99999999999 1e3 0x1.8p1 18446744073709551615 12.50")
check(s.scanInt(representation: .hexadecimal) == 255, "hex")
check(s.scanInt32() == -12, "Int32")
check(s.scanInt32() == Int32.max, "Int32 satura")
check(s.scanFloat() == 1000, "Float com expoente")
check(s.scanDouble(representation: .hexadecimal) == 3, "hex double")
check(s.scanUInt64() == UInt64.max, "UInt64 máximo")
check(s.scanDecimal() == Decimal(sign: .plus, exponent: -1, significand: 125), "Decimal")

// API antiga com ponteiro
s = Scanner(string: "chave = valor")
var r: NSString?
check(s.scanUpTo(" = ", into: &r) && r?.length == 5 && s.scanLocation == 5, "scanUpTo(_:into:)")

print("scanner: \(ok)/\(total) ok")
