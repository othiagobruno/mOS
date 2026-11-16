# OpenH264 2.6.0 (Cisco, BSD-2) — só o decodificador

Origem: https://github.com/cisco/openh264, tag v2.6.0 (tarball de 10/02/2025). Licença em `LICENSE` (BSD de 2 cláusulas).
Copiados só `codec/api/wels`, `codec/common/{inc,src}` e `codec/decoder/{core,plus}/{inc,src}`; sem o codificador, sem
assembly (o build do mOS não define `X86_ASM`/`HAVE_NEON`: código C genérico, o mesmo resultado em x86_64 e arm64).

## Alterações do mOS (`mos.patch` contra o original)

O decodificador original não sai bit-exato em fluxos Main/High com quadros B. Corrigido aqui, provado contra o ffmpeg
(`tools/h264_test.sh`):

- `rec_mb.cpp` — B 16x8/8x16 bipreditos: o laço por lista fazia o MC da L1 por cima da L0 e, na segunda partição,
  somava duas vezes o deslocamento do destino; B_Bi_4x4 somava de novo o deslocamento do 8x8 no destino da L1; a
  predição ponderada explícita só era aplicada na segunda metade dos B_L0/L1 8x4 e 4x8.
- `deblocking.cpp` — B_Skip e B_Direct_16x16 recebiam bS interno 0 (regra do P_Skip), mas os vetores do direto mudam
  por 8x8: a aresta interna precisa do cálculo normal de B.
- `mv_pred.cpp` — direto espacial: a lista 1 do macrobloco colocalizado só era lida quando o tipo do MB tinha a flag
  L1; num quadro B (b-pyramid) um B_8x8 usa L1 em sub-blocos sem essa flag, e o colZeroFlag saía errado.
- `decoder.cpp` — o pool de quadros era `num_ref_frames + 2`, sem os quadros B que esperam a saída: com 6 referências e
  b-pyramid strict acabava a memória. Agora segue o DPB do padrão (max_dec_frame_buffering ou MaxDpbMbs do nível).
- `welsDecoderExt.{h,cpp}` — `m_bOutputDecodeOrder`: entrega cada quadro logo depois de decodificado (ordem de
  decodificação, como o VideoToolbox sem `kVTDecodeFrame_EnableTemporalProcessing`).

Não decodifica vídeo entrelaçado (PAFF/MBAFF) nem perfis High 10/4:2:2/4:4:4.
