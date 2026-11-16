# Instalador de pacotes (.pkg / .mpkg)

Fase 8 de `docs/arquivos-volumes-plano.md`. `/usr/sbin/installer` e `/usr/sbin/pkgutil` são Mach-O do mOS
(`userland/cli-macos/installer.c`, `pkgutil.c`), sobre o motor `userland/pkg/pkgengine.c`. O Arquivos, ao abrir um
`.pkg`, roda `installer -pkg <arquivo> -target /` e mostra um banner no fim (`kernel/apps/coreservices.c`). O extrator
antigo dentro do kernel (`kernel/apps/pkgextract.c`) saiu.

## O que o motor faz

- **xar**: cabeçalho, TOC em zlib, checksum do TOC (SHA-1, SHA-256 ou MD5 no começo do heap), e para cada membro os
  checksums arquivado e extraído; codificações gzip/zlib, bzip2, xz e cru. Assinatura: detecta e conta os
  certificados (`pkgutil --check-signature`); a cadeia não é conferida (frente da Natalia, Security).
- **Product archive**: `Distribution` (título, `pkg-ref` → membro `#nome.pkg`, `choice` com `start_selected`/`selected`,
  `-applyChoiceChangesXML`, `-showChoicesXML`). Pacote de componente: o próprio xar.
- **PackageInfo**: `identifier`, `version`, `install-location`, `auth`, `payload`, `scripts` (nomes dos scripts),
  `bundle-version`.
- **Scripts**: o membro `Scripts` (cpio em gzip) vai para `$TMPDIR/PKInstallSandbox.<pid>/Scripts/<id>`; `preinstall`
  roda antes da carga e `postinstall` depois, com `$1` = pacote (`arquivo#membro` no product archive), `$2` = destino
  (volume + install-location), `$3` = volume, `$4` = volume, e o ambiente `PACKAGE_PATH`, `DSTROOT`, `DSTVOLUME`,
  `INSTALLER_TEMP`, `TMPDIR`, `SCRIPT_NAME`, `COMMAND_LINE_INSTALL=1`, `INSTALL_PKG_SESSION_ID`, `PATH`, `HOME`,
  `USER`, `LOGNAME`, com a pasta dos scripts como atual. Código de saída diferente de 0 cancela a instalação.
- **Payload**: gzip (ISIZE com mais de 4 GiB tratado), pbzx (blocos xz ou crus), bzip2, xz ou cru; cpio odc
  (070707) e newc (070701/070702); pastas, arquivos (modo e data do cpio), links simbólicos; dono do cpio só quando
  o instalador roda como root. Caminho com `..` recusa.
- **Formato antigo** (pasta `.pkg`/`.mpkg`): `Contents/Info.plist` (`CFBundleIdentifier`,
  `CFBundleShortVersionString`, `IFPkgFlagDefaultLocation`), `Contents/Archive.pax.gz`, `Contents/Archive.bom`,
  scripts em `Contents/Resources` (preflight, preinstall, postinstall, postflight); `.mpkg` com `Contents/Packages`.
- **Recibos**: `<volume>/var/db/receipts/<id>.plist` (InstallDate, InstallPrefixPath, InstallProcessName,
  PackageFileName, PackageIdentifier, PackageVersion) e `<id>.bom` (o Bom do pacote).
- **Bom**: leitor do `BOMStore` (índice de blocos, variável `Paths`, folhas da árvore com pai e nome); conferido
  contra um Bom real feito pelas ferramentas da Apple (o pacote público do Outset): mesma lista do Payload.

## installer

`-pkg`, `-target` (`/`, `LocalSystem`, `CurrentUserHomeDirectory`, `/Volumes/X` ou uma pasta), `-pkginfo`,
`-volinfo`, `-showChoicesXML`, `-applyChoiceChangesXML`, `-query RestartAction`, `-verbose`, `-dumplog`,
`-allowUntrusted`. Saída como a da Apple (`installer: Package name is …`, `The install was successful.`).

## pkgutil

`--pkgs[=regex]`, `--pkg-info`, `--pkg-info-plist`/`--export-plist`, `--files` (com `--only-files`/`--only-dirs`),
`--forget`, `--expand`, `--payload-files`, `--check-signature`, `--file-info`, `--volume`.

## O que não faz

- Os `installation-check`/`volume-check` em JavaScript da Distribution não são avaliados (tratados como aprovados).
- Sem root de verdade no mOS (a conta é 501), o dono dos arquivos do Payload fica com quem instala, e um script que
  exige root (chown, launchctl de sistema) pode falhar; quem resolve privilégios é a frente da Renata/Natalia.
- Relocação de bundles (`relocatable`) não é feita: instala no `install-location`.

## Testes

`sh tools/installer_test.sh` (30 verificações, ASan/UBSan): pacote de componente com gzip, scripts com argumentos e
ambiente e na ordem certa, modo/data/link do cpio, recibo, `pkgutil --pkgs/--pkg-info/regex`; product archive com
Distribution, pbzx e escolha desligada; recusas (script com erro, caminho com `..`, checksum errado, pacote
inexistente) sem deixar nada; `--forget`, `--expand`, `--payload-files`, `--check-signature`; formato antigo com
preflight/postflight. Pacote real (Outset, assinado): `-pkginfo`, `-showChoicesXML`, `--check-signature`,
`--payload-files` e o Bom conferidos à mão.
