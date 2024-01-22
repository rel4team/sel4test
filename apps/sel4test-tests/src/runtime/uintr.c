#include "uintr.h"
#include <sel4/sel4.h>

extern char uintrvec[];
extern char uintrret[];

void __handler_entry(struct __uintr_frame* frame, void* handler) {
	uint64_t irqs = uipi_read();
	csr_clear(CSR_UIP, MIE_USIE);
	uint64_t (*__handler)(struct __uintr_frame * frame, uint64_t) = handler;
	irqs = __handler(frame, irqs);
	uipi_write(irqs);
}

uint64_t __register_receiver(seL4_CPtr tcb, seL4_CPtr ntfn, void* handler) {
	// set user interrupt entry
	csr_write(CSR_UTVEC, uintrvec);
	csr_write(CSR_USCRATCH, handler);

	// enable U-mode interrupt handler
	csr_set(CSR_USTATUS, USTATUS_UIE);
	csr_set(CSR_UIE, MIE_USIE);

	// return __syscall0(__NR_uintr_register_receiver);
    return seL4_Uint_Notification_register_receiver(ntfn, tcb);
}

uint64_t __register_sender(seL4_CPtr ntfn) {
    return seL4_Uint_Notification_register_sender(ntfn);
}