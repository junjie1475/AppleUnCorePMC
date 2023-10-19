//
//  UncorePMC.c
//  UncorePMC
//
//  Created by Junjie on 17/10/2023.
//

#include <mach/mach_types.h>
#include <libkern/libkern.h>
#include <ptrauth.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/sysctl.h>

// To bypass kernel address checking may come handy when debugging
#define DEBUG_PRINT_PTR(ptr) (uint32_t)(((uint64_t)ptr) >> 32), (uint32_t)(((uint64_t)ptr) & 0xFFFFFFFF)
#define ASSERT(exp, msg) do { if(!exp) {printf(msg); printf("\n"); return -1; } }while(0)

#define ADRP_IMM(inst) ((((inst >> 5) & 0x7FFFF) << 2) | ((inst >> 29) & 0b11)) << 12
#define ADRP_OPC(inst) (inst >> 31 | ((inst >> 24) & 0x1F))

#define LDRB_IMM(inst) (inst >> 10) & 0xFFF

#define LDRH_IMM(inst) ((inst >> 10) & 0xFFF) << 1

#define uncore_enabled_adrp_offset active_ctrs_adrp_offset - 13

#define active_ctrs_adrp_offset -20

struct monotonic_config {
    uint64_t event;
    uint64_t allowed_ctr_mask;
    uint64_t cpu_mask;
};

kern_return_t UncorePMC_start(kmod_info_t * ki, void *d);
kern_return_t UncorePMC_stop(kmod_info_t *ki, void *d);
kern_return_t uncore_add_wrap(struct monotonic_config *config, uint32_t *ctr_out, uint8_t *mt_uncore_enabled, uint16_t *uncore_active_ctrs, void *uncore_add_ptr);

void *uncore_add_ptr = NULL;
uint8_t *mt_uncore_enabled = NULL;
uint16_t *uncore_active_ctrs = NULL;

uintptr_t unauthenticated_printf = (uintptr_t)NULL;

// Modify from https://github.com/saagarjha/TSOEnabler/blob/master/TSOEnabler/TSOEnabler.c
static int find_uncore_add_in_text_exec(char *text_exec, uint64_t text_exec_size) {
    /*
     Find this pattern
     cmp    w12, w9
     ...
     tst    w23, #0xffff
     */

    uint32_t *instructions = (uint32_t *)text_exec;
    for (uint64_t i = 0; i < text_exec_size / sizeof(uint32_t) - 5; ++i) {
        if (instructions[i + 0] == 0x6B09019F &&
            instructions[i + 2] == 0x72003EFF) {
            
            printf("uncorePMC: Found uncore_add pointer at %x%x\n", DEBUG_PRINT_PTR(&instructions[i + 2]));
            uncore_add_ptr = &instructions[i + 2];
            
            
            long imm = ADRP_IMM(instructions[i + uncore_enabled_adrp_offset]);
            unsigned int ldr_offset = LDRB_IMM(instructions[i + uncore_enabled_adrp_offset + 1]);
            printf("uncorePMC: ldrb offset: %x%x\n", DEBUG_PRINT_PTR(ldr_offset));
            
            mt_uncore_enabled = (uint8_t*)(((long)&instructions[i + uncore_enabled_adrp_offset]) & ~(0xFFF)) + imm + ldr_offset;
            printf("unscorePMC: mt_uncore_enable address: %x%x\n", DEBUG_PRINT_PTR(mt_uncore_enabled));

            
            imm = ADRP_IMM(instructions[i + active_ctrs_adrp_offset]);
            ldr_offset = LDRH_IMM(instructions[i + active_ctrs_adrp_offset + 1]);
            printf("uncorePMC: ldrh offset: %x%x\n", DEBUG_PRINT_PTR(ldr_offset));
            
            uncore_active_ctrs = (uint16_t*)(((long)&instructions[i + active_ctrs_adrp_offset]) & ~(0xFFF)) + imm + ldr_offset;
            printf("unscorePMC: uncore_active_ctrs address: %x%x\n", DEBUG_PRINT_PTR(uncore_active_ctrs));
            
            return 0;
        }
    }
    return -1;
}

static int find_text_exec_base(void) {
    // get printf in __TEXT_EXEC.__text section
    unauthenticated_printf = (uintptr_t)ptrauth_strip((void *)printf, ptrauth_key_function_pointer);

    printf("uncorePMC: unauthenticated_printf at 0x%x%x\n", DEBUG_PRINT_PTR(unauthenticated_printf));
    
    // search start from printf address
    return find_uncore_add_in_text_exec((char*)unauthenticated_printf, 0x700000);
}


static int sysctl_uncore_add SYSCTL_HANDLER_ARGS {
    void **ptrs[2] = {NULL, NULL};
    SYSCTL_IN(req, &ptrs, sizeof(ptrs));
    
    printf("uncorePMC: ptrs[0]: %x%x, ptrs[1]: %x%x", DEBUG_PRINT_PTR(ptrs[0]), DEBUG_PRINT_PTR(ptrs[1]));
    struct monotonic_config *config = (struct monotonic_config*)ptrs[0];
    uint32_t *ctr_out = (uint32_t*)ptrs[1];
    
    return uncore_add_wrap(config, ctr_out, mt_uncore_enabled, uncore_active_ctrs, uncore_add_ptr);
}
SYSCTL_PROC(_kern, OID_AUTO, uncore_add, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY,
            NULL, 0, &sysctl_uncore_add, "I", "add uncore events");

int isreg = -1;

kern_return_t UncorePMC_start(kmod_info_t * ki, void *d)
{
    if(find_text_exec_base() == -1) {
         printf("uncorePMC: couldn't find uncore_add!");
         return KERN_SUCCESS;
    }
    isreg = 1;
    sysctl_register_oid(&sysctl__kern_uncore_add);
    
    return KERN_SUCCESS;
}

kern_return_t UncorePMC_stop(kmod_info_t *ki, void *d)
{
    if(isreg != -1) sysctl_unregister_oid(&sysctl__kern_uncore_add);
    return KERN_SUCCESS;
}
