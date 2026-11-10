# Codex CLI 0.167.0 no harness: login, patch e o que bloqueava (10 nov. 2026)

O binário é o oficial `codex-x86_64-apple-darwin.tar.gz` da release `rust-v0.167.0`. O SHA-256
`d8eb4a865a0b17319cde10347523e5bab0978d01725b39f4a639f40905e01221` é o `digest` do GitHub. O binário não foi
modificado. Ninguém fez login com conta: a chave usada é falsa (`sk-mos-harness-falsa-0000`).

## Símbolos

`machocheck` no `libSystem.mos` da `main` (`d9e1fe5`): 630 imports, **0 ausentes**. A medição de 10 nov. continua valendo.

## O que bloqueava no harness

| Sintoma | Causa | Onde | Na VM? |
|---|---|---|---|
| `WARNING: … could not create PATH aliases: File exists (os error 17)` | o `SYS_MKDIR` do harness usava o `mkdir` do host. Com o pai ausente devolvia EEXIST, e o `create_dir_all` do Rust (`~/.codex/tmp/arg0-macos`) desistia | `tools/machotest.c` | não: no kernel, `vfs_mkdir` cria os pais e aceita a pasta que já existe |
| `syscall 98 não emulada` | `SYS_CHMOD` ausente no harness | `tools/machotest.c` | não |
| a chave pelo stdin chegava vazia | no harness, o fd 0 sempre dá fim de arquivo | `tools/machotest.c`: com `MACHOTEST_STDIN=1`, o fd 0 lê o stdin do host | não |
| `failed to clean up stale arg0 temp dirs: Unknown error (os error 66)` | o `readdir` do libSystem lia a pasta por índice ao vivo. O `remove_dir_all` apaga `apply_patch` e o índice pula o `.lock`, então o `rmdir` dá ENOTEMPTY e as pastas antigas se acumulam | `userland/libsystem/libc.c`: retrato da pasta no primeiro `readdir` (`dirsnap.inc`) | **sim**: o `SYS_READDIR` do kernel também é por índice. Vale para `rm -r` e para qualquer `remove_dir_all` |
| "Unknown error" no lugar do texto | `strerror` não tinha ENOTEMPTY, ETIMEDOUT, ECONNREFUSED, ELOOP e outros | `libc.c` | sim |

O kernel segue o `#mOS-link:` no `execve` e mantém o `argv[0]` (`exec_resolve`). Assim, o alias `apply_patch` que o
Codex cria entra no despacho por arg0 do próprio binário.

## Teste de host

```
tools/codex-cli-harness.sh <libSystem.mos> [codex-x86_64-apple-darwin]
```

Com o `libSystem.mos` desta branch:

```
ok   codex --version
ok   aliases apply_patch em ~/.codex/tmp/arg0
ok   link apply_patch gravado
ok   apply_patch respondeu Success
ok   nota.txt editado
ok   novo.txt criado
ok   segunda partida limpou o arg0 da primeira
ok   login status sem credencial
ok   login --with-api-key
ok   ~/.codex/auth.json gravado
ok   login status lê a chave
ok   seis partidas sem 'os error 66'
ok   diretórios arg0 vivos: 1
codex-cli-harness: tudo ok
```

Com o `libSystem.mos` da `main`, mesmo harness: `FALHA limpeza do arg0 falhou em 6 de 6 partidas`.

## O que só se prova na VM

- `codex login` com ChatGPT: navegador, servidor local na porta 1455 e o polling do token. O `--device-auth` já chega à
  tela do código ([compat-apps.md](compat-apps.md)). Falta alguém com conta concluir.
- `codex exec` com o modelo: precisa de rede e de credencial válida. O harness não tem `SYS_NET`.
- comandos do agente pelo `/usr/bin/sandbox-exec` e o alias `apply_patch` chamado por `PATH`: o harness não cria
  processo filho.
