/* mOS — Xadrez: regras completas (roque, en passant, promoção, xeque, xeque-mate, afogamento, 50 lances,
 * repetição tripla, material insuficiente), oponente do computador (alfa-beta com tabelas de posição),
 * desfazer, girar o tabuleiro e notação algébrica na lista de lances. Peças desenhadas vetorialmente. */
#include "kit.h"

extern app_t app_xadrez;

enum { EMPTY = 0, P = 1, N, B, R, Q, K };   /* brancas positivas, pretas negativas */
typedef struct { i8 from, to, promo, flags; } move_t;   /* flags: 1 captura, 2 en passant, 4 roque, 8 avanço duplo */
typedef struct {
    i8 b[64];                 /* a8 = 0 ... h1 = 63 */
    int side;                 /* 1 brancas, -1 pretas */
    u8 castle;                /* 1 K, 2 Q, 4 k, 8 q */
    int ep;                   /* casa de en passant ou -1 */
    int half, full;
} pos_t;

static struct {
    window_t *w;
    pos_t pos;
    pos_t hist[512];
    move_t mvhist[512];
    char san[512][12];
    int nhist;
    int sel;                  /* casa selecionada */
    move_t legal[256];
    int nlegal;
    bool flip, vs_cpu, thinking, over;
    int level;                /* 0 fácil, 1 médio, 2 difícil */
    int cpu_side;
    char status[96];
    int promo_from, promo_to; /* escolha de promoção pendente */
    u64 think_at;
    int last_from, last_to;
    int mscroll;
} X = {.vs_cpu = true, .level = 1, .cpu_side = -1, .sel = -1, .promo_from = -1, .last_from = -1, .last_to = -1};

#define ROW(s) ((s) >> 3)
#define COL(s) ((s) & 7)

static void pos_start(pos_t *p) {
    static const i8 back[8] = {R, N, B, Q, K, B, N, R};
    memset(p, 0, sizeof *p);
    for (int i = 0; i < 8; i++) { p->b[i] = -back[i]; p->b[8 + i] = -P; p->b[48 + i] = P; p->b[56 + i] = back[i]; }
    p->side = 1; p->castle = 15; p->ep = -1; p->full = 1;
}

/* ------------------------------------------------------------ geração de lances */
static const int kn_d[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}};
static const int kg_d[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};
static const int bi_d[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
static const int ro_d[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

/* a casa sq é atacada pelo lado `by`? */
static bool attacked(const pos_t *p, int sq, int by) {
    int r = ROW(sq), c = COL(sq);
    /* peões: um peão branco em (r+1, c±1) ataca (r, c) */
    int pr = r + (by == 1 ? 1 : -1);
    if (pr >= 0 && pr < 8) for (int dc = -1; dc <= 1; dc += 2) { int cc = c + dc; if (cc >= 0 && cc < 8 && p->b[pr * 8 + cc] == by * P) return true; }
    for (int i = 0; i < 8; i++) {
        int rr = r + kn_d[i][0], cc = c + kn_d[i][1];
        if (rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && p->b[rr * 8 + cc] == by * N) return true;
        rr = r + kg_d[i][0]; cc = c + kg_d[i][1];
        if (rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && p->b[rr * 8 + cc] == by * K) return true;
    }
    for (int i = 0; i < 4; i++) {
        for (int k = 1; k < 8; k++) {
            int rr = r + bi_d[i][0] * k, cc = c + bi_d[i][1] * k;
            if (rr < 0 || rr > 7 || cc < 0 || cc > 7) break;
            i8 x = p->b[rr * 8 + cc];
            if (x) { if (x == by * B || x == by * Q) return true; break; }
        }
        for (int k = 1; k < 8; k++) {
            int rr = r + ro_d[i][0] * k, cc = c + ro_d[i][1] * k;
            if (rr < 0 || rr > 7 || cc < 0 || cc > 7) break;
            i8 x = p->b[rr * 8 + cc];
            if (x) { if (x == by * R || x == by * Q) return true; break; }
        }
    }
    return false;
}
static int king_sq(const pos_t *p, int side) { for (int i = 0; i < 64; i++) if (p->b[i] == side * K) return i; return -1; }
static bool in_check(const pos_t *p, int side) { int k = king_sq(p, side); return k >= 0 && attacked(p, k, -side); }

static void add(move_t *m, int *n, int f, int t, int promo, int fl) { m[*n].from = (i8)f; m[*n].to = (i8)t; m[*n].promo = (i8)promo; m[*n].flags = (i8)fl; (*n)++; }

static int gen_pseudo(const pos_t *p, move_t *m) {
    int n = 0, s = p->side;
    for (int sq = 0; sq < 64; sq++) {
        i8 pc = p->b[sq];
        if (!pc || (pc > 0) != (s > 0)) continue;
        int t = pc * s, r = ROW(sq), c = COL(sq);
        if (t == P) {
            int dir = -s, r1 = r + dir;
            bool promo_rank = r1 == 0 || r1 == 7;
            if (r1 >= 0 && r1 < 8 && !p->b[r1 * 8 + c]) {
                if (promo_rank) for (int k = Q; k >= N; k--) add(m, &n, sq, r1 * 8 + c, k, 0);
                else add(m, &n, sq, r1 * 8 + c, 0, 0);
                int start = s == 1 ? 6 : 1;
                if (r == start && !p->b[(r + 2 * dir) * 8 + c]) add(m, &n, sq, (r + 2 * dir) * 8 + c, 0, 8);
            }
            for (int dc = -1; dc <= 1; dc += 2) {
                int cc = c + dc;
                if (cc < 0 || cc > 7 || r1 < 0 || r1 > 7) continue;
                int to = r1 * 8 + cc;
                if (p->b[to] && (p->b[to] > 0) != (s > 0)) {
                    if (promo_rank) for (int k = Q; k >= N; k--) add(m, &n, sq, to, k, 1);
                    else add(m, &n, sq, to, 0, 1);
                } else if (to == p->ep) add(m, &n, sq, to, 0, 3);
            }
        } else if (t == N || t == K) {
            const int (*d)[2] = t == N ? kn_d : kg_d;
            for (int i = 0; i < 8; i++) {
                int rr = r + d[i][0], cc = c + d[i][1];
                if (rr < 0 || rr > 7 || cc < 0 || cc > 7) continue;
                i8 x = p->b[rr * 8 + cc];
                if (!x) add(m, &n, sq, rr * 8 + cc, 0, 0);
                else if ((x > 0) != (s > 0)) add(m, &n, sq, rr * 8 + cc, 0, 1);
            }
            if (t == K) {
                /* roque: casas livres e não atacadas */
                int home = s == 1 ? 60 : 4;
                if (sq == home && !attacked(p, home, -s)) {
                    u8 ks = s == 1 ? 1 : 4, qs = s == 1 ? 2 : 8;
                    if ((p->castle & ks) && !p->b[home + 1] && !p->b[home + 2] && p->b[home + 3] == s * R && !attacked(p, home + 1, -s) && !attacked(p, home + 2, -s))
                        add(m, &n, sq, home + 2, 0, 4);
                    if ((p->castle & qs) && !p->b[home - 1] && !p->b[home - 2] && !p->b[home - 3] && p->b[home - 4] == s * R && !attacked(p, home - 1, -s) && !attacked(p, home - 2, -s))
                        add(m, &n, sq, home - 2, 0, 4);
                }
            }
        } else {
            const int (*d)[2];
            int nd, nd2 = 0;
            const int (*d2)[2] = NULL;
            if (t == B) { d = bi_d; nd = 4; } else if (t == R) { d = ro_d; nd = 4; } else { d = bi_d; nd = 4; d2 = ro_d; nd2 = 4; }
            for (int pass = 0; pass < 2; pass++) {
                const int (*dd)[2] = pass ? d2 : d;
                int nn = pass ? nd2 : nd;
                for (int i = 0; i < nn; i++)
                    for (int k = 1; k < 8; k++) {
                        int rr = r + dd[i][0] * k, cc = c + dd[i][1] * k;
                        if (rr < 0 || rr > 7 || cc < 0 || cc > 7) break;
                        i8 x = p->b[rr * 8 + cc];
                        if (!x) { add(m, &n, sq, rr * 8 + cc, 0, 0); continue; }
                        if ((x > 0) != (s > 0)) add(m, &n, sq, rr * 8 + cc, 0, 1);
                        break;
                    }
            }
        }
    }
    return n;
}

static void make(pos_t *p, move_t mv) {
    int s = p->side, f = mv.from, t = mv.to;
    i8 pc = p->b[f];
    bool cap = p->b[t] != 0 || (mv.flags & 2);
    p->b[t] = mv.promo ? (i8)(s * mv.promo) : pc;
    p->b[f] = 0;
    if (mv.flags & 2) p->b[t + (s == 1 ? 8 : -8)] = 0;
    if (mv.flags & 4) {
        if (t > f) { p->b[t - 1] = p->b[t + 1]; p->b[t + 1] = 0; }
        else { p->b[t + 1] = p->b[t - 2]; p->b[t - 2] = 0; }
    }
    p->ep = (mv.flags & 8) ? (f + t) / 2 : -1;
    /* direitos de roque */
    if (pc == K) p->castle &= ~3;
    if (pc == -K) p->castle &= ~12;
    if (f == 63 || t == 63) p->castle &= ~1;
    if (f == 56 || t == 56) p->castle &= ~2;
    if (f == 7 || t == 7) p->castle &= ~4;
    if (f == 0 || t == 0) p->castle &= ~8;
    p->half = (pc * s == P || cap) ? 0 : p->half + 1;
    if (s == -1) p->full++;
    p->side = -s;
}

static int gen_legal(const pos_t *p, move_t *out) {
    move_t m[256];
    int n = gen_pseudo(p, m), k = 0;
    for (int i = 0; i < n; i++) {
        pos_t q = *p;
        make(&q, m[i]);
        if (!in_check(&q, p->side)) out[k++] = m[i];
    }
    return k;
}

/* ------------------------------------------------------------ avaliação e busca */
static const int val[7] = {0, 100, 320, 330, 500, 900, 0};
static const i8 pst_p[64] = {0, 0, 0, 0, 0, 0, 0, 0, 50, 50, 50, 50, 50, 50, 50, 50, 10, 10, 20, 30, 30, 20, 10, 10, 5, 5, 10, 25, 25, 10, 5, 5,
                             0, 0, 0, 20, 20, 0, 0, 0, 5, -5, -10, 0, 0, -10, -5, 5, 5, 10, 10, -20, -20, 10, 10, 5, 0, 0, 0, 0, 0, 0, 0, 0};
static const i8 pst_n[64] = {-50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0, 0, 0, 0, -20, -40, -30, 0, 10, 15, 15, 10, 0, -30, -30, 5, 15, 20, 20, 15, 5, -30,
                             -30, 0, 15, 20, 20, 15, 0, -30, -30, 5, 10, 15, 15, 10, 5, -30, -40, -20, 0, 5, 5, 0, -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};
static const i8 pst_b[64] = {-20, -10, -10, -10, -10, -10, -10, -20, -10, 0, 0, 0, 0, 0, 0, -10, -10, 0, 5, 10, 10, 5, 0, -10, -10, 5, 5, 10, 10, 5, 5, -10,
                             -10, 0, 10, 10, 10, 10, 0, -10, -10, 10, 10, 10, 10, 10, 10, -10, -10, 5, 0, 0, 0, 0, 5, -10, -20, -10, -10, -10, -10, -10, -10, -20};
static const i8 pst_r[64] = {0, 0, 0, 0, 0, 0, 0, 0, 5, 10, 10, 10, 10, 10, 10, 5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5,
                             -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 5, 5, 0, 0, 0};
static const i8 pst_k[64] = {-30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30,
                             -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20, -20, -20, -20, -10, 20, 20, 0, 0, 0, 0, 20, 20, 20, 30, 10, 0, 0, 10, 30, 20};

static int evaluate(const pos_t *p) {
    int sc = 0;
    for (int sq = 0; sq < 64; sq++) {
        i8 pc = p->b[sq];
        if (!pc) continue;
        int t = pc > 0 ? pc : -pc, idx = pc > 0 ? sq : (7 - ROW(sq)) * 8 + COL(sq);
        int v = val[t];
        switch (t) {
        case P: v += pst_p[idx]; break;
        case N: v += pst_n[idx]; break;
        case B: v += pst_b[idx]; break;
        case R: v += pst_r[idx]; break;
        case Q: v += pst_b[idx] / 2; break;
        case K: v += pst_k[idx]; break;
        }
        sc += pc > 0 ? v : -v;
    }
    return sc * p->side;
}

static int nodes;
static int search(pos_t *p, int depth, int alpha, int beta, int ply) {
    nodes++;
    move_t m[256];
    int n = gen_legal(p, m);
    if (!n) return in_check(p, p->side) ? -100000 + ply : 0;
    if (p->half >= 100) return 0;
    if (depth <= 0) {
        /* quiescência: só capturas */
        int stand = evaluate(p);
        if (stand >= beta) return beta;
        if (stand > alpha) alpha = stand;
        if (ply > 10) return alpha;
        for (int i = 0; i < n; i++) {
            if (!(m[i].flags & 1) && !m[i].promo) continue;
            pos_t q = *p;
            make(&q, m[i]);
            int v = -search(&q, 0, -beta, -alpha, ply + 1);
            if (v >= beta) return beta;
            if (v > alpha) alpha = v;
        }
        return alpha;
    }
    /* ordena: capturas de maior valor primeiro (MVV-LVA simples) */
    int score[256];
    for (int i = 0; i < n; i++) {
        int cap = p->b[m[i].to];
        score[i] = (cap ? val[cap > 0 ? cap : -cap] * 10 - val[p->b[m[i].from] > 0 ? p->b[m[i].from] : -p->b[m[i].from]] : 0) + (m[i].promo ? 800 : 0);
    }
    for (int i = 0; i < n; i++) {
        int bi = i;
        for (int j = i + 1; j < n; j++) if (score[j] > score[bi]) bi = j;
        move_t tm = m[i]; m[i] = m[bi]; m[bi] = tm;
        int ts = score[i]; score[i] = score[bi]; score[bi] = ts;
        pos_t q = *p;
        make(&q, m[i]);
        int v = -search(&q, depth - 1, -beta, -alpha, ply + 1);
        if (v >= beta) return beta;
        if (v > alpha) alpha = v;
    }
    return alpha;
}

static move_t best_move(pos_t *p, int depth) {
    move_t m[256];
    int n = gen_legal(p, m), best = -1000000, bi = 0;
    nodes = 0;
    /* um pouco de variedade nas aberturas: embaralha a ordem inicial */
    for (int i = n - 1; i > 0; i--) { int j = rand32() % (i + 1); move_t t = m[i]; m[i] = m[j]; m[j] = t; }
    for (int i = 0; i < n; i++) {
        pos_t q = *p;
        make(&q, m[i]);
        int v = -search(&q, depth - 1, -1000000, -best, 1);
        if (v > best) { best = v; bi = i; }
    }
    return m[bi];
}

/* ------------------------------------------------------------ notação e estado */
static const char *pc_letter = " PCBTDR";   /* nomes em português: Cavalo, Bispo, Torre, Dama, Rei */
static void to_san(const pos_t *p, move_t mv, char *out) {
    int t = p->b[mv.from] > 0 ? p->b[mv.from] : -p->b[mv.from];
    char *o = out;
    if (mv.flags & 4) { strcpy(out, mv.to > mv.from ? "O-O" : "O-O-O"); o = out + strlen(out); }
    else {
        if (t != P) {
            *o++ = pc_letter[t];
            /* desambiguação */
            move_t m[256];
            int n = gen_legal(p, m);
            bool same_col = false, same_row = false, amb = false;
            for (int i = 0; i < n; i++)
                if (m[i].to == mv.to && m[i].from != mv.from && p->b[m[i].from] == p->b[mv.from]) {
                    amb = true;
                    if (COL(m[i].from) == COL(mv.from)) same_col = true;
                    if (ROW(m[i].from) == ROW(mv.from)) same_row = true;
                }
            if (amb) {
                if (!same_col) *o++ = (char)('a' + COL(mv.from));
                else if (!same_row) *o++ = (char)('8' - ROW(mv.from));
                else { *o++ = (char)('a' + COL(mv.from)); *o++ = (char)('8' - ROW(mv.from)); }
            }
        }
        if (mv.flags & 1) { if (t == P) *o++ = (char)('a' + COL(mv.from)); *o++ = 'x'; }
        *o++ = (char)('a' + COL(mv.to));
        *o++ = (char)('8' - ROW(mv.to));
        if (mv.promo) { *o++ = '='; *o++ = pc_letter[mv.promo]; }
    }
    pos_t q = *p;
    make(&q, mv);
    move_t m2[256];
    if (in_check(&q, q.side)) *o++ = gen_legal(&q, m2) ? '+' : '#';
    *o = 0;
}

static bool insufficient(const pos_t *p) {
    int minors = 0;
    for (int i = 0; i < 64; i++) {
        int t = p->b[i] > 0 ? p->b[i] : -p->b[i];
        if (t == P || t == R || t == Q) return false;
        if (t == N || t == B) minors++;
    }
    return minors <= 1;
}
static int repetitions(void) {
    int n = 0;
    for (int i = 0; i < X.nhist; i++)
        if (!memcmp(X.hist[i].b, X.pos.b, 64) && X.hist[i].side == X.pos.side && X.hist[i].castle == X.pos.castle) n++;
    return n + 1;
}

static void update_status(void) {
    X.nlegal = gen_legal(&X.pos, X.legal);
    bool chk = in_check(&X.pos, X.pos.side);
    const char *who = X.pos.side == 1 ? "Brancas" : "Pretas";
    X.over = false;
    if (!X.nlegal) {
        X.over = true;
        if (chk) snprintf(X.status, sizeof X.status, "Xeque-mate — %s vencem", X.pos.side == 1 ? "Pretas" : "Brancas");
        else snprintf(X.status, sizeof X.status, "Empate por afogamento");
    } else if (X.pos.half >= 100) { X.over = true; snprintf(X.status, sizeof X.status, "Empate — regra dos 50 lances"); }
    else if (insufficient(&X.pos)) { X.over = true; snprintf(X.status, sizeof X.status, "Empate — material insuficiente"); }
    else if (repetitions() >= 3) { X.over = true; snprintf(X.status, sizeof X.status, "Empate — repetição tripla"); }
    else snprintf(X.status, sizeof X.status, "%s jogam%s", who, chk ? " — Xeque!" : "");
    if (X.over) sound_alert();
}

static void play(move_t mv) {
    if (X.nhist >= 511) return;
    to_san(&X.pos, mv, X.san[X.nhist]);
    X.hist[X.nhist] = X.pos;
    X.mvhist[X.nhist] = mv;
    X.nhist++;
    make(&X.pos, mv);
    X.last_from = mv.from; X.last_to = mv.to;
    X.sel = -1;
    sound_click();
    update_status();
    if (X.vs_cpu && !X.over && X.pos.side == X.cpu_side) { X.thinking = true; X.think_at = timer_ticks() + 250; }
}

static void new_game(void) {
    pos_start(&X.pos);
    X.nhist = 0; X.sel = -1; X.thinking = false; X.promo_from = -1; X.last_from = X.last_to = -1;
    update_status();
    if (X.vs_cpu && X.cpu_side == 1) { X.thinking = true; X.think_at = timer_ticks() + 400; }
}
static void undo(void) {
    int k = X.vs_cpu ? 2 : 1;
    if (X.vs_cpu && X.nhist && X.pos.side != X.cpu_side * -1) k = 1;
    while (k-- > 0 && X.nhist > 0) { X.nhist--; X.pos = X.hist[X.nhist]; }
    X.thinking = false; X.sel = -1;
    X.last_from = X.nhist ? X.mvhist[X.nhist - 1].from : -1;
    X.last_to = X.nhist ? X.mvhist[X.nhist - 1].to : -1;
    update_status();
}

/* ------------------------------------------------------------ peças vetoriais */
static void draw_piece(surface_t *s, float x, float y, float sz, int pc) {
    int t = pc > 0 ? pc : -pc;
    bool white = pc > 0;
    u32 fill = white ? RGB(250, 248, 242) : RGB(38, 38, 42), line = white ? RGB(40, 40, 44) : RGB(220, 220, 224);
    float u = sz / 64.0f, cx = x + sz / 2;
    /* base comum */
    #define RR(a, b, w, h, r) do { gfx_round_rect(s, cx + (a) * u - 1.4f * u, y + (b) * u - 1.4f * u, (w) * u + 2.8f * u, (h) * u + 2.8f * u, (r) * u + 1.4f * u, line); } while (0)
    #define RF(a, b, w, h, r) gfx_round_rect(s, cx + (a) * u, y + (b) * u, (w) * u, (h) * u, (r) * u, fill)
    #define CI(a, b, r) do { gfx_circle(s, cx + (a) * u, y + (b) * u, (r) * u + 1.4f * u, line); gfx_circle(s, cx + (a) * u, y + (b) * u, (r) * u, fill); } while (0)
    gfx_shadow(s, (int)(cx - 18 * u), (int)(y + 50 * u), (int)(36 * u), (int)(6 * u), 3 * u, 3 * u, 60, (int)(2 * u));
    switch (t) {
    case P:
        RR(-15, 48, 30, 7, 3); RF(-15, 48, 30, 7, 3);
        { float b[] = {cx - 11 * u, y + 49 * u, cx - 6 * u, y + 30 * u, cx + 6 * u, y + 30 * u, cx + 11 * u, y + 49 * u};
          float bl[] = {cx - 13 * u, y + 50 * u, cx - 7.5f * u, y + 28.5f * u, cx + 7.5f * u, y + 28.5f * u, cx + 13 * u, y + 50 * u};
          gfx_polygon(s, bl, 4, line); gfx_polygon(s, b, 4, fill); }
        CI(0, 24, 8.5f);
        break;
    case R:
        RR(-17, 48, 34, 7, 3); RF(-17, 48, 34, 7, 3);
        RR(-12, 24, 24, 25, 2); RF(-12, 24, 24, 25, 2);
        RR(-15, 13, 30, 12, 2); RF(-15, 13, 30, 12, 2);
        for (int i = 0; i < 2; i++) gfx_fill(s, (int)(cx + (-5 + i * 8) * u), (int)(y + 12 * u), (int)(3 * u), (int)(6 * u), white ? RGB(205, 200, 190) : RGB(80, 80, 86));
        gfx_fill(s, (int)(cx - 12 * u), (int)(y + 26 * u), (int)(24 * u), MAX(1, (int)(1.2f * u)), line);
        break;
    case N: {
        RR(-17, 48, 34, 7, 3); RF(-17, 48, 34, 7, 3);
        float h[] = {cx - 12 * u, y + 48 * u, cx - 10 * u, y + 34 * u, cx - 2 * u, y + 26 * u, cx - 14 * u, y + 28 * u, cx - 16 * u, y + 20 * u,
                     cx - 4 * u, y + 10 * u, cx + 2 * u, y + 6 * u, cx + 4 * u, y + 11 * u, cx + 13 * u, y + 20 * u, cx + 15 * u, y + 48 * u};
        float hl[20];
        for (int i = 0; i < 10; i++) { hl[i * 2] = cx + (h[i * 2] - cx) * 1.12f; hl[i * 2 + 1] = y + 28 * u + (h[i * 2 + 1] - y - 28 * u) * 1.1f; }
        gfx_polygon(s, hl, 10, line);
        gfx_polygon(s, h, 10, fill);
        gfx_circle(s, cx - 4 * u, y + 17 * u, 1.8f * u, line);
        break; }
    case B:
        RR(-16, 48, 32, 7, 3); RF(-16, 48, 32, 7, 3);
        RR(-6, 30, 12, 19, 2); RF(-6, 30, 12, 19, 2);
        { float m[] = {cx, y + 6 * u, cx + 11 * u, y + 20 * u, cx + 8 * u, y + 32 * u, cx - 8 * u, y + 32 * u, cx - 11 * u, y + 20 * u};
          float ml[10];
          for (int i = 0; i < 5; i++) { ml[i * 2] = cx + (m[i * 2] - cx) * 1.14f; ml[i * 2 + 1] = y + 20 * u + (m[i * 2 + 1] - y - 20 * u) * 1.1f; }
          gfx_polygon(s, ml, 5, line); gfx_polygon(s, m, 5, fill); }
        gfx_line(s, cx + 1 * u, y + 14 * u, cx + 6 * u, y + 22 * u, 1.6f * u, line);
        CI(0, 5, 2.6f);
        break;
    case Q:
        RR(-17, 48, 34, 7, 3); RF(-17, 48, 34, 7, 3);
        { float c[] = {cx - 14 * u, y + 46 * u, cx - 17 * u, y + 16 * u, cx - 7 * u, y + 30 * u, cx, y + 12 * u, cx + 7 * u, y + 30 * u, cx + 17 * u, y + 16 * u, cx + 14 * u, y + 46 * u};
          float cl[14];
          for (int i = 0; i < 7; i++) { cl[i * 2] = cx + (c[i * 2] - cx) * 1.1f; cl[i * 2 + 1] = y + 30 * u + (c[i * 2 + 1] - y - 30 * u) * 1.08f; }
          gfx_polygon(s, cl, 7, line); gfx_polygon(s, c, 7, fill); }
        CI(-17, 14, 3); CI(0, 10, 3); CI(17, 14, 3);
        break;
    case K:
        RR(-17, 48, 34, 7, 3); RF(-17, 48, 34, 7, 3);
        { float c[] = {cx - 14 * u, y + 47 * u, cx - 16 * u, y + 26 * u, cx - 6 * u, y + 20 * u, cx + 6 * u, y + 20 * u, cx + 16 * u, y + 26 * u, cx + 14 * u, y + 47 * u};
          float cl[12];
          for (int i = 0; i < 6; i++) { cl[i * 2] = cx + (c[i * 2] - cx) * 1.1f; cl[i * 2 + 1] = y + 34 * u + (c[i * 2 + 1] - y - 34 * u) * 1.1f; }
          gfx_polygon(s, cl, 6, line); gfx_polygon(s, c, 6, fill); }
        RR(-2.5f, 5, 5, 16, 1); RR(-7, 9, 14, 5, 1);
        RF(-2.5f, 5, 5, 16, 1); RF(-7, 9, 14, 5, 1);
        break;
    }
    #undef RR
    #undef RF
    #undef CI
}

/* ------------------------------------------------------------ janela */
static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_fill(s, 0, w->toolbar_h, u->cw, u->ch - w->toolbar_h, T.dark ? RGB(36, 36, 38) : RGB(232, 230, 226));
    /* barra de ferramentas */
    static const char *modes[2] = {"Contra o computador", "Dois jogadores"};
    int mi = ui_segmented(u, UI_ID, rect_make(u->cw - 12 - 300, TOOLBAR_Y(28), 300, 28), modes, 2, X.vs_cpu ? 0 : 1);
    if ((mi == 0) != X.vs_cpu) { X.vs_cpu = mi == 0; X.thinking = false; if (X.vs_cpu && X.pos.side == X.cpu_side && !X.over) { X.thinking = true; X.think_at = timer_ticks() + 300; } }
    if (X.vs_cpu) {
        static const char *lv[3] = {"Fácil", "Médio", "Difícil"};
        X.level = ui_popup(u, UI_ID, rect_make(u->cw - 12 - 300 - 8 - 96, TOOLBAR_Y(28), 96, 28), lv, 3, X.level);
    }
    /* tabuleiro */
    int side_w = 220;
    int bs = MIN(u->cw - side_w - 48, u->ch - w->toolbar_h - 70);
    bs = MAX(240, bs) / 8 * 8;
    int sq = bs / 8, bx = 24, by = w->toolbar_h + 24;
    gfx_shadow(s, bx, by, bs, bs, 6, 14, 90, 6);
    gfx_round_rect(s, bx - 10, by - 10, bs + 20, bs + 20, 8, RGB(92, 64, 44));
    u32 light = RGB(238, 220, 186), dark = RGB(178, 128, 92);
    bool mine = !X.over && !X.thinking && (!X.vs_cpu || X.pos.side != X.cpu_side) && X.promo_from < 0;
    int chk = in_check(&X.pos, X.pos.side) ? king_sq(&X.pos, X.pos.side) : -1;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            int sqi = X.flip ? (7 - r) * 8 + (7 - c) : r * 8 + c;
            int x = bx + c * sq, y = by + r * sq;
            u32 col = (r + c) % 2 ? dark : light;
            if (sqi == X.last_from || sqi == X.last_to) col = color_mix(col, RGB(246, 230, 90), 120);
            if (sqi == X.sel) col = color_mix(col, RGB(90, 170, 240), 140);
            gfx_fill(s, x, y, sq, sq, col);
            if (sqi == chk) { gfx_circle(s, x + sq / 2.0f, y + sq / 2.0f, sq * 0.48f, RGBA(255, 40, 30, 110)); }
        }
    /* coordenadas */
    for (int i = 0; i < 8; i++) {
        char f[2] = {(char)('a' + (X.flip ? 7 - i : i)), 0}, rk[2] = {(char)('8' - (X.flip ? 7 - i : i)), 0};
        text_draw(s, font_get(FONT_SEMIBOLD), 10, bx + i * sq + sq - 9, by + bs - 3, (i % 2) ? light : dark, f);
        text_draw(s, font_get(FONT_SEMIBOLD), 10, bx + 3, by + i * sq + 12, (i % 2) ? light : dark, rk);
    }
    /* peças e alvos */
    for (int i = 0; i < 64; i++) {
        int r = X.flip ? 7 - ROW(i) : ROW(i), c = X.flip ? 7 - COL(i) : COL(i);
        int x = bx + c * sq, y = by + r * sq;
        if (X.pos.b[i]) draw_piece(s, x + sq * 0.06f, y + sq * 0.04f, sq * 0.88f, X.pos.b[i]);
        if (X.sel >= 0)
            for (int k = 0; k < X.nlegal; k++)
                if (X.legal[k].from == X.sel && X.legal[k].to == i) {
                    if (X.pos.b[i]) gfx_ring_arc(s, x + sq / 2.0f, y + sq / 2.0f, sq * 0.44f, sq * 0.07f, 0, 2 * PI_F, RGBA(30, 30, 30, 90));
                    else gfx_circle(s, x + sq / 2.0f, y + sq / 2.0f, sq * 0.15f, RGBA(30, 30, 30, 80));
                    break;
                }
    }
    /* cliques no tabuleiro */
    rect_t board = rect_make(bx, by, bs, bs);
    if (mine && ui_clicked(u, UI_ID, board)) {
        int c = (u->mx - bx) / sq, r = (u->my - by) / sq;
        int sqi = X.flip ? (7 - r) * 8 + (7 - c) : r * 8 + c;
        i8 pc = X.pos.b[sqi];
        bool moved = false;
        if (X.sel >= 0) {
            for (int k = 0; k < X.nlegal; k++)
                if (X.legal[k].from == X.sel && X.legal[k].to == sqi) {
                    if (X.legal[k].promo) { X.promo_from = X.sel; X.promo_to = sqi; }
                    else play(X.legal[k]);
                    moved = true;
                    break;
                }
        }
        if (!moved) X.sel = (pc && (pc > 0) == (X.pos.side > 0) && X.sel != sqi) ? sqi : -1;
    }
    if (ui_hit(u, board) && mine) u->cursor = CURSOR_HAND;
    /* escolha da promoção */
    if (X.promo_from >= 0) {
        rect_t pr = rect_make(bx + bs / 2 - 2 * sq - 12, by + bs / 2 - sq / 2 - 12, 4 * sq + 24, sq + 24);
        glass_panel(s, pr.x, pr.y, pr.w, pr.h, 14, GLASS_REGULAR);
        static const int opts[4] = {Q, R, B, N};
        for (int i = 0; i < 4; i++) {
            rect_t o = rect_make(pr.x + 12 + i * sq, pr.y + 12, sq, sq);
            if (ui_hit(u, o)) gfx_round_rect(s, o.x, o.y, o.w, o.h, 8, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 20));
            draw_piece(s, o.x + sq * 0.06f, o.y + sq * 0.04f, sq * 0.88f, opts[i] * X.pos.side);
            if (ui_clicked(u, 40000 + i, o))
                for (int k = 0; k < X.nlegal; k++)
                    if (X.legal[k].from == X.promo_from && X.legal[k].to == X.promo_to && X.legal[k].promo == opts[i]) { X.promo_from = -1; play(X.legal[k]); break; }
        }
        if (u->key == KEY_ESC) { X.promo_from = -1; X.sel = -1; u->key_used = true; }
    }
    /* status abaixo do tabuleiro */
    ui_label_center(u, rect_make(bx, by + bs + 14, bs, 22), X.thinking ? "O computador está pensando…" : X.status, FONT_SEMIBOLD, 15, X.over ? T.accent : T.text);
    /* painel lateral: lances */
    rect_t pnl = rect_make(bx + bs + 30, by - 10, u->cw - (bx + bs + 30) - 16, bs + 20);
    gfx_round_rect(s, pnl.x, pnl.y, pnl.w, pnl.h, 12, T.content_bg);
    ui_label(u, pnl.x + 14, pnl.y + 12, "Lances", FONT_BOLD, 14, T.text);
    rect_t lv = rect_make(pnl.x, pnl.y + 38, pnl.w, pnl.h - 38 - 50);
    rect_t pc = surface_push_clip(s, lv);
    int y = lv.y - X.mscroll;
    for (int i = 0; i < X.nhist; i += 2) {
        char num[8];
        snprintf(num, sizeof num, "%d.", i / 2 + 1);
        if (i / 2 % 2) gfx_fill(s, pnl.x + 6, y, pnl.w - 12, 22, T.alt_row);
        text_draw(s, font_get(FONT_REGULAR), 12.5f, pnl.x + 14, y + 16, T.text2, num);
        text_draw(s, font_get(FONT_MEDIUM), 12.5f, pnl.x + 50, y + 16, T.text, X.san[i]);
        if (i + 1 < X.nhist) text_draw(s, font_get(FONT_MEDIUM), 12.5f, pnl.x + 50 + (pnl.w - 60) / 2, y + 16, T.text, X.san[i + 1]);
        y += 22;
    }
    surface_pop_clip(s, pc);
    int content = (X.nhist + 1) / 2 * 22 + 10;
    static int last_n;
    if (last_n != X.nhist) { last_n = X.nhist; X.mscroll = MAX(0, content - lv.h); }
    ui_scroll(u, UI_ID, lv, content, &X.mscroll);
    int bw = (pnl.w - 36) / 3;
    if (ui_button_style(u, UI_ID, rect_make(pnl.x + 12, pnl.y + pnl.h - 40, bw, 28), "Novo", BTN_GRAY, true, NULL)) new_game();
    if (ui_button_style(u, UI_ID, rect_make(pnl.x + 18 + bw, pnl.y + pnl.h - 40, bw, 28), "Desfazer", BTN_GRAY, X.nhist > 0 && !X.thinking, NULL)) undo();
    if (ui_button_style(u, UI_ID, rect_make(pnl.x + 24 + 2 * bw, pnl.y + pnl.h - 40, bw, 28), "Girar", BTN_GRAY, true, NULL)) X.flip = !X.flip;
    /* lance do computador (depois de desenhar "pensando") */
    if (X.thinking) {
        u->repaint = true;
        if (timer_ticks() >= X.think_at) {
            int depth = X.level == 0 ? 1 : X.level == 1 ? 3 : 4;
            move_t mv = best_move(&X.pos, depth);
            X.thinking = false;
            play(mv);
        }
    }
    if ((u->mods & MOD_CMD) && u->key == 'z') { undo(); u->key_used = true; }
    if ((u->mods & MOD_CMD) && u->key == 'n') { new_game(); u->key_used = true; }
}

static void on_close(window_t *w) { X.w = NULL; X.thinking = false; }
static void launch(app_t *a) {
    if (X.w) { wm_focus(X.w); return; }
    X.w = wm_create(&app_xadrez, "Xadrez", -1, -1, 860, 660, WIN_RESIZABLE);
    if (!X.w) return;
    X.w->paint = paint;
    X.w->on_close = on_close;
    X.w->toolbar_h = TOOLBAR_H;
    X.w->min_w = 640;
    X.w->min_h = 520;
    if (!X.nhist) new_game();
}

enum { M_NEW = 1, M_UNDO, M_FLIP, M_WHITE, M_BLACK };
static menu_item_t m_game[] = {{"Novo Jogo", "⌘N", M_NEW}, {"Desfazer Lance", "⌘Z", M_UNDO}, {NULL},
                               {"Jogar com as Brancas", NULL, M_WHITE}, {"Jogar com as Pretas", NULL, M_BLACK}, {NULL}, {"Girar Tabuleiro", "⌘R", M_FLIP}};
static menu_t menus[] = {{"Jogo", m_game, 7}};
static void on_menu(app_t *a, int act) {
    if (act == M_NEW) new_game();
    else if (act == M_UNDO) undo();
    else if (act == M_FLIP) X.flip = !X.flip;
    else if (act == M_WHITE) { X.cpu_side = -1; X.flip = false; X.vs_cpu = true; new_game(); }
    else if (act == M_BLACK) { X.cpu_side = 1; X.flip = true; X.vs_cpu = true; new_game(); }
    if (X.w) wm_invalidate(X.w);
}

static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(120, 120, 126), RGB(44, 44, 48));
    draw_piece(s, x + 8 * u, y + 6 * u, 36 * u, N);
    draw_piece(s, x + 22 * u, y + 14 * u, 36 * u, -K);
}

app_t app_xadrez = {.name = "Xadrez", .exec = "xadrez", .draw_icon = icon, .launch = launch, .on_menu = on_menu, .menus = menus, .nmenus = 1};
