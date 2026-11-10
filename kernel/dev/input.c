/* mOS — fila de eventos de entrada + controlador PS/2 (teclado e mouse) */
#include "kernel.h"
#include "kbd_mod.h"

_Static_assert(KEY_LSHIFT == KBD_KEY_LSHIFT && KEY_RSHIFT == KBD_KEY_RSHIFT, "shift");
_Static_assert(KEY_LCTRL == KBD_KEY_LCTRL && KEY_LALT == KBD_KEY_LALT && KEY_LCMD == KBD_KEY_LCMD, "mods esquerdos");
_Static_assert(KEY_CAPS == KBD_KEY_CAPS && KEY_RCTRL == KBD_KEY_RCTRL && KEY_RALT == KBD_KEY_RALT && KEY_RCMD == KBD_KEY_RCMD, "mods direitos");
_Static_assert(MOD_SHIFT == 1 && MOD_CTRL == 2 && MOD_ALT == 4 && MOD_CMD == 8, "máscara");

/* ---------------------------------------------------------- fila */
#define QSIZE 512
static input_event_t queue[QSIZE];
static volatile u32 qhead, qtail;
static u16 mods;

void input_push(input_event_t *e) {
    u64 f = irq_save();
    /* agrega movimentos consecutivos do mouse para não lotar a fila */
    if (e->type == EV_MOUSE && qhead != qtail) {
        input_event_t *last = &queue[(qhead - 1) % QSIZE];
        if (last->type == EV_MOUSE && last->buttons == e->buttons && !last->dz && !e->dz) {
            last->dx += e->dx; last->dy += e->dy;
            irq_restore(f);
            return;
        }
    }
    /* posição absoluta (tablet USB): só a mais recente importa */
    if (e->type == EV_MOUSE_ABS && qhead != qtail) {
        input_event_t *last = &queue[(qhead - 1) % QSIZE];
        if (last->type == EV_MOUSE_ABS && last->buttons == e->buttons && !last->dz && !e->dz) {
            last->dx = e->dx; last->dy = e->dy;
            irq_restore(f);
            return;
        }
    }
    u32 n = (qhead + 1) % QSIZE;
    if (n != qtail) { queue[qhead] = *e; qhead = n; }
    irq_restore(f);
}

bool input_pop(input_event_t *e) {
    u64 f = irq_save();
    bool ok = qhead != qtail;
    if (ok) { *e = queue[qtail]; qtail = (qtail + 1) % QSIZE; }
    irq_restore(f);
    return ok;
}

u16 input_mods(void) { return mods; }

/* ---------------------------------------------------------- teclado */
int g_kbd_layout = 0; /* 0 = ABNT2 (Brasil), 1 = US */

/* scancode set 1 → caractere; [normal, shift] */
static const u16 us_map[128][2] = {
    [0x01] = {KEY_ESC, KEY_ESC}, [0x02] = {'1', '!'}, [0x03] = {'2', '@'}, [0x04] = {'3', '#'},
    [0x05] = {'4', '$'}, [0x06] = {'5', '%'}, [0x07] = {'6', '^'}, [0x08] = {'7', '&'},
    [0x09] = {'8', '*'}, [0x0A] = {'9', '('}, [0x0B] = {'0', ')'}, [0x0C] = {'-', '_'},
    [0x0D] = {'=', '+'}, [0x0E] = {KEY_BACKSPACE, KEY_BACKSPACE}, [0x0F] = {KEY_TAB, KEY_TAB},
    [0x10] = {'q', 'Q'}, [0x11] = {'w', 'W'}, [0x12] = {'e', 'E'}, [0x13] = {'r', 'R'},
    [0x14] = {'t', 'T'}, [0x15] = {'y', 'Y'}, [0x16] = {'u', 'U'}, [0x17] = {'i', 'I'},
    [0x18] = {'o', 'O'}, [0x19] = {'p', 'P'}, [0x1A] = {'[', '{'}, [0x1B] = {']', '}'},
    [0x1C] = {KEY_ENTER, KEY_ENTER}, [0x1E] = {'a', 'A'}, [0x1F] = {'s', 'S'}, [0x20] = {'d', 'D'},
    [0x21] = {'f', 'F'}, [0x22] = {'g', 'G'}, [0x23] = {'h', 'H'}, [0x24] = {'j', 'J'},
    [0x25] = {'k', 'K'}, [0x26] = {'l', 'L'}, [0x27] = {';', ':'}, [0x28] = {'\'', '"'},
    [0x29] = {'`', '~'}, [0x2B] = {'\\', '|'}, [0x2C] = {'z', 'Z'}, [0x2D] = {'x', 'X'},
    [0x2E] = {'c', 'C'}, [0x2F] = {'v', 'V'}, [0x30] = {'b', 'B'}, [0x31] = {'n', 'N'},
    [0x32] = {'m', 'M'}, [0x33] = {',', '<'}, [0x34] = {'.', '>'}, [0x35] = {'/', '?'},
    [0x37] = {'*', '*'}, [0x39] = {' ', ' '}, [0x56] = {'\\', '|'},
    [0x3B] = {KEY_F1, KEY_F1}, [0x3C] = {KEY_F2, KEY_F2}, [0x3D] = {KEY_F3, KEY_F3}, [0x3E] = {KEY_F4, KEY_F4},
    [0x3F] = {KEY_F5, KEY_F5}, [0x40] = {KEY_F6, KEY_F6}, [0x41] = {KEY_F7, KEY_F7}, [0x42] = {KEY_F8, KEY_F8},
    [0x43] = {KEY_F9, KEY_F9}, [0x44] = {KEY_F10, KEY_F10}, [0x57] = {KEY_F11, KEY_F11}, [0x58] = {KEY_F12, KEY_F12},
    [0x47] = {'7', '7'}, [0x48] = {'8', '8'}, [0x49] = {'9', '9'}, [0x4A] = {'-', '-'},
    [0x4B] = {'4', '4'}, [0x4C] = {'5', '5'}, [0x4D] = {'6', '6'}, [0x4E] = {'+', '+'},
    [0x4F] = {'1', '1'}, [0x50] = {'2', '2'}, [0x51] = {'3', '3'}, [0x52] = {'0', '0'}, [0x53] = {',', ','},
};

/* marcadores de teclas mortas */
#define DEAD_ACUTE 0x10001
#define DEAD_GRAVE 0x10002
#define DEAD_TILDE 0x10003
#define DEAD_CIRC  0x10004
#define DEAD_DIAER 0x10005

static i32 abnt2(u8 sc, bool shift) {
    switch (sc) {
    case 0x07: return shift ? DEAD_DIAER : '6';
    case 0x1A: return shift ? DEAD_GRAVE : DEAD_ACUTE;
    case 0x1B: return shift ? '{' : '[';
    case 0x27: return shift ? 0xC7 : 0xE7;          /* Ç ç */
    case 0x28: return shift ? DEAD_CIRC : DEAD_TILDE;
    case 0x29: return shift ? '"' : '\'';
    case 0x2B: return shift ? '}' : ']';
    case 0x35: return shift ? ':' : ';';
    case 0x56: return shift ? '|' : '\\';
    case 0x73: return shift ? '?' : '/';
    case 0x7E: return '.';
    default: return 0;
    }
}

static i32 compose(i32 dead, i32 c) {
    static const char *base = "aeiouAEIOUnNcC";
    static const u16 acute[] = {0xE1, 0xE9, 0xED, 0xF3, 0xFA, 0xC1, 0xC9, 0xCD, 0xD3, 0xDA, 0, 0, 0xE7, 0xC7};
    static const u16 grave[] = {0xE0, 0xE8, 0xEC, 0xF2, 0xF9, 0xC0, 0xC8, 0xCC, 0xD2, 0xD9, 0, 0, 0, 0};
    static const u16 tilde[] = {0xE3, 0, 0, 0xF5, 0, 0xC3, 0, 0, 0xD5, 0, 0xF1, 0xD1, 0, 0};
    static const u16 circ[]  = {0xE2, 0xEA, 0xEE, 0xF4, 0xFB, 0xC2, 0xCA, 0xCE, 0xD4, 0xDB, 0, 0, 0, 0};
    static const u16 diaer[] = {0xE4, 0xEB, 0xEF, 0xF6, 0xFC, 0xC4, 0xCB, 0xCF, 0xD6, 0xDC, 0, 0, 0, 0};
    const u16 *t = dead == DEAD_ACUTE ? acute : dead == DEAD_GRAVE ? grave : dead == DEAD_TILDE ? tilde : dead == DEAD_CIRC ? circ : diaer;
    for (int i = 0; base[i]; i++) if (base[i] == c && t[i]) return t[i];
    return 0;
}

static i32 dead_char(i32 d) {
    return d == DEAD_ACUTE ? 0xB4 : d == DEAD_GRAVE ? '`' : d == DEAD_TILDE ? '~' : d == DEAD_CIRC ? '^' : 0xA8;
}

static bool e0, caps;
static i32 pending_dead;
static u16 phys; /* esquerda e direita de cada modificador, para soltar um não apagar o outro */

static void emit_key(i32 key, bool down) {
    input_event_t ev = {0};
    ev.type = down ? EV_KEY_DOWN : EV_KEY_UP;
    ev.key = key;
    ev.mods = mods;
    input_push(&ev);
}

static void kbd_scancode(u8 sc, bool ext, bool up);

static void kbd_byte(u8 b) {
    if (b == 0xE0) { e0 = true; return; }
    bool ext = e0;
    e0 = false;
    kbd_scancode(b & 0x7F, ext, b & 0x80);
}

/* entrada comum para teclados não-PS/2 (USB HID): código = scancode set 1, bit 0x100 = prefixo E0 */
void input_kbd_scancode(u16 code, bool up) {
    u64 f = irq_save();
    kbd_scancode(code & 0x7F, code & 0x100, up);
    irq_restore(f);
}

/* traduz um scancode set 1 (já sem prefixo) em eventos, com layout ABNT2/US e teclas mortas */
static void kbd_scancode(u8 sc, bool ext, bool up) {
    int key = 0, mr = kbd_mod_apply(&mods, &phys, sc, ext, up, &key);
    /* Shift, Ctrl, Option e Command sozinhos: o evento chega ao first responder como flagsChanged */
    if (mr == 1) { emit_key(key, !up); return; }
    if (mr == 2) return;
    if (sc == 0x3A) {
        if (!up) { caps = !caps; mods ^= MOD_CAPS; }
        emit_key(KBD_KEY_CAPS, !up);
        return;
    }

    key = 0;
    if (ext) {
        switch (sc) {
        case 0x48: key = KEY_UP; break;    case 0x50: key = KEY_DOWN; break;
        case 0x4B: key = KEY_LEFT; break;  case 0x4D: key = KEY_RIGHT; break;
        case 0x47: key = KEY_HOME; break;  case 0x4F: key = KEY_END; break;
        case 0x49: key = KEY_PGUP; break;  case 0x51: key = KEY_PGDN; break;
        case 0x53: key = KEY_DELETE; break; case 0x52: key = KEY_INSERT; break;
        case 0x1C: key = KEY_ENTER; break; case 0x35: key = '/'; break;
        default: return;
        }
        emit_key(key, !up);
        return;
    }
    bool shift = (mods & MOD_SHIFT) != 0;
    if (g_kbd_layout == 0) key = abnt2(sc, shift);
    if (!key && sc < 128) key = us_map[sc][shift ? 1 : 0];
    if (!key) return;
    if (caps && key < 128 && isalpha(key)) key ^= 0x20;
    if (caps && (key == 0xE7 || key == 0xC7)) key ^= 0x20;
    if (up) {
        if (key < 0x10000) emit_key(key, false);
        return;
    }
    if (key >= 0x10000) {
        if (pending_dead) { emit_key(dead_char(pending_dead), true); pending_dead = 0; }
        else pending_dead = key;
        return;
    }
    if (pending_dead) {
        i32 c = compose(pending_dead, key);
        if (c) key = c;
        else if (key == ' ') key = dead_char(pending_dead);
        else emit_key(dead_char(pending_dead), true);
        pending_dead = 0;
    }
    emit_key(key, true);
}

/* ---------------------------------------------------------- mouse */
static u8 mpkt[4];
static int midx, mpkt_len = 3;

static void mouse_byte(u8 b) {
    if (midx == 0 && !(b & 0x08)) return; /* ressincroniza */
    mpkt[midx++] = b;
    if (midx < mpkt_len) return;
    midx = 0;
    if (mpkt[0] & 0xC0) return; /* overflow */
    input_event_t ev = {0};
    ev.type = EV_MOUSE;
    ev.buttons = mpkt[0] & 7;
    ev.dx = (i32)mpkt[1] - ((mpkt[0] & 0x10) ? 256 : 0);
    ev.dy = -((i32)mpkt[2] - ((mpkt[0] & 0x20) ? 256 : 0));
    if (mpkt_len == 4) ev.dz = (i8)(mpkt[3] << 4) >> 4;
    ev.mods = mods;
    input_push(&ev);
}

/* ---------------------------------------------------------- controlador 8042 */
static void ps2_wait_w(void) { for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {} }
static bool ps2_wait_r(void) { for (int i = 0; i < 100000; i++) if (inb(0x64) & 1) return true; return false; }
static void ps2_cmd(u8 c) { ps2_wait_w(); outb(0x64, c); }
static void ps2_data(u8 d) { ps2_wait_w(); outb(0x60, d); }
static u8 ps2_read(void) { return ps2_wait_r() ? inb(0x60) : 0; }
static void mouse_write(u8 v) { ps2_cmd(0xD4); ps2_data(v); ps2_read(); }

static void ps2_irq(int_frame_t *f) {
    (void)f;
    while (inb(0x64) & 1) {
        u8 st = inb(0x64);
        u8 b = inb(0x60);
        if (st & 0x20) mouse_byte(b); else kbd_byte(b);
    }
}

void ps2_init(void) {
    ps2_cmd(0xAD); ps2_cmd(0xA7);
    while (inb(0x64) & 1) inb(0x60);
    ps2_cmd(0x20);
    u8 cfg = ps2_read();
    cfg |= 0x03;        /* IRQ1 e IRQ12 */
    cfg &= ~0x30;       /* clocks habilitados */
    cfg |= 0x40;        /* tradução para scancode set 1 */
    ps2_cmd(0x60); ps2_data(cfg);
    ps2_cmd(0xAE); ps2_cmd(0xA8);
    /* mouse: padrões, depois sequência mágica do IntelliMouse para roda */
    mouse_write(0xF6);
    mouse_write(0xF3); mouse_write(200);
    mouse_write(0xF3); mouse_write(100);
    mouse_write(0xF3); mouse_write(80);
    ps2_cmd(0xD4); ps2_data(0xF2); ps2_read();
    u8 id = ps2_read();
    if (id == 3 || id == 4) mpkt_len = 4;
    mouse_write(0xF3); mouse_write(100);
    mouse_write(0xF4);
    while (inb(0x64) & 1) inb(0x60);
    irq_install(1, ps2_irq);
    irq_install(12, ps2_irq);
    kprintf("PS/2: teclado + mouse (id %d, %s)\n", id, mpkt_len == 4 ? "com roda" : "3 botões");
}
