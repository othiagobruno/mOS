#pragma once
/* posix_spawn FILEPORT_DUP2: o fd de destino fica com o caminho, o mode e as flags da porta.
 * CLOEXEC não passa. Devolve 0, ou -1 se a porta não tem descritor. */
int fileport_dup2(unsigned char *used, int *flags, unsigned *mode, char *path, unsigned pathcap,
                  int snap_used, int snap_flags, unsigned snap_mode, const char *snap_path);
