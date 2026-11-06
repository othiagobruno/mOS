# mOS - gera tools/testdata/ocsp/: uma CA de teste, um certificado de servidor
# (mock.mos.test), responders OCSP delegados e respostas OCSP assinadas (good,
# revoked, unknown, delegadas, falsas...) para os testes offline de tools/tlstest.c.
# Usa só o .NET do Windows (CertificateRequest + RSA); o DER do OCSP é montado à mão.
#   powershell -ExecutionPolicy Bypass -File tools\gen-ocsp-testdata.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'tools\testdata\ocsp'
New-Item -ItemType Directory -Force $out | Out-Null
Add-Type -AssemblyName System.Security
$X = 'System.Security.Cryptography.X509Certificates'
$sha256 = [System.Security.Cryptography.HashAlgorithmName]::SHA256
$pkcs1 = [System.Security.Cryptography.RSASignaturePadding]::Pkcs1

# ---------------------------------------------------------------- DER
function DerLen([int]$n) {
    if ($n -lt 0x80) { return [byte[]]@($n) }
    if ($n -lt 0x100) { return [byte[]]@(0x81, $n) }
    if ($n -lt 0x10000) { return [byte[]]@(0x82, ($n -shr 8), ($n -band 0xff)) }
    return [byte[]]@(0x83, ($n -shr 16), (($n -shr 8) -band 0xff), ($n -band 0xff))
}
function Tlv([int]$tag, [byte[]]$v) { if ($null -eq $v) { $v = [byte[]]@() }; [byte[]](@([byte]$tag) + (DerLen $v.Length) + $v) }
function Concat { $r = New-Object System.Collections.Generic.List[byte]; foreach ($a in $args) { if ($null -ne $a) { $r.AddRange([byte[]]$a) } }; ,$r.ToArray() }
function Seq { Tlv 0x30 (Concat @args) }
function GTime([datetime]$d) { Tlv 0x18 ([Text.Encoding]::ASCII.GetBytes($d.ToString('yyyyMMddHHmmss') + 'Z')) }
function Oid([byte[]]$b) { Tlv 0x06 $b }
function HexB([string]$h) { [byte[]]($h -split '(..)' | Where-Object { $_ } | ForEach-Object { [Convert]::ToByte($_, 16) }) }
function Int([byte[]]$b) { if ($b[0] -ge 0x80) { $b = [byte[]](@(0) + $b) }; Tlv 0x02 $b }
$OID_SHA1 = HexB '2b0e03021a'
$OID_SHA256RSA = HexB '2a864886f70d01010b'
$OID_BASIC = HexB '2b0601050507300101'

# ---------------------------------------------------------------- certificados
$nb = [datetime]'2025-01-01T00:00:00Z'; $na = [datetime]'2030-01-01T00:00:00Z'
function NewKey { New-Object System.Security.Cryptography.RSACng(2048) }
function NewReq([string]$dn, $key) { New-Object "$X.CertificateRequest"($dn, $key, $sha256, $pkcs1) }

$caKey = NewKey
$req = NewReq 'CN=mOS Teste OCSP CA, O=mOS' $caKey
$req.CertificateExtensions.Add((New-Object "$X.X509BasicConstraintsExtension"($true, $false, 0, $true)))
$req.CertificateExtensions.Add((New-Object "$X.X509KeyUsageExtension"(([System.Security.Cryptography.X509Certificates.X509KeyUsageFlags]'KeyCertSign, CrlSign'), $true)))
$ca = $req.CreateSelfSigned($nb.ToUniversalTime(), $na.ToUniversalTime())

function Issue([string]$dn, [string]$serialHex, [bool]$ocspEku, [bool]$san) {
    $k = NewKey
    $r = NewReq $dn $k
    if ($ocspEku) {
        $oids = New-Object System.Security.Cryptography.OidCollection
        $null = $oids.Add((New-Object System.Security.Cryptography.Oid('1.3.6.1.5.5.7.3.9')))
        $r.CertificateExtensions.Add((New-Object "$X.X509EnhancedKeyUsageExtension"($oids, $false)))
    }
    if ($san) {
        $sb = New-Object "$X.SubjectAlternativeNameBuilder"
        $sb.AddDnsName('mock.mos.test')
        $r.CertificateExtensions.Add($sb.Build($false))
    }
    $c = $r.Create($ca, $nb.ToUniversalTime().AddDays(1), $na.ToUniversalTime().AddDays(-1), (HexB $serialHex))
    return @{ cert = $c; key = $k }
}
$leaf = Issue 'CN=mock.mos.test' '1a2b3c4d5e6f7081' $false $true
$resp = Issue 'CN=mOS Teste OCSP Responder' '2233445566778899' $true $false
$respNoEku = Issue 'CN=mOS Teste Responder sem EKU' '3344556677889900' $false $false
$rogueKey = NewKey

# ---------------------------------------------------------------- OCSP
$sha1 = [System.Security.Cryptography.SHA1]::Create()
$nameHash = $sha1.ComputeHash($ca.SubjectName.RawData)
$keyHash = $sha1.ComputeHash($ca.PublicKey.EncodedKeyValue.RawData)
$thisUp = [datetime]'2026-01-10T00:00:00'; $nextUp = [datetime]'2026-01-17T00:00:00'

function OcspResp([string]$status, [string]$serialHex, $signKey, $responderCert, [byte[]]$nameRaw) {
    $certId = Seq (Seq (Oid $OID_SHA1) (Tlv 0x05 $null)) (Tlv 0x04 $nameHash) (Tlv 0x04 $keyHash) (Int (HexB $serialHex))
    switch ($status) {
        'good'    { $st = Tlv 0x80 $null }
        'unknown' { $st = Tlv 0x82 $null }
        'revoked' { $st = Tlv 0xa1 (Concat (GTime ([datetime]'2026-01-05T12:00:00')) (Tlv 0xa0 (Tlv 0x0a ([byte[]]@(1))))) }
    }
    $single = Seq $certId $st (GTime $thisUp) (Tlv 0xa0 (GTime $nextUp))
    $rd = Seq (Tlv 0xa1 $nameRaw) (GTime $thisUp) (Seq $single)
    $sig = $signKey.SignData($rd, $sha256, $pkcs1)
    $certs = $null
    if ($responderCert) { $certs = Tlv 0xa0 (Seq $responderCert.RawData) }
    $basic = Seq $rd (Seq (Oid $OID_SHA256RSA) (Tlv 0x05 $null)) (Tlv 0x03 (Concat ([byte[]]@(0)) $sig)) $certs
    return Seq (Tlv 0x0a ([byte[]]@(0))) (Tlv 0xa0 (Seq (Oid $OID_BASIC) (Tlv 0x04 $basic)))
}
function Save([string]$name, [byte[]]$b) { [IO.File]::WriteAllBytes((Join-Path $out $name), $b) }

$ser = '1a2b3c4d5e6f7081'
Save 'good-ca.der'           (OcspResp 'good' $ser $caKey $null $ca.SubjectName.RawData)
Save 'revoked-ca.der'        (OcspResp 'revoked' $ser $caKey $null $ca.SubjectName.RawData)
Save 'unknown-ca.der'        (OcspResp 'unknown' $ser $caKey $null $ca.SubjectName.RawData)
Save 'good-delegado.der'     (OcspResp 'good' $ser $resp.key $resp.cert $resp.cert.SubjectName.RawData)
Save 'revoked-delegado.der'  (OcspResp 'revoked' $ser $resp.key $resp.cert $resp.cert.SubjectName.RawData)
Save 'delegado-sem-eku.der'  (OcspResp 'revoked' $ser $respNoEku.key $respNoEku.cert $respNoEku.cert.SubjectName.RawData)
Save 'assinatura-falsa.der'  (OcspResp 'good' $ser $rogueKey $null $ca.SubjectName.RawData)
Save 'outro-serial.der'      (OcspResp 'revoked' '0102030405060708' $caKey $null $ca.SubjectName.RawData)
Save 'try-later.der'         (Seq (Tlv 0x0a ([byte[]]@(3))))

# cadeia (formato .chain de tlstest: u24 comprimento || DER, folha primeiro) e a CA em PEM
$chain = Concat ([byte[]]@(0, ($leaf.cert.RawData.Length -shr 8), ($leaf.cert.RawData.Length -band 0xff))) $leaf.cert.RawData `
             ([byte[]]@(0, ($ca.RawData.Length -shr 8), ($ca.RawData.Length -band 0xff))) $ca.RawData
Save 'mock.chain' $chain
$pem = "-----BEGIN CERTIFICATE-----`n" + [Convert]::ToBase64String($ca.RawData, 'InsertLineBreaks') + "`n-----END CERTIFICATE-----`n"
[IO.File]::WriteAllText((Join-Path $out 'ca.pem'), $pem.Replace("`r`n", "`n"), (New-Object Text.UTF8Encoding($false)))
Get-ChildItem $out | Select-Object Name, Length | Format-Table -AutoSize
