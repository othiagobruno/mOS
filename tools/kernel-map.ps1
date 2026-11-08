# mOS - recompila só o kernel com informação de depuração (PDB) e traduz RIPs de pânico em funções.
#   .\build.ps1 -OutDir build-exec ; .\tools\kernel-map.ps1 -OutDir build-exec          (kernel com PDB no esp)
#   .\tools\kernel-map.ps1 -OutDir build-exec -Addr 0x1234,0x5678                        (RIP-base do log "*** kernel em ...")
# Mesmas flags do build.ps1, com -g no lugar de -g0. A tradução usa o dbghelp.dll do Windows (SymFromAddr).
param([string]$OutDir = 'build-exec', [string[]]$Addr = @())
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
if (-not $Addr) {
    $zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
    $zig = Join-Path $zig.FullName 'zig.exe'
    $sources = Get-ChildItem "$root\kernel" -Recurse -Include *.c, *.S | ForEach-Object { $_.FullName.Substring($root.Length + 1) }
    $cflags = @('-target', 'x86_64-uefi-msvc', '-O2', '-g', '-ffreestanding', '-fno-stack-protector', '-fno-stack-check',
        '-mno-stack-arg-probe', '-mno-red-zone', '-fshort-wchar', '-fno-sanitize=all', '-fno-strict-aliasing', '-nostdlib',
        '-Ikernel/include', '-I.', '-std=gnu11', '-w')
    & $zig cc @cflags @sources -o "$OutDir/BOOTX64.EFI" 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host 'kernel FAILED' -ForegroundColor Red; exit 1 }
    Copy-Item -Force "$OutDir/BOOTX64.EFI" "$OutDir\esp\EFI\BOOT\BOOTX64.EFI"
    Write-Host "OK: $OutDir\BOOTX64.EFI + BOOTX64.pdb (rode o stage ou o mkimg para a imagem)" -ForegroundColor Green
    exit 0
}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class MosSym {
    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern bool SymInitializeW(IntPtr h, string path, bool invade);
    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern ulong SymLoadModuleExW(IntPtr h, IntPtr f, string image, string mod, ulong baseAddr, uint size, IntPtr data, uint flags);
    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern uint SymSetOptions(uint o);
    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern bool SymFromAddrW(IntPtr h, ulong addr, out ulong disp, IntPtr info);
    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern bool SymGetLineFromAddrW64(IntPtr h, ulong addr, out uint disp, IntPtr line);
    public static string Name(IntPtr h, ulong a) {
        IntPtr buf = Marshal.AllocHGlobal(88 + 2 * 512);
        try {
            for (int i = 0; i < 88 + 2 * 512; i++) Marshal.WriteByte(buf, i, 0);
            Marshal.WriteInt32(buf, 0, 88);          // SizeOfStruct (SYMBOL_INFOW)
            Marshal.WriteInt32(buf, 80, 512);        // MaxNameLen
            ulong d;
            if (!SymFromAddrW(h, a, out d, buf)) return "?";
            string n = Marshal.PtrToStringUni(new IntPtr(buf.ToInt64() + 84));
            string where = "";
            IntPtr ln = Marshal.AllocHGlobal(48);
            for (int i = 0; i < 48; i++) Marshal.WriteByte(ln, i, 0);
            Marshal.WriteInt32(ln, 0, 40);           // IMAGEHLP_LINEW64
            uint ld;
            if (SymGetLineFromAddrW64(h, a, out ld, ln))
                where = " (" + Marshal.PtrToStringUni(Marshal.ReadIntPtr(ln, 24)) + ":" + Marshal.ReadInt32(ln, 16) + ")";
            Marshal.FreeHGlobal(ln);
            return n + "+0x" + d.ToString("x") + where;
        } finally { Marshal.FreeHGlobal(buf); }
    }
}
'@
$h = [IntPtr]12345
[void][MosSym]::SymSetOptions(0x2 -bor 0x10)     # UNDNAME | LOAD_LINES
[void][MosSym]::SymInitializeW($h, (Resolve-Path $OutDir).Path, $false)
$base = [uint64]0x10000000
$img = (Resolve-Path "$OutDir\BOOTX64.EFI").Path
if ([MosSym]::SymLoadModuleExW($h, [IntPtr]::Zero, $img, 'mos', $base, 0, [IntPtr]::Zero, 0) -eq 0) { Write-Host 'dbghelp: não carregou o PDB'; exit 1 }
foreach ($a in ($Addr -join "," -split "[, ]+" | Where-Object { $_ })) {
    $rva = [Convert]::ToUInt64(($a -replace '^0x', ''), 16)
    "{0} -> {1}" -f $a, [MosSym]::Name($h, $base + $rva)
}
