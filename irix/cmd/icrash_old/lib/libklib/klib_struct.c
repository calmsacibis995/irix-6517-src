#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/klib_struct.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include "klib.h"
#include "klib_extern.h"

/*
 * kl_get_struct() -- Get any type of struct and load into bufp
 */
k_ptr_t
kl_get_struct(klib_t *klp, kaddr_t addr, int size, k_ptr_t bufp, char *s)
{
	k_ptr_t bp;

	if (DEBUG(KLDC_FUNCTRACE, 4)) {
		fprintf(KL_ERRORFP, "kl_get_struct: addr=0x%llx, size=%d, "
			"bufp=0x%x, s=\"%s\"\n", addr, size, bufp, s);
	}

	kl_reset_error();

	/* Make sure that the kernel virtual address is valid (physical 
	 * addresses are not allowed here). Also make sure the (potentially
	 * valid) virtual address maps to a valid physical address on the 
	 * system (an error will be set if it's not).
	 */
	kl_is_valid_kaddr(klp, addr, (k_ptr_t)NULL, WORD_ALIGN_FLAG);
	if (KL_ERROR) {
		return((k_ptr_t)NULL);
	}

	/* In most cases, the size of the struct will be passed in.
	 * It's much faster than calling kl_struct_len() all the time. However,
	 * in case the size isn't provided, get it here (this should really 
	 * never fail unless the struct name is bogus).
	 */
	if ((size == 0) && !(size = kl_struct_len(klp, s))) {
		KL_SET_ERROR_CVAL(KLE_INVALID_STRUCT_SIZE, s);
		return((k_ptr_t)NULL);
	}

	/* If an error occurs somewhere down the line, bp will be NULL
	 * and error will be set to the relevent error code.
	 */
	bp = kl_get_block(klp, addr, size, bufp, s);
	return(bp);
}

/*
 * kl_get_graph_edge_s() 
 * 
 */
k_ptr_t
kl_get_graph_edge_s(klib_t *klp, k_ptr_t v, kaddr_t value, 
	int mode, k_ptr_t gep, int flags)
{
	int num_edge;
	kaddr_t edge, info_list;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_graph_edge_s: v=0x%x, value=%lld, "
				"mode=%d, ", v, value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_graph_edge_s: value=0x%llx, "
				"mode=%d, ", value, mode);
		}
		fprintf(KL_ERRORFP, "gvp=0x%x, flags=0x%x\n", gep, flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if (mode == 2) {
		if (!value) {
			KL_SET_ERROR_NVAL(KLE_BAD_STRUCT, value, mode);
			return ((k_ptr_t)NULL);
		}
		edge = value;
	}
	else if (mode == 0) {

		/* We have to get the numbered edge from vertex v
		 */

		num_edge = KL_UINT(klp, v, "graph_vertex_s", "v_num_edge");
		if (value >= num_edge) {
			KL_SET_ERROR_NVAL(KLE_BAD_GRAPH_VERTEX_S, value, mode);
			return ((k_ptr_t)NULL);
		}
		edge = vertex_edge_ptr(klp, v) + (value * GRAPH_EDGE_S_SIZE(klp));
	}

	kl_get_struct(klp, edge, GRAPH_EDGE_S_SIZE(klp), gep, "graph_edge_s");
	if (KL_ERROR) {
		return((k_ptr_t)NULL);
	}
	return(gep);
}

/*
 * kl_get_graph_info_s() 
 * 
 */
k_ptr_t
kl_get_graph_info_s(klib_t *klp, k_ptr_t v, kaddr_t value, 
	int mode, k_ptr_t gip, int flags)
{
	int num_lbl;
	kaddr_t lbl, info_list;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_graph_info_s: v=0x%x, value=%lld, "
				"mode=%d, ", v, value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_graph_info_s: value=0x%llx, "
				"mode=%d, ", value, mode);
		}
		fprintf(KL_ERRORFP, "gip=0x%x, flags=0x%x\n", gip, flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if (mode == 2) {
		if (!value) {
			KL_SET_ERROR_NVAL(KLE_BAD_STRUCT, value, mode);
			return ((k_ptr_t)NULL);
		}
		lbl = value;
	}
	else if (mode == 0) {

		/* We have to get the numbered label from vertex v
		 */

		num_lbl = KL_UINT(klp, v, "graph_vertex_s", "v_num_info_LBL");
		if (value >= num_lbl) {
			KL_SET_ERROR_NVAL(KLE_BAD_VERTEX_INFO, value, mode);
			return ((k_ptr_t)NULL);
		}
		lbl = vertex_lbl_ptr(klp, v) + (value * GRAPH_INFO_S_SIZE(klp));
	}

	kl_get_struct(klp, lbl, GRAPH_INFO_S_SIZE(klp), gip, "graph_info_s");
	if (KL_ERROR) {
		return((k_ptr_t)NULL);
	}
	return(gip);
}

/*
 * kl_get_graph_vertex_s() 
 * 
 */
k_ptr_t
kl_get_graph_vertex_s(klib_t *klp, kaddr_t value, int mode, 
	k_ptr_t gvp, int flags)
{
	kaddr_t vertex;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_graph_vertex_s: value=%lld, "
				"mode=%d, ", value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_graph_vertex_s: value=0x%llx, "
				"mode=%d, ", value, mode);
		}
		fprintf(KL_ERRORFP, "gvp=0x%x, flags=0x%x\n", gvp, flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if (mode == 2) {
		if (!value) {
			KL_SET_ERROR_NVAL(KLE_BAD_STRUCT, value, mode);
			return ((k_ptr_t)NULL);
		}
		vertex = value;
	}
	else if (mode == 0) {
		vertex = handle_to_vertex(klp, value);
		if (KL_ERROR) {
			return ((k_ptr_t)NULL);
		}
	}
	else {
		KL_SET_ERROR_NVAL(KLE_BAD_STRUCT, value, mode);
		return((k_ptr_t)NULL);
	}

	/* If this is mode 0, we need to convert the vertex handle to
	 * the actual vertex address.
	 */
	kl_get_struct(klp, vertex, VERTEX_SIZE(klp), gvp, "graph_vertex_s");
	if (KL_ERROR) {
		return((k_ptr_t)NULL);
	}
	return(gvp);
}

#ifdef ANON_ITHREADS
/*
 * kl_get_ithread_s() -- Get a ithread_s struct
 * 
 *   This routine returns a pointer to a ithread_s struct for a valid
 *   ithread (unless the K_ALL flag is set). 
 */
k_ptr_t
kl_get_ithread_s(klib_t *klp, kaddr_t value, int mode, int flags)
{
	kaddr_t ithread;
	k_ptr_t itp;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "get_ithread_s: value=%lld, mode=%d, ",
				value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "get_ithread_s: value=0x%llx, mode=%d, ",
				value, mode);
		}
		fprintf(KL_ERRORFP, "flags=0x%x\n", flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if ((mode == 2) && !value) {
		KL_SET_ERROR_NVAL(KLE_BAD_ITHREAD_S, value, mode);
		return ((k_ptr_t)NULL);
	}

	if (mode == 0) {
		KL_SET_ERROR_NVAL(KLE_BAD_ITHREAD_S, value, mode);
		return((k_ptr_t)NULL);
	}
	else if (mode == 1) {
#ifdef XXX
		ithread = id_to_ithread((int)value);
#endif
	}
	else {
		ithread = value;
	}

	/* Allocate a block for a ithread_s struct. Make sure and check to
	 * see if a K_PERM block needs to be allocated.
	 */
	if (flags & K_PERM) {
		itp = klib_alloc_block(klp, ITHREAD_S_SIZE(klp), K_PERM);
	}
	else {
		itp = klib_alloc_block(klp, ITHREAD_S_SIZE(klp), K_TEMP);
	}
	kl_get_struct(klp, ithread, ITHREAD_S_SIZE(klp), itp, "ithread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_ITHREAD_S;
		return((k_ptr_t)NULL);
	}

	 * Make an attempt to verify that the ithread is valid. If
	 * it is valid, or if the K_ALL flag is set, return the 
	 * pointer to the block containing the ithread_s struct. 
	 * Otherwise return an error.
	 */
	if  (IS_ITHREAD(klp, itp) || (flags & K_ALL)) {
		return (itp);
	}
	else {
		KL_SET_ERROR_NVAL(KLE_BAD_ITHREAD_S, ithread, 2);
		klib_free_block(klp, itp);
		return ((k_ptr_t)NULL);
	}
}
#endif

/*
 * kl_get_kthread() -- Get a kthread struct
 * 
 *   This routine takes a kernel pointer to a kthread struct and returns 
 *   a pointer to a buffer containing the structure the kthread struct
 *   is part of. For example, if the kthread is part of a uthread_s struct,
 *   then the entire uthread_s struct is read into a buffer and a pointer 
 *   to that buffer is returned. If the kthread pointer is not valid or 
 *   some error occurred, a NULL pointer value will be returned and error 
 *   will be set to an appropriate value.
 */
k_ptr_t
kl_get_kthread(klib_t *klp, kaddr_t kthread, int flags)
{
	k_ptr_t ktp, typ;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "get_kthread: kthread=0x%llx, flags=0x%x",
			kthread, flags);
	}

	/* Just in case... 
	 */
	if (!kthread) {
		KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, 0, 2);
		return ((k_ptr_t)NULL);
	}

	/* Allocate a buffer to hold a kthread struct. 
	 */
	ktp = klib_alloc_block(klp, KTHREAD_SIZE(klp), K_TEMP);

	/* Get the kthread structure
	 */
	kl_get_struct(klp, kthread, KTHREAD_SIZE(klp), ktp, "kthread"); 
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_KTHREAD;
		klib_free_block(klp, ktp);
		return((k_ptr_t*)NULL);
	}

	/* Determine what type of kthread this is (KT_UTHREAD, KT_ITHREAD, or
     * KT_STHREAD) and get a block with the appropriate structure loaded
	 * into it. If an error occurs, typ will be NULL and the global
	 * error value will be set. Just return typ.
	 */
	if (IS_UTHREAD(klp, ktp)) {
		typ = kl_get_uthread_s(klp, kthread, 2, flags);
	}
#ifdef ANON_ITHREADS
	else if (IS_ITHREAD(klp, ktp)) {
		typ = kl_get_ithread_s(klp, kthread, 2, flags);
	}
#endif
	else if (IS_XTHREAD(klp, ktp)) {
		typ = kl_get_xthread_s(klp, kthread, 2, flags);
	}
	else if (IS_STHREAD(klp, ktp)) {
		typ = kl_get_sthread_s(klp, kthread, 2, flags);
	}
	else {
		if (flags & K_ALL) {
			return(ktp);
		}
		else {
			KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, kthread, 2);
			klib_free_block(klp, ktp);
			return ((k_ptr_t)NULL);
		}
	}

	/* Free the block holding the kthread and return the block holding the
	 * structure the kthread struct is part of.
	 */
	klib_free_block(klp, ktp);
	return(typ);
}

/*
 * kl_get_nodepda_s() -- Get a nodepda_s structure and store it in a buffer.
 */
kaddr_t
kl_get_nodepda_s(klib_t *klp, kaddr_t value, k_ptr_t npb)
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
    if (value < K_NUMNODES(klp)) {

        /* Get the pointer to the nodepda_s struct for cpuid in nodepdaindr
         */
        nodepdaval = K_NODEPDAINDR(klp) + (value * K_NBPW(klp));
        kl_get_kaddr(klp, nodepdaval, (k_ptr_t)&nodepdap, "nodepdap");
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
    kl_get_struct(klp, nodepdap, NODEPDA_S_SIZE(klp), npb, "nodepda_s");
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
kl_get_pda_s(klib_t *klp, kaddr_t value, k_ptr_t pdabuf)
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
	if (value < K_MAXCPUS(klp)) {

		int cpuid;

		/* Get the pointer to the pda_s struct for cpuid in pdaindr
		 */
		pdaval = K_PDAINDR(klp) + (value * (2 * K_NBPW(klp))) + K_NBPW(klp);

		kl_get_kaddr(klp, pdaval, (k_ptr_t)&pdap, "pdap");
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
	kl_get_struct(klp, pdap, PDA_S_SIZE(klp), pdabuf, "pda_s");
	if (KL_ERROR) {
		KL_SET_ERROR_NVAL(KL_ERROR|KLE_BAD_PDA, value, mode);
		return((kaddr_t)NULL);
	} 
	else {
		return(pdap);
	}
}

/*
 * kl_get_pde()
 */
k_ptr_t
kl_get_pde(klib_t *klp, kaddr_t addr, k_ptr_t pdep)
{
	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "kl_get_pde: addr=0x%llx, pdep=0x%x\n", 
			addr, pdep);
	}

	kl_virtop(klp, addr, (k_ptr_t)NULL);
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_PDE;
		return((k_ptr_t)NULL);
	}

	/* Make sure pointer is properly aligned
	 */
	if (addr % PDE_SIZE(klp)) {
		KL_SET_ERROR_NVAL(KLE_INVALID_VADDR_ALIGN|KLE_BAD_PDE, addr, 2);
		return((k_ptr_t)NULL);
	}

	kl_get_block(klp, addr, PDE_SIZE(klp), pdep, "pde_t");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_PDE;
		return((k_ptr_t)NULL);
	}
	return(pdep);
}

/*
 * kl_get_proc() -- Get a proc struct
 * 
 *   This routine returns a pointer to a proc struct for a valid
 *   process (unless the K_ALL flag is set). If the process is not 
 *   valid or an error occurred, a NULL value is returned (error 
 *   will be set to the appropriate value).
 */
k_ptr_t
kl_get_proc(klib_t *klp, kaddr_t value, int mode, int flags)
{
	kaddr_t proc, pdata;
	k_ptr_t procp;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_proc: value=%lld, mode=%d, ",
				value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_proc: value=0x%llx, mode=%d, ",
				value, mode);
		}
		fprintf(KL_ERRORFP, "flags=0x%x\n", flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if ((mode == 2) && !value) {
		KL_SET_ERROR_NVAL(KLE_BAD_PROC, value, mode);
		return ((k_ptr_t)NULL);
	}

	if (mode == 0) {
		KL_SET_ERROR_NVAL(KLE_BAD_PROC, value, mode);
		return((k_ptr_t)NULL);
	}
	else if (mode == 1) {
		proc = kl_pid_to_proc(klp, (int)value);
	}
	else {
		proc = value;
	}

	/* Allocate a block for a proc struct. Make sure and check to
	 * see if a K_PERM block needs to be allocated.
	 */
	if (flags & K_PERM) {
		procp = klib_alloc_block(klp, PROC_SIZE(klp), K_PERM);
	}
	else {
		procp = klib_alloc_block(klp, PROC_SIZE(klp), K_TEMP);
	}
	kl_get_struct(klp, proc, PROC_SIZE(klp), procp, "proc");
	if (KL_ERROR) {
		/* We need to make sure the error values reflect that this
		 * is a process type kthread and that the proper value and
		 * node are used (the values that generated the error in
		 * get_kthread() may not be the same).
		 */
		KL_ERROR |= KLE_BAD_PROC;
		KLIB_ERROR.e_nval = value;
		KLIB_ERROR.e_mode = mode;
		return((k_ptr_t)NULL);
	}

	/* Make sure this is actually a proc struct. The bd_pdata field
	 * of the p_bhv struct contains a pointer to the proc. If we 
	 * get a match, we can assume this is a valid proc struct. If
	 * not then return an errro (unless the K_ALL flag is set).
	 */
	pdata = kl_bhv_pdata(klp, proc + kl_member_offset(klp, "proc", "p_bhv")); 
	if ((proc != pdata) && !(flags & K_ALL)) {
		klib_free_block(klp, procp);
		KL_SET_ERROR_NVAL(KLE_BAD_PROC, value, mode);
		return ((k_ptr_t)NULL);
	}

	/* Make sure the process is valid (active), or that the 
	 * K_ALL flag is set. 
	 */
	if (KL_INT(klp, procp, "proc", "p_stat") || (flags & K_ALL)) {
		return (procp);
	}
	else {
		KL_SET_ERROR_NVAL(KLE_BAD_PROC, value, mode);
		klib_free_block(klp, procp);
		return ((k_ptr_t)NULL);
	}
}

/*
 * kl_get_sthread_s() -- Get a sthread_s struct
 * 
 *   This routine returns a pointer to a sthread_s struct for a valid
 *   sthread (unless the K_ALL flag is set). 
 */
k_ptr_t
kl_get_sthread_s(klib_t *klp, kaddr_t value, int mode, int flags)
{
	kaddr_t sthread;
	k_ptr_t stp;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_sthread_s: value=%lld, mode=%d, ",
				value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_sthread_s: value=0x%llx, mode=%d, ",
				value, mode);
		}
		fprintf(KL_ERRORFP, "flags=0x%x\n", flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if ((mode == 2) && !value) {
		KL_SET_ERROR_NVAL(KLE_BAD_STHREAD_S, value, mode);
		return ((k_ptr_t)NULL);
	}

	if (mode == 0) {
		KL_SET_ERROR_NVAL(KLE_BAD_STHREAD_S, value, mode);
		return((k_ptr_t)NULL);
	}
	else if (mode == 1) {
#ifdef XXX
		sthread = id_to_sthread((int)value);
#endif
	}
	else {
		sthread = value;
	}

	/* Allocate a block for a sthread_s struct. Make sure and check to
	 * see if a K_PERM block needs to be allocated.
	 */
	if (flags & K_PERM) {
		stp = klib_alloc_block(klp, STHREAD_S_SIZE(klp), K_PERM);
	}
	else {
		stp = klib_alloc_block(klp, STHREAD_S_SIZE(klp), K_TEMP);
	}
	kl_get_struct(klp, sthread, STHREAD_S_SIZE(klp), stp, "sthread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_STHREAD_S;
		return((k_ptr_t)NULL);
	}

	/* Make an attempt to verify that the sthread is valid. If
	 * it is valid, or if the K_ALL flag is set, return the
	 * pointer to the block containing the sthread_s struct.
	 * Otherwise return an error.
	 */
	if (IS_STHREAD(klp, stp) || (flags & K_ALL)) {
		return (stp);
	}
	else {
		KL_SET_ERROR_NVAL(KLE_BAD_STHREAD_S, sthread, 2);
		klib_free_block(klp, stp);
		return ((k_ptr_t)NULL);
	}
}

/*
 * kl_get_uthread_s() -- Get a uthread_s struct
 * 
 *   This routine returns a pointer to a uthread_s struct for a valid
 *   uthread (unless the K_ALL flag is set). 
 */
k_ptr_t
kl_get_uthread_s(klib_t *klp, kaddr_t value, int mode, int flags)
{
	kaddr_t uthread;
	k_ptr_t utp;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_uthread_s: value=%lld, mode=%d, ",
				value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_uthread_s: value=0x%llx, mode=%d, ",
				value, mode);
		}
		fprintf(KL_ERRORFP, "flags=0x%x\n", flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if ((mode == 2) && !value) {
		KL_SET_ERROR_NVAL(KLE_BAD_UTHREAD_S, value, mode);
		return ((k_ptr_t)NULL);
	}

	if (mode == 0) {
		KL_SET_ERROR_NVAL(KLE_BAD_UTHREAD_S, value, mode);
		return((k_ptr_t)NULL);
	}
	else if (mode == 1) {
#ifdef XXX
		uthread = id_to_uthread((int)value);
#endif
	}
	else {
		uthread = value;
	}

	/* Allocate a block for a uthread_s struct. Make sure and check to
	 * see if a K_PERM block needs to be allocated.
	 */
	if (flags & K_PERM) {
		utp = klib_alloc_block(klp, UTHREAD_S_SIZE(klp), K_PERM);
	}
	else {
		utp = klib_alloc_block(klp, UTHREAD_S_SIZE(klp), K_TEMP);
	}
	kl_get_struct(klp, uthread, UTHREAD_S_SIZE(klp), utp, "uthread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_UTHREAD_S;
		return((k_ptr_t)NULL);
	}

	/* Make an attempt to verify that the uthread is valid. If
	 * it is valid, or if the K_ALL flag is set, return the
	 * pointer to the block containing the uthread_s struct.
	 * Otherwise return an error.
	 */
	if (IS_UTHREAD(klp, utp) || (flags & K_ALL)) {
		return (utp);
	}
	else {
		KL_SET_ERROR_NVAL(KLE_BAD_UTHREAD_S, uthread, 2);
		klib_free_block(klp, utp);
		return ((k_ptr_t)NULL);
	}
}

/*
 * kl_get_xthread_s() -- Get a xthread_s struct
 * 
 *   This routine returns a pointer to a xthread_s struct for a valid
 *   xthread (unless the K_ALL flag is set). 
 */
k_ptr_t
kl_get_xthread_s(klib_t *klp, kaddr_t value, int mode, int flags)
{
	kaddr_t xthread;
	k_ptr_t xtp;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_xthread_s: value=%lld, mode=%d, ",
				value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_xthread_s: value=0x%llx, mode=%d, ",
				value, mode);
		}
		fprintf(KL_ERRORFP, "flags=0x%x\n", flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if ((mode == 2) && !value) {
		KL_SET_ERROR_NVAL(KLE_BAD_XTHREAD_S, value, mode);
		return ((k_ptr_t)NULL);
	}

	if (mode == 0) {
		KL_SET_ERROR_NVAL(KLE_BAD_XTHREAD_S, value, mode);
		return((k_ptr_t)NULL);
	}
	else if (mode == 1) {
#ifdef XXX
		xthread = id_to_xthread((int)value);
#endif
	}
	else {
		xthread = value;
	}

	/* Allocate a block for a xthread_s struct. Make sure and check to
	 * see if a K_PERM block needs to be allocated.
	 */
	if (flags & K_PERM) {
		xtp = klib_alloc_block(klp, XTHREAD_S_SIZE(klp), K_PERM);
	}
	else {
		xtp = klib_alloc_block(klp, XTHREAD_S_SIZE(klp), K_TEMP);
	}
	kl_get_struct(klp, xthread, XTHREAD_S_SIZE(klp), xtp, "xthread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_XTHREAD_S;
		return((k_ptr_t)NULL);
	}

	/* Make an attempt to verify that the xthread is valid. If
	 * it is valid, or if the K_ALL flag is set, return the
	 * pointer to the block containing the xthread_s struct.
	 * Otherwise return an error.
	 */
	if (IS_XTHREAD(klp, xtp) || (flags & K_ALL)) {
		return (xtp);
	}
	else {
		KL_SET_ERROR_NVAL(KLE_BAD_XTHREAD_S, xthread, 2);
		klib_free_block(klp, xtp);
		return ((k_ptr_t)NULL);
	}
}
