#!/bin/bash
# Monta /Applications/HostTeste.app com três extensões para o teste do ExtensionFoundation (raiz em $1).
set -eu
app="$1/Applications/HostTeste.app/Contents"
mkdir -p "$app/Extensions/Nova.appex/Contents" "$app/PlugIns/Antiga.appex/Contents" "$app/Extensions/Outra.appex/Contents"
cat > "$app/Extensions/Nova.appex/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key><string>com.mos.teste.ext</string>
    <key>CFBundleDisplayName</key><string>Extensão Teste</string>
    <key>CFBundleName</key><string>Nova</string>
    <key>EXAppExtensionAttributes</key>
    <dict><key>EXExtensionPointIdentifier</key><string>com.mos.teste.ponto</string></dict>
</dict>
</plist>
EOF
cat > "$app/PlugIns/Antiga.appex/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key><string>com.mos.teste.antiga</string>
    <key>CFBundleName</key><string>Antiga</string>
    <key>NSExtension</key>
    <dict><key>NSExtensionPointIdentifier</key><string>com.mos.teste.antigo</string></dict>
</dict>
</plist>
EOF
cat > "$app/Extensions/Outra.appex/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key><string>com.mos.teste.outra</string>
    <key>EXAppExtensionAttributes</key>
    <dict><key>EXExtensionPointIdentifier</key><string>com.mos.teste.outroponto</string></dict>
</dict>
</plist>
EOF
