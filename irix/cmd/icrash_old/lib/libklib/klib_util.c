#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/klib_util.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>

#include "klib.h"
#include "klib_extern.h"

/*
 * kl_shift_value()
 */
int
kl_shift_value(k_uint_t value)
{
	int i;

	if (value == 0) {
		return(-1);
	}

	for (i = 0; i < 64; i++) {
		if (value & (((k_uint_t)1) << i)) {
			break;
		}
	}
	return(i);
}

/*
 * kl_get_bit_value()
 *
 * x = byte_size, y = bit_size, z = bit_offset
 */
k_uint_t
kl_get_bit_value(k_ptr_t ptr, int x, int y, int z)
{
	k_uint_t value, mask;
	int right_shift, upper_bits;

	right_shift = ((x * 8) - (y + z)) + ((8 - x) * 8);

	if (y >= 32) {
		int upper_bits = y - 32;
		mask = ((1 << upper_bits) - 1);
		mask = (mask << 32) | 0xffffffff;
	}
	else {
		mask = ((1 << y) - 1);
	}
	bcopy(ptr, &value, x);
	value = value >> right_shift;
	return (value & mask);
}

/*
 * kl_sp_to_pdap()
 */
k_ptr_t
kl_sp_to_pdap(klib_t *klp, kaddr_t sp, k_ptr_t pdap) 
{
	int cpu, size, pda_alloced = 0;
	kaddr_t saddr, curkthread;

	if (DEBUG(KLDC_FUNCTRACE, 4)) {
		fprintf(KL_ERRORFP, "sp=0x%llx, pdap=0x%x\n", sp, pdap);
	}

	kl_reset_error();

	/* Before doing anything, make sure that sp is a valid kernel
	 * address and that it is properly aligned.
	 */
	kl_is_valid_kaddr(klp, sp, (k_ptr_t)NULL, WORD_ALIGN_FLAG);
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_SP;
		return((k_ptr_t)NULL);
	}

	/* If no buffer pointer was passed in, then allocate pda_s buffer
	 * space now. If we don't find a match, we will have to free it
	 * before returning.
	 */
	if (!pdap) {
		pdap = klib_alloc_block(klp, PDA_S_SIZE(klp), K_TEMP);
		pda_alloced = 1;
	}

	/* Check to see which cpu intstack/bootstack (if any) this stack
	 * address comes from. 
	 */
	for (cpu = 0; cpu < K_MAXCPUS(klp); cpu++) {

		/* Get the pda_s for this cpu and see if sp belongs
		 * to one of its stacks.
		 */
		kl_get_pda_s(klp, (kaddr_t)cpu, pdap);
		if (KL_ERROR) {
			continue;
		}
		if (kl_is_cpu_stack(klp, sp, pdap)) {
			return(pdap);	
		}

		/* Check to see if there is a kthread (it may or may not be
		 * a proc) running on the cpu. If there is, then check to see 
		 * if addr is from the kthread stack.
		 */
		if (curkthread = kl_kaddr(klp, pdap, "pda_s", "p_curkthread")) {
			if (saddr = kl_kthread_stack(klp, curkthread, &size)) {
				if ((sp >= saddr) && (sp < (saddr + size))) {
					return(pdap);
				}
			}
		}
	}
	if (pda_alloced) {
		klib_free_block(klp, pdap);
	}
	return((k_ptr_t)NULL);
}

/*
 * kl_is_cpu_stack()
 *
 *   Checks to see if sp is from either the intstack or bootstack for
 *   a particular cpu. Returns 1 if TRUE, 0 if FALSE.
 */
int
kl_is_cpu_stack(klib_t *klp, kaddr_t addr, k_ptr_t pdap) 
{
	int size;
	kaddr_t saddr = 0, lastframe, intstack, bootstack;

	/* Get the intstack and see if addr is from it
	 */
	intstack = kl_kaddr(klp, pdap, "pda_s", "p_intstack");
	lastframe = kl_kaddr(klp, pdap, "pda_s", "p_intlastframe");
	size = (lastframe + EFRAME_S_SIZE(klp)) - intstack;
	if ((addr >= intstack) && (addr < (intstack + size))) {
		return(1);
	}

	/* Get the bootstack and see if addr is from it
	 */
	bootstack = kl_kaddr(klp, pdap, "pda_s", "p_bootstack");
	size = (K_PAGESZ(klp) - (bootstack & (K_PAGESZ(klp) - 1)));
	if ((addr >= bootstack) && (addr < (bootstack + size))) {
		return(1);
	}
	return(0);
}

/*
 * kl_get_curcpu() 
 *
 *  Return the cpuid for the CPU that defkthread was running on or 
 *  for the CPU that generated a panic if defkthread is not set.
 */
int
kl_get_curcpu(klib_t *klp, k_ptr_t pdap)
{
	cpuid_t cpuid = -1;
	k_ptr_t ktp;

	if (K_DEFKTHREAD(klp)) {
		ktp = kl_get_kthread(klp, K_DEFKTHREAD(klp), 0);
		if (!KL_ERROR) {
			/* Get the cpuid the kthread is running on.
			 */
			cpuid = KL_UINT(klp, ktp, "kthread", "k_sonproc");
			if (cpuid == -1) {
				KL_SET_ERROR_NVAL(KLE_DEFKTHREAD_NOT_ON_CPU, 
					K_DEFKTHREAD(klp), 2);
			}
			klib_free_block(klp, ktp);
		}
	}
	else if (K_DUMPPROC(klp)) {
		cpuid = K_DUMPCPU(klp);
	}
	else {
		/* Use the SP stored in the dumpregs to determine the panicing
		 * CPU that initiated the dump.
		 */
		kaddr_t sp;

		sp = ((kaddr_t*)K_DUMPREGS(klp))[8];
		if (PTRSZ32(klp)) {
			sp &= 0xffffffff;
		}
		if (kl_sp_to_pdap(klp, sp, pdap)) {
			cpuid = KL_UINT(klp, pdap, "pda_s", "p_cpuid");
		}
	}
	if (cpuid != -1) {
		kl_get_pda_s(klp, cpuid, pdap);
		if (KL_ERROR) {
			return(-1);
		}
	}
	else {
		KL_SET_ERROR(KLE_NO_CURCPU);
	}
	return(cpuid);
}

/*
 * kl_pda_s_addr() -- get the address of the pda_s struct for cpuid
 */
kaddr_t
kl_pda_s_addr(klib_t *klp, int cpuid)
{
	kaddr_t pdaval, pdap;

	if (cpuid < K_MAXCPUS(klp)) {

		/* Get the pointer to the pda_s struct for cpuid in pdaindr
		 */
		pdaval = K_PDAINDR(klp) + (cpuid * (2 * K_NBPW(klp))) + K_NBPW(klp);
		if (!kl_get_kaddr(klp, pdaval, (k_ptr_t)&pdap, "pdap")) {
			if (DEBUG(KLDC_GLOBAL, 1)) {
				fprintf(KL_ERRORFP, "pda_s_addr: Could not get kaddr at "
						"0x%llx\n", pdaval);
			}
			return((kaddr_t)NULL);
		}
		return(pdap);
	}
	return((kaddr_t)NULL);
}

/*
 * kl_get_cpuid()
 */
int
kl_get_cpuid(klib_t *klp, kaddr_t pda)
{
	int i;
	kaddr_t pdap;

	kl_reset_error();

	for (i = 0; i < K_MAXCPUS(klp); i++) {
		kl_get_kaddr(klp, (K_PDAINDR(klp) + 
			(i * (2 * K_NBPW(klp))) + K_NBPW(klp)), &pdap, "pdap");
		if (pdap == pda) {
			return(i);
		}
	}
	KL_SET_ERROR_NVAL(KLE_BAD_PDA, pda, 2);
	return(-1);
}

/*
 * kl_nodepda_s_addr()
 */
kaddr_t
kl_nodepda_s_addr(klib_t *klp, int nodeid)
{
	kaddr_t nodepdaval, npdap;

	if (nodeid < K_NUMNODES(klp)) {

		/* Get the pointer to the nodepda_s struct for nodeid in Nodepdaindr
		 */
		nodepdaval = K_NODEPDAINDR(klp) + (nodeid * K_NBPW(klp));
		if (!kl_get_kaddr(klp, nodepdaval, (k_ptr_t)&npdap, "npdap")) {
			if (DEBUG(KLDC_GLOBAL, 1)) {
				fprintf(KL_ERRORFP, "nodepda_s_addr: Could not get kaddr at "
						"0x%llx\n", nodepdaval);
			}
			return((kaddr_t)NULL);
		}
		return(npdap);
	}
	return((kaddr_t)NULL);
}

/*
 * kl_get_nodeid()
 */
int
kl_get_nodeid(klib_t *klp, kaddr_t nodepda)
{
	int i;
	kaddr_t nodepdap;

	kl_reset_error();

	for (i = 0; i < K_NUMNODES(klp); i++) {
		kl_get_kaddr(klp, 
			(K_NODEPDAINDR(klp) + (i * K_NBPW(klp))), &nodepdap, "nodepdap");
		if (nodepdap == nodepda) {
			return(i);
		}
	}
	KL_SET_ERROR_NVAL(KLE_BAD_NODEPDA, nodepda, 2);
	return(-1);
}

/*
 * kl_cpu_nasid()
 */
int
kl_cpu_nasid(klib_t *klp, int cpuid)
{
	int nasid;
	k_ptr_t pdap;
	kaddr_t nodepdap;

    if (K_IP(klp) == 27) {
		/* Get the pointer to the pda_s struct for cpuid. This structure
		 * contains a pointer to the nodepda_s struct for the node the
		 * cpu is installed on. The address of the nodepda_s struct 
		 * will allow us to determine the nasid value to access node 
		 * memory.
		 */
		pdap = klib_alloc_block(klp, PDA_S_SIZE(klp), K_TEMP);
		kl_get_pda_s(klp, cpuid, pdap);
		if (KL_ERROR) {
			klib_free_block(klp, pdap);
			return(-1);
		}
		nodepdap = kl_kaddr(klp, pdap, "pda_s", "p_nodepda");
		nasid = kl_addr_to_nasid(klp, nodepdap);
		klib_free_block(klp, pdap);
	}
	else {
		nasid = 0;
	}
	return(nasid);
}

/**
 ** save area of kernel nmi regs (lifted from sys/SN/kldir.h)
 **/

/*  
 * save area of kernel nmi regs in the prom format
 */
#define IP27_NMI_KREGS_OFFSET       0x11400
#define IP27_NMI_KREGS_CPU_SIZE     0x200
#define SP_OFFSET					(29 * 8)
#define RA_OFFSET					(31 * 8)
#define ERROR_EPC_OFFSET			(36 * 8)
#define EPC_OFFSET					(37 * 8)

/*
 * save area of kernel nmi regs in eframe format
 */
#define IP27_NMI_EFRAME_OFFSET     	0x11800
#define IP27_NMI_EFRAME_SIZE     	0x200


/*
 * kl_NMI_regbase()
 *
 *  Returns the start address of the register dump area for a CPU in the
 *  event of an NMI. If an eframe buffer pointer is passed in, stuff the
 *  eframe the address points to into the buffer.
 */
kaddr_t
kl_NMI_regbase(klib_t *klp, int cpuid)
{
	int nasid;
	k_ptr_t pdap, nmi_regs;
	kaddr_t nodepdap, nmi_base = 0;

    if (K_IP(klp) == 27) {
		/* Get the pointer to the pda_s struct for cpuid. This structure
		 * contains a pointer to the nodepda_s struct for the node the
		 * cpu is installed on. The address of the nodepda_s struct 
		 * will allow us to determine the nasid value to access node 
		 * memory.
		 */
		pdap = klib_alloc_block(klp, PDA_S_SIZE(klp), K_TEMP);
		kl_get_pda_s(klp, cpuid, pdap);
		if (KL_ERROR) {
			klib_free_block(klp, pdap);
			return(-1);
		}
		nodepdap = kl_kaddr(klp, pdap, "pda_s", "p_nodepda");
		nasid = kl_addr_to_nasid(klp, nodepdap);

		/* Determine the start address of the NMI saveregs for cpuid
		 * Half of the 0x400 bytes is for saved registers in PROM format
		 * and the other half is for the same register values in eframe_s
		 * format. When the cpu is the B cpu, we have to bump nmi_base
		 * by 2 times IP27_NMI_KREGS_CPU_SIZE;
		 */
		nmi_base = K_K0BASE(klp) | 
			(((k_uint_t)nasid) << K_NASID_SHIFT(klp)) + IP27_NMI_KREGS_OFFSET;
		if (cpuid % 2) {
			nmi_base += IP27_NMI_KREGS_CPU_SIZE;
		}
	}
	else {

		/* Check to see if the dump was initiated by an NMI...If it was,
		 * use nmi_saveregs to get the starting pc, sp, and ra.
		 */
		if (IS_NMI(klp)) {

			nmi_base = kl_sym_addr(klp, "nmi_saveregs");
		}
	}
	return(nmi_base);
}

/*
 * kl_NMI_eframe()
 */
k_ptr_t
kl_NMI_eframe(klib_t *klp, int cpuid, kaddr_t *e)
{
	int nasid;
	k_ptr_t eframep;
	kaddr_t eframe;

	nasid = kl_cpu_nasid(klp, cpuid);
	if (KL_ERROR) {
		return((k_ptr_t *)NULL);
	}
	eframep = klib_alloc_block(klp, EFRAME_S_SIZE(klp), K_TEMP);
	eframe = K_K0BASE(klp) |
		(((k_uint_t)nasid) << K_NASID_SHIFT(klp)) + IP27_NMI_EFRAME_OFFSET;
	if (cpuid % 2) {
		eframe += IP27_NMI_EFRAME_SIZE;
	}
		
	kl_get_struct(klp, eframe, EFRAME_S_SIZE(klp), eframep, "eframe_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_EFRAME_S;
		klib_free_block(klp, eframep);
		return((k_ptr_t *)NULL);
	}
	if (e) {
		*e = eframe;
	}
	return(eframep);
}

/*
 * kl_NMI_saveregs()
 *
 */
int
kl_NMI_saveregs(klib_t *klp, int cpuid, kaddr_t *errorepc, kaddr_t *epc, 
		kaddr_t *sp, kaddr_t *ra)
{
	int nasid;
	k_ptr_t pdap, nmi_regs, eframep;
	kaddr_t nodepdap, nmi_base;

	kl_reset_error();

    if (K_IP(klp) == 27) {

		nmi_base = kl_NMI_regbase(klp, cpuid);
		eframep = kl_NMI_eframe(klp, cpuid, (kaddr_t *)NULL);

		/*   The key register values (epc, ra, and sp) are saved in two
		 *   ways:
		 *
		 *   - in prom format
		 *
		 *   - in eframe format
		 *   
		 *   The call to kl_NMI_regbase() returns a kernel pointer to 
		 *   the base of the PROM format saved register values. The call
		 *   to kl_NMI_eframe() will return a filled in eframe buffer. We 
		 *   can use either set of register values. We'll use the eframe 
		 *   values for sp, ra, and epc and use the PROM value for errorepc.
		 *   In the event that the eframe values are NULL, we'll use the
		 *   PROM values instead.
		 */
		*epc = *(kaddr_t*)((uint)eframep + (FIELD("eframe_s", "ef_epc")));
		if (*epc == NULL) {
			kl_get_kaddr(klp, nmi_base + EPC_OFFSET, epc, "epc");
		}
		*ra = *(kaddr_t*)((uint)eframep + (FIELD("eframe_s", "ef_ra")));
		if (*ra == NULL) {
			kl_get_kaddr(klp, nmi_base + RA_OFFSET, ra, "ra");
		}
		*sp = *(kaddr_t*)((uint)eframep + (FIELD("eframe_s", "ef_sp")));
		if (*sp == NULL) {
			kl_get_kaddr(klp, nmi_base + SP_OFFSET, sp, "sp");
		}
		kl_get_kaddr(klp, nmi_base + ERROR_EPC_OFFSET, errorepc, "errorepc");

		if (eframep) {
			klib_free_block(klp, eframep);
		}
	}
	else {

		/* For IP19, IP21, and IP25 systems, check to see if the dump 
		 * was initiated by an NMI...If it was, use nmi_saveregs to get 
		 * the starting pc, sp, and ra.
		 */
		if (IS_NMI(klp)) {

			nmi_base = kl_NMI_regbase(klp, cpuid);

			/* Get the PC, RA and SP from the NMI dumpregs.
			 */
			kl_get_kaddr(klp, nmi_base + (cpuid * 32), epc, "pc");
			kl_get_kaddr(klp, nmi_base + (cpuid * 32) + 16, sp, "sp");
			kl_get_kaddr(klp, nmi_base + (cpuid * 32) + 24, ra, "ra");
			*errorepc = *epc;
		}
	}
	if (!(*errorepc) | !(*epc) | !(*sp)) {
		return(-1);
	}
	else {
		return(0);
	}
}

/** Functions to make virtual object behavir descriptor data 
 ** easier to get at.
 **/

/* 
 * kl_bhv_pdata()
 *
 *   Take a kernel pointer to a bhv_desc struct and return the contents
 *   of the bd_pdata field (which points to the actual object).
 */
kaddr_t
kl_bhv_pdata(klib_t *klp, kaddr_t bhv)
{
	kaddr_t pdata;
	k_ptr_t bhvp;

	bhvp = klib_alloc_block(klp, BHV_DESC_SIZE(klp), K_TEMP);
	kl_get_struct(klp, bhv, BHV_DESC_SIZE(klp), bhvp, "bhv_desc");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_BHV_DESC;
		klib_free_block(klp, bhvp);
		return((kaddr_t)NULL);
	}
	pdata = kl_bhvp_pdata(klp, bhvp);
	klib_free_block(klp, bhvp);
	return(pdata);
}

/* 
 * kl_bhv_vobj()
 *
 *   Take a kernel pointer to a bhv_desc struct and return the contents
 *   of the bd_vobj field (which points to the virtual object).
 */
kaddr_t
kl_bhv_vobj(klib_t *klp, kaddr_t bhv)
{
	kaddr_t vobj;
	k_ptr_t bhvp;

	bhvp = klib_alloc_block(klp, BHV_DESC_SIZE(klp), K_TEMP);
	kl_get_struct(klp, bhv, BHV_DESC_SIZE(klp), bhvp, "bhv_desc");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_BHV_DESC;
		klib_free_block(klp, bhvp);
		return((kaddr_t)NULL);
	}
	vobj = kl_bhvp_vobj(klp, bhvp);
	klib_free_block(klp, bhvp);
	return(vobj);
}

/*
 * kl_bhvp_pdata()
 * 
 *  Take a pointer to a block of memory containing a bhv_desc struct
 *  and return the contents of the bd_pdata field (which points to the 
 *  actual object).
 */
kaddr_t
kl_bhvp_pdata(klib_t *klp, k_ptr_t bhvp)
{
	kaddr_t pdata;

	pdata = kl_kaddr(klp, bhvp, "bhv_desc", "bd_pdata");
	return(pdata);
}

/*
 * kl_bhvp_vobj()
 * 
 *  Take a pointer to a block of memory containing a bhv_desc struct
 *  and return the contents of the bd_vobj field (which points to the 
 *  virtual object).
 */
kaddr_t
kl_bhvp_vobj(klib_t *klp, k_ptr_t bhvp)
{
	kaddr_t vobj;

	vobj = kl_kaddr(klp, bhvp, "bhv_desc", "bd_vobj");
	return(vobj);

}

/* 
 * kl_hold_signals() -- Hold signals in critical blocks of code
 */
void
kl_hold_signals()
{
	sighold((int)SIGINT);
	sighold((int)SIGPIPE);
}

/* 
 * kl_release_signals() -- Allow signals again
 */
void
kl_release_signals()
{
	sigrelse((int)SIGINT);
	sigrelse((int)SIGPIPE);
}

/* 
 * kl_enqueue() -- Add a new element to the tail of doubly linked list.
 */
void
kl_enqueue(element_t **list, element_t *new)
{
	element_t *head;

	/* 
	 * If there aren't any elements on the list, then make new element the 
	 * head of the list and make it point to itself (next and prev).
	 */
	if (!(head = *list)) {
		new->next = new;
		new->prev = new;
		*list = new;
	}
	else {
		head->prev->next = new;
		new->prev = head->prev;
		new->next = head;
		head->prev = new;
	}
}

/* 
 * kl_dequeue() -- Remove an element from the head of doubly linked list.
 */
element_t *
kl_dequeue(element_t **list)
{
	element_t *head;

	/* If there's nothing queued up, just return 
	 */
	if (!*list) {
		return((element_t *)NULL);
	}

	head = *list;

	/* If there is only one element on list, just remove it 
	 */
	if (head->next == head) {
		*list = (element_t *)NULL;
	}
	else {
		head->next->prev = head->prev;
		head->prev->next = head->next;
		*list = head->next;
	}
	head->next = 0;
	return(head);
}

/*
 * kl_findqueue()
 */
int
kl_findqueue(element_t **list, element_t *item)
{
	element_t *e;

	/* If there's nothing queued up, just return 
	 */
	if (!*list) {
		return(0);
	}

	e = *list;

	/* Check to see if there is only one element on the list. 
	 */
	if (e->next == e) {
		if (e != item) {
			return(0);
		}
	}
	else {
		/* Now walk linked list looking for item
		 */
		while(1) {
			if (e == item) {
				break;
			}
			else if (e->next == *list) {
				return(0);
			}
			e = e->next;
		}
	}
	return(1);
}

/*
 * kl_findqueue()
 */
int
kl_findlist_queue(list_of_ptrs_t **list,  list_of_ptrs_t *item, 
		  int (*compare)(void *,void *))
{
	list_of_ptrs_t *e;

	/* If there's nothing queued up, just return 
	 */
	if (!*list) {
		return(0);
	}

	e = *list;

	/* Check to see if there is only one element on the list. 
	 */
	if (((element_t *)e)->next == (element_t *)e) {
		if (compare(e,item)) {
			return(0);
		}
	}
	else {
		/* Now walk linked list looking for item
		 */
		while(1) {
			if (!compare(e,item)) {
				break;
			}
			else if (((element_t *)e)->next == (element_t *)*list) {
				return(0);
			}
			e = (list_of_ptrs_t *)((element_t *)e)->next;
		}
	}
	return(1);
}

/* 
 * kl_remqueue() -- Remove specified element from doubly linked list.
 */
void
kl_remqueue(element_t **list, element_t *item)
{
	/* Check to see if item is first on the list
	 */
	if (*list == item) {
		if (item->next == item) {
			*list = (element_t *)NULL;
			return;
		}
		else {
			*list = item->next;
		}
	}

	/* Remove item from list
	 */
	item->next->prev = item->prev;
	item->prev->next = item->next;
}

/*
 * max()
 */
int
max(int x, int y)
{
    return((x > y) ? x : y);
}

/*
 * alloc_btnode()
 */
btnode_t *
alloc_btnode(char *key)
{
    btnode_t *np;

    np = (btnode_t *)malloc(sizeof(*np));
    bzero(np, sizeof(*np));

    np->bt_key = strdup(key);
    return (np);
}

/*
 * free_btnode()
 */
void
free_btnode(btnode_t *np)
{
    free(np->bt_key);
    free(np);
}

/*
 * btnode_height()
 */
int
btnode_height(btnode_t *root)
{
    if (!root) {
        return(-1);
    }
    return(root->bt_height);
}

/*
 * set_btnode_height()
 */
void
set_btnode_height(btnode_t *np)
{
    if (np) {
        np->bt_height = 
			1 + max(btnode_height(np->bt_left), btnode_height(np->bt_right));
    }
}

/*
 * insert_btnode()
 */
int
insert_btnode(btnode_t **root, btnode_t *np, int flags)
{
    int ret, status = 0;

    if (*root == (btnode_t *)NULL) {
        *root = np;
    }
    else {
        ret = strcmp(np->bt_key, (*root)->bt_key);
        if (ret < 0) {
            status = insert_btnode(&((*root)->bt_left), np, flags);
            set_btnode_height(*root);
            balance_tree(root);
        }
        else if ((ret == 0) && !(flags & DUPLICATES_OK)) {
            status = -1;
        }
        else {
            status = insert_btnode(&((*root)->bt_right), np, flags);
            set_btnode_height(*root);
            balance_tree(root);
        }
    }
    return(status);
}

/*
 * swap_btnodes()
 *
 *   This function finds the next largest key in tree starting at
 *   root and swaps the node containing the key with node pointed
 *   to by npp. The routine is called recursively so that all nodes
 *   visited are height adjusted and balanced. This function is
 *   used in conjunction with delete_betnode() to remove a node
 *   from the tree while maintaining a reasonable balance in the
 *   tree.
 */
void
swap_btnode(btnode_t **root, btnode_t **npp)
{
    btnode_t *tmp;

    if ((*root)->bt_right == (btnode_t *)NULL) {

        /* We're at the end of the line, so go ahead and do the
         * swap.
         */
        tmp = *npp;
        *npp = *root;
        *root = (*root)->bt_left;
        (*npp)->bt_left = tmp->bt_left;
        (*npp)->bt_right = tmp->bt_right;
        (*npp)->bt_height = tmp->bt_height;
        if (*root) {
            set_btnode_height(*root);
            balance_tree(root);
        }
    }
    else {
        swap_btnode(&(*root)->bt_right, npp);
        set_btnode_height(*root);
        balance_tree(root);
    }
}

/*
 * delete_btnode()
 *
 *    This function finds a node in a tree and then removes it, making
 *    sure to keep the tree in a reasonably balanced state. A pointer
 *    to the just freed node is then passed to the free function (passed in
 *    as a parameter).
 */
int
delete_btnode(btnode_t **root, btnode_t *np, void(*free)(), int flags)
{
    int ret;
    btnode_t *hold;

    if (!np) {
        return(0);
    }

    if (*root == np) {

        /* We found it
         */
        if (!(*root)->bt_left && !(*root)->bt_right) {

            /* This node has no children, just remvoe it
             */
            hold = *root;
            *root = (btnode_t *)NULL;
        }
        else if ((*root)->bt_left && !(*root)->bt_right) {

            /* There is only a left child. Remove np and
             * link the child to root.
             */
            hold = *root;
            *root = (*root)->bt_left;
        }
        else if ((*root)->bt_right && !(*root)->bt_left) {

            /* There is only a right child. Remove np and
             * link the child to root.
             */
            hold = *root;
            *root = (*root)->bt_right;
        }
        else {

            /* We have both a left and right child. This is
             * more complicated. We have to find the node with
             * the next smallest key and exchange this node with
             * that. It will then be the other node which will
             * be deleted from the tree.
             */
            hold = *root;
            swap_btnode(&((*root)->bt_left), root);
        }
        if (*root) {
            set_btnode_height(*root);
            balance_tree(root);
        }
        (*free)(hold);
        return(0);
    }

    /* Continue walking the tree searching for the key that
     * needs to be deleted.
     */
    ret = strcmp(np->bt_key, (*root)->bt_key);
    if (ret < 0) {
        delete_btnode(&((*root)->bt_left), np, free, flags);
        set_btnode_height(*root);
        balance_tree(root);
    }
    else {
        delete_btnode(&((*root)->bt_right), np, free, flags);
        set_btnode_height(*root);
        balance_tree(root);
    }
    return(0);
}

/*
 * single_left()
 */
void
single_left(btnode_t **root)
{
    btnode_t *old_root, *old_right, *middle;

    /* Get the relevant pointer values
     */
    old_root = *root;
    old_right = old_root->bt_right;
    middle = old_right->bt_left;

    /* Rearrange the nodes
     */
    old_right->bt_left = old_root;
    old_root->bt_right = middle;
    *root = old_right;

    /* Adjust the node heights
     */
    set_btnode_height(old_root);
    set_btnode_height(old_right);
}

/*
 * single_right()
 */
void
single_right(btnode_t **root)
{
    btnode_t *old_root, *old_left, *middle;

    /* Get the relevant pointer values
     */
    old_root = *root;
    old_left = old_root->bt_left;
    middle = old_left->bt_right;

    /* Rearrange the nodes
     */
    old_left->bt_right = old_root;
    old_root->bt_left = middle;
    *root = old_left;

    /* Adjust the node heights
     */
    set_btnode_height(old_root);
    set_btnode_height(old_left);
}

/*
 * double_left()
 */
void
double_left(btnode_t **root)
{
    single_right(&((*root)->bt_right));
    single_left(root);
}

/*
 * double_right()
 */
void
double_right(btnode_t **root)
{
    single_left(&((*root)->bt_left));
    single_right(root);
}

/*
 * balance_tree()
 */
void
balance_tree(btnode_t **root)
{
    int left_height, right_height;
    btnode_t *subtree;

    left_height = btnode_height((*root)->bt_left);
    right_height = btnode_height((*root)->bt_right);

    if (left_height > (right_height + 1)) {

        /* Too deep to the left
         */
        subtree = (*root)->bt_left;

        left_height = btnode_height(subtree->bt_left);
        right_height = btnode_height(subtree->bt_right);

        if (left_height >= right_height) {

            /* Too deep to the outside
             */
            single_right(root);
        }
        else {

            /* Too deep to the inside
             */
            double_right(root);
        }
    }
    else if ((left_height + 1) < right_height) {

        /* Too deep to the right
         */
        subtree = (*root)->bt_right;

        left_height = btnode_height(subtree->bt_left);
        right_height = btnode_height(subtree->bt_right);

        if (right_height >= left_height) {

            /* Too deep to the outside
             */
            single_left(root);
        }
        else {

            /* Too deep to the inside
             */
            double_left(root);
        }
    }
}

/*
 * find_btnode()
 */
btnode_t *
find_btnode(btnode_t *np, char *key, int *max_depth)
{
    int ret;
    btnode_t *found = NULL;

    if (!np) {
        return((btnode_t *)NULL);
    }

    /* Bump the counter (if a pointer for one was passed in)
     * to calculate total nodes searched before finding/not
     * finding a key.
     */
    if (max_depth) {
        (*max_depth)++;
    }

    /* See if this node is the one we are looking for
     */
    ret = strcmp(key, np->bt_key);
    if (ret == 0) {
        return(np);
    }
    else if (ret < 0) {
        if (found = find_btnode(np->bt_left, key, max_depth)) {
            return(found);
        }
    }
    else if (ret > 0) {
        if (found = find_btnode(np->bt_right, key, max_depth)) {
            return(found);
        }
    }
    return((btnode_t *)NULL);
}

/*
 * kl_init_string_table()
 */
string_table_t *
kl_init_string_table(klib_t *klp, int flag)
{
	string_table_t *st;

	if (flag == K_PERM) {
		st = (string_table_t*)klib_alloc_block(klp, 	
			sizeof(string_table_t), K_PERM);
		st->block_list = klib_alloc_block(klp, 4096, K_PERM);
	}
	else {
		st = (string_table_t*)klib_alloc_block(klp, 
			sizeof(string_table_t), K_TEMP);
		st->block_list = klib_alloc_block(klp, 4096, K_TEMP);
	}
	return(st);
}

/*
 * kl_free_string_table()
 */
void
kl_free_string_table(klib_t *klp, string_table_t *st)
{
	k_ptr_t block, next_block;

	block = st->block_list;

	while (block) {
		next_block = *(k_ptr_t*)block;
		klib_free_block(klp, block);
		block = next_block;
	}
	klib_free_block(klp, (k_ptr_t)st);
}

/*
 * kl_in_block()
 */
int
in_block(char *s, k_ptr_t block)
{
	if ((s >= (char*)block) && (s < ((char*)block + 4096))) {
		return(1);
	}
	return(0);
}

/*
 * kl_get_string()
 */
char *
kl_get_string(klib_t *klp, string_table_t *st, char *str, int flag)
{
	int len;
	k_ptr_t block, last_block;
	char *s;

	block = st->block_list;
	s = (char *)((uint)block + 4);

	/* Walk the strings in the table looking for str. If we find it
	 * return a pointer to the string.
	 */
	while (*s) {
		if (!strcmp(str, s)) {
			return(s);
		}
		s += strlen(s) +1;
		if (!in_block((s + strlen(str)), block)) {
			last_block = block;
			block = *(k_ptr_t*)last_block;
			if (!block) {
				break;
			}
			s = (char *)((uint)block + 4);
		}
	} 

	/* If we didn't find the string, we have to add it to the
	 * table and then return a pointer to it. If we are still
	 * in the middle of a block, make sure there is enough room
	 * for the new string. If there isn't, allocate a new block
	 * and put the string there (after linking in the new block).
	 */
	if (block) {
		if (in_block((s + strlen(str)), block)) {
			strcpy(s, str);
			st->num_strings++;
			return(s);
		}
		else {
			last_block = block;
			block = *(k_ptr_t*)block;
		}
	}

	/* If we get here, it means that there wasn't enough string in 
	 * an existing block for the new string. So, allocate a new block
	 * and put the string there.
	 */
	if (flag & K_PERM) {
		block = klib_alloc_block(klp, 4096, K_PERM);
	}
	else {
		block = klib_alloc_block(klp, 4096, K_TEMP);
	}
	*(k_ptr_t*)last_block = block;

	s = (char *)((uint)block + 4);
	strcpy(s, str);
	st->num_strings++;
	return(s);
}

/*
 * klib_alloc_block()
 */
k_ptr_t 
klib_alloc_block(klib_t *klp, int size, int flags) 
{
	k_ptr_t blk;

	if (K_BLOCK_ALLOC(klp)) {
		blk = (k_ptr_t)K_BLOCK_ALLOC(klp)(0, size, flags);
	}
	else {
		blk = (k_ptr_t)malloc(size);
		bzero(blk, size);
	}
	return(blk);
}

/*
 * klib_free_block()
 */
void
klib_free_block(klib_t *klp, k_ptr_t blk)
{
	if (K_BLOCK_FREE(klp)) {
		K_BLOCK_FREE(klp)(0, blk);
	}
	else {
		free(blk);
	}
}

