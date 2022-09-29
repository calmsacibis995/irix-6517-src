

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/sema.h>

uint _tlbdropin(unsigned char *, caddr_t, pte_t);
void _tlbdrop2in(unchar, caddr_t, pte_t, pte_t);


#if DEBUG
uint tlbdropin(unsigned char *tlbpidp, caddr_t vaddr, pte_t pte)
{
        return _tlbdropin(tlbpidp, vaddr, pte);
}

void tlbdrop2in(unsigned char tlb_pid, caddr_t vaddr, pte_t pte, pte_t pte2)
{
        _tlbdrop2in(tlb_pid, vaddr, pte, pte2);
}
#endif

