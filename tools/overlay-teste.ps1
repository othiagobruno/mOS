# mOS - overlay de teste do mkimg (--test-overlay), para os scripts que montam imagens de teste (tools\stage-*.ps1 e os
# tools\build-*.ps1 que geram imagem).
# Desde o PR #4, o mkimg recusa /mOS/Users e /mOS/Sistema/Biblioteca/Preferencias no staging (a esp é o payload que o
# instalador copia) e marca toda imagem como LIVE. No boot LIVE, o assistente de configuração abre em tela cheia
# (kernel/ui/setup.c) e recebe todos os cliques. A imagem de teste leva então um overlay à parte, fora do payload e do
# manifesto, com:
#   - sempre o marcador Preferencias\com.mos.teste.sem-assistente: o live abre direto na sessão do Visitante;
#   - o que o teste puser em <overlay>\mOS\Users\... ou <overlay>\mOS\Sistema\Biblioteca\Preferencias\... (o mkimg recusa
#     qualquer outra área, contas e logs).
# O build de release (build.ps1) nunca passa --test-overlay. Uso (dot-source):
#   . "$PSScriptRoot\overlay-teste.ps1"
#   $ovt = New-OverlayTeste $root $OutDir      # apaga e recria <OutDir>\overlay-teste: nada de uma rodada anterior sobra
#   & (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
# Um mkimg.exe de antes do --test-overlay recusa o argumento ("argumento desconhecido"): rode .\build.ps1 -OutDir <OutDir>.
function New-OverlayTeste([string]$Root, [string]$OutDir) {
    $ovt = Join-Path $Root "$OutDir\overlay-teste"
    if (Test-Path -LiteralPath $ovt) { Remove-Item -Recurse -Force -LiteralPath $ovt -ErrorAction SilentlyContinue }
    if (Test-Path -LiteralPath $ovt) { Write-Host "overlay de teste: $ovt não pôde ser apagado (arquivo em uso?) - mkimg FAILED" -ForegroundColor Red; exit 1 }
    $pref = "$ovt\mOS\Sistema\Biblioteca\Preferencias"
    New-Item -ItemType Directory -Force $pref | Out-Null
    [IO.File]::WriteAllText("$pref\com.mos.teste.sem-assistente", "imagem de teste: o live abre na sessao do Visitante, sem o assistente`n", [Text.Encoding]::ASCII)
    $ovt
}
