# mOS - boots build\mOS.img in QEMU with the hardware mOS supports
#   -Headless : no window, QMP on tcp:4444 (used by tools\qemu-test.ps1)
#   -ExtraDisk <file> : second SATA disk (raw) on the AHCI controller
#   -Persist  : headless without -snapshot (writes reach the image; used to test persistence)
#   -NvmeDisk <file> : extra NVMe disk (raw)    -BootNvme : attach -Image as an NVMe SSD instead of SATA
#   -BootUsb  : attach -Image as a USB stick (usb-storage on xHCI) instead of SATA
#   -NoNet    : no wired NIC (e1000), so Wi-Fi (e.g. the simulated radio of -Tests images) becomes the active interface
#   -WavOut <file> : (headless) records the guest audio output to a WAV file (default: $env:MOS_WAVOUT)
#   -Vga virtio : virtio-vga (virtio-gpu) instead of the standard VGA; mOS then presents frames through the GPU queue
#   -Monitores N : (with -Vga virtio) N outputs; with a window, QEMU shows one per monitor. Headless, outputs 2+ stay
#                  "unplugged" unless the image was built with build.ps1 -MonitorExtra WxH
#   -UsbInput : adds USB test devices on the xHCI bus (usb-kbd, usb-mouse, usb-tablet, usb-storage
#               backed by build\usb-stick.img, a 16 MiB raw disk with an MBR signature, created on demand)
param([switch]$Headless, [int]$Memory = 1024, [string]$Image = 'build/mOS.img', [int]$Port = 4444, [string]$Serial = 'build/serial-test.log', [string]$ExtraDisk = '', [switch]$Persist, [switch]$UsbInput, [string]$NvmeDisk = '', [switch]$BootNvme, [switch]$BootUsb, [string]$WavOut = $env:MOS_WAVOUT, [switch]$NoNet, [int]$Cpus = 4, [string]$Vga = 'std', [int]$Monitores = 1)
$root = $PSScriptRoot
Set-Location $root
$qemuDir = 'C:\Program Files\qemu'
$qemu = Join-Path $qemuDir 'qemu-system-x86_64.exe'
if (-not (Test-Path $qemu)) { $qemu = (Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue).Source }
$fw = Join-Path $qemuDir 'share\edk2-x86_64-code.fd'
$audio = if ($Headless) { if ($WavOut) { "wav,id=snd0,path=$WavOut" } else { 'none,id=snd0' } } else { 'dsound,id=snd0' }

if ($BootNvme) { $bootDisk = @('-drive', "format=raw,file=$Image,if=none,id=nvboot", '-device', 'nvme,serial=mosboot,drive=nvboot') }
elseif ($BootUsb) { $bootDisk = @() }   # added after the xHCI controller below
else { $bootDisk = @('-drive', "format=raw,file=$Image,if=ide,index=0") }
$qargs = @(
    '-machine', 'q35', '-m', "$Memory", '-cpu', 'max', '-smp', "$Cpus",
    '-drive', "if=pflash,format=raw,readonly=on,file=$fw"
) + $bootDisk + @(
    '-audiodev', $audio, '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0',
    '-device', 'qemu-xhci,id=xhci',
    '-no-reboot'
)
if ($Vga -eq 'virtio' -and $Monitores -gt 1) { $qargs += @('-vga', 'none', '-device', "virtio-vga,id=vga0,max_outputs=$Monitores") }
else { $qargs += @('-vga', $Vga) }
if (-not $NoNet) { $qargs += @('-netdev', 'user,id=n0', '-device', 'e1000,netdev=n0') }
else { $qargs += @('-nic', 'none') }   # sem isto o QEMU põe uma e1000e padrão
if ($BootUsb) { $qargs += @('-drive', "format=raw,file=$Image,if=none,id=usbboot", '-device', 'usb-storage,bus=xhci.0,drive=usbboot,bootindex=0') }
if ($NvmeDisk) { $qargs += @('-drive', "format=raw,file=$NvmeDisk,if=none,id=nvdata", '-device', 'nvme,serial=mosdata,drive=nvdata') }
if ($ExtraDisk) { $qargs += @('-drive', "format=raw,file=$ExtraDisk,if=none,id=extra0", '-device', 'ide-hd,drive=extra0,bus=ide.1') }
if ($UsbInput) {
    $stick = Join-Path $root 'build\usb-stick.img'
    if (-not (Test-Path $stick)) {
        $bytes = New-Object byte[] (16MB)
        $bytes[510] = 0x55; $bytes[511] = 0xAA
        [System.IO.File]::WriteAllBytes($stick, $bytes)
    }
    $qargs += @('-device', 'usb-kbd,bus=xhci.0', '-device', 'usb-mouse,bus=xhci.0', '-device', 'usb-tablet,bus=xhci.0',
        '-drive', "if=none,id=stick,format=raw,file=$stick", '-device', 'usb-storage,bus=xhci.0,drive=stick')
}
if ($Headless) { $qargs += @('-display', 'none', '-qmp', "tcp:127.0.0.1:$Port,server,nowait", '-serial', "file:$Serial"); if (-not $Persist) { $qargs += '-snapshot' } }
else { $qargs += @('-display', 'sdl', '-serial', 'file:build/serial.log') }
# MOS_QEMU_LOG=<arquivo>: o QEMU registra resets da CPU (com os registradores) e erros do convidado. Com -no-reboot,
# um triple fault do Nucleo fecha o QEMU sem deixar rastro no log serial; este arquivo diz onde foi.
if ($env:MOS_QEMU_LOG) { $qargs += @('-d', 'cpu_reset,guest_errors', '-D', $env:MOS_QEMU_LOG) }
& $qemu @qargs
