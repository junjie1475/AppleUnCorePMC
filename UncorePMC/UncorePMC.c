//
//  UncorePMC.c
//  UncorePMC
//
//  Created by Junjie on 17/10/2023.
//

#include <mach/mach_types.h>

kern_return_t UncorePMC_start(kmod_info_t * ki, void *d);
kern_return_t UncorePMC_stop(kmod_info_t *ki, void *d);

kern_return_t UncorePMC_start(kmod_info_t * ki, void *d)
{
    return KERN_SUCCESS;
}

kern_return_t UncorePMC_stop(kmod_info_t *ki, void *d)
{
    return KERN_SUCCESS;
}
