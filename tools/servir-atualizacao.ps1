# mOS — servidor de atualizações (HTTP) para o atualizador do sistema (kernel/fs/update.c).
#
# Entrega o pacote gerado pelo build no MESMO layout de um pendrive do mOS:
#   GET /mOS/Sistema/Instalacao/manifesto.txt  -> <OutDir>\manifesto.txt  (o manifesto assinado que o mkimg gravou)
#   GET /mOS/Sistema/Biblioteca/chave-atualizacao.pub -> <OutDir>\chave-atualizacao.pub (o mkimg a põe no payload)
#   GET /<caminho>                             -> <OutDir>\esp\<caminho>   (EFI\BOOT\BOOTX64.EFI, mOS\...)
#
#   .\build.ps1                                # gera build\esp, build\manifesto.txt e build\mOS.img
#   .\tools\servir-atualizacao.ps1             # escuta em 127.0.0.1:8080 (no QEMU, o host é http://10.0.2.2:8080)
#   .\tools\servir-atualizacao.ps1 -Rede       # escuta em todas as interfaces (máquina real na rede local)
#
# No mOS: Ajustes > Geral > Atualização de Software > Procurar (ou, no Terminal, "atualizar procurar http://10.0.2.2:8080").
# O sistema só aceita o pacote se o manifesto estiver assinado pela mesma chave da imagem com que foi instalado.
# Servidor simples, um pedido por vez, só GET; sem TLS (a autenticidade vem da assinatura Ed25519 do manifesto).
param([string]$OutDir = 'build', [int]$Port = 8080, [switch]$Rede)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = if ([IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $root $OutDir }
$esp = Join-Path $out 'esp'
$man = Join-Path $out 'manifesto.txt'
$pub = Join-Path $out 'chave-atualizacao.pub'   # o mkimg a põe no payload; no staging ela não existe
if (-not (Test-Path $man) -or -not (Test-Path $esp)) { Write-Error "Pacote ausente: rode .\build.ps1 antes ($man e $esp)"; exit 1 }
$espFull = (Resolve-Path $esp).Path.TrimEnd('\')
$ver = ((Get-Content $man -TotalCount 1) -split ' ')[3]
$ansi = [Text.Encoding]::Default      # o mkimg grava os nomes do payload na página de código ANSI

function Decode([string]$p) {          # %XX -> bytes -> texto ANSI (o kernel codifica os bytes do manifesto)
    $bytes = New-Object System.Collections.Generic.List[byte]
    for ($i = 0; $i -lt $p.Length; $i++) {
        if ($p[$i] -eq '%' -and $i + 2 -lt $p.Length) { $bytes.Add([Convert]::ToByte($p.Substring($i + 1, 2), 16)); $i += 2 }
        else { foreach ($b in $ansi.GetBytes([string]$p[$i])) { $bytes.Add($b) } }
    }
    return $ansi.GetString($bytes.ToArray())
}

function Reply($stream, [int]$code, [string]$msg, [string]$body) {
    $b = [Text.Encoding]::UTF8.GetBytes($body)
    $h = [Text.Encoding]::ASCII.GetBytes("HTTP/1.1 $code $msg`r`nContent-Type: text/plain; charset=utf-8`r`nContent-Length: $($b.Length)`r`nConnection: close`r`n`r`n")
    $stream.Write($h, 0, $h.Length); $stream.Write($b, 0, $b.Length)
}

$addr = if ($Rede) { [Net.IPAddress]::Any } else { [Net.IPAddress]::Loopback }
$lst = New-Object Net.Sockets.TcpListener($addr, $Port)
$lst.Start()
Write-Host "Servindo a atualização $ver em http://$($addr):$Port (QEMU: http://10.0.2.2:$Port). Ctrl+C para parar." -ForegroundColor Green
try {
    while ($true) {
        $c = $lst.AcceptTcpClient()
        $s = $c.GetStream()
        $s.ReadTimeout = 15000
        try {
            # cabeçalho do pedido (até a linha em branco)
            $buf = New-Object byte[] 8192; $hdr = ''
            while ($hdr -notmatch "`r`n`r`n") {
                $n = $s.Read($buf, 0, $buf.Length)
                if ($n -le 0) { break }
                $hdr += [Text.Encoding]::ASCII.GetString($buf, 0, $n)
                if ($hdr.Length -gt 65536) { break }
            }
            $first = ($hdr -split "`r`n")[0]
            if ($first -notmatch '^(GET|HEAD) (\S+) HTTP/1\.[01]$') { Reply $s 400 'Bad Request' "pedido inválido`n"; continue }
            $method = $Matches[1]
            $path = Decode(($Matches[2] -split '\?')[0])
            if ($path -eq '/mOS/Sistema/Instalacao/manifesto.txt') { $file = $man }
            elseif ($path -eq '/mOS/Sistema/Biblioteca/chave-atualizacao.pub') { $file = $pub }
            elseif ($path -match '(^|/)\.\.(/|$)' -or $path -notmatch '^/') { Reply $s 403 'Forbidden' "caminho recusado`n"; continue }
            else { $file = Join-Path $espFull ($path.TrimStart('/').Replace('/', '\')) }
            $full = [IO.Path]::GetFullPath($file)
            if ($file -ne $man -and $file -ne $pub -and -not $full.StartsWith($espFull + '\', [StringComparison]::OrdinalIgnoreCase)) { Reply $s 403 'Forbidden' "caminho recusado`n"; continue }
            if (-not [IO.File]::Exists($full)) { Write-Host "404 $path" -ForegroundColor Yellow; Reply $s 404 'Not Found' "não existe: $path`n"; continue }
            $fs = [IO.File]::OpenRead($full)
            try {
                $h = [Text.Encoding]::ASCII.GetBytes("HTTP/1.1 200 OK`r`nContent-Type: application/octet-stream`r`nContent-Length: $($fs.Length)`r`nConnection: close`r`n`r`n")
                $s.Write($h, 0, $h.Length)
                if ($method -eq 'GET') { $fs.CopyTo($s, 1048576) }
                Write-Host ("200 {0} ({1:N0} KB)" -f $path, ($fs.Length / 1KB))
            } finally { $fs.Dispose() }
        } catch {
            Write-Host "erro: $($_.Exception.Message)" -ForegroundColor Red
        } finally {
            try { $s.Flush() } catch {}
            $c.Close()
        }
    }
} finally { $lst.Stop() }
