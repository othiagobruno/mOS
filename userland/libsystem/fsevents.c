/*
 * FSEvents (CoreServices). O kernel não tem fila de eventos de vnode: a entrega é a
 * varredura de fsevents_core.inc (stat + readdir neste processo). O que o VFS não
 * guarda está em docs/fsevents.md — este arquivo não muda o kernel.
 */
#include "sys.h"
#include "fsevents_core.inc"
