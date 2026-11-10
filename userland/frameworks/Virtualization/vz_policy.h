/* Política do Virtualization.framework no mOS. Sem hipervisor: nada responde que a VM existe. */
#pragma once
#define VZ_UNSUPPORTED_MSG "não suportado"
#define VZ_ERROR_DOMAIN "VZErrorDomain"
#define VZ_ERROR_CODE 1
static inline int vz_is_supported(void) { return 0; }
