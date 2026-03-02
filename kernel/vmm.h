#pragma once

#include "e820.h"

#include <stdint.h>

#define HHDM_BASE 0xFFFF800000000000ull

void vmm_map_usable(const struct e820_map *map);
void vmm_hhdm_init(void);
/* New PML4 with kernel/HHDM entries from the boot tables; user half empty. */
uint64_t vmm_clone_pml4(void);
uint64_t vmm_boot_cr3(void);
void vmm_set_cr3(uint64_t cr3);
/* Map/unmap a user page in the given PML4 (task CR3 phys). */
int vmm_map_user(uint64_t cr3, uint64_t virt, uint64_t phys);
int vmm_unmap_user(uint64_t cr3, uint64_t virt);
/* Unmap every user PTE in cr3 and free leaf frames plus user page tables. */
void vmm_teardown_user(uint64_t cr3);
/* Eager-copy every user page from src into dst (new frames). 0 ok, -1 fail. */
int vmm_copy_user(uint64_t dst_cr3, uint64_t src_cr3);
/* Walk task PML4: present+user PTEs only; phys includes page offset. */
int vmm_translate_user(uint64_t cr3, uint64_t virt, uint64_t *phys_out);
int vmm_drop_identity(int row);
__attribute__((noreturn)) void vmm_switch_stack(void (*cont)(void));
uint64_t phys_to_virt(uint64_t phys);
uint64_t virt_to_phys(uint64_t virt);
int vmm_print(int row);
