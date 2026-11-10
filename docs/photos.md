# Photos

O `Photos.framework` (`userland/frameworks/Photos`) e o mesmo código no libSystem
(`userland/libsystem/photos_fw.c`) indexam uma pasta local. Não há iCloud, álbum
inventado nem asset de exemplo.

## Pasta

O app Fotos já varre `~/Imagens` (e também Mesa e Downloads). O framework usa só
`NSHomeDirectory()/Imagens`, que `account_ensure_dirs` cria. `MOS_PHOTOS_LIBRARY`
aponta outra pasta, para teste. O identificador do asset é o caminho absoluto do arquivo.

Arquivo cujo nome começa com ponto é oculto. `includeHiddenAssets` precisa estar ligado
para ele entrar. `.txt` e o resto que não é imagem nem vídeo ficam de fora.

## Permissão

`authorizationStatus` e `requestAuthorization:` passam por `cx_tcc_state` /
`cx_tcc_request` com o serviço `kTCCServicePhotos`. Concedido vira
`PHAuthorizationStatusAuthorized` (3). Negado vira 2. Ausente ou pendente fica 0.
O handler roda na hora, com esse estado. Sem concessão, `fetchAssetsWithOptions:`
devolve `PHFetchResult` com `count` 0. Pasta inexistente também: o índice guarda
"pasta da biblioteca não existe ou não pode ser lida" e o fetch não finge foto.

## Metadados

PNG e JPEG: `mos_img_decode` do ImageIO que está na main (`imageio_decode.c`).
Este lado não altera o ImageIO. GIF e BMP não são decodificados por esse ImageIO;
a largura e a altura saem do cabeçalho do arquivo. Vídeo MP4/MOV: `mvhd` (duração)
e `tkhd` (quadro), se o atom estiver no trecho lido (até 16 MiB). Sem atom, duração 0
e pixels 0.

`PHFetchOptions` usa `predicate` (`evaluateWithObject:` / `valueForKey:`),
`sortDescriptors` (`creationDate`, `localIdentifier`, `pixelWidth`, `pixelHeight`,
`duration`), `fetchLimit` e `includeHiddenAssets`. O teste de host
(`tools/photostest.c`) cobre o índice, o filtro, a ordem, o limite, a pasta vazia
de erro e o mapa do TCC. Não cobre o `NSPredicate` do runtime ObjC nem a carga do
binário do Claude.
