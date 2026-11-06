/* mOS — estado de energia (bateria) para a barra de menus e os Ajustes.
 * Fontes: driver de bateria registrado (futuro: ACPI _BST/_BIF) ou a bateria simulada de teste
 * (arquivo /Sistema/Biblioteca/bateria-simulada com a carga inicial em %, como o Wi-Fi simulado).
 * Sem nenhuma fonte, present = false e a interface esconde o ícone (como num computador de mesa). */
#pragma once
#include "kernel.h"

typedef struct {
    bool present;          /* existe bateria */
    int  percent;          /* 0..100 */
    bool charging;         /* carregando agora */
    bool on_ac;            /* ligado na tomada */
    int  minutes_left;     /* estimativa (-1 = calculando) */
    const char *source;    /* "Bateria" / "Adaptador de Energia" */
    const char *driver;    /* nome do provedor */
} battery_status_t;

typedef struct {
    const char *name;
    bool (*read)(battery_status_t *st);   /* devolve false se a leitura falhou */
} battery_driver_t;

void battery_register(const battery_driver_t *d);   /* drivers de hardware se registram aqui */
void battery_status(battery_status_t *st);
