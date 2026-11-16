# Ícones do mOS

## Entrega de 15/11/2026

Primeiro conjunto rasterizado, revisto com camadas de vidro sutis e menos volume: **Arquivos, Horizonte e Terminal**. Os outros apps e os símbolos de sistema continuam com arte vetorial existente; esta entrega não conclui toda a frente.

Arte original gerada com a ferramenta imagegen. Nenhum arquivo ou glifo da Apple foi usado como referência. Os originais ficam em `assets/icones/originais/`; os prompts e detalhes da exportação em `assets/icones/README.md`.

Cada um dos três ícones tem PNG RGBA em 16, 32, 64, 128, 256, 512 e 1024 px, mais as variantes @2x (32 a 2048 px): **42 arquivos**. `@2x` representa densidade; 2048 px é interpolado a partir do original gerado, não detalhe nativo adicional. Exportação reproduzível: `python3 tools/exportar-icones.py` (Pillow).

## Carregamento

- `kernel/ui/icons.c:icon_for()` mantém o cache por app, tamanho e tema, e preserva os ícones dos bundles externos.
- `kernel/ui/icons_tahoe.c:original_icon()` procura `/Sistema/Biblioteca/Icones/<exec>-<tamanho>.png`, escolhendo a menor resolução que cobre o pedido de até 256 px. Decodifica PNG, escala para a superfície e libera a imagem temporária. Arquivo ausente, inválido ou falha de alocação retorna ao desenho vetorial existente.
- As variantes de 512/1024 e @2x ficam disponíveis como arquivos, mas o cache atual de `icon_for()` limita a renderização a 256 px.
- `kernel/ui/desktop.c` usa `icon_for()` no Dock, Busca, grade de apps e alternador. `kernel/ui/wm.c` usa o mesmo carregador nas janelas.
- `tools/build.sh` copia `root/mOS/` para a imagem do sistema. A nova pasta entra nesse payload sem alterar o build.

## Inventário de apps

Tabela de `kernel/ui/icons_tahoe.c`, cruzada com `kernel/apps/registry.c` e `docs/apps-sistema.md`. Os callbacks próprios dos apps em `kernel/apps/` continuam disponíveis quando não há entrada no catálogo.

| Exec | Desenho vetorial existente | Arte usada nesta entrega |
|---|---|---|
| `arquivos` | `i_arquivos` | PNG novo |
| `horizonte` | `i_horizonte` | PNG novo |
| `terminal` | `i_terminal` | PNG novo |
| `notas` | `i_notas` | Vetor existente |
| `editor` | `i_editor` | Vetor existente |
| `calculadora` | `i_calc` | Vetor existente |
| `calendario` | `i_calendario` | Vetor existente |
| `musica` | `i_musica` | Vetor existente |
| `minas` | `i_minas` | Vetor existente |
| `visualizador` | `i_visualizador` | Vetor existente |
| `atividade` | `i_atividade` | Vetor existente |
| `console` | `i_console` | Vetor existente |
| `preferencias` | `i_prefs` | Vetor existente |
| `sobre` | `i_sobre` | Vetor existente |
| `componentes` | `i_componentes` | Vetor existente |
| `instalador` | `i_instalador` | Vetor existente |
| `lembretes` | `i_lembretes` | Vetor existente |
| `contatos` | `i_contatos` | Vetor existente |
| `relogio` | `i_relogio` | Vetor existente |
| `stickies` | `i_stickies` | Vetor existente |
| `xadrez` | `i_xadrez` | Vetor existente |
| `sysinfo` | `i_sysinfo` | Vetor existente |
| `disco` | `i_disco` | Vetor existente |
| `captura` | `i_captura` | Vetor existente |
| `medidor` | `i_medidor` | Vetor existente |
| `fontes` | `i_fontes` | Vetor existente |
| `grapher` | `i_grapher` | Vetor existente |
| `gravador` | `i_gravador` | Vetor existente |
| `fotos` | `i_fotos` | Vetor existente |
| `diario` | `i_diario` | Vetor existente |
| `dicas` | `i_dicas` | Vetor existente |
| `freeform` | `i_freeform` | Vetor existente |
| `tempo` | `i_tempo` | Vetor existente |
| `mapas` | `i_mapas` | Vetor existente |
| `bolsa` | `i_bolsa` | Vetor existente |
| `script` | `i_script` | Vetor existente |
| `senhas` | `i_senhas` | Vetor existente |
| `dicionario` | `i_dicionario` | Vetor existente |
| `chaves` | `i_chaves` | Vetor existente |
| `audiomidi` | `i_audiomidi` | Vetor existente |
| `paciencia` | `i_paciencia` | Vetor existente |
| `jogos` | `i_jogos` | Vetor existente |
| `atalhos` | `i_atalhos` | Vetor existente |
| `mail` | `i_mail` | Vetor existente |
| `livros` | `i_livros` | Vetor existente |
| `quicktime` | `i_quicktime` | Vetor existente |
| `automator` | `i_automator` | Vetor existente |
| `imagecapture` | `i_imagecapture` | Vetor existente |

## Pastas, arquivos, volumes e símbolos

Funções existentes em `kernel/ui/icons.c`; os ícones de pasta/documento/disco/lixeira e glifos monocromáticos ainda são vetoriais. Os glifos preservam cor, estado de rede/volume, direção e contraste do tema.

- `glyph_mos_logo`
- `glyph_search`
- `glyph_list`
- `glyph_wifi`
- `glyph_ethernet`
- `glyph_bluetooth`
- `glyph_speaker`
- `glyph_chevron`
- `glyph_plus`
- `glyph_reload`
- `glyph_check`
- `glyph_gear`
- `glyph_grid`
- `glyph_house`
- `glyph_clock`
- `icon_folder`
- `icon_document`
- `icon_disk`
- `icon_trash`
- `icon_arquivos`
- `icon_horizonte`
- `icon_terminal`
- `icon_prefs`
- `icon_editor`
- `icon_notas`
- `icon_calc`
- `icon_calendario`
- `icon_atividade`
- `icon_console`
- `icon_musica`
- `icon_minas`
- `icon_visualizador`
- `glyph_copy`
- `glyph_globe`
- `glyph_sb_recents`
- `glyph_sb_shared`
- `glyph_sb_desktop`
- `glyph_sb_document`
- `glyph_sb_download`
- `glyph_sb_cloud`
- `glyph_sb_home`
- `glyph_sb_trash`
- `glyph_sb_photo`
- `glyph_sb_music`
- `glyph_sb_apps`
- `glyph_sb_disk`
- `glyph_sidebar_toggle`

## Verificação e limites

Verificados os 42 PNGs (integridade, dimensões, RGBA, transparência e canto externo transparente), sintaxe de `kernel/ui/icons_tahoe.c` com Clang e `git diff --check`. Ainda não foi validada a renderização dentro do mOS, nem o comportamento visual no modo escuro. Nenhum build do projeto ou QEMU executado.
