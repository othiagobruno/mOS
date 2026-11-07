export PATH=/usr/local/bin:/usr/bin:/bin
R=/usr/local/Homebrew/Library/Homebrew/vendor/portable-ruby/current/bin/ruby
t() { echo "diag: == $1"; /usr/local/bin/timeout -s KILL 60 $R -e "$2" > /tmp-r.txt 2>&1; echo "diag: rc=$?"; while IFS= read -r l; do echo "diag:   ${l:0:200}"; done < /tmp-r.txt; }
t "versao" 'puts RUBY_VERSION'
t "popen array" 'puts IO.popen(["echo","oi"]).read'
t "popen -" 'IO.popen("-") { |p| if p then puts "pai leu: " + p.read.inspect else exec("echo", "do filho") end }'
t "popen - err" 'IO.popen("-") { |p| if p then puts "pai: " + p.read.inspect else $stderr.reopen(File::NULL); exec("echo", "filho2") end }'
t "fork" 'pid = fork { puts "filho #{Process.pid}" }; Process.wait(pid); puts "status #{$?.exitstatus}"'
t "fork+thread" 't = Thread.new { sleep 0.2 }; pid = fork { puts "filho ok" }; Process.wait(pid); t.join; puts "ok"'
echo "diag: fim"
