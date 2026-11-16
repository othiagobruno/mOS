# Fontes dos ícones originais

Ferramenta: imagegen integrada, saída transparente. Originais preservados em `originais/`; exportações em `root/mOS/Sistema/Biblioteca/Icones/`. Os três originais são quadrados; o script usa Lanczos com alfa pré-multiplicado para exportar sem halos de redução. Não há imagem de referência de terceiros.

## Direção dos prompts finais

- Arquivos: squircle azul/ciano quase plano com uma única pasta frontal em vidro branco azulado translúcido; aba à esquerda, linhas retas e realce fino.
- Horizonte: squircle branco frio quase plano, disco azul com uma meridiana e duas latitudes em vidro branco fosco, ponteiro turquesa assimétrico. A pasta nova foi usada apenas como referência de estilo.
- Terminal: squircle grafite quase plano com prompt `>_` branco, traços finos e discreta refração azulada. As duas artes novas foram usadas apenas como referência de estilo.

Comum aos três prompts: um único ícone original do mOS, estética Liquid Glass do Tahoe, composição frontal ortográfica, camadas rasas e discretas, bordas finas, cantos contínuos, aproximadamente 84% de ocupação e margem externa transparente limpa. Excluídos cerâmica, marfim, molduras grossas, aparência inflada, brilho neon e perspectiva. Sem marcas nem reprodução de glifos da Apple.

Referência de linguagem visual: [apresentação oficial do macOS Tahoe](https://www.apple.com/newsroom/2025/06/macos-tahoe-26-makes-the-mac-more-capable-productive-and-intelligent-than-ever/), consultada como documentação; nenhuma imagem da Apple foi usada no gerador ou incluída no projeto.

Os prompts solicitaram tela de 1024, mas os três originais retornados pela ferramenta medem 1254 × 1254 px. A exportação de 2048 é interpolada.

A nomenclatura `nome-32@2x.png` indica 64 pixels físicos. Reexportar com `python3 tools/exportar-icones.py` usando Pillow. Os originais preservam a saída do gerador, incluindo sombras e bordas alfa.
