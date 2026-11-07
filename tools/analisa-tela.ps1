# mOS - analisa uma captura da tela (PNG do QMP) procurando a janela de um app Electron e se o conteúdo foi desenhado.
# Uso: .\tools\analisa-tela.ps1 caminho.png  ->  "janela=True|False conteudo=N cor=#rrggbb"
# janela: botão vermelho de fechar na barra de título (procura na faixa y 30..60 da metade esquerda).
# conteudo: pixels da área da janela que diferem da cor dominante (texto, ícones, barras); janela vazia dá ~0.
param([Parameter(Mandatory)][string]$Png)
Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Png))
try {
    $W = $bmp.Width; $H = $bmp.Height
    # botões da barra de título: vermelho/amarelo/verde (janela ativa) ou três cinzas iguais (inativa, ex.: com um
    # alerta na frente), 20 px um do outro, numa barra de título branca
    function Cinza($p) { $p.R -ge 190 -and $p.R -le 222 -and [Math]::Abs($p.R - $p.G) -lt 6 -and [Math]::Abs($p.G - $p.B) -lt 6 }
    $red = $null
    for ($y = 30; $y -le 60 -and -not $red; $y++) {
        for ($x = 4; $x -lt [int]($W / 2); $x++) {
            $p = $bmp.GetPixel($x, $y)
            $am = $bmp.GetPixel([Math]::Min($x + 20, $W - 1), $y); $vd = $bmp.GetPixel([Math]::Min($x + 40, $W - 1), $y)
            $fundo = $bmp.GetPixel([Math]::Max($x - 8, 0), $y)
            if ($p.R -gt 220 -and $p.G -lt 110 -and $p.B -lt 110) {
                if ($am.R -gt 200 -and $am.G -gt 150 -and $am.B -lt 120 -and $vd.G -gt 150 -and $vd.R -lt 150) { $red = @($x, $y) }
                break
            }
            if ((Cinza $p) -and (Cinza $am) -and (Cinza $vd) -and $fundo.R -ge 250 -and $fundo.G -ge 250 -and $fundo.B -ge 250) { $red = @($x, $y); break }
        }
    }
    if (-not $red) { "janela=False conteudo=0 cor=-"; return }
    # área de conteúdo: da barra de título para baixo; largura pelo fim da barra de título clara
    $x0 = $red[0] - 14
    # fim da barra de título (clara): primeira linha escura abaixo do botão, numa coluna livre de texto
    $y0 = $red[1] + 4
    while ($y0 -lt $red[1] + 60) { $p = $bmp.GetPixel($red[0] + 70, $y0); if ($p.R -lt 200 -or $p.G -lt 200 -or $p.B -lt 200) { break }; $y0++ }
    $y0 += 2
    $x1 = $x0
    while ($x1 -lt $W - 1) { $p = $bmp.GetPixel($x1, $red[1] - 8); if ($p.R -lt 200 -or $p.G -lt 200 -or $p.B -lt 200) { break }; $x1++ }
    if ($x1 - $x0 -lt 200) { $x1 = [Math]::Min($x0 + 1200, $W - 1) }
    $y1 = [Math]::Min($H - 100, $y0 + 760)     # acima do Dock
    # alertas/folhas por cima da janela: botões vermelho+amarelo dentro da área; o retângulo deles não conta
    $alertas = @()
    for ($y = $y0; $y -lt $y1; $y += 2) {
        for ($x = $x0 + 2; $x -lt $x1 - 42; $x += 2) {
            $p = $bmp.GetPixel($x, $y)
            if ($p.R -gt 220 -and $p.G -lt 110 -and $p.B -lt 110) {
                $am = $bmp.GetPixel($x + 20, $y)
                if ($am.R -gt 200 -and $am.G -gt 150 -and $am.B -lt 120) { $alertas += , @(($x - 24), ($y - 16), ($x + 440), ($y + 210)); $y += 210; break }
            }
        }
    }
    $hist = @{}
    $pts = New-Object System.Collections.Generic.List[int]
    for ($y = $y0; $y -lt $y1; $y += 3) {
        for ($x = $x0 + 2; $x -lt $x1 - 2; $x += 3) {
            $fora = $false
            foreach ($a in $alertas) { if ($x -ge $a[0] -and $x -le $a[2] -and $y -ge $a[1] -and $y -le $a[3]) { $fora = $true; break } }
            if ($fora) { continue }
            $c = $bmp.GetPixel($x, $y).ToArgb() -band 0xFFFFFF
            $pts.Add($c)
            $hist[$c] = 1 + [int]$hist[$c]
        }
    }
    $dom = ($hist.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1).Key
    $dr = ($dom -shr 16) -band 255; $dg = ($dom -shr 8) -band 255; $db = $dom -band 255
    $diff = 0
    foreach ($c in $pts) {
        $r = ($c -shr 16) -band 255; $g = ($c -shr 8) -band 255; $b = $c -band 255
        if ([Math]::Abs($r - $dr) -gt 24 -or [Math]::Abs($g - $dg) -gt 24 -or [Math]::Abs($b - $db) -gt 24) { $diff++ }
    }
    "janela=True conteudo=$diff cor=#{0:x6} area=({1},{2})-({3},{4}) cores={5} alertas={6}" -f $dom, $x0, $y0, $x1, $y1, $hist.Count, $alertas.Count
} finally { $bmp.Dispose() }
