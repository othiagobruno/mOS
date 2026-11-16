# Artes originais dos 48 apps nativos

Geradas individualmente com imagegen integrada, fundo transparente; os três primeiros originais aprovados foram preservados. O catálogo `catalogo.json` corresponde integralmente ao registro estático de apps. Nenhuma imagem ou glifo da Apple foi incluído ou usado como entrada no gerador.

## Direção comum dos prompts

Um único ícone original do mOS por chamada. Linguagem visual Tahoe/Liquid Glass revisada pelo Thiago: desenho quase plano, frontal ortográfico, squircle de cantos contínuos centralizado em aproximadamente 84% da tela, formas simples reconhecíveis a 16 px, vidro fosco em camadas rasas, bordas finas e sombras mínimas. Margens externas transparentes. Excluídos molduras grossas, relevo inflado, cerâmica, perspectiva, aparência de brinquedo, marcas e reprodução de glifos da Apple. Tela solicitada de 1024; os originais retornados medem 1254 × 1254 px. Exportações de 2048 são interpoladas.

Referência de linguagem visual: [apresentação oficial do Tahoe](https://www.apple.com/newsroom/2025/06/macos-tahoe-26-makes-the-mac-more-capable-productive-and-intelligent-than-ever/), consultada como documentação. Somente artes próprias foram usadas como referências de estilo entre as gerações.

## Motivos pedidos por app

| Exec | Direção do motivo |
|---|---|
| `arquivos` | Pasta frontal única em vidro branco azulado sobre azul/ciano. |
| `horizonte` | Globo azul, grade de vidro branco e ponteiro turquesa sobre branco frio. |
| `terminal` | Prompt branco fino >_ sobre grafite. |
| `notas` | Cartão de nota fosco com três linhas, checkbox amarelo e faixa superior dourada. |
| `editor` | Documento branco com dobra e caneta azul diagonal. |
| `calculadora` | Painel prateado, visor vazio, quatro botões e igual laranja. |
| `calendario` | Calendário branco com cabeçalho coral, duas abas e número 15. |
| `musica` | Nota musical branca sobre coral/framboesa, disco translúcido discreto. |
| `minas` | Quatro células de vidro verde e bandeira coral. |
| `visualizador` | Cartões de paisagem com montanha, sol e lupa azul. |
| `atividade` | Painel grafite, pulso verde-menta e barras de desempenho. |
| `console` | Painel de log escuro com linhas claras, pontos ciano e alerta âmbar. |
| `preferencias` | Engrenagem prateada e controle deslizante azul sobre branco frio. |
| `sobre` | Símbolo de informação branco em círculo sobre azul. |
| `componentes` | Toggle azul, slider violeta e quadrado branco sobre lavanda. |
| `instalador` | Caixa de vidro âmbar aberta e seta de instalação azul. |
| `lembretes` | Lista branca com três círculos azul/coral/âmbar e uma marca de conclusão. |
| `contatos` | Cartão fosco com silhueta de pessoa e abas coloridas sobre areia. |
| `relogio` | Mostrador branco sem números, ponteiros finos e segundos coral sobre grafite. |
| `stickies` | Notas autoadesivas amarela e rosa com canto dobrado e dois traços. |
| `xadrez` | Cavalo grafite, peão branco fosco e três casas do tabuleiro. |
| `sysinfo` | Microchip grafite com selo de informação ciano. |
| `disco` | SSD prateado com disco ciano e pequena chave de manutenção azul. |
| `captura` | Quatro cantos de seleção azul, cartão translúcido e câmera branca sobre lavanda. |
| `medidor` | Círculos ciano/magenta/amarelo e conta-gotas grafite. |
| `fontes` | Cartão azul com Aa e página prateada atrás. |
| `grapher` | Painel de gráfico com eixos, senoide azul e parábola coral. |
| `gravador` | Sete barras de forma de onda coral e ponto de gravação sobre grafite. |
| `fotos` | Três cartões ciano/coral/amarelo com paisagem branca, sem roseta floral. |
| `diario` | Caderno lilás, marcador coral e brilho branco de quatro pontas. |
| `dicas` | Lâmpada branca com três raios sobre amarelo/âmbar. |
| `freeform` | Círculo ciano, quadrado lavanda, traço coral e nota amarela. |
| `tempo` | Nuvem branca fosca, sol amarelo e duas gotas azuis. |
| `mapas` | Blocos de mapa verdes, ruas brancas, rio azul e pin azul. |
| `bolsa` | Candlesticks claros e linha de mercado ascendente ciano sobre grafite. |
| `script` | Documento azul com chaves de código brancas e triângulo coral. |
| `senhas` | Chave branca e campo de três pontos sobre azul. |
| `dicionario` | Livro branco aberto com Aa e marcador âmbar sobre vinho. |
| `chaves` | Argola prateada com duas chaves foscas dourada e azul. |
| `audiomidi` | Mixer grafite com três sliders ciano e pequenas teclas brancas. |
| `paciencia` | Duas cartas brancas com coração coral e naipe preto sobre verde. |
| `jogos` | Controle branco fosco com cruz e dois botões sobre índigo/violeta. |
| `atalhos` | Dois elos arredondados coral/ciano com seta, sem losangos empilhados. |
| `mail` | Envelope branco com abas em V e pequeno cartão ciano atrás. |
| `livros` | Livro branco com linhas curtas e marcador coral sobre laranja. |
| `quicktime` | Disco azul de reprodução com play branco e ponto coral, sem letra Q. |
| `automator` | Três nós azuis em degraus ligados por setas, engrenagem no nó central. |
| `imagecapture` | Câmera grafite com lente azul, cartão de imagem e seta de importação. |

## Bases dinâmicas

As duas artes em `bases/` foram editadas com imagegen a partir dos próprios originais: Calendário sem o número 15 e Relógio sem ponteiros/pivô, preservando formas, cores e marcações. O exportador gera cinco resoluções de cada base. O carregador desenha a data e hora reais sobre elas; os 672 arquivos públicos continuam como amostras estáticas.

## Arquivos

- `originais/<exec>.png`: 48 originais RGBA preservados.
- `png/<exec>-<tamanho>[@2x].png`: 672 exportações públicas.
- `../../root/mOS/Sistema/Biblioteca/Icones/`: 250 arquivos pequenos para a imagem do sistema (240 variantes até 256 px e 10 bases dinâmicas), sem instalar as variantes grandes ou duplicações @2x.
- `../../tools/exportar-icones.py`: reexportação com Pillow e alfa pré-multiplicado.
- `../../tools/verificar-icones.py`: conferência de catálogo e arquivos; aceita o caminho do teste C para comparar a decodificação real do kernel com Pillow.

`nome-32@2x.png` tem 64 pixels físicos. O gerador preservou pequenas diferenças no relevo e na geometria; não se alega identidade de grade subpixel entre artes. Pastas e glifos de sistema permanecem vetoriais e não fazem parte desta entrega de apps.
