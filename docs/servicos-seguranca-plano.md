# Frente 19 — Serviços e segurança do sistema (Natalia)

Plano e placar da frente: Security/Keychain/CryptoKit/CommonCrypto, TCC com prompts, UserNotifications,
Launch Services, Spotlight (`mdfind`/`mdls`), Apple Events/AppleScript (`osascript`), Sparkle e auto-update,
login items, CloudKit/iCloud sem crash. Cada item só vira ✅ com teste de host registrado aqui.

## Levantamento (main `1f2fe93`, 15 nov. 2026)

| Área | Onde | Estado encontrado |
|---|---|---|
| Keychain de senhas | `userland/libsystem/security.c` | Real: SecItem* e SecKeychain* de senha, arquivo cifrado (v2). Sem `kSecClassKey`, sem grupos de acesso |
| SecKey | `security.c` | **Tudo stub** (`errSecUnimplemented`/NULL): criar, importar, exportar, assinar, verificar, cifrar, ECDH |
| SecCertificate | `security.c` | Só criar/copiar o DER. CN, resumo, serial, chave, emissor: stub |
| SecPolicy / SecTrust | `security.c`, `sectrust.c` | Cadeia real com as raízes da Mozilla, mas só quando o host vem do SecureTransport. `SecPolicyCreateSSL` perde o nome; âncoras, data, política básica, `CopyKey`, `CopyResult`, `EvaluateAsync*`: stub |
| PKCS#12, `SecItemImport`, transforms de digest/assinatura | `security.c` | Stub |
| Assinatura de código | `security.c` | Tudo "não assinado"; não lê `LC_CODE_SIGNATURE` |
| Authorization | `security.c` | Pedir direito = negado, sem prompt |
| CommonCrypto | `commoncrypto.c`, `commoncrypto2.c` | MD5/SHA/HMAC/PBKDF2/AES ECB-CBC-CTR. `CCCalibratePBKDF` stub; sem GCM/3DES/RC4/Blowfish/CAST, sem `CCRSA*`/`CCEC*`, `CCKDF*`, `CCRandom*` parcial |
| CryptoKit | `cryptokit.c`, `ck_*.c`, `frameworks/CryptoKit` | Real (SHA, HMAC, HKDF, AES-GCM, ChaChaPoly, P-256/384/521, X25519, Ed25519). Falta `_RSA` e `SecureEnclave` (indisponível, sem crash) |
| TCC | `tcc.c`, `tcc_core.inc`, `kernel/ui/tcc_prompt.c` | Arquivo de decisões; prompt só para tela e acessibilidade. Câmera, microfone, contatos, calendário, fotos, automação (Apple Events), pastas, disco total: sem prompt. Sem `tccutil` |
| UserNotifications | `usernotifications.c`, `un_center.c` | Banner, clique, autorização. Conferir gatilhos agendados, categorias/ações, anexos, entregues, badge |
| Launch Services | `kernel/compat/launchsvc.c`, `openurl.c` | Forte (doc `launch-services.md`). Falta `lsregister` e auditoria de símbolos |
| Spotlight | `mdquery.c`, `mdq_core.c` | Varredura sem índice; sem atualização ao vivo; **sem `mdfind`, `mdls`, `mdutil`**; `NSMetadataQuery` a conferir |
| Apple Events / AppleScript | `openurl.c` (só recebe `odoc`/`GURL`) | **Não há** `AESend` entre processos, `NSAppleScript`, `OSAScript`, nem `osascript` |
| Sparkle | apps trazem o framework | Depende de assinatura de código, `ditto`, `hdiutil`, XPC do instalador, `NSTask`. Sem teste de ponta a ponta |
| Login items | `loginitems.c`, `smlogin.c`, `smjob.c`, `ServiceManagement` | Registro em arquivo; conferir subida no login pelo init |
| CloudKit / iCloud | `frameworks/CloudKit/CloudKit.c` (vazio) + 41 stubs Swift em `sys_zerados.c` | Sem classes: um app que chame `CKContainer` cai em seletor inexistente |

## Fases (ordem de entrega)

1. **SecKey e criptografia assimétrica** — RSA (geração, PKCS#1 v1.5, PSS, OAEP, raw), EC P-256/384/521
   (ECDSA X9.62 e RFC 4754, ECDH padrão e cofator), formatos externos do macOS (PKCS#1 para RSA, X9.63 para EC),
   `SecKeyCopyAttributes`, `SecKeyCopyPublicKey`, `SecKeyIsAlgorithmSupported`, `SecKeyRaw*`, `SecKeyEncrypt/Decrypt`.
2. **Certificados e confiança** — acessores de `SecCertificate`, `SecPolicy` com nome e tipo, `SecTrust` completo
   (âncoras, só âncoras, data, política básica, `CopyKey`, `CopyResult`, assíncrono), `SecPKCS12Import`, `SecItemImport`
   (PEM/DER/PKCS#8), chaves no Keychain (`kSecClassKey`, tag de aplicação), transforms de digest/assinatura.
3. **CommonCrypto restante** — `CCCalibratePBKDF`, GCM, 3DES, `CCKDF`, `CCRandom*`, e o que o `machocheck` dos apps pedir.
4. **Assinatura de código** — `LC_CODE_SIGNATURE`, CodeDirectory e hashes de página, entitlements, team ID do CMS,
   `SecStaticCodeCheckValidity` real, requisitos (subconjunto da linguagem), `SecTask*`.
5. **TCC completo** — todos os serviços que apps pedem, prompt para cada um, `tccutil reset`, painel de Privacidade
   (com a Camila, dona do Ajustes).
6. **UserNotifications** — gatilhos de tempo e calendário persistidos, categorias e ações, anexos, `NSUserNotification`.
7. **Spotlight** — índice com atualização por FSEvents, `mdfind`, `mdls`, `mdutil`, `NSMetadataQuery` ao vivo.
8. **Apple Events e AppleScript** — Apple Event Manager entre processos (Mach), `NSAppleEventDescriptor`,
   `NSAppleScript`, interpretador do subconjunto usado na prática e `/usr/bin/osascript`, portão de automação no TCC.
9. **Sparkle** — fluxo do Sparkle 1 e 2 conferido em harness: appcast, EdDSA, `ditto`/`hdiutil`, instalador XPC.
10. **Login items** — `SMAppService`, `SMLoginItemSetEnabled`, `LSSharedFileList` subindo no login.
11. **CloudKit/iCloud** — classes `CK*` respondendo "sem conta" (`CKAccountStatusNoAccount`, `CKErrorNotAuthenticated`),
    os 41 símbolos Swift, `NSUbiquitousKeyValueStore` local, `ubiquityIdentityToken` nulo.
12. **Launch Services** — `lsregister` e lacunas do `machocheck`.

## Placar

| Fase | Estado | Teste |
|---|---|---|
| 1 SecKey | ✅ (15 nov.); falta ECIES e OAEP+AESGCM | `gcc -O2 -DMOS_HOST_TEST -Itools -Ikernel/include tools/seckeytest.c kernel/net/crypto.c kernel/net/pk.c -o /tmp/seckeytest`, `/tmp/seckeytest self` → ok; `python3 -I tools/seckeytest.py /tmp/seckeytest` → **225/225** contra o OpenSSL 3.5.5 (RSA 1024/2048/3072, EC P-256/384/521) |
| 3 CommonCrypto | ✅ (15 nov.) | `python3 -I tools/cctest.py` → **512/512** contra OpenSSL/hashlib (AES ECB/CBC/CFB/CFB8/CTR/OFB/XTS/GCM, DES, 3DES, RC2, RC4, Blowfish, MD2/MD4/MD5/SHA-1/2, HMAC, PBKDF2) |
| 2 Certificados, confiança, PKCS#12, Keychain de chaves | ✅ (15 nov.) | `python3 -I tools/seccerttest.py` → **59/59** (PKCS#12 de 6 variantes do cryptography e do `openssl pkcs12`, PEM, nomes, cadeia); SecKey agora **231/231** |
| 5 TCC | ✅ diálogo, pedidos e painel (16 nov.; painel "Permissões dos apps" da Camila, PR #366) | `tools/tccprompttest.c` **134/134**, `tools/tccutiltest.sh` **8/8** |
| 11 CloudKit/iCloud | ✅ código e nomes (16 nov.); não executado em VM | `CloudKit.m` compila para Mach-O (170 classes); overlay `CloudKit.swift` compilado contra o Foundation Swift real: os **41/41** nomes mangled que os apps pedem saem dele |
| 7 Spotlight | ✅ (16 nov.); não executado em VM | `tools/agentes_host.c` **39/39** (predicado com `$time.*` e `InRange`), `tools/mdclitest.sh` **18/18** (mdfind, mdls, mdutil) |
| 8 Apple Events, AppleScript, osascript | ✅ (16 nov.); não executado em VM | `tools/astest.c` **118/118** sob ASan (descritores, API C `AEDesc`, linguagem), `tools/osascripttest.sh` **18/18** |
| 6 UserNotifications | ✅ (16 nov.); não executado em VM | `userland/tests-appkit/un_host.c` **49/49** sob ASan (gatilhos, calendário em hora local, Central, ações, persistência) |
| 4, 9, 10, 12 | a fazer | — |

## Fase 1 — o que entrou (15 nov.)

- `userland/libsystem/seckey_core.inc`: inteiros grandes (até 8192 bits), Montgomery, Knuth D, Miller-Rabin,
  geração RSA (e = 65537, p > q, d mod λ, CRT), PKCS#1 v1.5, PSS, OAEP, raw; ECDSA X9.62/RFC 4754; ECDH com KDF X9.63;
  importação de PKCS#1, PKCS#8, SPKI, SEC1 e X9.63.
- `userland/libsystem/seckey.c`: `SecKeyCreateRandomKey`, `SecKeyGeneratePair`, `SecKeyCreateWithData`,
  `SecKeyCopyPublicKey`, `SecKeyCopyExternalRepresentation`, `SecKeyCopyAttributes` (com `klbl` = SHA-1 da pública),
  `SecKeyGetBlockSize`, `SecKeyIsAlgorithmSupported`, `SecKeyCreateSignature`, `SecKeyVerifySignature`,
  `SecKeyCreateEncryptedData`, `SecKeyCreateDecryptedData`, `SecKeyCopyKeyExchangeResult`, `SecKeyRawSign/RawVerify`,
  `SecKeyEncrypt/Decrypt`, `SecCertificateCopyKey`, `SecCertificateCopyPublicKey`, `SecTrustCopyKey`,
  `SecTrustCopyPublicKey`. `SecIdentityCopyPrivateKey` passou a devolver um SecKey.
- Medido: geração RSA 2048 em ~0,5 s e 4096 em ~0,9 s no host (gcc -O2, Ryzen R2544).
- Limites atuais: sem Secure Enclave (erro -4, como Mac sem T2); ECIES e OAEP+AESGCM dizem "não suportado";
  chave com `kSecAttrIsPermanent` ainda não vai ao Keychain (fase 2). Não rodou em VM (build proibido até o fim).

## Fase 3 — CommonCrypto (15 nov.)

- `commoncrypto_cryptor.inc` (incluído por `commoncrypto2.c`) troca o CCCryptor só-AES por um genérico:
  AES, DES, 3DES, RC2, RC4, Blowfish; modos ECB, CBC, CFB, CFB8, CTR, OFB, RC4, XTS (`CCCryptorEncryptDataBlock`)
  e GCM (`CCCryptorGCM*`, `CCCryptorGCMOneshot*`, IV de qualquer tamanho). `cc_ciphers.inc` tem as cifras; as tabelas
  do Blowfish são os dígitos de pi calculados (Machin), não copiados.
- HMAC e PBKDF2 com SHA-224 de verdade (antes caíam no SHA-256 sem avisar). Novos: `CC_SHA224*`, `CC_MD2*`,
  `CC_MD4*`, `CCCalibratePBKDF`; os stubs desses nomes saíram de `sys_zerados.c`.
- Fora: CAST5 (`kCCUnimplemented`, as S-boxes não são deriváveis e nenhum app medido pede), modos F8/LRW/CCM.

## Fase 2 — certificados, confiança, PKCS#12 e Keychain (15 nov.)

- `seccert.c` + `seccert_core.inc`: `SecCertificateCopySubjectSummary`, `CopyCommonName`, `CopyEmailAddresses`,
  `CopySerialNumber(Data)`, `CopyNormalizedIssuer/SubjectSequence`, `CopyNotValidBefore/AfterDate`, `CopyValues`
  (assunto, emissor, série, validade no formato `{label, type, value}`), `SecItemImport`/`SecItemExport` (PEM, DER,
  PKCS#1, PKCS#8, SEC1, SPKI; com Keychain, os itens também vão para ele), `SecPKCS12Import` (PBES2 AES/3DES com
  PBKDF2-SHA1/224/256/384/512, PBE-SHA1-3DES, RC2-40/128, RC4, MAC, BER indefinido; a identidade vai para o Keychain
  como no macOS), `SecTrustSettingsSet/RemoveTrustSettings` e os domínios usuário/admin de `SecTrustSettingsCopy*`
  (arquivos `mos-trust-user.txt`/`mos-trust-admin.txt`), `SecCertificateAddToKeychain` (antes fingia sucesso).
- `security.c`: `SecPolicyCreateSSL` guarda o nome; `SecPolicyCreateWithProperties`, `SecPolicyCopyProperties`;
  `SecTrust` com âncoras do app, "só âncoras", data, política básica/SSL servidor/cliente, ajustes do usuário
  (âncora e "Nunca confiar"), `CopyResult`, `CopyProperties`, `CopyPolicies`, `GetTrustResult`, `EvaluateAsync*`
  na fila pedida, CFError com o OSStatus do macOS (expirado -67818, nome -67602...). O avaliador é
  `cx_sectrust_eval` (`sectrust.c`, o mesmo `x509.c` do kernel).
- Keychain: classes `kSecClassKey` e `kSecClassCertificate` (atributos `atag`, `kcls`, `type`, `klbl`, `subj`,
  `issr`, `slnr` preenchidos sozinhos), busca por `kSecValueRef`, `kSecReturnRef` devolve SecKey/SecCertificate.
  Arquivos antigos continuam legíveis (campos novos opcionais). `kSecAttrIsPermanent` grava as duas metades.
- Limites: sem busca de intermediários/OCSP pela rede (AIA); `CopyNormalized*` não normaliza maiúsculas; sem
  exportar PKCS#12/PKCS#7.

## Fase 5 — TCC (16 nov.)

- Diálogo para todos os serviços (tabela em `kernel/ui/tcc_prompt_core.inc`), inclusive Apple Events por app alvo
  (`kTCCServiceAppleEvents+<bundle>`) e notificações ("Não Permitir"/"Permitir"); ícone de cadeado para os serviços
  sem desenho próprio. Doc: `docs/tcc-prompt.md`.
- `cx_tcc_request_async` (`tcc.c`): o bloco de `requestAccessForMediaType:`, `CNContactStore requestAccess…`,
  `PHPhotoLibrary requestAuthorization:` e `UNUserNotificationCenter requestAuthorization…` volta depois da escolha
  (antes voltava na hora com "não"; o das notificações nunca voltava).
- `/usr/bin/tccutil reset` (`userland/cli-macos/tccutil.c`).

## Fase 11 — CloudKit e iCloud (16 nov.)

- `userland/frameworks/CloudKit/CloudKit.m` (substitui o `CloudKit.c` vazio): contêiner "sem conta"
  (`accountStatus` = `CKAccountStatusNoAccount` sem erro), bancos, todas as operações `CK*Operation` terminando com
  `CKErrorNotAuthenticated` no bloco de conclusão (e depois o `completionBlock`), objetos locais completos (CKRecord
  com valores, `changedKeys`, `encryptedValues`, cópia, `encodeSystemFieldsWithCoder:`; IDs com igualdade e hash;
  zonas, referências, assets, consultas, assinaturas, compartilhamento, `CKNotification` a partir do payload).
- `CloudKit.swift`: o overlay da Apple (subscripts, `CKRecordValueProtocol` e conformidades, `CKError` com
  `clientRecord`/`serverRecord`/`retryAfterSeconds`/`partialErrorsByItemID`, inits, blocos `…ResultBlock` com `Result`,
  `modifyRecordZones` async, `configuredWith`). Os 41 stubs saíram de `sys_zerados.c`.
- Build: `swift-needs` (Foundation) adia o overlay na 1ª passada de `build-frameworks-objc.sh`; `tools/build.sh`
  recompila esses frameworks depois do Foundation Swift.
- `userland/libsystem/icloud.c`: `NSUbiquitousKeyValueStore` local (~/Library/SyncedPreferences, limites da Apple,
  aviso de cota) e `NSFileManager` sem contêiner (erro 4353).
- Limite honesto: nada disso rodou num app ainda (precisa do build final). CKSyncEngine não existe.

## Fase 7 — Spotlight (16 nov.)

- Predicado (`mdq_core.c`): `$time.now/today/yesterday/this_week/this_month/this_year/iso` com deslocamento e fuso,
  `InRange(atributo, a, b)`.
- `mdquery.c`: datas (criação, modificação, último uso, adicionado), tamanhos lógico e físico, dono, invisível,
  número de itens da pasta, versão do app, `kMDItemWhereFroms` (xattr), `kMDItemTextContent` (texto até 256 KB);
  `MDItemCopyAttribute` devolve NSDate/NSNumber/NSArray; `.metadata_never_index` respeitado; `kMDQueryWantsUpdates`
  pelo FSEventStream com `kMDQueryDidUpdateNotification` (adicionados/alterados/removidos) e
  `MDQueryDisableUpdates` represando.
- `NSMetadataQuery`/`NSMetadataItem` reais (substituem a fachada de `foundation_more.c`): NSPredicate do app como
  filtro, escopos, ordem, notificações de início/fim/atualização no fio principal. `groupedResults`/`valueLists`
  vazios (o mOS não agrupa).
- `/usr/bin/mdfind`, `/usr/bin/mdls`, `/usr/bin/mdutil` (`userland/cli-macos`). Não há índice persistente: cada
  busca lê o disco (até 50 000 itens); `mdutil -E` só informa isso.

## Fase 8 — Apple Events, AppleScript e osascript (16 nov.)

- **Descritores** (`ae_core.inc`): escalares, listas, registros e eventos com atributos; coerções (texto ↔ números,
  `utf8`/`utxt`/`TEXT`, lista de um item, booleanos, `furl`, `ldt `); fio próprio (`'mOSE'`) entre processos.
- **API C** (`ae_capi.inc`): `AEDesc` com o layout da Apple (12 bytes, `pack(2)`), `AECreateDesc`, `AEDisposeDesc`,
  `AEDuplicateDesc`, `AECoerceDesc`/`AECoercePtr`, listas (`AECreateList`, `AEPutPtr`, `AEPutDesc`, `AEGetNthPtr`,
  `AEGetNthDesc`, `AECountItems`, `AEDeleteItem`), registros e parâmetros (`AEPutParamPtr`, `AEGetParamDesc`,
  `AESizeOfParam`, `AEDeleteParam`, `AE*KeyDesc`), atributos (`evcl`, `evid`, `rtid`, `tran`, `addr` e livres),
  `AECreateAppleEvent` (returnID automático) e `CreateObjSpecifier` (registro `'obj '`).
- **Entre apps** (`appleevents.c`): cada app atende `com.mos.ae.<bundle>` e `com.mos.ae.pid.<pid>` (pelo
  `cx_openurl_listen`); `AESend`/`AESendMessage` mandam pelo Mach e esperam a resposta (`kAEWaitReply`, timeout em
  ticks, `kNoTimeOut`); alvo fechado é aberto pelo Launch Services (menos `quit`). Tratador no fio principal:
  `AEInstallEventHandler`, `-[NSAppleEventManager setEventHandler:...]` (com curinga `****`), padrões do AppKit
  (`oapp`, `rapp`, `actv`, `quit`, `odoc`, `GURL`). `openurl.c` entrega documentos e URLs pelo mesmo caminho (as
  classes falsas `MOSURLAppleEvent` e companhia saíram).
- **Automação (TCC)**: mandar para outro app pede `kTCCServiceAppleEvents+<alvo>`; sem escolha abre o diálogo e o
  envio espera; `kAENeverInteract` dá `-1744`, negado dá `-1743`; `AEDeterminePermissionToAutomateTarget` real.
- **NSAppleEventDescriptor** completo (fábricas de texto, número, booleano, enum, tipo, data, arquivo, bundle, pid,
  lista, registro, evento; acessores; `sendEventWithOptions:timeout:error:`; `aeDesc`).
- **AppleScript** (`as_interp.inc`): a linguagem que scripts e apps usam, com `property`, `global`/`local`,
  `on run argv`, handlers; `applescript.c` liga ao sistema (Apple Events, `NSAlert` para `display dialog`/`alert` e
  `choose from list`, banner para `display notification`, `/bin/sh -c`, CGEvents com Acessibilidade, NSPasteboard,
  Launch Services). `NSAppleScript` (fonte, `compileAndReturnError:`, `executeAndReturnError:`,
  `executeAppleEvent:error:` com `psbr`/`snam`) e `NSUserAppleScriptTask`.
- **`/usr/bin/osascript`** (`cli-macos/osascript.c`): `-e` repetido, arquivo ou entrada padrão, argumentos,
  `-s h|s|e|o`, `-i`, erros no formato do macOS; TCC o vê como `com.mos.osascript`.
- Limites: `.scpt` compilado (binário) não roda (erro claro); só AppleScript (`-l JavaScript` dá erro); dicionários
  de script (`sdef`) dos apps não são lidos — comandos próprios de um app (fora de abrir/ativar/sair/abrir
  documento/URL) chegam como erro `-1708`.

## Fase 6 — UserNotifications (16 nov.)

- **Centro** (`un_center.c`): pendentes e entregues em listas separadas; gatilho de intervalo (repetição ≥ 60 s) e
  de calendário com a regra do NSCalendar (unidades abaixo da menor informada valem 0), em hora local e com dia da
  semana; mesmo identificador substitui; Central com as 64 mais novas; badge; categorias com ações. Tudo vai para
  `~/Library/Application Support/com.mos.usernotifications/<bundle>.txt`: o que venceu com o app fechado é entregue
  quando ele abre (uma vez só, e a repetição segue).
- **UserNotifications** (`usernotifications.c`): conteúdo completo (título, subtítulo, corpo, som, badge, userInfo em
  plist, categoria, thread, anexos, nível de interrupção, relevância), cópia mutável, pedidos, gatilhos com
  `nextTriggerDate`, `getPending`/`getDelivered`, remoções, categorias, `UNNotificationAction`/`UNTextInputNotificationAction`
  com ícone, `UNNotificationSound` (crítico também), anexos validados e movidos como no macOS, configurações,
  `setBadgeCount:`, `.provisional`; delegates no fio principal (na frente só aparece se o delegate pedir) e respostas
  com `UNTextInputNotificationResponse`. A tabela fixa de 48 objetos saiu (vazava e travava depois de 48).
- **Banner da Interface** (Camila, PR #402): `MOS_DESK_BANNER_EX` com subtítulo e até 4 ações (destrutiva, texto,
  abrir o app), `MOS_DESK_TAKE_EX` (clique, botão, texto, fechar) e `MOS_DESK_REMOVE`.
- **NSUserNotification** (`nsusernotification.c`): entregar, agendar (data e repetição por dias/horas/semanas, 1 mês
  ou 1 ano), botões de ação e resposta, ações adicionais, entregues/agendadas, delegate `shouldPresent`/`didDeliver`/`didActivate`.
- Limites: sem APNs (push falha com `UNErrorDomain`); a imagem do anexo não aparece no banner; outras repetições do
  `NSUserNotification` (ex.: a cada 2 meses) dão aviso e não são agendadas.

