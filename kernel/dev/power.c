/* mOS — estado da bateria: um driver registrado (hardware) ou a bateria simulada de teste */
#include "power.h"
#include "vfs.h"

static const battery_driver_t *drv;

void battery_register(const battery_driver_t *d) { drv = d; kprintf("energia: bateria via %s\n", d->name); }

/* bateria simulada: começa na carga escrita no arquivo e descarrega 1% a cada 2 minutos;
 * abaixo de 20% "liga o carregador" e sobe até 100%, para exercitar os dois estados */
static bool sim_read(battery_status_t *st) {
    static int checked = -1, start = 80;
    static u64 t0;
    if (checked < 0) {
        vnode_t *f = vfs_lookup("/Sistema/Biblioteca/bateria-simulada");
        checked = f != NULL;
        if (f && f->data && f->size) {
            int v = 0;
            for (usize i = 0; i < f->size && f->data[i] >= '0' && f->data[i] <= '9'; i++) v = v * 10 + (f->data[i] - '0');
            if (v > 0 && v <= 100) start = v;
        }
        t0 = timer_ticks();
    }
    if (!checked) return false;
    int steps = (int)((timer_ticks() - t0) / 120000);
    int span = start - 19;                         /* descarga até 19%, depois carga até 100% */
    int cyc = span + 81;
    int k = cyc > 0 ? steps % cyc : 0;
    bool charging = k >= span;
    st->present = true;
    st->percent = charging ? 19 + (k - span) : start - k;
    st->charging = charging && st->percent < 100;
    st->on_ac = charging;
    st->minutes_left = charging ? (100 - st->percent) * 2 : st->percent * 5;
    st->source = st->on_ac ? "Adaptador de Energia" : "Bateria";
    st->driver = "Bateria simulada (teste)";
    return true;
}

void battery_status(battery_status_t *st) {
    memset(st, 0, sizeof *st);
    st->minutes_left = -1;
    st->source = "Adaptador de Energia";
    if (drv && drv->read && drv->read(st)) return;
    memset(st, 0, sizeof *st);
    st->minutes_left = -1;
    st->source = "Adaptador de Energia";
    sim_read(st);
}
