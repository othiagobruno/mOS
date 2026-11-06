/* mOS — recursos embutidos no kernel (fontes Inter, licença SIL OFL 1.1) */
    .section .rdata,"dr"
    .balign 16
    .globl font_regular_ttf, font_regular_ttf_end
font_regular_ttf:
    .incbin "assets/fonts/Inter-Regular.ttf"
font_regular_ttf_end:
    .balign 16
    .globl font_medium_ttf, font_medium_ttf_end
font_medium_ttf:
    .incbin "assets/fonts/Inter-Medium.ttf"
font_medium_ttf_end:
    .balign 16
    .globl font_semibold_ttf, font_semibold_ttf_end
font_semibold_ttf:
    .incbin "assets/fonts/Inter-SemiBold.ttf"
font_semibold_ttf_end:
    .balign 16
    .globl font_bold_ttf, font_bold_ttf_end
font_bold_ttf:
    .incbin "assets/fonts/Inter-Bold.ttf"
font_bold_ttf_end:
