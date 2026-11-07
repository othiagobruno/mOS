# mOS - servidor HTTP mínimo em [::1]:porta no host, para o teste de TCP sobre IPv6 (userland\tests-net6):
# o user-net do QEMU leva as conexões do convidado para [fec0::2]:porta até o ::1 do host.
#   powershell -File tools\testdata\http6-servidor.ps1 [-Porta 8086] [-Vezes 20]
param([int]$Porta = 8086, [int]$Vezes = 20)
$l = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::IPv6Loopback, $Porta)
$l.Start()
Write-Host "http6: ouvindo em [::1]:$Porta"
for ($i = 0; $i -lt $Vezes; $i++) {
    $c = $l.AcceptTcpClient()
    $s = $c.GetStream()
    $s.ReadTimeout = 3000
    $buf = New-Object byte[] 2048
    try { [void]$s.Read($buf, 0, $buf.Length) } catch {}
    $corpo = "ola do host via IPv6`n"
    $r = [System.Text.Encoding]::ASCII.GetBytes("HTTP/1.0 200 OK`r`nContent-Type: text/plain`r`nContent-Length: $($corpo.Length)`r`nConnection: close`r`n`r`n$corpo")
    $s.Write($r, 0, $r.Length)
    $c.Close()
    Write-Host "http6: atendeu $($c.Client.RemoteEndPoint)"
}
$l.Stop()
