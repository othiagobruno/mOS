# Apps do sistema (equivalentes aos do macOS Tahoe)

Os apps nativos do mOS são escritos em C dentro do kernel (`kernel/apps/`), com o toolkit Cupertino
(`kernel/ui/`) e utilidades comuns em [`kernel/apps/kit.h`](../kernel/apps/kit.h): datas, arquivos TSV em
`~/Biblioteca/<App>`, leitor de JSON, base64, BMP, WAV e a área de texto reutilizável (`textarea_*`, o motor do
Editor de Texto). O objetivo é cobrir todos os apps do macOS que **não exigem conta Apple**, com a aparência do Tahoe:
janelas unificadas, barra lateral de vidro, cápsulas, folhas, popovers e modo escuro.

Para registrar um app: acrescente `&app_x` em `all_apps[]` ([registry.c](../kernel/apps/registry.c)) e o `extern` em
[apps.h](../kernel/include/apps.h). `app_t.always_tick` faz o `tick` rodar com o app fechado (alarmes, lembretes,
alertas do Calendário, Mail).

## Inventário

| macOS | mOS | O que funciona | Dados |
|---|---|---|---|
| Finder | Arquivos | (existente) | — |
| Safari | Horizonte | (existente) | — |
| Terminal | Terminal | (existente) | — |
| TextEdit | Editor de Texto | texto simples, desfazer, salvar | arquivos do usuário |
| Notas | Notas | pastas, fixar, busca em todas, listas (○/●, ⇧⌘L, ⇧⌘U), Apagadas Recentemente, salvamento automático | `~/Documentos/Notas/*.txt` |
| Lembretes | Lembretes | listas com cor, listas inteligentes (Hoje, Agendados, Todos, Sinalizados, Concluídos), data/hora com aviso, prioridade, notas, busca, selo no Dock | `~/Biblioteca/Lembretes/dados.tsv` |
| Calendário | Calendário | Dia/Semana/Mês/Ano, eventos com repetição e alerta, vários calendários, feriados do Brasil (inclusive móveis), aniversários dos Contatos, busca, iCalendar | `~/Biblioteca/Calendário/eventos.ics` |
| Contatos | Contatos | grupos, busca, cartão com edição, aniversário, exportar/importar vCard 3.0 | `~/Biblioteca/Contatos/contatos.vcf` |
| Relógio | Relógio | Relógio Mundial (36 cidades), Alarmes (tocam com o app fechado, adiar), Cronômetro com voltas, Timer | `~/Biblioteca/Relógio/dados.tsv` |
| Calculadora | Calculadora | Básica, Científica (graus/rad, 2nd, memória), Programador (bases, bits), histórico | — |
| Mail | Mail | IMAP com TLS (993), SMTP 465/587 (STARTTLS), pastas, MIME (multipart, base64, QP, RFC 2047, charsets), responder, encaminhar, apagar, cache local | `~/Biblioteca/Mail` |
| Mapas | Mapas | tiles do OpenStreetMap, busca (Nominatim), favoritos, rotas de carro/a pé/bicicleta (OSRM) | `~/Biblioteca/Mapas/favoritos.tsv` |
| Tempo | Tempo | Open-Meteo: atual, por hora, 10 dias, detalhes, busca de cidades | `~/Biblioteca/Tempo/locais.tsv` |
| Bolsa | Bolsa | cotações diárias (Stooq, Yahoo como alternativa), gráfico com períodos, estatísticas | `~/Biblioteca/Bolsa/lista.tsv` |
| Fotos | Fotos | biblioteca (Imagens, Mesa, Downloads), favoritos, álbuns, visualizador, edição (luz, contraste, cor, filtros, girar) | `~/Biblioteca/Fotos/dados.tsv` |
| Pré-Visualização | Pré-Visualização | PNG/JPEG/GIF/BMP/PPM, miniaturas da pasta, zoom, girar, espelhar, marcação, ajustar tamanho, exportar BMP | — |
| Música | Música | biblioteca WAV de `~/Música` (álbuns por pasta), playlists, aleatório/repetir, toca com a janela fechada; faixas de demonstração sintetizadas | `~/Biblioteca/Música/playlists.tsv` |
| Gravador | Gravador | grava do microfone, forma de onda, ±15 s, renomear, melhorar (normalizar) | `~/Música/Gravações/*.wav` |
| QuickTime Player | QuickTime Player | gravação de tela (GIF animado), gravação de áudio (WAV), reprodução de GIF e WAV | `~/Filmes` |
| Livros | Livros | EPUB (ZIP + OPF + XHTML) e TXT, duas colunas, temas, tamanho de fonte, sumário, posição lembrada | `~/Livros`, `~/Biblioteca/Livros` |
| Freeform | Freeform | quadros com notas, formas, texto, caneta e setas; mover, redimensionar, cores, zoom | `~/Biblioteca/Freeform/*.freeform` |
| Diário | Diário | entradas, sugestões de escrita, marcadores, busca, resumo (sequência, palavras) | `~/Biblioteca/Diário/entradas.tsv` |
| Notas Autoadesivas | Notas Autoadesivas | notas coloridas soltas na mesa, posição e cor lembradas | `~/Biblioteca/Notas Autoadesivas/notas.tsv` |
| Senhas | Senhas | cofre ChaCha20-Poly1305 com chave derivada da senha da conta, códigos TOTP, gerador, alertas de senhas fracas/repetidas | `~/Biblioteca/Senhas/cofre.bin` |
| Atalhos | Atalhos | galeria, editor de ações (abrir app/URL, notificação, esperar, volume, aparência, nota, lembrete, copiar, som, captura) | `~/Biblioteca/Atalhos/atalhos.tsv` |
| Dicas | Dicas | coleções com "Experimentar" | — |
| Dicionário | Dicionário | Wikcionário (pt/en) e Wikipédia, histórico | — |
| Xadrez | Xadrez | regras completas, IA alfa-beta em 3 níveis, notação algébrica, desfazer | — |
| Jogos | Jogos, Paciência | biblioteca de jogos; Paciência Klondike (virar 1/3, desfazer, pontos) | — |
| Livro de Fontes | Livro de Fontes | amostra, repertório de glifos, texto personalizado, métricas | — |
| Grapher | Grapher | até 6 funções y = f(x), arrastar, zoom, coordenadas sob o cursor | — |
| Editor de Script | Editor de Script | JavaScript no motor do sistema, resultado e mensagens | `~/Documentos/Scripts` |
| Utilitários | Monitor de Atividade, Console, Informações do Sistema, Utilitário de Disco, Medidor de Cor Digital, Captura de Tela, Acesso às Chaves, Configuração de Áudio e MIDI | dados reais do hardware, do log do kernel, dos discos e das 118 raízes confiáveis do TLS | — |

Serviços de rede usados (todos públicos, sem conta): Open-Meteo, OpenStreetMap (tiles e Nominatim), OSRM,
Stooq/Yahoo Finance e Wikimedia (Wikcionário/Wikipédia). Os apps mostram a atribuição exigida por cada um.

## Fora do escopo ou com limitação

- **Exigem conta Apple**: App Store, FaceTime, Mensagens (iMessage), Buscar, Casa, TV, News, Espelhamento do iPhone,
  Telefone, Image Playground. Não serão feitos.
- **Photo Booth** e **Captura de Imagem**: o mOS ainda não tem driver de câmera/scanner (UVC exige transferências
  isócronas no xHCI).
- **Podcasts**: falta um decodificador de MP3; a Música e o QuickTime tocam só WAV PCM de 16 bits.
- **Automator**: coberto pelo Atalhos.
- No QEMU (TCG) o sistema roda abaixo do tempo real; a gravação de tela usa 1/4 da resolução a 2 quadros/s.

## Como testar

```powershell
.\build.ps1 -OutDir build-apps
.\tools\qemu-test.ps1 -OutDir build-apps -Steps "wait:24000;key:meta_l+spc;wait:800;type:Lembretes;key:ret;wait:2500;shot:lembretes"
```

A Busca é ⌘Espaço (`key:meta_l+spc`); `ctrl+spc` troca a fonte de entrada. O estado dos apps fica gravado no
`mOS.img`: recrie a imagem (`build-apps\mkimg.exe build-apps\mOS.img build-apps\esp 128`) para começar do zero.
