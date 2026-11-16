# Ícones dos apps nativos do mOS

## Catálogo completo

**48 de 48 apps nativos** de `kernel/apps/registry.c:all_apps[]` têm arte original rasterizada no estilo revisado: vista frontal, cantos contínuos, formas simples, camadas rasas de vidro e realces discretos. Esta entrega acrescenta os 45 que faltavam aos três primeiros do PR #265.

`assets/icones/catalogo.json` lista os nomes de execução; há um original RGBA por app em `assets/icones/originais/`. A verificação exige igualdade entre catálogo, registro, tabela de carregamento e arquivos originais, e rejeita originais repetidos.

Cada app tem PNG RGBA em **16, 32, 64, 128, 256, 512 e 1024 px**, mais as respectivas variantes **@2x** (32 a 2048 px): **672 PNGs**. 2048 px é interpolado dos originais gerados, sem alegação de detalhe nativo adicional.

## Carregamento no sistema

`kernel/ui/icons_tahoe.c:tahoe_icon()` verifica o nome na tabela de apps nativos e então carrega `/Sistema/Biblioteca/Icones/<exec>-<tamanho>.png`. A menor variante entre 16 e 256 px que cobre o pedido é decodificada e escalada para a superfície. A checagem de cabeçalho limita dimensões e alocação; a imagem temporária é liberada após o desenho. Arquivo ausente, inválido ou falha de alocação mantém o desenho vetorial existente.

`kernel/ui/icons.c:icon_for()` preserva os ícones dos bundles externos e mantém o cache por app, tamanho e tema. O catálogo nativo já contém sua placa e margens: o carregador não coloca uma segunda placa em torno desses ícones, mesmo quando os cantos da arte são transparentes. O tratamento existente do modo escuro continua aplicado após o desenho.

Dock, Busca, grade de apps e alternador em `kernel/ui/desktop.c`, e janelas em `kernel/ui/wm.c`, usam o mesmo `icon_for()`. As variantes de 512/1024 e @2x são entregáveis; o limite atual desse cache de renderização é 256 px. `tools/build.sh` copia `root/mOS/` para o payload sem necessidade de alterar o build.

## Inventário dos apps

| App nativo | Exec / nome dos PNGs | Vetor de recuperação |
|---|---|---|
| Arquivos | `arquivos` | `i_arquivos` |
| Horizonte | `horizonte` | `i_horizonte` |
| Terminal | `terminal` | `i_terminal` |
| Notas | `notas` | `i_notas` |
| Editor de Texto | `editor` | `i_editor` |
| Calculadora | `calculadora` | `i_calc` |
| Calendário | `calendario` | `i_calendario` |
| Música | `musica` | `i_musica` |
| Campo Minado | `minas` | `i_minas` |
| Pré-Visualização | `visualizador` | `i_visualizador` |
| Monitor de Atividade | `atividade` | `i_atividade` |
| Console | `console` | `i_console` |
| Ajustes do Sistema | `preferencias` | `i_prefs` |
| Sobre este mOS | `sobre` | `i_sobre` |
| Componentes | `componentes` | `i_componentes` |
| Instalar mOS | `instalador` | `i_instalador` |
| Lembretes | `lembretes` | `i_lembretes` |
| Contatos | `contatos` | `i_contatos` |
| Relógio | `relogio` | `i_relogio` |
| Notas Autoadesivas | `stickies` | `i_stickies` |
| Xadrez | `xadrez` | `i_xadrez` |
| Informações do Sistema | `sysinfo` | `i_sysinfo` |
| Utilitário de Disco | `disco` | `i_disco` |
| Captura de Tela | `captura` | `i_captura` |
| Medidor de Cor Digital | `medidor` | `i_medidor` |
| Livro de Fontes | `fontes` | `i_fontes` |
| Grapher | `grapher` | `i_grapher` |
| Gravador | `gravador` | `i_gravador` |
| Fotos | `fotos` | `i_fotos` |
| Diário | `diario` | `i_diario` |
| Dicas | `dicas` | `i_dicas` |
| Freeform | `freeform` | `i_freeform` |
| Tempo | `tempo` | `i_tempo` |
| Mapas | `mapas` | `i_mapas` |
| Bolsa | `bolsa` | `i_bolsa` |
| Editor de Script | `script` | `i_script` |
| Senhas | `senhas` | `i_senhas` |
| Dicionário | `dicionario` | `i_dicionario` |
| Acesso às Chaves | `chaves` | `i_chaves` |
| Configuração de Áudio e MIDI | `audiomidi` | `i_audiomidi` |
| Paciência | `paciencia` | `i_paciencia` |
| Jogos | `jogos` | `i_jogos` |
| Atalhos | `atalhos` | `i_atalhos` |
| Mail | `mail` | `i_mail` |
| Livros | `livros` | `i_livros` |
| QuickTime Player | `quicktime` | `i_quicktime` |
| Automator | `automator` | `i_automator` |
| Captura de Imagem | `imagecapture` | `i_imagecapture` |

## Pastas, documentos e símbolos de sistema

O escopo desta entrega é o conjunto completo de apps nativos. As funções abaixo de `kernel/ui/icons.c` continuam com os desenhos vetoriais existentes para pastas, documentos, discos, lixeira e símbolos dinâmicos. A frente de ícones de sistema continua separadamente pendente; a entrega dos 48 apps não é a conclusão de toda a iconografia do sistema.

`glyph_mos_logo`, `glyph_search`, `glyph_list`, `glyph_wifi`, `glyph_ethernet`, `glyph_bluetooth`, `glyph_speaker`, `glyph_chevron`, `glyph_plus`, `glyph_reload`, `glyph_check`, `glyph_gear`, `glyph_grid`, `glyph_house`, `glyph_clock`, `icon_folder`, `icon_document`, `icon_disk`, `icon_trash`, `icon_arquivos`, `icon_horizonte`, `icon_terminal`, `icon_prefs`, `icon_editor`, `icon_notas`, `icon_calc`, `icon_calendario`, `icon_atividade`, `icon_console`, `icon_musica`, `icon_minas`, `icon_visualizador`, `glyph_copy`, `glyph_globe`, `glyph_sb_recents`, `glyph_sb_shared`, `glyph_sb_desktop`, `glyph_sb_document`, `glyph_sb_download`, `glyph_sb_cloud`, `glyph_sb_home`, `glyph_sb_trash`, `glyph_sb_photo`, `glyph_sb_music`, `glyph_sb_apps`, `glyph_sb_disk`, `glyph_sidebar_toggle`

## Reprodução e verificação

```sh
python3 tools/exportar-icones.py
python3 tools/verificar-icones.py
cc -std=c11 -ffreestanding -ffunction-sections -fdata-sections -Ikernel/include tools/test-icones-png.c -Wl,--gc-sections -o /tmp/mos-icon-png
python3 tools/verificar-icones.py /tmp/mos-icon-png
clang -std=c11 -ffreestanding -fsyntax-only -Ikernel/include kernel/ui/icons.c kernel/ui/icons_tahoe.c
git diff --check
```

O exportador/verificador exige Pillow; reduz com alfa pré-multiplicado e Lanczos. Foram conferidos cobertura, originais distintos, integridade, dimensões, RGBA, transparência, cantos externos com alfa até 8/255 e limite de arquivo do carregador nos 672 PNGs. Os **240 PNGs usados em runtime** tiveram dimensões e hash de todos os pixels pré-multiplicados comparados entre o decodificador PNG real do kernel e Pillow. Todos coincidiram. As duas unidades C alteradas passaram na checagem de sintaxe.

Nenhum build do projeto ou QEMU foi executado. Renderização dentro do mOS, comportamento da recuperação vetorial no sistema em execução e aparência efetiva no modo escuro ainda não foram validados em uma sessão do sistema.
