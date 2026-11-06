/*
 * mOS SDK — <sysdir.h> do macOS (o zig não traz): diretórios padrão do sistema (o que o FileManager usa para
 * .documentDirectory, .cachesDirectory...). Declarações com o ABI do macOS; a implementação fica no libSystem do mOS.
 */
#ifndef __SYSDIR_H__
#define __SYSDIR_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYSDIR_DIRECTORY_APPLICATION = 1,
    SYSDIR_DIRECTORY_DEMO_APPLICATION = 2,
    SYSDIR_DIRECTORY_DEVELOPER_APPLICATION = 3,
    SYSDIR_DIRECTORY_ADMIN_APPLICATION = 4,
    SYSDIR_DIRECTORY_LIBRARY = 5,
    SYSDIR_DIRECTORY_DEVELOPER = 6,
    SYSDIR_DIRECTORY_USER = 7,
    SYSDIR_DIRECTORY_DOCUMENTATION = 8,
    SYSDIR_DIRECTORY_DOCUMENT = 9,
    SYSDIR_DIRECTORY_CORESERVICE = 10,
    SYSDIR_DIRECTORY_AUTOSAVED_INFORMATION = 11,
    SYSDIR_DIRECTORY_DESKTOP = 12,
    SYSDIR_DIRECTORY_CACHES = 13,
    SYSDIR_DIRECTORY_APPLICATION_SUPPORT = 14,
    SYSDIR_DIRECTORY_DOWNLOADS = 15,
    SYSDIR_DIRECTORY_INPUT_METHODS = 16,
    SYSDIR_DIRECTORY_MOVIES = 17,
    SYSDIR_DIRECTORY_MUSIC = 18,
    SYSDIR_DIRECTORY_PICTURES = 19,
    SYSDIR_DIRECTORY_PRINTER_DESCRIPTION = 20,
    SYSDIR_DIRECTORY_SHARED_PUBLIC = 21,
    SYSDIR_DIRECTORY_PREFERENCE_PANES = 22,
    SYSDIR_DIRECTORY_APPLICATION_SCRIPTS = 23,
    SYSDIR_DIRECTORY_ITEM_REPLACEMENT = 99,
    SYSDIR_DIRECTORY_ALL_APPLICATIONS = 100,
    SYSDIR_DIRECTORY_ALL_LIBRARIES = 101,
} sysdir_search_path_directory_t;

typedef enum {
    SYSDIR_DOMAIN_MASK_USER = (1UL << 0),
    SYSDIR_DOMAIN_MASK_LOCAL = (1UL << 1),
    SYSDIR_DOMAIN_MASK_NETWORK = (1UL << 2),
    SYSDIR_DOMAIN_MASK_SYSTEM = (1UL << 3),
    SYSDIR_DOMAIN_MASK_ALL = 0x0ffff,
} sysdir_search_path_domain_mask_t;

typedef unsigned int sysdir_search_path_enumeration_state;

sysdir_search_path_enumeration_state sysdir_start_search_path_enumeration(sysdir_search_path_directory_t dir,
                                                                          sysdir_search_path_domain_mask_t domainMask);
sysdir_search_path_enumeration_state sysdir_get_next_search_path_enumeration(sysdir_search_path_enumeration_state state,
                                                                             char *path);

#ifdef __cplusplus
}
#endif

#endif
