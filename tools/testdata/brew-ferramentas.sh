# mOS — coreutils 9.11 e portable-ruby 3.4.5 do Homebrew rodando pelo bash (tools/stage-bash.ps1 -Script).
# Cada linha "ferr:" vai para o log serial; "ferr: ok" / "ferr: FALHOU" resumem cada caso.
ok=0; falhou=0
t() { local nome=$1; shift; local saida; saida=$("$@" 2>&1); local rc=$?
      if [ $rc -eq 0 ]; then ok=$((ok+1)); echo "ferr: ok      $nome -> ${saida:0:120}"
      else falhou=$((falhou+1)); echo "ferr: FALHOU  $nome (rc $rc) -> ${saida:0:200}"; fi; }

echo "ferr: inicio"
t "ls --version"       ls --version
t "ls /usr/local"      ls /usr/local
t "ls -l opt"          ls -l /usr/local/opt
t "uname -a"           uname -a
sh_tr() { echo "Homebrew no mOS" | tr a-z A-Z; }
t "echo|tr"            sh_tr
mkdir -p /tmp-ferr && cd /tmp-ferr
printf 'b\na\nc\n' > lista.txt
t "sort"               sort lista.txt
t "wc -l"              wc -l lista.txt
t "head -n1"           head -n 1 lista.txt
t "cat|tail"           tail -n 1 lista.txt
t "cp"                 cp lista.txt copia.txt
sh_cmp() { [ "$(md5sum < lista.txt)" = "$(md5sum < copia.txt)" ]; }
t "cp conteúdo igual"  sh_cmp
t "mv"                 mv copia.txt movida.txt
t "rm"                 rm movida.txt
t "ln -s + readlink"   ln -sf lista.txt link.txt
t "readlink"           readlink link.txt
t "sha256sum"          sha256sum lista.txt
t "md5sum"             md5sum lista.txt
t "basename/dirname"   basename /usr/local/bin/bash
t "dirname"            dirname /usr/local/bin/bash
sh_p() { seq 1 50 | sort -rn | head -n 3 | tr '\n' ' '; }
t "seq|sort -rn|head"  sh_p
t "date"               date -u +%Y
t "expr"               expr 6 \* 7
t "factor (gmp)"       factor 1234567
t "du -sh"             du -sh /usr/local/opt/gmp
t "env"                env -i FOO=bar /usr/local/bin/printenv FOO
t "id -un"             id -un
t "test -x"            test -x /usr/local/bin/bash

t "ruby -v"            ruby -v
t "ruby puts"          ruby -e 'puts 6*7'
t "ruby string"        ruby -e 'puts "brew".upcase.reverse'
t "ruby hash/json"     ruby -rjson -e 'puts JSON.generate({ "a" => [1, 2], "b" => nil })'
t "ruby File"          ruby -e 'File.write("/tmp-ferr/r.txt", "oi"); puts File.read("/tmp-ferr/r.txt")'
t "ruby Dir"           ruby -e 'puts Dir.children("/usr/local/opt").sort.join(",")'
t "ruby system"        ruby -e 'system("echo", "do-ruby") or exit 1'
t "ruby backtick"      ruby -e 'puts `uname -s`'
t "ruby symlink"       ruby -e 'File.symlink("/tmp-ferr/r.txt", "/tmp-ferr/rl") rescue nil; puts File.readlink("/tmp-ferr/rl")'
t "ruby zlib"          ruby -rzlib -e 'puts Zlib.crc32("mOS")'
t "ruby thread"        ruby -e 't = Thread.new { 21 * 2 }; puts t.value'
t "ruby fiber"         ruby -e 'f = Fiber.new { Fiber.yield 1; 2 }; puts [f.resume, f.resume].inspect'
t "ruby pathname"      ruby -rpathname -e 'puts Pathname.new("/usr/local/bin/../opt").cleanpath'

echo "ferr: fim ok=$ok falhou=$falhou"
exit $falhou
