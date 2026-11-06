/*
 * mOS SDK — mach/mach_vm.h (no macOS é gerado pelo MIG; os cabeçalhos do zig não trazem). As funções estão
 * em userland/libsystem/mach.c.
 */
#pragma once
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/vm_region.h>

#ifdef __cplusplus
extern "C" {
#endif
kern_return_t mach_vm_allocate(vm_map_t target, mach_vm_address_t *address, mach_vm_size_t size, int flags);
kern_return_t mach_vm_deallocate(vm_map_t target, mach_vm_address_t address, mach_vm_size_t size);
kern_return_t mach_vm_protect(vm_map_t target, mach_vm_address_t address, mach_vm_size_t size, boolean_t set_maximum, vm_prot_t new_protection);
kern_return_t mach_vm_region(vm_map_read_t target_task, mach_vm_address_t *address, mach_vm_size_t *size, vm_region_flavor_t flavor,
                             vm_region_info_t info, mach_msg_type_number_t *infoCnt, mach_port_t *object_name);
#ifdef __cplusplus
}
#endif
