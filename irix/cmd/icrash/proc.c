#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/RCS/proc.c,v 1.1 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#define _KERNEL 0
#include <sys/file.h>
#undef _KERNEL
#include <klib/klib.h>
#include "icrash.h"

extern int vfile_hash(kaddr_t key);
extern int val64_compare(list_of_ptrs_t *list1,list_of_ptrs_t *list2);

/*
 *  Just a reminder: The below macros take arguments which are
 *  pointers in icrash process address space and not in kernel address space.
 *  
 *  We use ICRASH_* for any macro which is also defined by the kernel.
 */

#define P_PRXY(proc)  ((k_ptr_t)((__psunsigned_t)proc + \
	(__psunsigned_t)kl_member_offset("proc","p_proxy")))

#define ICRASH_ISSHDFD(prxy) \
 (kl_uint(prxy,"proc_proxy_s","prxy_shmask",0) & PR_SFDS)

#define S_ZOMB 2

static int file_cnt = 0;

/* 
 * This is a class of functions which return a boolean value depending on 
 * if the vfile has a certain property. In this case here we just want to
 * print the vfile out. So we always return 0. i.e., don't save this vfile
 * after we are done printing it. One restriction of this function class
 * is that.. they all have to have the same argument list.. irrespective of
 * whether the function uses all the arguments or not.
 *
 * Another example of a function in this class is test_vfile_vsocket. 
 * test_vfile_vsocket tests if the vfile is a vsocket. If it is it adds the 
 * vsocket to the hash table.
 */
int test_print_vfile(kaddr_t proc,kaddr_t caddr,char *fbuf,int flags,int j,
			 FILE *ofp,list_of_ptrs_t *list,int *first_time)
{
	if (caddr && get_vfile(caddr, fbuf, flags)) {
		if (*first_time || (flags & (C_NEXT|C_FULL))) {
			if (*first_time) {
				fprintf(ofp, "\nOPEN FILES FOR PROC 0x%llx:\n\n", proc);
				*first_time = FALSE;
			}
			fprintf(ofp, "   FD   ");
			vfile_banner(ofp, BANNER);
			fprintf(ofp, "  ------");
			vfile_banner(ofp, SMINOR);
		}
		fprintf(ofp, "  %3d   ", j);
		print_vfile(caddr, fbuf, flags, ofp);
		file_cnt++;
	}
	return(0);
}

/*
 * This function is similar to test_print_vfile. Look at comments there.
 * This function looks to see if the vfile is a vsocket. If it is a vsocket
 * the vsocket is stored in the vlist.
 *
 * This function needs to have the same arguments as test_print_vfile.
 */
int test_vfile_vsocket(kaddr_t proc,kaddr_t caddr,char *fbuf,int flags,int j,
			   FILE *ofp,list_of_ptrs_t *list,int *first_time)
{
	if (caddr && get_vfile(caddr, fbuf, flags) &&
				(KL_UINT(fbuf,"vfile", "vf_flag") & FSOCKET)) {
		list->val64 = kl_kaddr(fbuf, "vfile", "__vf_data__");
		return 1;
	}
	return 0;
}


int test_vfile_socket(kaddr_t proc,kaddr_t caddr,char *fbuf,int flags,int j,
		 FILE *ofp,list_of_ptrs_t *list,int *first_time)
{	
		kaddr_t socket, bhv,vsocket;
	k_ptr_t sp,rcvp,sndp;
	k_ptr_t vbuf;

	kl_reset_error();
	list->val64=NULL;
	if(!test_vfile_vsocket(proc, caddr, fbuf, flags, j, 
			ofp, list, first_time) || !list->val64) {
		return(0);
	}
	vbuf = kl_alloc_block(VSOCKET_SIZE, K_TEMP);
	vsocket=list->val64;
	kl_get_struct(vsocket,VSOCKET_SIZE, vbuf, "vsocket");
	if (!KL_ERROR) {
		bhv = kl_kaddr(vbuf, "vsocket", "vs_bh");
		kl_free_block(vbuf);
		socket = kl_bhv_pdata(bhv);
#if 0
		fprintf(ofp,"proc=%llx,vfile=%llx vsocket=%llx socket=%llx\n",
			proc, caddr, vsocket, socket);
#endif
		if (KL_ERROR) {
			kl_print_error();
			return(0);
		}
		list->val64 = socket;
		return(1);
	}
	kl_free_block(vbuf);
	return(0);
}

/*
 * test_vfile_mbufsocket()
 *
 * This function looks to see if the vfile is a socket with an mbuf.
 * It assumes that fbuf is a buffer of size vfile. If it is a socket with an
 * mbuf.... the socket is stored in the list.
 *
 */
int 
test_vfile_mbufsocket(
	kaddr_t proc,
	kaddr_t caddr,
	char *fbuf, 
	int flags, 
	int j,
	FILE *ofp,
	list_of_ptrs_t *list,
	int *first_time)
{	
	kaddr_t socket, bhv;
	k_ptr_t sp, rcvp, sndp;

	kl_reset_error();
	list->val64=NULL;

   /* We need this hack so that get_vfile() only reads vfiles
	* which are valid.
	*/
	flags &= ~C_ALL;			      

	if(!test_vfile_socket(proc, caddr, fbuf, flags, j, 
			ofp, list, first_time) || !list->val64) {
		return(0);
	}
	
	socket = list->val64;
	sp = kl_alloc_block(SOCKET_SIZE, K_TEMP);
	get_socket(socket, sp, flags);
	if (KL_ERROR) {
		kl_print_error();
		kl_free_block(sp);
		return(0);
	}
	
	rcvp = (k_ptr_t)((uint)sp + kl_member_offset("socket", "so_rcv"));
	sndp = (k_ptr_t)((uint)sp + kl_member_offset("socket", "so_snd"));
	
	if (kl_kaddr(rcvp,"sockbuf","sb_mb") || kl_kaddr(sndp,"sockbuf","sb_mb")) {	
		/* We have a socket with an mbuf. We don't have to assign
		 * to list->val64... since upper layer call was also a socket.
		 */
		kl_free_block(sp);
		return(1);
	}
	kl_free_block(sp);
	return(0);
}

/*
 * list_proc_files() -- Dump out files related to a certain process.
 */
int
list_proc_files(kaddr_t paddr, int mode, int flags, FILE *ofp)
{
	k_ptr_t *Phashbase;
	int ret;

	Phashbase =
		(k_ptr_t *)kl_alloc_block(VFILE_HASHSIZE*sizeof(k_ptr_t *), K_TEMP);
	bzero(Phashbase,VFILE_HASHSIZE*sizeof(k_ptr_t *));
	ret = findall_proc_vfiles(paddr, mode, flags, ofp, 
			Phashbase, test_print_vfile);
	kl_free_block(Phashbase);
	return(ret);
}

/*
 * find_proc_vfiles() --  Call test_vfile on all the vfiles we find and add
 *                        them to the list.
 *
 * ASSUMPTIONS THIS FUNCTION MAKES:
 *
 *   It allocates and returns the list of vfiles in Phashbase.
 *   Caller's responsibility to free the pointers to the list of vfiles.
 */
int
findall_proc_vfiles(
	kaddr_t paddr, 
	int mode, 
	int flags, 
	FILE *ofp, 
	k_ptr_t *Phashbase, 
	int (*test_vfile)(kaddr_t, kaddr_t, char *, int, 
		int, FILE *, list_of_ptrs_t *, int *))
{
	int first_time = TRUE, i, j = 0;
	int hashval, nfds, fdsize;
	k_ptr_t procp, fbuf, cbuf, sbuf, fdtbuf,prxy;
	kaddr_t proc, fdlist, caddr, base, fdt;
	list_of_ptrs_t *list;

	file_cnt = 0;
	if (DEBUG(DC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "add_proc_vsockets: "); 
		switch (mode) {

			case 1 : /* PID */
				fprintf(KL_ERRORFP, "proc=[PID=%lld], ",paddr); 
				break;

			case 2 : /* virtual address */
				fprintf(KL_ERRORFP, "proc=0x%llx, ", paddr); 
				break;

			default : /* should not happen! */
				fprintf(KL_ERRORFP, "proc=ERROR!, "); 
				break;
		}
		fprintf(KL_ERRORFP, "mode=%d, flags=0x%x\n", mode, flags);
	}

	kl_reset_error();

	procp = kl_get_proc(paddr, mode, flags);
	if (KL_ERROR) {
		return(-1);
	}

	if (mode == 1) {
		proc = kl_pid_to_proc((int)paddr);
	}
	else {
		proc = paddr;
	}

	/*
	 * Check for zombie process.
	 */
	if(!kl_kaddr(procp, "proc_proxy_s", "prxy_nthreads") ||
			   kl_kaddr(procp, "proc","p_stat") == S_ZOMB) {
		kl_free_block(procp);
		return(-1);
	}
					
	fdtbuf = kl_alloc_block(FDT_SIZE, K_TEMP);
	fdt = kl_kaddr(procp, "proc", "p_fdt");
	kl_get_struct(fdt, FDT_SIZE, fdtbuf, "fdt");
	if (!KL_ERROR) {
		fdlist = kl_kaddr(fdtbuf, "fdt", "fd_flist");
		nfds = kl_int(fdtbuf, "fdt", "fd_nofiles", 0);
	}
	if (KL_ERROR || !fdlist || (nfds == 0)) {
		kl_free_block(procp);
		kl_free_block(fdtbuf);
		return(-1);
	}

	fdsize = nfds * UFCHUNK_SIZE;
	cbuf = kl_alloc_block(fdsize, K_TEMP);
	kl_get_block(fdlist, fdsize, cbuf, "fdlist");
	if (KL_ERROR) {
		kl_free_block(cbuf);
		kl_free_block(procp);
		return(-1);
	}
	fbuf = kl_alloc_block(VFILE_SIZE,  K_TEMP);
	sbuf = kl_alloc_block(kl_struct_len("shaddr_s"), K_TEMP);

	for (i = 0; i < nfds; i++, j++) {
		if (PTRSZ64) {
			caddr = *(kaddr_t *)((unsigned)cbuf + (UFCHUNK_SIZE * i));
		} 
		else {
			caddr = *(__uint32_t *)((unsigned)cbuf + (UFCHUNK_SIZE * i));
		}

		list = (list_of_ptrs_t *)kl_alloc_block(sizeof(list_of_ptrs_t), K_TEMP);
		if (!test_vfile || 
				test_vfile(proc,caddr,fbuf,flags,j,ofp, list,&first_time)) {
			hashval=vfile_hash(list->val64);
			if(FINDLIST_QUEUE(&Phashbase[hashval], list, val64_compare)) {
				/* Duplicate!.
				 */ 
				kl_free_block(list);
			}
			else {
				ENQUEUE(&Phashbase[hashval], list);
				file_cnt++;
			}
		}
		else {
			kl_free_block(list);
		}
	}

	if ((!(base = kl_kaddr(procp, "proc", "p_shaddr"))) ||
		(kl_get_struct(base, kl_struct_len("shaddr_s"), sbuf, "shaddr_s"))) {
			kl_free_block(procp);
			kl_free_block(sbuf);
			kl_free_block(fbuf);
			kl_free_block(cbuf);
			kl_free_block(fdtbuf);
			return(file_cnt);
	}

	fdt = kl_kaddr(sbuf, "shaddr_s", "s_fdt");
	kl_get_struct(fdt, FDT_SIZE, fdtbuf, "fdt");
	if (!KL_ERROR) {
		fdlist = kl_kaddr(fdtbuf, "fdt", "fd_flist");
		nfds = kl_int(fdtbuf, "fdt", "fd_nofiles", 0);
	}

	prxy = P_PRXY(procp);

	if (!fdlist || (nfds == 0) || (ICRASH_ISSHDFD(prxy))) {

		/* We don't print the vfiles if they are shared...
		 * This happens in the case of SPROC's which ask to 
		 * share file descriptors. The correct way to find if a
		 * fd is shared is by using the macro...ISSHDFD..
		 * We could also have just compared the fdt pointers.
		 *
		 * -- No point printing duplicate entries.
		 * May be we should put out a printf?. mentioning this.
		 */
		kl_free_block(procp);
		kl_free_block(sbuf);
		kl_free_block(fbuf);
		kl_free_block(cbuf);
		kl_free_block(fdtbuf);
		return(file_cnt);
	}

	kl_free_block(cbuf);
	fdsize = nfds * UFCHUNK_SIZE;
	cbuf = kl_alloc_block(fdsize, K_TEMP);
	kl_get_block(fdlist, fdsize, cbuf, "fdlist");
	if (KL_ERROR) {
		return(-1);
	}

	for (i = 0; i < nfds; i++, j++) {
		if (PTRSZ64) {
			caddr = *(kaddr_t *)((unsigned)cbuf + (UFCHUNK_SIZE * i));
		} 
		else {
			caddr = *(__uint32_t *)((unsigned)cbuf + (UFCHUNK_SIZE * i));
		}


		list = kl_alloc_block(sizeof(list_of_ptrs_t),
				   K_TEMP);
		if (!test_vfile || test_vfile(proc,caddr,fbuf,flags,
						  j,ofp,list,&first_time)) {
			hashval=vfile_hash(list->val64);
			if(FINDLIST_QUEUE(&Phashbase[hashval], list,val64_compare)) {
				/* Duplicate!.
				 */ 
				kl_free_block(list);
			}
			else {
				ENQUEUE(&Phashbase[hashval], list); 
				file_cnt++;
			}
		}
		else {
			kl_free_block(list);
		}
	}
	kl_free_block(procp);
	kl_free_block(sbuf);
	kl_free_block(fbuf);
	kl_free_block(cbuf);
	kl_free_block(fdtbuf);
	return(file_cnt);
}
