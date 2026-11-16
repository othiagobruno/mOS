# Frente de mídia — plano e estado

Dona: Isabela (`isabela/*`). Escopo: AVFoundation com vídeo, VideoToolbox, CoreMedia, AAC/Opus, câmera e
microfone USB (UVC), ScreenCaptureKit e CoreImage. Cada linha diz o que está pronto **com teste**, e o que falta.
Licenças de terceiros em [midia-licencas.md](midia-licencas.md). Build completo só no fim, pela fila
`build-final.sh`; até lá, só testes de host (`tools/*_test.c`) e `zig cc -fsyntax-only`.

## Levantamento (15 nov. 2026, `main` 97c611b)

| Área | Já existia | Falta |
|---|---|---|
| Áudio PCM / MP3 / ALAC | CoreAudio, AudioQueue/Unit, AVAudioPlayer/Engine, minimp3, ALAC 16/20/24/32 bits | — |
| AAC | só reconhecido (ADTS, `mp4a`) | decodificar AAC-LC, HE-AAC v1/v2 (SBR/PS), xHE; codificar (AudioConverter, AVAssetWriter) |
| Opus | só reconhecido (Ogg, CAF) | decodificar e codificar (libopus) |
| Contêiner | `walk_mp4` mínimo (só stsz/ALAC) | demuxer ISO BMFF completo (MP4/MOV/M4A/fMP4), Matroska/WebM, Ogg, ADTS |
| H.264 | decodificador Baseline/Main/High (OpenH264 corrigido, bit-exato), codificador OpenH264 (Baseline/Main/High, controle de taxa, IDR) | entrelaçado (PAFF/MBAFF), High 10/4:2:2, quadros B no codificador |
| HEVC | stub (-12906/-12908) | decodificador Main/Main10 |
| VP9 / AV1 | nada | libvpx (BSD) e dav1d (BSD-2) |
| AVPlayer vídeo | AVPlayer só áudio | demux + decodificação em linha de tempo, AVPlayerItemVideoOutput, AVPlayerLayer, AVSampleBufferDisplayLayer, sincronismo A/V |
| AVAsset/Reader/Writer | áudio; imagem por AVAssetImageGenerator parcial | faixas de vídeo, AVAssetReader de vídeo, AVAssetWriter MP4 com H.264/AAC, AVAssetExportSession |
| Câmera/mic USB | mic do HDA no anel; câmera sem driver | classe UVC (isócrono no xHCI, com a Larissa), USB Audio Class 1/2, AVCaptureDevice/Session/VideoDataOutput |
| ScreenCaptureKit | SCStream lê DEV_SCREEN_CAPTURE | SCShareableContent completo, filtros por janela/app, SCScreenshotManager, áudio do sistema |
| CoreImage | CPU com pixels (frameworks/CoreImage) | auditar filtros que os apps usam, CIContext com CVPixelBuffer |

## Ordem de entrega

1. **Áudio comprimido**: libopus (Ogg/CAF/MP4 `Opus`) e fdk-aac (ADTS, M4A, HE-AAC). Ligados ao `audiodec`.
2. **Demuxer ISO BMFF** completo (`mp4demux.c`), base para vídeo, AAC em M4A e AVAssetReader.
3. **AVPlayer com vídeo**: relógio de áudio como mestre, fila de quadros CVPixelBuffer, AVPlayerItemVideoOutput,
   AVPlayerLayer no CALayer do compositor.
4. **H.264 completo** (Main/High) e codificador real; depois VP9 (libvpx), AV1 (dav1d), HEVC.
5. **Matroska/WebM** e Ogg demux.
6. **AVAssetWriter/ExportSession** (MP4 mux).
7. **UVC + UAC** no kernel (dependência: transferência isócrona do xHCI da Larissa) e AVCapture*.
8. **ScreenCaptureKit** completo e CoreImage.

## Estado por entrega

Atualizado a cada PR. "Host" = teste de host em Linux; VM/hardware só depois do build final.

| Entrega | Estado | Teste |
|---|---|---|
| Plano | ✅ | — |
| Demuxer ISO BMFF `userland/libsystem/mp4demux.c` (MP4/MOV/M4A/fMP4, stbl completo, ctts, stss, stz2, co64, elst, trex/tfhd/tfdt/trun, esds, avcC/hvcC/dOps/vpcC/av1C/alac, `encv`/`enca` marcados como DRM) | ✅ | `tools/mp4demux_test.sh`: **11/11** arquivos idênticos ao `ffprobe` (posição, tamanho, pts e quadro-chave de cada pacote): H.264 com B-frames, faststart, fMP4, HEVC, VP9, AV1, Opus, ALAC, AAC, PCM em MOV, MJPEG |
| Opus (libopus 1.5.2, ponto fixo) em Ogg, MP4 e CAF | ✅ host | `tools/midia_audio_test.sh`: Ogg estéreo/mono, MP4 e CAF com atraso 0 e SNR 68–72 dB contra o ffmpeg |
| AAC (FDK AAC 2.0.3, C++ freestanding) em ADTS, M4A, MOV | ✅ host | mesmo teste: atraso 0, SNR 56–69 dB; alinhado com o sinal original (o limitador e a ocultação 2 do FDK atrasavam 15 ms e 1 quadro: desligados) |
| MP3 dentro de MP4 | ✅ host | mesmo teste: SNR 101 dB |
| ALAC 16/20/24/32 bits (reconstrução int32 com bytes deslocados) | ✅ host | mesmo teste: ALAC de 16 e de 24 bits **idênticos** ao ffmpeg (SNR 200 dB) |
| AudioConverter decodificando AAC/HE-AAC/MP3/Opus/ALAC → LPCM (`cx_pdec_*`, cookie `dmgc` em esds, ASC, OpusHead ou `alac`), com reamostragem | ✅ host | mesmo teste: decodificação por pacote igual à do arquivo inteiro (7/7); `AudioConverterFillComplexBuffer` com callback pacote a pacote, float32, ±0 LSB (6/6), reamostrado 48→44,1 e 44,1→48 kHz com correlação 1,00000; o erro do callback volta ao app como no macOS |
| Codecs no libSystem x86_64 (`tools/build-midia.sh`, chamado pelo `tools/build.sh`) | ✅ objetos | 234 objetos freestanding; só dependem de libc que o libSystem já define; nenhum `.init_array`; exportações só `cx_opus_*`/`cx_aac_*`. O link completo fica para o build final |

**Pendências desta parte, registradas:** AAC em CAF não tem teste (o ffmpeg não grava; só o `afconvert`). HE-AAC
não tem teste (o ffmpeg daqui não tem codificador HE-AAC; entra junto do codificador FDK). O `tools/build-libsystem.ps1`
(Windows) e o libSystem arm64 ainda não ligam `audiodec.c` nem os codecs. A decodificação ainda é do arquivo inteiro
ao abrir; a leitura em fluxo entra com o AVPlayer de vídeo.

## H.264 Main/High: OpenH264 2.6.0 (BSD-2) corrigido

Critério: decodificação H.264 é determinística, então o decodificador tem de sair **bit-exato** ao ffmpeg.
O OpenH264 original errava em quadros B (Main/High). Corrigido e vendorizado em `userland/third_party/openh264`
(só o decodificador, sem assembly; alterações e motivo de cada uma em `README.mos.md`, patch em `mos.patch`):
16x8/8x16 bipreditos, sub-blocos 8x4/4x8/4x4, bS interno de B_Skip/B_Direct, L1 do colocalizado num quadro B,
pool de quadros pelo DPB do nível e saída em ordem de decodificação (para o VideoToolbox).

| Teste | Resultado |
|---|---|
| `tools/h264_test.sh`: 20 fluxos do x264 (Baseline, Main, High, CAVLC e CABAC, b-pyramid normal/strict, direto espacial e temporal, ponderada P e B, 6 referências, b-adapt 2 com 8 B, slices, sem deblocking, QP 10 e 45, recorte 426x238, 720p) | **20/20 bit-exatos** ao ffmpeg; na ordem de decodificação saem os mesmos quadros |
| Fora do repo (harness em Agents/isabela/h264-harness): os mesmos + 1080p e conteúdo `life`/`mandelbrot` | 24/24 bit-exatos |
| Entrelaçado (PAFF/MBAFF) | o OpenH264 não decodifica: falha limpa (0 quadros); item separado |

Integrado: `tools/build-midia.sh` compila o OpenH264 nos dois alvos (#494), o `VTDecompressionSession` usa a casca
`cx_h264_*` (#499) e o h264bsd saiu.

## Vídeo no AVFoundation: `avvideo.c`

Núcleo em C (testável no host): abre MP4/MOV (mp4demux), faixa H.264, e entrega o quadro exibido em qualquer tempo t
(o último com pts <= t), decodificando adiante ou recomeçando no quadro-chave quando o tempo volta ou salta; converte
para BGRA com a matriz e a faixa do VUI, gira pelo tkhd (preferredTransform) e reduz por média de área.
`tools/h264_test.sh` (avvideo_*): todos os quadros em sequência, entre quadros, 200 acessos aleatórios e bordas
iguais ao ffmpeg, em MP4 com B e GOP curto, MOV com áudio e 720p; BGRA contra a fórmula, rotação e redução exatas.
Usado por: `AVAssetImageGenerator` (quadro real, actualTime, appliesPreferredTrackTransform, maximumSize).
Próximo: AVAssetTrack de vídeo, AVPlayerItemVideoOutput, AVPlayerLayer e o relógio do AVPlayer sem áudio.