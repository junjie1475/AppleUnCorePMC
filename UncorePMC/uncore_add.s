.global _uncore_add_wrap
	// TODO: This is hacky way, better implementation?

	 /*
	  * uncore_add_warp
	  * A wrapper around uncore_add, to bypasss kernel cpc event checking.
	  * x0 - struct monotonic_config *config
	  * x1 - uint32_t *ctr_out
	  * x2 - uint8_t *mt_uncore_enabled
	  * x3 - uint16_t *uncore_active_ctrs
	  * x5 - void *uncore_add_ptr
	 */
_uncore_add_wrap:
	pacibsp
	sub   sp, sp, #0x90
	stp   x28, x27, [sp, #0x30]
	stp   x26, x25, [sp, #0x40]
	stp   x24, x23, [sp, #0x50]
	stp   x22, x21, [sp, #0x60]
	stp   x20, x19, [sp, #0x70]
	stp   fp, lr, [sp, #0x80]
	add   fp, sp, #0x80
	
	ldrb	w8, [x2]
	cbz	w8, check_available

	mov	w0, #0x10 
err:
   ldp   fp, lr, [sp, #0x80]
   ldp   x20, x19, [sp, #0x70]
   ldp   x22, x21, [sp, #0x60]
   ldp   x24, x23, [sp, #0x50]
   ldp   x26, x25, [sp, #0x40]
   ldp   x28, x27, [sp, #0x30]  
	add   sp, sp, #0x90
	retab

check_available:
	mov	x20, x0
	ldrh  w8, [x3]
	ldr   x9, [x0, #0x8]
	bic	x25, x9, x8	
	cbz   w25, err_nospc
	
	mov	x19, x1
	ldr	x22, [x20]
	ands  w9, w22, #0xff
	b		call
   
err_nospc:
	mov	w0, #0x1c
	b     err
    
call:
   br			x5
