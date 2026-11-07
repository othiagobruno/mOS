# mOS — diagnóstico do shim de curl do Homebrew
export HOMEBREW_LIBRARY=/usr/local/Homebrew/Library HOMEBREW_CURL=/usr/bin/curl
bash -x /usr/local/Homebrew/Library/Homebrew/shims/shared/curl --homebrew=print-path > /tmp-d.txt 2>&1
echo "diag: rc=$?"
while IFS= read -r l; do echo "diag:   ${l:0:200}"; done < /tmp-d.txt
echo "diag: fim"
