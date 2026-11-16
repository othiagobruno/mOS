# PhotosUI do Mac Catalyst no mOS

Apps Catalyst (o WhatsApp é um) ligam `/System/iOSSupport/System/Library/Frameworks/PhotosUI.framework` (classes ObjC)
e `/System/iOSSupport/usr/lib/swift/libswiftPhotosUI.dylib` (as structs Swift). As duas partes são reimplementação
própria, com os nomes do SDK da Apple. Nada de código ou arte da Apple.

## Onde está

- `userland/frameworks/PhotosUI/PhotosUI.m` (compilado por `tools/build-frameworks-objc.sh`; `install-path` dá o caminho do
  Catalyst):
  - `PHPickerFilter`: imagens, vídeos, Live Photos, panoramas, capturas de tela, câmera lenta, timelapse, profundidade,
    cinematográfico, espacial; `any`/`all`/`not` em árvore. Rajadas e gravações de tela não casam com nada: a
    biblioteca do mOS não guarda essas marcas.
  - `PHPickerConfiguration` (limite, seleção default/ordered/continuous/continuousAndOrdered, filtro, pré-seleção,
    modo de representação, `mode`, capacidades desligadas, margens), `PHPickerResult`.
  - `PHPickerViewController : UIViewController`: grade própria da biblioteca do `Photos.framework` do mOS
    (`~/Imagens`, docs/photos.md), miniatura pelo ImageIO, duração nos vídeos, selo com número na seleção ordenada,
    "Cancelar" (lista vazia ao delegate) e "Adicionar". Com limite 1 e seleção não contínua, o toque já conclui; na
    seleção contínua cada toque entrega. Sem `PHPhotoLibrary` no init, o resultado não leva `assetIdentifier`, como na Apple.
    `deselectAssetsWithIdentifiers:`, `moveAssetWithIdentifier:afterAssetWithIdentifier:`, `scrollToInitialPosition`,
    `updatePickerUsingConfiguration:`.
  - `_PHPickerItemProvider : NSItemProvider` de arquivo: tipos pelo UTI da extensão (conformidade com `public.image`,
    `public.movie`, `public.data`, `public.item`), `loadDataRepresentation…`, `loadFileRepresentation…` (cópia em `/tmp`,
    apagada quando o handler volta), `loadInPlaceFileRepresentation…`, `loadItemForTypeIdentifier…`,
    `loadObjectOfClass:` (`UIImage`, `NSURL`), `suggestedName`.
  - `PHLivePhotoView`: propriedades e delegate. A biblioteca do mOS não guarda o par HEIC + MOV, então não há Live Photo
    para tocar; `startPlaybackWithStyle:` avisa início e fim ao delegate.
- Overlay Swift `userland/sdk/swift-objc/PhotosUI/PhotosUI.swift` sobre o módulo clang `userland/sdk/swift-objc/sdk/PhotosUI`
  (classes `NS_REFINED_FOR_SWIFT`, como na Apple): `PHPickerFilter`, `PHPickerConfiguration` (+ `AssetRepresentationMode`,
  `Selection`), `PHPickerMode`, `PHPickerResult`, o protocolo Swift `PHPickerViewControllerDelegate` (ponte para o ObjC
  por objeto associado; o delegate do app fica fraco) e `PHPickerViewController.configuration`/`init(configuration:)`/
  `delegate`. Compilado e ligado por `tools/build-swift-uikit-objc.sh` (dois níveis com o libswiftCore real).

## Teste

`tools/test-photosui.sh` (`userland/tests-swiftobjc/photosui/main.swift`): raiz própria com três PNG em
`/Users/Visitante/Imagens` e o `kTCCServicePhotos` concedido; **photosui 18/18** no `build/machotest` (filtros,
configuração ida e volta, delegate, toques, ordem, limite, cancelar, pré-seleção, contínua, NSItemProvider).

## Medida

- WhatsApp 2.26.45.13: os 32 símbolos Swift de PhotosUI que ele importa existem no `libswiftPhotosUI.dylib` com o mesmo
  nome mangled; os 27 `$s8PhotosUI…` de `sys_zerados.c` saíram.
- Falta: o `PhotosUI.framework` do macOS (`/System/Library/Frameworks`, `PHPickerViewController : NSViewController`) para
  apps AppKit; o quadro de vídeo na miniatura (AVFoundation, frente da Isabela).
