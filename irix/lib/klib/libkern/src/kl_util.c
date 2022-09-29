#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <klib/klib.h>

int
walk_uthreads(int (*cb)(), void *data)
{
	int i, kthread_cnt;
	k_ptr_t pidp, vprocp, bhvdp, procp, utp;
	kaddr_t pid, vproc, bhv, proc, kthread;

	pidp = kl_alloc_block(PID_ENTRY_SIZE, K_TEMP);
	vprocp = kl_alloc_block(VPROC_SIZE, K_TEMP);
	pid = K_PID_ACTIVE_LIST;

	/* We have to step over the first entry as it is just
	 * the list head.
	 */
	kl_get_kaddr(pid, (k_ptr_t)&pid, "pid_entry");
	if (KL_ERROR) {
		kl_free_block(pidp);
		kl_free_block(vprocp);
		return(0);
	}

	do {
		kl_get_struct(pid, PID_ENTRY_SIZE, pidp, "pid_entry");
		if (KL_ERROR) {
			break;
		}
		vproc = kl_kaddr(pidp, "pid_entry", "pe_vproc");
		kl_get_struct(vproc, VPROC_SIZE, vprocp, "vproc");
		bhv = kl_kaddr(vprocp, "vproc", "vp_bhvh");
		proc = kl_bhv_pdata(bhv);
		if (KL_ERROR) {
			kl_print_error();
			kl_get_kaddr(pid, (k_ptr_t)&pid, "pid_entry");
			if (KL_ERROR) {
				break;
			}
			continue;
		}
	
		procp = kl_get_proc(proc, 2, K_TEMP);
		if (KL_ERROR) {
			kl_print_error();
			kl_get_kaddr(pid, (k_ptr_t)&pid, "pid_entry");
			if (KL_ERROR) {
				break;
			}
			continue;
		}
	
		kthread = kl_proc_to_kthread(procp, &kthread_cnt);
		kl_free_block(procp);
	
		for (i = 0; i < kthread_cnt; i++) {
			utp = kl_get_uthread_s(kthread, 2, K_TEMP);
			if (KL_ERROR) {
				kl_print_error();
				break;
			}
			if(cb(kthread, data))  {
				kl_free_block(utp);
				kl_free_block(pidp);
				kl_free_block(vprocp);
				return 1;
			}
			kthread = kl_kaddr(utp, "uthread_s", "ut_next");
			kl_free_block(utp);
		}
		kl_get_kaddr(pid, (k_ptr_t)&pid, "pid_entry");
		if (KL_ERROR) {
			break;
		}
	} while (pid != K_PID_ACTIVE_LIST);
	kl_free_block(pidp);
	kl_free_block(vprocp);
	return 0;
}

int
walk_xthreads(int (*cb)(), void *data)
{
	k_ptr_t xtp;
	kaddr_t xthread;

	xtp = kl_alloc_block(XTHREAD_S_SIZE, K_TEMP);
	kl_get_struct(K_XTHREADLIST, XTHREAD_S_SIZE, xtp, "xthread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_XTHREAD_S;
		kl_print_error();
		return(0);
	}
	xthread = kl_kaddr(xtp, "xthread_s", "xt_next");
	kl_free_block(xtp);
	while(xthread != K_XTHREADLIST) {
		xtp = kl_get_xthread_s(xthread, 2, K_TEMP);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_XTHREAD_S;
			kl_print_error();
			return 0;
		}
		else {
			if(cb(xthread, data)) {
				kl_free_block(xtp);
				return 1;
			}
			kl_free_block(xtp);
			xthread = kl_kaddr(xtp, "xthread_s", "xt_next");
		}
	}
	return 0;
}
int
walk_sthreads(int (*cb)(), void *data)
{
	int sthread_cnt = 0;
	k_ptr_t stp;
	kaddr_t sthread;

	stp = kl_alloc_block(STHREAD_S_SIZE, K_TEMP);
	kl_get_struct(K_STHREADLIST, STHREAD_S_SIZE, stp, "sthread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_STHREAD_S;
		kl_print_error();
		return(0);
	}
	sthread = kl_kaddr(stp, "sthread_s", "st_next");
	kl_free_block(stp);
	while(sthread != K_STHREADLIST) {
		stp = kl_get_sthread_s(sthread, 2, K_TEMP);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_STHREAD_S;
			kl_print_error();
			return(sthread_cnt);
		}
		else {
			if(cb(sthread, data)) {
				kl_free_block(stp);
				return 1;
			}
			kl_free_block(stp);
			sthread_cnt++;
			sthread = kl_kaddr(stp, "sthread_s", "st_next");
		}
	}
	return 0;
}

int
walk_all_threads(int (*cb)(), void *data)
{
	if(walk_sthreads(cb, data) ||
	   walk_xthreads(cb, data) ||
	   walk_uthreads(cb, data)) return 1;
	return 0;
}

typedef struct {

	kaddr_t sp;
	kaddr_t kt;

} cbr;

static int
check_ktsp(kaddr_t kt, cbr *val)
{
kaddr_t saddr;
int size;

	if (saddr = kl_kthread_stack(kt, &size)) {
		if ((val->sp >= saddr) && (val->sp < (saddr + size))) {

			val->kt=kt;
			return 1;
		}
	}
	return 0;
}

/*
 * kl_sp_to_kthread()
 */

kaddr_t
kl_sp_to_kthread(kaddr_t sp)
{
	int cpu, size, pda_alloced = 0;
	kaddr_t saddr, curkthread;

	if (DEBUG(KLDC_FUNCTRACE, 4)) {
		fprintf(KL_ERRORFP, "sp=0x%llx\n", sp);
	}

	kl_reset_error();

	/* Before doing anything, make sure that sp is a valid kernel
	 * address and that it is properly aligned.
	 */
	kl_is_valid_kaddr(sp, (k_ptr_t)NULL, WORD_ALIGN_FLAG);
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_SP;
		return((kaddr_t)NULL);
	}

	/* walk each list */
	{
	cbr x;

		x.sp=sp;
		if(walk_all_threads(check_ktsp, &x)) return x.kt;
	}
	

	return((kaddr_t)NULL);
}
/*
 * kl_sp_to_pdap()
 */
k_ptr_t
kl_sp_to_pdap(kaddr_t sp, k_ptr_t pdap) 
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
	kl_is_valid_kaddr(sp, (k_ptr_t)NULL, WORD_ALIGN_FLAG);
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_SP;
		return((k_ptr_t)NULL);
	}

	/* If no buffer pointer was passed in, then allocate pda_s buffer
	 * space now. If we don't find a match, we will have to free it
	 * before returning.
	 */
	if (!pdap) {
		pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
		pda_alloced = 1;
	}

	/* Check to see which cpu intstack/bootstack (if any) this stack
	 * address comes from. 
	 */
	for (cpu = 0; cpu < K_MAXCPUS; cpu++) {

		/* Get the pda_s for this cpu and see if sp belongs
		 * to one of its stacks.
		 */
		kl_get_pda_s((kaddr_t)cpu, pdap);
		if (KL_ERROR) {
			continue;
		}
		if (kl_is_cpu_stack(sp, pdap)) {
			return(pdap);	
		}

		/* Check to see if there is a kthread (it may or may not be
		 * a proc) running on the cpu. If there is, then check to see 
		 * if addr is from the kthread stack.
		 */
		if (curkthread = kl_kaddr(pdap, "pda_s", "p_curkthread")) {
			if (saddr = kl_kthread_stack(curkthread, &size)) {
				if ((sp >= saddr) && (sp < (saddr + size))) {
					return(pdap);
				}
			}
		}
	}
	if (pda_alloced) {
		kl_free_block(pdap);
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
kl_is_cpu_stack(kaddr_t addr, k_ptr_t pdap) 
{
	int size;
	kaddr_t saddr = 0, lastframe, intstack, bootstack;

	/* Get the intstack and see if addr is from it
	 */
	intstack = kl_kaddr(pdap, "pda_s", "p_intstack");
	lastframe = kl_kaddr(pdap, "pda_s", "p_intlastframe");
	size = (lastframe + EFRAME_S_SIZE) - intstack;
	if ((addr >= intstack) && (addr < (intstack + size))) {
		return(1);
	}

	/* Get the bootstack and see if addr is from it
	 */
	bootstack = kl_kaddr(pdap, "pda_s", "p_bootstack");
	size = (K_PAGESZ - (bootstack & (K_PAGESZ - 1)));
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
kl_get_curcpu(k_ptr_t pdap)
{
	cpuid_t cpuid = -1;
	k_ptr_t ktp;

	if (K_DEFKTHREAD) {
		ktp = kl_get_kthread(K_DEFKTHREAD, 0);
		if (!KL_ERROR) {
			/* Get the cpuid the kthread is running on.
			 */
			cpuid = KL_UINT(ktp, "kthread", "k_sonproc");
			kl_free_block(ktp);
			if (cpuid == -1) {
				KL_SET_ERROR_NVAL(KLE_DEFKTHREAD_NOT_ON_CPU, 
					K_DEFKTHREAD, 2);
			}
		}
	}
	else if (K_DUMPPROC) {
		cpuid = K_DUMPCPU;
	}
	else {
		/* Use the SP stored in the dumpregs to determine the panicing
		 * CPU that initiated the dump.
		 */
		kaddr_t sp;

		sp = ((kaddr_t*)K_DUMPREGS)[8];
		if (PTRSZ32) {
			sp &= 0xffffffff;
		}
		if (kl_sp_to_pdap(sp, pdap)) {
			cpuid = KL_UINT(pdap, "pda_s", "p_cpuid");
		}
	}
	if (cpuid != -1) {
		kl_get_pda_s(cpuid, pdap);
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
kl_pda_s_addr(int cpuid)
{
	kaddr_t pdaval, pdap;

	if (cpuid < K_MAXCPUS) {

		/* Get the pointer to the pda_s struct for cpuid in pdaindr
		 */
		pdaval = K_PDAINDR + (cpuid * (2 * K_NBPW)) + K_NBPW;
		kl_get_kaddr(pdaval, (k_ptr_t)&pdap, "pdap");
		if (KL_ERROR) {
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
kl_get_cpuid(kaddr_t pda)
{
	int i;
	kaddr_t pdap;

	kl_reset_error();

	for (i = 0; i < K_MAXCPUS; i++) {
		kl_get_kaddr((K_PDAINDR + (i * (2 * K_NBPW)) + K_NBPW), &pdap, "pdap");
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
kl_nodepda_s_addr(int nodeid)
{
	kaddr_t nodepdaval, npdap;

	if (nodeid < K_NUMNODES) {

		/* Get the pointer to the nodepda_s struct for nodeid in Nodepdaindr
		 */
		nodepdaval = K_NODEPDAINDR + (nodeid * K_NBPW);
		kl_get_kaddr(nodepdaval, (k_ptr_t)&npdap, "npdap");
		if (KL_ERROR) {
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
kl_get_nodeid(kaddr_t nodepda)
{
	int i;
	kaddr_t nodepdap;

	kl_reset_error();

	for (i = 0; i < K_NUMNODES; i++) {
		kl_get_kaddr((K_NODEPDAINDR + (i * K_NBPW)), &nodepdap, "nodepdap");
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
kl_cpu_nasid(int cpuid)
{
	int nasid;
	k_ptr_t pdap;
	kaddr_t nodepdap;

	if (K_IP == 27) {
		/* Get the pointer to the pda_s struct for cpuid. This structure
		 * contains a pointer to the nodepda_s struct for the node the
		 * cpu is installed on. The address of the nodepda_s struct 
		 * will allow us to determine the nasid value to access node 
		 * memory.
		 */
		pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
		kl_get_pda_s(cpuid, pdap);
		if (KL_ERROR) {
			kl_free_block(pdap);
			return(-1);
		}
		nodepdap = kl_kaddr(pdap, "pda_s", "p_nodepda");
		nasid = kl_addr_to_nasid(nodepdap);
		kl_free_block(pdap);
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
kl_NMI_regbase(int cpuid)
{
	int nasid;
	k_ptr_t pdap, nmi_regs;
	kaddr_t nodepdap, nmi_base = 0;

	if (K_IP == 27) {
		/* Get the pointer to the pda_s struct for cpuid. This structure
		 * contains a pointer to the nodepda_s struct for the node the
		 * cpu is installed on. The address of the nodepda_s struct 
		 * will allow us to determine the nasid value to access node 
		 * memory.
		 */
		pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
		kl_get_pda_s(cpuid, pdap);
		if (KL_ERROR) {
			kl_free_block(pdap);
			return(-1);
		}
		nodepdap = kl_kaddr(pdap, "pda_s", "p_nodepda");
		nasid = kl_addr_to_nasid(nodepdap);

		/* Determine the start address of the NMI saveregs for cpuid
		 * Half of the 0x400 bytes is for saved registers in PROM format
		 * and the other half is for the same register values in eframe_s
		 * format. When the cpu is the B cpu, we have to bump nmi_base
		 * by 2 times IP27_NMI_KREGS_CPU_SIZE;
		 */
		nmi_base = K_K0BASE | 
			(((k_uint_t)nasid) << K_NASID_SHIFT) + IP27_NMI_KREGS_OFFSET;
		if (cpuid % 2) {
			nmi_base += IP27_NMI_KREGS_CPU_SIZE;
		}
	}
	else {

		/* Check to see if the dump was initiated by an NMI...If it was,
		 * use nmi_saveregs to get the starting pc, sp, and ra.
		 */
		if (IS_NMI) {

			nmi_base = kl_sym_addr("nmi_saveregs");
		}
	}
	return(nmi_base);
}

/*
 * kl_NMI_eframe()
 */
k_ptr_t
kl_NMI_eframe(int cpuid, kaddr_t *e)
{
	int nasid;
	k_ptr_t eframep;
	kaddr_t eframe;

	nasid = kl_cpu_nasid(cpuid);
	if (KL_ERROR) {
		return((k_ptr_t *)NULL);
	}
	eframep = kl_alloc_block(EFRAME_S_SIZE, K_TEMP);
	eframe = K_K0BASE |
		(((k_uint_t)nasid) << K_NASID_SHIFT) + IP27_NMI_EFRAME_OFFSET;
	if (cpuid % 2) {
		eframe += IP27_NMI_EFRAME_SIZE;
	}
		
	kl_get_struct(eframe, EFRAME_S_SIZE, eframep, "eframe_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_EFRAME_S;
		kl_free_block(eframep);
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
kl_NMI_saveregs(int cpuid, kaddr_t *errorepc, kaddr_t *epc, 
		kaddr_t *sp, kaddr_t *ra)
{
	int nasid;
	k_ptr_t pdap, nmi_regs, eframep;
	kaddr_t nodepdap, nmi_base;

	kl_reset_error();

	if (K_IP == 27) {

		nmi_base = kl_NMI_regbase(cpuid);
		eframep = kl_NMI_eframe(cpuid, (kaddr_t *)NULL);

		/*   The key register values (epc, ra, and sp) are saved in two
		 *   ways:
		 *
		 *   - in prom format
		 *
		 *   - in eframe format
		 *   
		 *   The call to kl_NMI_regbase() returns a kernel pointer to 
		 *   the base of the PROM format saved register values. It will
		 *   also return a filled in eframe buffer. We can use either
		 *   set of register values. For now, we'll use the eframe
		 *   values for sp, ra, and epc and use the PROM value for
		 *   errorepc.
		 */
#ifdef XXX 
		kl_get_kaddr(nmi_base + SP_OFFSET, sp, "sp");
		kl_get_kaddr(nmi_base + RA_OFFSET, ra, "ra");
		kl_get_kaddr(nmi_base + EPC_OFFSET, epc, "epc");
#endif
		*epc = *(kaddr_t*)((uint)eframep + 
				(kl_member_offset("eframe_s", "ef_epc")));
		*ra = *(kaddr_t*)((uint)eframep + 
				(kl_member_offset("eframe_s", "ef_ra")));
		*sp = *(kaddr_t*)((uint)eframep + 
				(kl_member_offset("eframe_s", "ef_sp")));
		kl_get_kaddr(nmi_base + ERROR_EPC_OFFSET, errorepc, "errorepc");

		if (eframep) {
			kl_free_block(eframep);
		}
	}
	else {

		/* For IP19, IP21, and IP25 systems, check to see if the dump 
		 * was initiated by an NMI...If it was, use nmi_saveregs to get 
		 * the starting pc, sp, and ra.
		 */
		if (IS_NMI) {

			nmi_base = kl_NMI_regbase(cpuid);

			/* Get the PC, RA and SP from the NMI dumpregs.
			 */
			kl_get_kaddr(nmi_base + (cpuid * 32), epc, "pc");
			kl_get_kaddr(nmi_base + (cpuid * 32) + 16, sp, "sp");
			kl_get_kaddr(nmi_base + (cpuid * 32) + 24, ra, "ra");
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
kl_bhv_pdata(kaddr_t bhv)
{
	kaddr_t pdata;
	k_ptr_t bhvp;

	bhvp = kl_alloc_block(BHV_DESC_SIZE, K_TEMP);
	kl_get_struct(bhv, BHV_DESC_SIZE, bhvp, "bhv_desc");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_BHV_DESC;
		kl_free_block(bhvp);
		return((kaddr_t)NULL);
	}
	pdata = kl_bhvp_pdata(bhvp);
	kl_free_block(bhvp);
	return(pdata);
}

/* 
 * kl_bhv_vobj()
 *
 *   Take a kernel pointer to a bhv_desc struct and return the contents
 *   of the bd_vobj field (which points to the virtual object).
 */
kaddr_t
kl_bhv_vobj(kaddr_t bhv)
{
	kaddr_t vobj;
	k_ptr_t bhvp;

	bhvp = kl_alloc_block(BHV_DESC_SIZE, K_TEMP);
	kl_get_struct(bhv, BHV_DESC_SIZE,  bhvp, "bhv_desc");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_BHV_DESC;
		kl_free_block(bhvp);
		return((kaddr_t)NULL);
	}
	vobj = kl_bhvp_vobj(bhvp);
	kl_free_block(bhvp);
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
kl_bhvp_pdata(k_ptr_t bhvp)
{
	kaddr_t pdata;

	pdata = kl_kaddr(bhvp, "bhv_desc", "bd_pdata");
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
kl_bhvp_vobj(k_ptr_t bhvp)
{
	kaddr_t vobj;

	vobj = kl_kaddr(bhvp, "bhv_desc", "bd_vobj");
	return(vobj);

}

/*
 * kl_get_nodepda_s() -- Get a nodepda_s structure and store it in a buffer.
 */
kaddr_t
kl_get_nodepda_s(kaddr_t value, k_ptr_t npb)
{
	int mode;
	kaddr_t nodepdaval, nodepdap;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "kl_get_nodepda_s: value=0x%llx, npb=0x%x\n",
			value, npb);
	}

	kl_reset_error();

	/* Since kl_get_nodepda_s() returns a kaddr_t value, it cannot return
	 * a pointer to a dynamically allocated block. Consequently,
	 * npb cannot be NULL (it must contain a pointer to a block
	 * large enough to hold the size of a nodepda_s struct).
	 */
	if (!npb) {
		if (DEBUG(KLDC_GLOBAL, 1)) {
			fprintf(KL_ERRORFP, "kl_get_nodepda_s: npb is NULL!\n");
		}
		KL_SET_ERROR(KLE_NULL_BUFF);
		return((kaddr_t)NULL);
	}

	/* Check to see if value is a nodeid (since value is unsigned, it
	 * can never be less than zero).
	 */
	if (value < K_NUMNODES) {

		/* Get the pointer to the nodepda_s struct for cpuid in nodepdaindr
		 */
		nodepdaval = K_NODEPDAINDR + (value * K_NBPW);
		kl_get_kaddr(nodepdaval, (k_ptr_t)&nodepdap, "nodepdap");
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_NODEPDA;
			return((kaddr_t)NULL);
		}
		mode = 0;
	}
	else {

		/* value MUST be a valid kernel address. Give it a try...
		 */
		nodepdap = value;
		mode = 2;
	}

	/* Now get the nodepda_s struct
	 */
	kl_get_struct(nodepdap, NODEPDA_S_SIZE, npb, "nodepda_s");
	if (KL_ERROR) {
		KL_SET_ERROR_NVAL(KL_ERROR|KLE_BAD_NODEPDA, value, mode);
		return((kaddr_t)NULL);
	}
	else {
		return(nodepdap);
	}
}

/* 
 * kl_get_pda_s() -- Get a pda_s structure and store it in a buffer.
 */
kaddr_t
kl_get_pda_s(kaddr_t value, k_ptr_t pdabuf)
{
	int mode;
	kaddr_t pdaval, pdap;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "get_pda_s: value=0x%llx, pdabuf=0x%x\n",
			value, pdabuf);
	}

	kl_reset_error();

	/* Since get_pda_s() returns a kaddr_t value, it cannot return
	 * a pointer to a dynamically allocated block. Consequently, 
	 * pdabuf cannot be NULL (it must contain a pointer to a block
	 * large enough to hold the size of a pda_s struct).
	 */
	if (!pdabuf) {
		if (DEBUG(KLDC_GLOBAL, 1)) {
			fprintf(KL_ERRORFP, "get_pda_s: pdabuf is NULL!\n");
		}
		KL_SET_ERROR(KLE_NULL_BUFF);
		return((kaddr_t)NULL);
	}

	/* Check to see if value is a cpuid (since value is unsigned, it
	 * can never be less than zero).
	 */
	if (value < K_MAXCPUS) {

		int cpuid;

		/* Get the pointer to the pda_s struct for cpuid in pdaindr
		 */
		pdaval = K_PDAINDR + (value * (2 * K_NBPW)) + K_NBPW;

		kl_get_kaddr(pdaval, (k_ptr_t)&pdap, "pdap");
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_PDA;
			return((kaddr_t)NULL);
		}

		/* Make sure there is a CPU installed. It's possible for a node
		 * to have only one CPU installed.
		 */
		if (pdap == 0) {
			KL_SET_ERROR_NVAL(KLE_NO_CPU, value, 0);
			return((kaddr_t)NULL);
		}
		mode = 0;
	} 
	else {

		/* value must be a valid kernel address. Give it a try...
		 */
		pdap = value;
		mode = 2;
	}

	/* Now get the pda_s struct
	 */
	kl_get_struct(pdap, PDA_S_SIZE, pdabuf, "pda_s");
	if (KL_ERROR) {
		KL_SET_ERROR_NVAL(KL_ERROR|KLE_BAD_PDA, value, mode);
		return((kaddr_t)NULL);
	} 
	else {
		return(pdap);
	}
}

/*
 * dump_string()
 */
kaddr_t
dump_string(kaddr_t addr, int flags, FILE *ofp)
{
	int length, done = 0;
	unsigned *buf;

	if (DEBUG(KLDC_GLOBAL, 3)) {
		fprintf(KL_ERRORFP, "dump_string: addr=0x%llx, flags=%d\n",
			addr, flags);
	}

	kl_reset_error();

	buf = (unsigned *)kl_alloc_block(IO_NBPC, K_TEMP);
	while (!done) {
		kl_get_block(addr, IO_NBPC, buf, "string");
		if (KL_ERROR) {
			kl_free_block((k_ptr_t)buf);
			return(-1);
		}
		fprintf(ofp, "%s", (char*)buf);
		if ((length = strlen((char *)buf)) < IO_NBPC) {
			done = 1;
		}
		else {
			addr += IO_NBPC;
		}
	}
	kl_free_block((k_ptr_t)buf);
	return (addr + length + 1);
}

/*
 * print_string()
 *
 *   print out a string, translating all embeded control characters
 *   (e.g., '\n' for newline, '\t' for tab, etc.)
 */
void
print_string(char *s, FILE *ofp)
{
	char *sp, *cp;

	kl_reset_error();

	if (!(sp = s)) {
		KL_SET_ERROR(KLE_BAD_STRING);
		return;
	}

	while (sp) {
		if (cp = strchr(sp, '\\')) {
			switch (*(cp + 1)) {

				case 'n' :
					*cp++ = '\n';
					*cp++ = 0;
					break;

				case 't' :
					*cp++ = '\t';
					*cp++ = 0;
					break;

				default :
					if (*(cp + 1) == 0) {
						KL_SET_ERROR(KLE_BAD_STRING);
						return;
					}
					/* Change the '\' character to a zero and then print
					 * the string (the rest of the string will be picked
					 * up on the next pass).
					 */
					*cp++ = 0;
					break;
			}
			fprintf(ofp, "%s", sp);
			sp = cp;
		}
		else {
			fprintf(ofp, "%s", sp);
			sp = 0;
		}
	}
}

