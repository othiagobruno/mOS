# Tabelas Unicode do Swift

Geradas pelo projeto Swift (`utils/gen-unicode-data`) a partir do Unicode Character Database; copiadas de
`stdlib/public/stubs/Unicode/Common` da tag `swift-6.4.2-RELEASE`. Licença: Apache 2.0 com Runtime Library Exception
(`LICENSE.txt`), dados do Unicode sob a Unicode License.

- `NormalizationData.h`, `GraphemeData.h`, `EmojiData.h`, `WordData.h`: cópia sem mudança, exceto o `#include`
  de tipos (`swift_unicode_types.h` no lugar de `swift/shims/SwiftStdint.h`).
- `CaseMappingData.h`: só as tabelas `_swift_stdlib_mappings*` e `_swift_stdlib_special_mappings*` de
  `ScalarPropsData.h` (o arquivo inteiro tem 2,5 MB, a maior parte nomes de escalares).

Usadas por `userland/libsystem/swiftrt.c`.
