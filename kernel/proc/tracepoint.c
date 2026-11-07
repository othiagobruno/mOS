/*
 * mOS — tracepoints de uma vez só, para diagnóstico de binários sem símbolos (porte do VS Code).
 *
 * /Sistema/Biblioteca/diag-tracepoints, uma linha por grupo de pontos:
 *     <parte do nome do processo>|<nome da imagem>|<deslocamento hex>[,<deslocamento hex>...]
 * ex.: Renderer|Electron Framework|0x1ced5c8,0x1ced5f5,0x2017084@r14
 * "@registrador" depois do deslocamento também despeja 128 bytes a partir do endereço nesse registrador;
 * "@pilha" imprime a pilha de chamadas (até 24 endereços de retorno, pela cadeia de rbp).
 *
 * Quando o carregador Mach-O monta um processo cujo nome contém o primeiro campo, cada imagem cujo nome de
 * arquivo é o segundo campo recebe um int3 (0xCC) em base + (deslocamento - vmin): o deslocamento é o endereço
 * que o llvm-objdump mostra na imagem. No ring 3 o int3 vira #GP (a porta 3 da IDT tem DPL 0). proc_fault
 * pergunta a tp_hit: se o rip é um ponto armado deste processo, os registradores vão para o log serial, o byte
 * original volta e a instrução roda normalmente. Cada ponto dispara uma vez.
 */
#include "proc.h"
#include "vfs.h"

#define TP_MAX 256
/* left: disparos que ainda faltam ("0xdesl*N" = N; sem *, 1). Depois de um disparo com left > 1 a instrução original roda
 * em passo único (TF) e o #DB seguinte (vetor 1) recoloca o 0xCC: step_tid = a thread que está passando por ele. */
static struct { int pid; u64 va, off; u8 orig; i8 dump; char img[40]; int left, hits, step_tid; } tps[TP_MAX];
static int ntp;

/* nomes na ordem do int_frame_t (r15 primeiro); -1 = sem despejo */
static const char *const regnames[] = {"r15", "r14", "r13", "r12", "r11", "r10", "r9", "r8", "rbp", "rdi", "rsi",
                                       "rdx", "rcx", "rbx", "rax"};
static int parse_reg(const char **s) {
    const char *p = *s;
    char n[4];
    int k = 0;
    while (k < 3 && ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) n[k++] = *p++;
    n[k] = 0;
    *s = p;
    if (!strcmp(n, "rsp")) return 100;
    if (!strcmp(n, "pil")) { if (!strncmp(p, "ha", 2)) *s = p + 2; return 101; }   /* "@pilha": a cadeia de rbp */
    for (int i = 0; i < (int)(sizeof regnames / sizeof *regnames); i++) if (!strcmp(n, regnames[i])) return i;
    return -1;
}

static u64 parse_hex(const char **s) {
    const char *p = *s;
    while (*p == ' ') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    u64 v = 0;
    for (;; p++) {
        int d = *p >= '0' && *p <= '9' ? *p - '0' : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : *p >= 'A' && *p <= 'F' ? *p - 'A' + 10 : -1;
        if (d < 0) break;
        v = v * 16 + (u64)d;
    }
    *s = p;
    return v;
}

/* o carregador chama para cada imagem já copiada para o processo */
void tp_arm(process_t *p, const char *path, u64 base, u64 vmin) {
    static int checked;
    static vnode_t *cfg;
    if (!checked) { checked = 1; cfg = vfs_lookup("/Sistema/Biblioteca/diag-tracepoints"); }
    if (!cfg || !cfg->data || !cfg->size || !path) return;
    const char *bn = strrchr(path, '/');
    bn = bn ? bn + 1 : path;
    const char *s = (const char *)cfg->data, *end = s + cfg->size;
    while (s < end) {
        const char *eol = s;
        while (eol < end && *eol != '\n' && *eol != '\r') eol++;
        /* campos: processo | imagem | deslocamentos */
        const char *b1 = s, *b2 = NULL, *b3 = NULL;
        for (const char *q = s; q < eol; q++) if (*q == '|') { if (!b2) b2 = q + 1; else if (!b3) { b3 = q + 1; break; } }
        if (b2 && b3) {
            usize lp = (usize)(b2 - 1 - b1), li = (usize)(b3 - 1 - b2);
            char pn[64], im[64];
            if (lp < sizeof pn && li < sizeof im) {
                memcpy(pn, b1, lp); pn[lp] = 0;
                memcpy(im, b2, li); im[li] = 0;
                if (strstr(p->name, pn) && !strcmp(bn, im)) {
                    const char *q = b3;
                    while (q < eol && ntp < TP_MAX) {
                        u64 off = parse_hex(&q);
                        int dump = -1, count = 1;
                        if (q < eol && *q == '*') {             /* "*N": dispara até N vezes */
                            q++;
                            count = 0;
                            while (q < eol && *q >= '0' && *q <= '9') count = count * 10 + (*q++ - '0');
                            if (count < 1) count = 1;
                        }
                        if (q < eol && *q == '@') { q++; dump = parse_reg(&q); }
                        if (off >= vmin) {
                            u64 va = base + (off - vmin);
                            u8 orig = 0, cc = 0xCC;
                            if (uvm_copy_in(p, &orig, va, 1) && uvm_copy_out(p, va, &cc, 1)) {
                                tps[ntp].pid = p->pid; tps[ntp].va = va; tps[ntp].off = off; tps[ntp].orig = orig;
                                tps[ntp].dump = (i8)dump;
                                tps[ntp].left = count; tps[ntp].hits = 0; tps[ntp].step_tid = 0;
                                strncpy(tps[ntp].img, im, sizeof tps[ntp].img - 1);
                                ntp++;
                                kprintf("tracepoint: %s (pid %d) %s+0x%llx armado em 0x%llx\n", p->name, p->pid, im,
                                        (unsigned long long)off, (unsigned long long)va);
                            }
                        }
                        while (q < eol && (*q == ',' || *q == ' ')) q++;
                        if (q < eol && !((*q >= '0' && *q <= '9') || (*q >= 'a' && *q <= 'f') || (*q >= 'A' && *q <= 'F'))) break;
                    }
                }
            }
        }
        s = eol;
        while (s < end && (*s == '\n' || *s == '\r')) s++;
    }
}

/* #GP (ou #BP) num ponto armado: registra, devolve o byte e segue. true = tratado. */
bool tp_hit(process_t *p, int_frame_t *f) {
    if (!ntp) return false;
    /* passo único depois de um disparo repetível: a instrução original já rodou; recoloca o int3 e tira o TF */
    if (f->vector == 1) {
        thread_t *t = thread_current();
        for (int i = 0; i < ntp; i++) {
            if (tps[i].pid != p->pid || !tps[i].step_tid || !t || tps[i].step_tid != t->id) continue;
            u8 cc = 0xCC;
            uvm_copy_out(p, tps[i].va, &cc, 1);
            tps[i].step_tid = 0;
            f->rflags &= ~0x100ULL;
            return true;
        }
        return false;
    }
    if (f->vector != 13 && f->vector != 3) return false;
    u64 at = f->vector == 3 ? f->rip - 1 : f->rip;
    for (int i = 0; i < ntp; i++) {
        if (tps[i].pid != p->pid || tps[i].va != at) continue;
        thread_t *t = thread_current();
        tps[i].hits++;
        kprintf("tracepoint: %s (pid %d, thread %d) %s+0x%llx #%d: rax 0x%llx rbx 0x%llx rcx 0x%llx rdx 0x%llx "
                "rsi 0x%llx rdi 0x%llx r8 0x%llx r12 0x%llx r13 0x%llx r14 0x%llx r15 0x%llx rsp 0x%llx rbp 0x%llx\n",
                p->name, p->pid, t ? t->id : -1, tps[i].img, (unsigned long long)tps[i].off, tps[i].hits,
                f->rax, f->rbx, f->rcx, f->rdx, f->rsi, f->rdi, f->r8, f->r12, f->r13, f->r14, f->r15, f->rsp, f->rbp);
        if (tps[i].dump == 101) {
            /* "@pilha": percorre a cadeia de rbp ({rbp salvo, endereço de retorno}) e imprime os retornos —
             * em código com frame pointer (o Chromium tem) é a pilha de chamadas inteira */
            u64 fp = f->rbp;
            for (int k = 0; k < 24 && fp; k++) {
                u64 fr[2];
                if ((fp & 7) || !uvm_copy_in(p, fr, fp, sizeof fr)) break;
                kprintf("tracepoint:   pilha #%d retorno 0x%llx (quadro 0x%llx)\n", k, (unsigned long long)fr[1], (unsigned long long)fp);
                if (fr[0] <= fp) break;                 /* a pilha cresce para baixo: o próximo quadro fica acima */
                fp = fr[0];
            }
        } else if (tps[i].dump >= 0) {
            u64 src = tps[i].dump == 100 ? f->rsp : ((u64 *)f)[tps[i].dump];
            u64 w[16];
            if (uvm_copy_in(p, w, src, sizeof w))
                for (int k = 0; k < 16; k += 4)
                    kprintf("tracepoint:   [0x%llx+0x%02x] %016llx %016llx %016llx %016llx\n", (unsigned long long)src, k * 8,
                            w[k], w[k + 1], w[k + 2], w[k + 3]);
            else kprintf("tracepoint:   0x%llx ilegível\n", (unsigned long long)src);
        }
        uvm_copy_out(p, at, &tps[i].orig, 1);
        f->rip = at;
        if (--tps[i].left > 0 && t) {
            /* repetível: roda a instrução original em passo único; o #DB recoloca o int3 (acima) */
            tps[i].step_tid = t->id;
            f->rflags |= 0x100ULL;
        } else tps[i] = tps[--ntp];
        return true;
    }
    return false;
}
