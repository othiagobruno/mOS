# mOS — diagnóstico do git pela rede (git-remote-https, pipes entre processos)
echo "diag: inicio"
cd /tmp-rede 2>/dev/null || { mkdir -p /tmp-rede; cd /tmp-rede; }
GIT_TRACE=1 GIT_TRACE_PACKET=1 GIT_CURL_VERBOSE=1 timeout -s KILL 60 git ls-remote https://github.com/Homebrew/brew refs/heads/main > /tmp-g1.txt 2>&1
echo "diag: rc=$?"
n=0; while IFS= read -r l; do n=$((n+1)); [ $n -le 80 ] && echo "diag:   ${l:0:200}"; done < /tmp-g1.txt
echo "diag: linhas=$n"
echo "diag: fim"
