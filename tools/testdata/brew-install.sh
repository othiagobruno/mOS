# mOS — Homebrew de ponta a ponta: brew --version, config e install de bottles .sonoma do ghcr.io.
export HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_ANALYTICS=1 HOMEBREW_NO_ENV_HINTS=1 HOMEBREW_NO_INSTALL_CLEANUP=1
export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Sistema/Programas
b() { local nome=$1; shift; echo "brew: == $nome"; timeout -s KILL 900 "$@" > /tmp-b.txt 2>&1; local rc=$?
      local n=0; while IFS= read -r l; do n=$((n+1)); [ $n -le 60 ] && echo "brew:   ${l:0:220}"; done < /tmp-b.txt
      echo "brew: rc=$rc ($n linhas)"; }
echo "brew: inicio"
b "brew --version"       brew --version
b "brew config"          brew config
b "brew install tree"    brew install tree
b "tree"                 tree -L 1 /usr/local
b "brew install jq"      brew install jq
b "jq"                   jq -n '{mOS: "brew", ok: true}'
b "brew list"            brew list --versions
b "link brew antes"     ls -l /usr/local/bin/brew
b "brew uninstall tree"  brew uninstall tree
b "link brew depois"    ls -l /usr/local/bin/brew
b "brew doctor"          /usr/local/Homebrew/bin/brew doctor
echo "brew: fim"
