# mOS — sed, grep, tar, gzip, xz, curl e git do Homebrew pelo bash (tools/stage-bash.ps1 -Script).
ok=0; falhou=0
t() { local nome=$1; shift; local saida; saida=$("$@" 2>&1); local rc=$?
      saida=${saida//$'\n'/ | }
      if [ $rc -eq 0 ]; then ok=$((ok+1)); echo "rede: ok      $nome -> ${saida:0:140}"
      else falhou=$((falhou+1)); echo "rede: FALHOU  $nome (rc $rc) -> ${saida:0:300}"; fi; }

echo "rede: inicio"
mkdir -p /tmp-rede && cd /tmp-rede || exit 1
printf 'alfa 1\nbeta 2\ngama 3\n' > a.txt
t "sed --version"      sed --version
t "sed s///"           sed 's/beta/BETA/' a.txt
t "sed -n p"           sed -n '2p' a.txt
t "sed -E"             sed -E 's/([a-z]+) ([0-9])/\2:\1/' a.txt
t "grep -V"            grep -V
t "grep"               grep gama a.txt
t "grep -c -E"         grep -cE '^(alfa|gama)' a.txt
t "grep -P (pcre2)"    grep -oP '\d+' a.txt
t "grep -r"            grep -rl alfa /tmp-rede
t "tar --version"      tar --version
t "tar czf"            tar czf pacote.tar.gz a.txt
t "tar tzf"            tar tzf pacote.tar.gz
mkdir -p sai
t "tar xzf -C"         tar xzf pacote.tar.gz -C sai
t "conteúdo extraído"  cat sai/a.txt
t "gzip -k"            gzip -kf a.txt
t "gzip -dc"           gzip -dc a.txt.gz
t "xz"                 xz -kf a.txt
t "xz -dc"             xz -dc a.txt.xz
t "tar cJf (xz)"       tar cJf p.tar.xz a.txt
t "tar tJf"            tar tJf p.tar.xz

t "curl --version"     curl --version
t "git --version"      git --version
t "git init"           git init -q repo
cd repo
git config user.name "mOS"; git config user.email "mos@localhost"
echo "oi" > LEIAME
t "git add"            git add LEIAME
t "git commit"         git commit -qm "primeiro commit no mOS"
t "git log"            git log --oneline
t "git status"         git status --short
t "git branch"         git branch --show-current
cd ..
# rede por último: se travar, o resto já foi medido
echo "rede: (local) ok=$ok falhou=$falhou"
t "curl https (API)"   timeout -s KILL 40 curl -m 20 -fsSL -o hello.json https://formulae.brew.sh/api/formula/hello.json
t "json baixado"       grep -o '"name":"hello"' hello.json
t "curl -I github"     timeout -s KILL 40 curl -m 20 -fsSI https://github.com
t "git ls-remote https" timeout -s KILL 30 git ls-remote https://github.com/Homebrew/brew refs/heads/main
t "git clone https"    timeout -s KILL 60 git clone -q --depth 1 https://github.com/Homebrew/homebrew-portable-ruby.git pr
t "clone tem arquivos" ls pr

echo "rede: fim ok=$ok falhou=$falhou"
exit $falhou
