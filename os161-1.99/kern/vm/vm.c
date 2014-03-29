
#include "opt-A3.h"

#ifdef UW
/* This was added just to see if we can get things to compile properly and
 * to provide a bit of guidance for assignment 3 */

#include "opt-vm.h"
#if OPT_VM

#include <types.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>

#if OPT_A3
#include <kern/errno.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <uw-vmstats.h>
#include <pt.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

paddr_t
getppages(unsigned long npages)
{
        paddr_t addr;

        spinlock_acquire(&stealmem_lock);

        addr = ram_stealmem(npages);
        
        spinlock_release(&stealmem_lock);
        return addr;
}

static unsigned int next_victim = 0;

void reset_next_victim(void){
	next_victim = 0;
}

static int tlb_get_rr_victim(void){
	int victim;

	victim = next_victim;
	next_victim = (next_victim + 1) % NUM_TLB;
	return victim;

}
#endif /* OPT-A3 */

void
vm_bootstrap(void)
{
	#if OPT_A3
	/* May need to add code. */
	#endif /* OPT-A3 */
}

#if 0 
/* You will need to call this at some point */
static
paddr_t
getppages(unsigned long npages)
{

	#if OPT_A3
        paddr_t addr;

        spinlock_acquire(&stealmem_lock);

        addr = ram_stealmem(npages);
        
        spinlock_release(&stealmem_lock);
        return addr;
	#else
   /* Adapt code form dumbvm or implement something new */
	 (void)npages;
	 panic("Not implemented yet.\n");
   return (paddr_t) NULL;
	#endif /* OPT-A3 */
}
#endif

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	#if OPT_A3
        paddr_t pa;
        pa = getppages(npages);
        if (pa==0) {
                return 0;
        }
        return PADDR_TO_KVADDR(pa);
	#else
   /* Adapt code form dumbvm or implement something new */
	 (void)npages;
	 panic("Not implemented yet.\n");
   return (vaddr_t) NULL;
	#endif /* OPT-A3 */
}

void 
free_kpages(vaddr_t addr)
{
	#if OPT_A3
        /* nothing - leak the memory. */

        (void)addr;
	#else
	/* nothing - leak the memory. */

	(void)addr;
	#endif /* OPT-A3 */
}

void
vm_tlbshootdown_all(void)
{
	panic("Not implemented yet.\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("Not implemented yet.\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{

	#if OPT_A3

        vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
        paddr_t paddr;
        int i;
        uint32_t ehi, elo;
        struct addrspace *as;
        int spl;

        faultaddress &= PAGE_FRAME;

        DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

        switch (faulttype) {
            case VM_FAULT_READONLY:
		//read-only
//		return 1;
                /* We always create pages read-write, so we can't get this */
                panic("dumbvm: got VM_FAULT_READONLY\n");
            case VM_FAULT_READ:
            case VM_FAULT_WRITE:
                break;
            default:
                return EINVAL;
        }

        if (curproc == NULL) {
                /*
                 * No process. This is probably a kernel fault early
                 * in boot. Return EFAULT so as to panic instead of
                 * getting into an infinite faulting loop.
                 */
                return EFAULT;
        }

        as = curproc_getas();
        if (as == NULL) {
                /*
                 * No address space set up. This is probably also a
                 * kernel fault early in boot.
                 */
                return EFAULT;
        }

        /* Assert that the address space has been set up properly. */
        KASSERT(as->as_vbase1 != 0);
        KASSERT(as->as_npages1 != 0);
        KASSERT(as->as_vbase2 != 0);
        KASSERT(as->as_npages2 != 0);
        KASSERT(as->as_stackpbase != 0);
        KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
        KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
        KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

        vbase1 = as->as_vbase1;
        vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
        vbase2 = as->as_vbase2;
        vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
        stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
        stacktop = USERSTACK;

	//get the paddr
	int result = pt_getEntry(faultaddress, &paddr);
	if(result) return result;

	//no need any more
//	int segment_type;
/*
        if (faultaddress >= vbase1 && faultaddress < vtop1) {
                paddr = (faultaddress - vbase1) + as->as_pbase1;
		segment_type = 0;
        }
        else if (faultaddress >= vbase2 && faultaddress < vtop2) {
                paddr = (faultaddress - vbase2) + as->as_pbase2;
		segment_type = 1;
        }
        else if (faultaddress >= stackbase && faultaddress < stacktop) {
                paddr = (faultaddress - stackbase) + as->as_stackpbase;
		segment_type = 2;
        }
        else {
                return EFAULT;
        }
*/
        /* make sure it's page-aligned */
        KASSERT((paddr & PAGE_FRAME) == paddr);

        /* Disable interrupts on this CPU while frobbing the TLB. */
        spl = splhigh();

	vmstats_inc(VMSTAT_TLB_FAULT);

	//if there exists free TLB entry
        for (i=0; i<NUM_TLB; i++) {
                tlb_read(&ehi, &elo, i);
                if (elo & TLBLO_VALID) {
                        continue;
                }
		vmstats_inc(VMSTAT_TLB_FAULT_FREE);
                ehi = faultaddress;
//		if(segment_type == 0){
//			elo = paddr | TLBLO_VALID | TLBLO_DIRTY;
//		}
//		else{
	                elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
//		}
                DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
                tlb_write(ehi, elo, i);
                splx(spl);
                return 0;
        }

	//RR replacement in TLB
	vmstats_inc(VMSTAT_TLB_FAULT_REPLACE);
        int index = tlb_get_rr_victim();
        ehi = faultaddress;
//	if(segment_type == 0){
//		elo = paddr | TLBLO_VALID | TLBLO_DIRTY;
//	}
//	else{
	        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
//	}
        DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
        tlb_write(ehi, elo, index);
        splx(spl);
        return 0;

/*
        kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
        splx(spl);
        return EFAULT;
*/
	#else
  /* Adapt code form dumbvm or implement something new */
	(void)faulttype;
	(void)faultaddress;
	panic("Not implemented yet.\n");
  return 0;
	#endif /* OPT-A3 */

}
#endif /* OPT_VM */

#endif /* UW */

