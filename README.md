# mOS

**mOS** é um sistema operacional open source, escrito do zero, inspirado na experiência do Mac:
barra de menus, Dock com ampliação, janelas com "semáforos", modo claro/escuro, cores de destaque e
um visual de vidro translúcido no estilo do macOS Tahoe (o tema oficial do mOS). Ele dá boot direto de um pendrive em qualquer PC x86_64 com UEFI e
pode ser instalado num disco interno (SATA, NVMe ou outro pendrive).

> mOS não contém código, ícones, fontes ou imagens da Apple. Toda a arte é original e gerada
> vetorialmente pelo próprio sistema; a fonte é a Inter (SIL OFL). "Mac" e "macOS" são marcas da Apple Inc.;
> mOS não é afiliado nem endossado pela Apple.

## O que tem dentro

| Camada | Componentes |
|---|---|
| **Boot** | O kernel é um executável UEFI (`EFI/BOOT/BOOTX64.EFI`): escolhe o modo de vídeo (GOP), lê a ACPI, copia `\mOS\` do pendrive para a RAM e chama `ExitBootServices`. Sem GRUB, sem Limine. |
| **Kernel "Nucleo"** | GDT/TSS, IDT e exceções, PIC 8259, PIT 1 kHz, alocador físico por bitmap, paginação própria (identidade em páginas de 2 MiB, framebuffer write-combining, MMIO não-cacheável), heap, PCI, ACPI (desligar/reiniciar), RTC |
| **Processos** | Escalonador preemptivo, processos em ring 3 com syscalls (int 0x80) e loader ELF64, isolamento de falhas; LAPIC/SMP com os núcleos extras como operários de `par_for` |
| **Drivers** | Framebuffer, PS/2 (teclado ABNT2/US com acentos, mouse com roda), serial, Intel e1000 (8254x/8257x/I21x), RTL8139, Intel HD Audio e AC'97, xHCI (USB 2/3) com hubs, HID (teclado, mouse, tablet), armazenamento USB (leitura/escrita), Bluetooth HCI via USB, AHCI (SATA), NVMe |
| **Armazenamento** | FAT12/16/32 com leitura e escrita, GPT, persistência dos arquivos no pendrive/disco, montagem de imagens .dmg (HFS+/APFS, somente leitura), instalador |
| **Rede** | Ethernet, ARP, IPv4, ICMP, UDP, DHCP, DNS, TCP, HTTP/1.1 e TLS 1.3 (X25519, AES-GCM, ChaCha20-Poly1305) |
| **Som** | Intel HD Audio, mixer de 16 vozes, sintetizador (piano, órgão, sintetizador), sons do sistema originais |
| **Gráficos** | Compositor com regiões sujas, primitivas antialiasadas (SDF), sombras suaves, blur de vidro, rasterizador TrueType próprio com kerning GPOS |
| **Cupertino** | O design system: tokens claro/escuro, 8 cores de destaque, botões, checkbox, rádio, switch, slider, segmented control, pop-up, campos de texto com anel de foco, listas, barras de rolagem, sidebar |
| **Desktop** | Barra de menus com menus por app e itens de status, Dock com ampliação, indicadores e animação, papel de parede procedural "Dunas" (dia/noite), Busca (⌘Espaço) com calculadora, notificações, alertas |
| **Apps** | Arquivos, Horizonte (navegador com HTML/CSS/PNG/JPEG próprios), Terminal (shell `msh`, roda programas de usuário), Notas, Lembretes, Calendário, Contatos, Relógio, Mail (IMAP/SMTP), Mapas (OpenStreetMap), Tempo, Bolsa, Fotos, Pré-Visualização, Música, Gravador, QuickTime Player, Livros (EPUB), Freeform, Diário, Notas Autoadesivas, Senhas, Atalhos, Dicas, Dicionário, Editor de Texto, Editor de Script, Calculadora, Grapher, Livro de Fontes, Xadrez, Paciência, Campo Minado, Jogos, Monitor de Atividade, Console, Informações do Sistema, Utilitário de Disco, Medidor de Cor Digital, Captura de Tela, Acesso às Chaves, Configuração de Áudio e MIDI, Ajustes do Sistema, Instalar mOS, Componentes, Sobre este mOS — inventário em [docs/apps-sistema.md](docs/apps-sistema.md) |

## Compilar (Windows)

Requisitos: [Zig](https://ziglang.org/download/) (usado como compilador C, extraia em `.tools\`) e, para testar,
[QEMU](https://qemu.weilnetz.de/) em `C:\Program Files\qemu`.

```powershell
.\build.ps1          # gera build\mOS.img (GPT + FAT32) e build\esp\
.\build.ps1 -Run     # compila e abre no QEMU
.\tools\qemu-test.ps1 -Steps "wait:8000;shot:desktop"   # teste automatizado com screenshot
```

## Gravar no pendrive

**Opção 1 — imagem (recomendado):** grave `build\mOS.img` com o [Rufus](https://rufus.ie) (modo *DD*) ou o
[balenaEtcher](https://etcher.balena.io). Isso apaga o pendrive.

**Opção 2 — copiar arquivos:** formate o pendrive em FAT32 (esquema GPT ou MBR) e copie o conteúdo de
`build\esp\` (as pastas `EFI` e `mOS`) para a raiz.

Depois, no PC: entre no menu de boot do firmware (F12, F11, F9 ou Esc, conforme o fabricante), escolha
o pendrive **em modo UEFI** e desative o *Secure Boot* (o mOS ainda não é assinado).

### Primeiro boot: o assistente

O pendrive abre o **Assistente de Configuração e Instalação** em tela cheia, antes do login: idioma (pt-BR/English) e
teclado (ABNT2/US) valem antes de qualquer senha, e depois você escolhe:

- **Live (padrão):** nada é gravado em nenhum disco; a conta que você cria existe só na RAM e some ao desligar.
- **Live com persistência no pendrive (USB):** só depois de um consentimento explícito; grava contas, preferências e
  arquivos no próprio pendrive (somente na partição dele; outros discos nunca são tocados). Não há criptografia.
- **Instalar:** em um **disco inteiro** (apaga o disco) ou em uma **partição dedicada já existente** (tipo ESP, vazia ou só
  com um mOS anterior; a tabela de partições e as demais partições não são alteradas). Antes da primeira gravação você
  digita o identificador do alvo (ex.: `sata0` ou `sata0p2`). A cópia vem de um payload com manifesto de SHA-256 gerado no
  build, é verificada arquivo a arquivo, e o carregador EFI é o último a aparecer no destino. A conta e a senha são
  criadas no **primeiro boot do sistema instalado**, sem o pendrive. Não há editor de partições, redimensionamento nem
  ESP compartilhada; um alvo incompatível nunca vira "apagar o disco".

Se o mOS não conseguir identificar **com certeza** de qual disco/partição foi iniciado (caminho de dispositivo UEFI +
identidade física + identidade da partição), ele roda só na RAM e a persistência e a instalação ficam bloqueadas.
O firmware precisa entregar o caminho do dispositivo de boot; o desempate por "melhor pontuação" foi removido.

## Atalhos

| Atalho | Ação |
|---|---|
| ⌘ (tecla Windows) + Espaço ou Ctrl + Espaço | Busca |
| ⌘Q / ⌘W / ⌘M | Encerrar app / fechar janela / minimizar |
| ⌘Tab | Alternar apps |
| ⌘, | Preferências |
| ⌥⌘Esc | Forçar encerrar |

## Estado atual e limitações honestas

- Testado no QEMU (q35, OVMF). Som, USB, Bluetooth, SATA, NVMe e rede ainda não foram validados em hardware real;
  laptops específicos podem precisar de ajustes (ex.: amplificadores de áudio, Ethernet de outros fabricantes).
- Wi-Fi: pilha 802.11 com WPA2 Pessoal (CCMP), testada só com um rádio simulado. Placas reais dependem de driver
  e firmware por chipset (ver `kernel/wifi/`); WPA3 e redes corporativas não são suportados.
- Bluetooth: pareamento LE (Just Works e Secure Connections) e clássico (SSP), HID (teclado/mouse) e áudio A2DP.
  O QEMU não emula um adaptador, então esse código nunca rodou num adaptador de verdade.
- Navegador: motor JavaScript próprio (ES5 + ES6 e posteriores: classes, módulos import/export, geradores,
  async/await, `for await`, BigInt, Promise, DOM, eventos, timers, fetch e XMLHttpRequest); sem await no nível
  superior de módulos nem import maps. Flexbox/floats são aproximados. Páginas que alteram o DOM sem parar atualizam
  a ~4 quadros/s no QEMU.
- TLS 1.3 e 1.2 (ECDHE + AES-GCM/ChaCha20) com validação de certificados (cadeia até as raízes da Mozilla, datas
  e nome), RSA até 8192 bits e revogação por OCSP grampeado (sites que não grampeiam passam sem verificação de
  revogação; não há consulta OCSP online nem CRL); sem cifras CBC/RSA estático.
- A lógica da interface, a rede, o USB e o áudio rodam numa única thread; os programas de usuário são preemptivos
  entre si. O trabalho pesado de gráficos (composição, desfoque, vidro, sombras, cópia para o framebuffer) é
  dividido entre todos os núcleos (`par_for`, em `kernel/arch/smp.c`) e usa SSE2. Não há driver de GPU: a
  aceleração é de CPU multinúcleo. Num QEMU com 4 núcleos, um quadro em tela cheia fica ~2,3x mais rápido
  (imagens `build.ps1 -Tests` imprimem a medição no log serial: `bench:`).
- Volumes .dmg são somente leitura. Volumes FAT/HFS+ de outros discos aparecem em `/Volumes` **somente leitura por padrão,
  inclusive no Live**. Para gravar, o usuário consente **por volume**: cadeado ao lado do volume no app Arquivos → alerta
  (o botão padrão é "Cancelar"). O consentimento libera só a faixa daquele volume, vale até ejetá-lo, e é recusado para
  partições EFI, para o disco do sistema, para HFS+ com journal pendente e sempre que a origem do sistema não foi identificada.
- Sem gerador de números aleatórios de hardware (RDRAND/RDSEED) o mOS **não cria senhas** (o sal não viria de uma fonte
  adequada): a persistência no pendrive é recusada, o primeiro boot de um sistema instalado fica bloqueado com aviso, e o
  Live efêmero cria a sessão sem senha. Ter RDRAND/RDSEED **não** é uma validação de entropia; só indica que há uma fonte de hardware.
- O payload de instalação é só o sistema: o `mkimg` **rejeita** (o build falha e nenhuma imagem é gerada) um staging com contas,
  logs, preferências, estado, pastas pessoais ou resíduos de sessão. Os exemplos de fábrica (`root-live/mOS/Users`) entram na
  imagem apenas como overlay do Live, fora do payload e do manifesto; um sistema instalado não os recebe.
- A senha das contas controla a tela de início de sessão: **não há criptografia de disco nem permissões de arquivo**.
  Quem tiver acesso ao disco/pendrive lê os dados, e programas em execução enxergam os arquivos de contas.
- Instalação testada só no QEMU/OVMF (SATA, NVMe e USB); setor lógico diferente de 512 bytes (4Kn), Secure Boot e
  hardware real não foram validados. A instalação não grava entrada de boot na NVRAM: o disco entra pelo caminho
  `\EFI\BOOT\BOOTX64.EFI`, escolhido no menu de boot do firmware.

## Estrutura

```
kernel/boot   estágio UEFI          kernel/arch  CPU, interrupções, assembly
kernel/mm     memória               kernel/dev   drivers
kernel/net    rede                  kernel/gfx   gráficos e fontes
kernel/ui     window server, desktop, widgets, ícones, tema
kernel/apps   aplicativos           kernel/fs    imagens de disco (DMG/HFS+/APFS)
tools/        mkimg (gera a imagem GPT/FAT32), testes QEMU
root/mOS      arquivos copiados para o pendrive (pasta do usuário)
```

Licença: MIT (veja `LICENSE`). Fonte Inter: SIL OFL 1.1.
