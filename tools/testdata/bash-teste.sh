echo "bash-teste: inicio, $BASH_VERSION"
x=$((6*7)); echo "bash-teste: aritmetica $x"
for i in 1 2 3; do printf "bash-teste: laco %s\n" "$i"; done
f() { echo "bash-teste: funcao $1"; }; f arg
v=$(echo subshell); echo "bash-teste: captura $v"
[[ "abc123" =~ ^([a-z]+)([0-9]+)$ ]] && echo "bash-teste: regex ${BASH_REMATCH[1]} ${BASH_REMATCH[2]}"
case foo.txt in *.txt) echo "bash-teste: case/glob ok";; esac
arr=(a b c); echo "bash-teste: array ${#arr[@]} ${arr[1]}"
/Sistema/Programas/filho via-bash; echo "bash-teste: filho saiu com $?"
echo "linha um" > /tmp-bash.txt; echo "linha dois" >> /tmp-bash.txt; while read -r l; do echo "bash-teste: leu [$l]"; done < /tmp-bash.txt
echo "bash-teste: pipe $(echo abc | /Sistema/Programas/wc)"
echo "bash-teste: fim"
exit 0
