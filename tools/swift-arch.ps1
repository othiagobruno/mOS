# mOS - arquitetura da cadeia Swift (tradutor arm64, docs/tradutor-arm64.md, frente LIBSYS). Incluído com
#   . "$PSScriptRoot\swift-arch.ps1" $Arch
# pelos scripts tools\build-swift-*-objc.ps1, link-swift-*.ps1, build-frameworks-objc.ps1 e build-swiftui-objc.ps1.
# x86_64 (padrão): alvos e saída de sempre (build\swift). arm64: alvo arm64-apple-macos26 / aarch64-macos e saída em
# build\swift-arm64 (mesma estrutura), juntada com o x86_64 por tools\build-swift-arm64.ps1 (tools\lipo-mos.ps1).
param([string]$MosSwiftArch = 'x86_64')   # nome único: o dot-source tipa a variável no script que inclui
if ($MosSwiftArch -notin 'x86_64', 'arm64') { throw "arquitetura desconhecida: $MosSwiftArch (x86_64 ou arm64)" }
$SwiftTriple = "$MosSwiftArch-apple-macos26"                                     # swift-frontend -target
$SwiftModuleTriple = "$MosSwiftArch-apple-macos"                                 # nome do .swiftmodule dentro de Swift.swiftmodule\
$ZigTarget = if ($MosSwiftArch -eq 'arm64') { 'aarch64-macos' } else { 'x86_64-macos' }   # zig cc/c++ -target
$SwiftBuild = "$(Split-Path $PSScriptRoot -Parent)\build\swift$(if ($MosSwiftArch -eq 'arm64') { '-arm64' })"
