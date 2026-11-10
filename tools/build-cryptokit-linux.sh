#!/bin/sh
# Equivalente, no Linux, de tools/build-frameworks-objc.ps1 -Only CryptoKit.
# A toolchain é a oficial de swift.org (Swift 6.4.2, Ubuntu 24.04), com a assinatura
# da Swift 6.x Release Signing Key (52BB 7E3D E28A 71BE 22EC 05FF EF80 A866 B47A 981F).
# O Foundation do Linux 6.4 mangleia como FoundationEssentials. Este script compila um
# módulo Foundation mínimo, só para o nome de ABI ($s10Foundation…), e o CryptoKit.swift
# de verdade por cima. A saída é um ELF; o Mach-O da imagem continua o do .ps1 no Windows.
# Uso: SWIFT_ROOT=/caminho/swift-6.4.2-RELEASE-ubuntu24.04 tools/build-cryptokit-linux.sh [saida]
set -e
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
swift=${SWIFT_ROOT:?defina SWIFT_ROOT para a toolchain oficial}
out=${1:-$root/build/CryptoKit}
fe=$swift/usr/bin/swift-frontend
src=$root/userland/frameworks/CryptoKit/CryptoKit.swift
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/Foundation.swift" << 'EOF'
public protocol ContiguousBytes {
    func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R
}
public protocol DataProtocol: ContiguousBytes {
    var count: Int { get }
    subscript(bounds: Range<Int>) -> Data { get }
    subscript(bounds: PartialRangeFrom<Int>) -> Data { get }
}
public struct Data: DataProtocol, ContiguousBytes, Sequence {
    var b: [UInt8]
    public init() { b = [] }
    public init<S: Sequence>(_ s: S) where S.Element == UInt8 { b = Array(s) }
    public init<D: DataProtocol>(_ d: D) { b = d.withUnsafeBytes { Array($0) } }
    public init<C: ContiguousBytes>(_ c: C) { b = c.withUnsafeBytes { Array($0) } }
    public var count: Int { b.count }
    public subscript(bounds: Range<Int>) -> Data { Data(b[bounds]) }
    public subscript(bounds: PartialRangeFrom<Int>) -> Data { Data(b[bounds]) }
    public func prefix(_ n: Int) -> Data { Data(b.prefix(n)) }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R {
        try b.withUnsafeBytes(body)
    }
    public static func + (l: Data, r: Data) -> Data { Data(l.b + r.b) }
    public func makeIterator() -> Array<UInt8>.Iterator { b.makeIterator() }
}
EOF
"$fe" -frontend -c -target x86_64-unknown-linux-gnu -module-name Foundation -parse-as-library -O \
    -module-abi-name Foundation -emit-module-path "$tmp/Foundation.swiftmodule" \
    -module-cache-path "$tmp/mod" -o "$tmp/Foundation.o" "$tmp/Foundation.swift"
"$fe" -frontend -c -target x86_64-unknown-linux-gnu -module-name CryptoKit -parse-as-library -O \
    -enable-library-evolution -import-underlying-module \
    -I "$root/userland/frameworks/CryptoKit/include" -I "$tmp" \
    -module-cache-path "$tmp/mod2" -o "$tmp/CryptoKit.o" "$src"
mkdir -p "$(dirname "$out")"
gcc -shared -o "$out" "$tmp/CryptoKit.o" "$tmp/Foundation.o" -Wl,--unresolved-symbols=ignore-all
echo "OK: $out"
