# mOS — casks do Homebrew: um .zip (keycastr), um .dmg (monitorcontrol) e o Slack x64 (um dos 7 apps).
export HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_ANALYTICS=1 HOMEBREW_NO_ENV_HINTS=1 HOMEBREW_NO_INSTALL_CLEANUP=1
export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Sistema/Programas
b() { local nome=$1; shift; echo "cask: == $nome"; timeout -s KILL 1500 "$@" > /tmp-b.txt 2>&1; local rc=$?
      local n=0; while IFS= read -r l; do n=$((n+1)); [ $n -le 50 ] && echo "cask:   ${l:0:220}"; done < /tmp-b.txt
      echo "cask: rc=$rc ($n linhas)"; }
echo "cask: inicio"
b "hdiutil imageinfo (sem arquivo)" hdiutil imageinfo -format /nao-existe.dmg
b "hdiutil imageinfo (zip)" hdiutil imageinfo -format /usr/local/Homebrew/.git/HEAD
RB=/usr/local/Homebrew/Library/Homebrew/vendor/portable-ruby/current/bin/ruby
b "tempfile .bom"  $RB -rtempfile -e 'Tempfile.create(["", ".bom"]) { |f| f.close; p [f.path, File.directory?(f.path), File.file?(f.path)]; system("ls", "-la", f.path); system("stat", f.path) }'
b "tmp private"    ls -la /private /tmp
b "install --cask keycastr (.zip)"    brew install --cask keycastr
b "app keycastr"                      ls /Applications/KeyCastr.app/Contents/MacOS
b "install --cask monitorcontrol (.dmg)" brew install --cask monitorcontrol
b "app monitorcontrol"                ls /Applications/MonitorControl.app/Contents/MacOS
b "volumes depois"                    ls /Volumes
b "brew list --cask"                  brew list --cask --versions
b "uninstall --cask keycastr"         brew uninstall --cask keycastr
b "app keycastr removido"             ls /Applications
b "install --cask slack"              brew install --cask slack
b "app slack"                         ls /Applications/Slack.app/Contents/MacOS
echo "cask: fim"
