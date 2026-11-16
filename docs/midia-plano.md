# Frente de mídia — plano e estado

Dona: Isabela (`isabela/*`). Escopo: AVFoundation com vídeo, VideoToolbox, CoreMedia, AAC/Opus, câmera e
microfone USB (UVC), ScreenCaptureKit e CoreImage. Cada linha diz o que está pronto **com teste**, e o que falta.
Licenças de terceiros em [midia-licencas.md](midia-licencas.md). Build completo só no fim, pela fila
`build-final.sh`; até lá, só testes de host (`tools/*_test.c`) e `zig cc -fsyntax-only`.

## Levantamento (15 nov. 2026, `main` 97c611b)

| Área | Já existia | Falta |
|---|---|---|
| Áudio PCM / MP3 / ALAC | CoreAudio, AudioQueue/Unit, AVAudioPlayer/Engine, minimp3, ALAC 16 bits | ALAC 20/24/32 bits |
| AAC | só reconhecido (ADTS, `mp4a`) | decodificar AAC-LC, HE-AAC v1/v2 (SBR/PS), xHE; codificar (AudioConverter, AVAssetWriter) |
| Opus | só reconhecido (Ogg, CAF) | decodificar e codificar (libopus) |
| Contêiner | `walk_mp4` mínimo (só stsz/ALAC) | demuxer ISO BMFF completo (MP4/MOV/M4A/fMP4), Matroska/WebM, Ogg, ADTS |
| H.264 | decodificador Baseline (h264bsd), codificador só I_PCM | Main/High (CABAC, B, 8x8, entrelaçado), codificador real (intra+P, CAVLC) |
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
| Codecs no libSystem x86_64 (`tools/build-midia.sh`, chamado pelo `tools/build.sh`) | ✅ objetos | 234 objetos freestanding; só dependem de libc que o libSystem já define; nenhum `.init_array`; exportações só `cx_opus_*`/`cx_aac_*`. O link completo fica para o build final |

**Pendências desta parte, registradas:** AAC em CAF não tem teste (o ffmpeg não grava; só o `afconvert`). HE-AAC
não tem teste (o ffmpeg daqui não tem codificador HE-AAC; entra junto do codificador FDK). O `tools/build-libsystem.ps1`
(Windows) e o libSystem arm64 ainda não ligam `audiodec.c` nem os codecs. A decodificação ainda é do arquivo inteiro
ao abrir; a leitura em fluxo entra com o AVPlayer de vídeo.

## H.264 Main/High: avaliação do OpenH264 2.6.0 (BSD-2)

Critério: decodificação H.264 é determinística, então o decodificador tem de sair **bit-exato** ao ffmpeg.
Harness de avaliação fora do repositório (Annex B → I420 → `cmp` contra `ffmpeg -f rawvideo`), 352x288, x264:

| Fluxo x264 | Resultado |
|---|---|
| Baseline | bit-exato |
| Main / High (padrão do x264, com B-frames) | **difere** só nos quadros B; I e P bit-exatos |
| B sem subpartições (`partitions=none`), CAVLC ou CABAC, i4x4, i8x8 | bit-exato |
| B com `b8x8` (direto espacial, temporal ou nenhum) ou com `p8x8` | **difere** |

Conclusão: o OpenH264 tem defeito na predição/reconstrução de macroblocos B com subpartições 8x8. Próximo passo:
corrigir no fonte vendido (com teste bit-exato) ou usar outro decodificador permissivo. Entrelaçado (PAFF/MBAFF)
o OpenH264 não decodifica; isso fica como item separado.
