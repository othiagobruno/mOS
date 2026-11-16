# Fontes dos ícones originais

Ferramenta: imagegen integrada, saída transparente. Originais preservados em `originais/`; exportações em `root/mOS/Sistema/Biblioteca/Icones/`. Os três originais são quadrados; o script usa Lanczos com alfa pré-multiplicado para exportar sem halos de redução. Não há imagem de referência de terceiros.

## Direção dos prompts

- Arquivos: tile azul cerâmico com divisórias de arquivo em marfim e abas ciano; sem rosto, letras ou marcas.
- Horizonte: tile azul-petróleo com arco em marfim, sol coral, camadas de horizonte turquesa e arco orbital prateado; sem bússola, letras ou marcas.
- Terminal: tile grafite com chevron em fita verde-menta e cursor marfim; sem letras ou marcas.

Comum aos três: um ícone centralizado, fundo externo transparente, quadrado com cantos contínuos ocupando aproximadamente 84% da tela, vista frontal, luz suave superior, relevo discreto e formas reconhecíveis em tamanho pequeno. A ferramenta foi instruída a produzir arte original sem reproduzir ícones da Apple. Os prompts solicitaram tela de 1024, mas os originais retornados pela ferramenta medem 1254 × 1254 px. A exportação de 2048 é interpolada.

A nomenclatura `nome-32@2x.png` indica 64 pixels físicos. Reexportar com `python3 tools/exportar-icones.py` usando Pillow. Os originais preservam a saída do gerador, incluindo sombras e bordas alfa.
