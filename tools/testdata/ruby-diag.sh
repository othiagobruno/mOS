echo "rb: inicio"
ruby -e "puts 6*7" > /tmp-rb.txt 2>&1; echo "rb: rc $?"
while IFS= read -r l; do case "$l" in "[thread]"*) ;; *) echo "rb: $l";; esac; done < /tmp-rb.txt
echo "rb: fim"
