#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/klib_error.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "klib.h"
#include "klib_extern.h"

error_t klib_error;             /* Global (declared extern) error structure  */ 

/*
 * kl_reset_error()
 */
void
kl_reset_error()
{
	KLIB_ERROR.e_flags = 0;
	KLIB_ERROR.e_code = 0;
	KLIB_ERROR.e_nval = 0;
	KLIB_ERROR.e_cval = 0;
	KLIB_ERROR.e_mode = 0;
}

/*
 * kl_set_error()
 */
void
kl_set_error(k_uint_t code, k_uint_t nval, char *cval, int mode)
{
	KL_ERROR = code;

	if (cval) {
		KLIB_ERROR.e_cval = cval;
		KLIB_ERROR.e_flags |= EF_CVAL_VALID;
	}
	else if (mode != -1) {
		KLIB_ERROR.e_mode = mode;
		KLIB_ERROR.e_nval = nval;
		KLIB_ERROR.e_flags |= EF_NVAL_VALID;
	}
}


/*
 * kl_print_debug()
 */
void
kl_print_debug(klib_t *klp, char* m)
{
	fprintf(KL_ERRORFP, "%s: ", m);
	kl_print_error(klp);
}

/*
 * kl_print_error()
 */
void
kl_print_error(klib_t *klp)
{
	k_uint_t base_error, struct_error;

	/* Now print out the value. If the value is in string from, just
	 * print out the contents of the string. If the value is in 
	 * numeric form, then we have to use mode to determine what type
	 * of value to print (decimal or hex pirmarily). If neither
	 * flag is set, then don't print any value.
	 */
	if (KLIB_ERROR.e_flags & EF_CVAL_VALID) {
		fprintf(KL_ERRORFP, "%s: ", KLIB_ERROR.e_cval);
	}
	else if (KLIB_ERROR.e_flags & EF_NVAL_VALID) {

		switch (KLIB_ERROR.e_mode) {
			case 0:
			case 1:
			case 3:
				fprintf(KL_ERRORFP, "%lld: ", KLIB_ERROR.e_nval);
				break;

			case 2:
				fprintf(KL_ERRORFP, "0x%llx: ", KLIB_ERROR.e_nval);
				break;
		}
	}

	/* Make sure the error code is in the libklib range (0 through
	 * KLIB_ERROR_MAX). If it isn't, check to see if there is a 
	 * print_error callback function registered. If there is, then 
	 * call that routine to print the first part of the error message.
	 */
	base_error = (KL_ERROR & 0xffffffff);
	if (base_error > KLIB_ERROR_MAX) {
		if (K_PRINT_ERROR(klp)) {
			K_PRINT_ERROR(klp)();
		}
		else {
			fprintf(KL_ERRORFP, "UNKNOWN ERROR CODE: %lld", base_error); 
		}
	}
	else if (base_error) {

		switch (base_error) {

			case KLE_INVALID_VALUE:
				fprintf(KL_ERRORFP, "invalid input value");
				break;

			case KLE_INVALID_VADDR:
				fprintf(KL_ERRORFP, "invalid kernel virtual address");
				break;

			case KLE_INVALID_VADDR_ALIGN:
				fprintf(KL_ERRORFP, "invalid virtual address alignment");
				break;

			case KLE_INVALID_K2_ADDR:
				fprintf(KL_ERRORFP, "K2 address not mapped");
				break;

			case KLE_INVALID_KERNELSTACK:
				fprintf(KL_ERRORFP, "could not map kernelstack");
				break;

			case KLE_INVALID_LSEEK:
				fprintf(KL_ERRORFP, "bad lseek to physical address");
				break;

			case KLE_INVALID_READ:
				if (ACTIVE(klp)) {
					fprintf(KL_ERRORFP, "bad memory read");
				}
				else {
					fprintf(KL_ERRORFP, "not found in vmcore image");
				}
				break;

			case KLE_INVALID_CMPREAD:
				fprintf(KL_ERRORFP, "not found in compressed vmcore image");
				break;

			case KLE_INVALID_STRUCT_SIZE:
				fprintf(KL_ERRORFP, "bad struct size");
				break;

			case KLE_BEFORE_RAM_OFFSET:
				fprintf(KL_ERRORFP, "physical address proceeds start of RAM "
					"(0x%llx)", K_RAM_OFFSET(klp)); 
				break;

			case KLE_AFTER_MAXPFN:
				fprintf(KL_ERRORFP, "exceeds maximum possible PFN (%lld)", 
					K_MAXPFN(klp));
				break;

			case KLE_PHYSMEM_NOT_INSTALLED:
				fprintf(KL_ERRORFP, "physical memory not installed\n");
				break;

			case KLE_AFTER_PHYSMEM:
				fprintf(KL_ERRORFP, "exceeds physical memory (0x%llx)", 
					(k_uint_t)(K_PHYSMEM(klp) * K_PAGESZ(klp)));
				break;

			case KLE_AFTER_MAXMEM:
				fprintf(KL_ERRORFP, "exceeds maximum physical address "
					"(0x%llx)", K_MAXPHYS(klp));
				break;

			case KLE_IS_UPROC:
				if (KLIB_ERROR.e_flags & EF_CVAL_VALID) {
					fprintf(KL_ERRORFP, "references a process");
				}
				else if (KLIB_ERROR.e_mode == 2) {
					fprintf(KL_ERRORFP, "is a proc pointer");
				}
				else {
					fprintf(KL_ERRORFP, "references a process");
				}
				break;

#ifdef ANON_ITHREADS
			case KLE_IS_ITHREAD:
				if (KLIB_ERROR.e_flags & EF_CVAL_VALID) {
					fprintf(KL_ERRORFP, "references an ithread");
				}
				else if (KLIB_ERROR.e_mode == 2) {
					fprintf(KL_ERRORFP, "is an ithread_s pointer");
				}
				else {
					fprintf(KL_ERRORFP, "references an ithread");
				}
				break;
#endif

			case KLE_IS_STHREAD:
				if (KLIB_ERROR.e_flags & EF_CVAL_VALID) {
					fprintf(KL_ERRORFP, "references an sthread");
				}
				else if (KLIB_ERROR.e_mode == 2) {
					fprintf(KL_ERRORFP, "is an sthread_s pointer");
				}
				else {
					fprintf(KL_ERRORFP, "references an sthread");
				}
				break;

			case KLE_WRONG_KTHREAD_TYPE:
				fprintf(KL_ERRORFP, "wrong type of kthread");
				break;

			case KLE_NO_DEFKTHREAD:
				fprintf(KL_ERRORFP, "default kthread not set");
				break;

			case KLE_PID_NOT_FOUND:
				fprintf(KL_ERRORFP, "PID not found");
				break;

			case KLE_INVALID_DUMPPROC: 
				fprintf(KL_ERRORFP, "0x%llx: invalid dumpproc", 
					K_DUMPPROC(klp));
				break;

			case KLE_NULL_BUFF:
				fprintf(KL_ERRORFP, "no buffer pointer");
				break;

			case KLE_ACTIVE:
				fprintf(KL_ERRORFP, "operation not supported on a live "
					"system");
				break;

			case KLE_DEFKTHREAD_NOT_ON_CPU:
				fprintf(KL_ERRORFP, "process/kthread (0x%llx) not running "
					"on a cpu\n", K_DEFKTHREAD(klp));
				break;

			case KLE_NO_CURCPU:
				fprintf(KL_ERRORFP, "current cpu could not be determined");
				break;

			case KLE_NO_CPU:
				fprintf(KL_ERRORFP, "CPU not installed");
				break;

			case KLE_NOT_IMPLEMENTED:
				fprintf(KL_ERRORFP, "operation not implemented yet");
				break;

			case KLE_UNKNOWN_ERROR:
			default:
				fprintf(KL_ERRORFP, "unknown error (%lld)", KL_ERROR);
				break;
		}
	}

	if (struct_error = ((KL_ERROR >> 32) << 32)) {
		if (base_error) {
			fprintf(KL_ERRORFP, " (");
		}
		else {
			fprintf(KL_ERRORFP, "invalid ");
		}

		switch (struct_error) {

			case KLE_BAD_AVLNODE:
				fprintf(KL_ERRORFP, "avlnode pointer");
				break;

			case KLE_BAD_BLOCK_S:
				fprintf(KL_ERRORFP, "block_s pointer");
				break;

			case KLE_BAD_BUCKET_S:
				fprintf(KL_ERRORFP, "bucket_s pointer");
				break;

			case KLE_BAD_EFRAME_S:
				fprintf(KL_ERRORFP, "eframe_s pointer");
				break;

			case KLE_BAD_VFILE:
				fprintf(KL_ERRORFP, "vfile pointer");
				break;

			case KLE_BAD_GRAPH_VERTEX_S:
				fprintf(KL_ERRORFP, "graph_vertex_s pointer");
				break;

			case KLE_BAD_GRAPH_EDGE_S:
				fprintf(KL_ERRORFP, "graph_edge_s pointer");
				break;

			case KLE_BAD_GRAPH_INFO_S:
				fprintf(KL_ERRORFP, "graph_info_s pointer");
				break;

			case KLE_BAD_INODE: 	    
				fprintf(KL_ERRORFP, "inode pointer");
				break;

			case KLE_BAD_INPCB: 	    
				fprintf(KL_ERRORFP, "inpcb pointer");
				break;

			case KLE_BAD_KTHREAD:
				fprintf(KL_ERRORFP, "kthread pointer");
				break;

#ifdef ANON_ITHREADS
			case KLE_BAD_ITHREAD_S:
				fprintf(KL_ERRORFP, "ithread pointer");
				break;
#endif

			case KLE_BAD_STHREAD_S:
				fprintf(KL_ERRORFP, "sthread pointer");
				break;

			case KLE_BAD_MBSTAT:
				fprintf(KL_ERRORFP, "mbstat pointer");
				break;

			case KLE_BAD_MBUF:
				fprintf(KL_ERRORFP, "mbuf pointer");
				break;

			case KLE_BAD_NODEPDA:
				fprintf(KL_ERRORFP, "nodepda_s pointer");
				break;

			case KLE_BAD_PDA:
				fprintf(KL_ERRORFP, "pda_s pointer");
				break;

			case KLE_BAD_PDE:
				fprintf(KL_ERRORFP, "pde pointer");
				break;

			case KLE_BAD_PID_ENTRY:
				if (KLIB_ERROR.e_cval) {
					fprintf(KL_ERRORFP, "pid entry");
				}
				else if (KLIB_ERROR.e_mode == 2) {
					fprintf(KL_ERRORFP, "pid_entry pointer");
				}
				else if (KLIB_ERROR.e_mode == 1) {
					fprintf(KL_ERRORFP, "process PID");
				}
				else {
					fprintf(KL_ERRORFP, "pidtab slot number");
				}
				break;

			case KLE_BAD_PFDAT:
				fprintf(KL_ERRORFP, "pfdat pointer");
				break;

			case KLE_BAD_PFDATHASH:
				fprintf(KL_ERRORFP, "pfdat hash entry");
				break;

			case KLE_BAD_PREGION:
				fprintf(KL_ERRORFP, "pregion pointer");
				break;

			case KLE_BAD_PROC:
				if (KLIB_ERROR.e_cval) {
					fprintf(KL_ERRORFP, "process entry");
				}
				else if (KLIB_ERROR.e_mode == 2) {
					fprintf(KL_ERRORFP, "proc pointer");
				}
				else if (KLIB_ERROR.e_mode == 1) {
					fprintf(KL_ERRORFP, "process PID");
				}
				else {
					fprintf(KL_ERRORFP, "proc slot number");
				}
				break;

			case KLE_BAD_QUEUE: 
				fprintf(KL_ERRORFP, "queue pointer");
				break;

			case KLE_BAD_REGION:
				fprintf(KL_ERRORFP, "region pointer");
				break;

			case KLE_BAD_RNODE:
				fprintf(KL_ERRORFP, "rnode pointer");
				break;

			case KLE_BAD_SEMA_S:
				fprintf(KL_ERRORFP, "sema_s pointer");
				break;

			case KLE_BAD_LSNODE:
				fprintf(KL_ERRORFP, "lsnode pointer");
				break;

			case KLE_BAD_CSNODE:
				fprintf(KL_ERRORFP, "csnode pointer");
				break;

			case KLE_BAD_SOCKET:
				fprintf(KL_ERRORFP, "socket pointer");
				break;

			case KLE_BAD_STDATA:
				fprintf(KL_ERRORFP, "stdata pointer");
				break;

			case KLE_BAD_STRSTAT:
				fprintf(KL_ERRORFP, "strstat pointer");
				break;

			case KLE_BAD_SWAPINFO:
				fprintf(KL_ERRORFP, "swapinfo pointer");
				break;

			case KLE_BAD_TCPCB: 
				fprintf(KL_ERRORFP, "tcpcb pointer");
				break;

			case KLE_BAD_UNPCB: 
				fprintf(KL_ERRORFP, "unpcb pointer");
				break;

			case KLE_BAD_UTHREAD_S:
				fprintf(KL_ERRORFP, "uthread pointer");
				break;

			case KLE_BAD_VFS:
				fprintf(KL_ERRORFP, "vfs pointer");
				break;

			case KLE_BAD_VFSSW:
				if (KLIB_ERROR.e_mode == 2) {
					fprintf(KL_ERRORFP, "vfssw pointer");
				}
				else {
					fprintf(KL_ERRORFP, "fs entry");
				}
				break;

			case KLE_BAD_VNODE:
				fprintf(KL_ERRORFP, "vnode pointer");
				break;

			case KLE_BAD_VPROC:
				fprintf(KL_ERRORFP, "vproc pointer");
				break;

			case KLE_BAD_VSOCKET:
				fprintf(KL_ERRORFP, "vsocket pointer");
				break;

			case KLE_BAD_XTHREAD_S:
				fprintf(KL_ERRORFP, "xthread pointer");
				break;

			case KLE_BAD_ZONE:
				fprintf(KL_ERRORFP, "zone pointer");
				break;

			case KLE_BAD_DIE:
				fprintf(KL_ERRORFP, "Dwarf DIE");
				break;

			case KLE_BAD_BASE_VALUE:
				fprintf(KL_ERRORFP, "bad bit field");
				break;

			case KLE_BAD_CPUID:
				if (KLIB_ERROR.e_cval) {
					fprintf(KL_ERRORFP, "cpu entry");
				}
				else if (KLIB_ERROR.e_mode == 2) {
					fprintf(KL_ERRORFP, "pda_s pointer");
				}
				else {
					fprintf(KL_ERRORFP, "CPUID");
				}
				break;

			case KLE_BAD_STREAM:
				fprintf(KL_ERRORFP, "stdata pointer");
				break;

			case KLE_BAD_LINENO:
				fprintf(KL_ERRORFP, "number of source lines");
				break;

			case KLE_BAD_SYMNAME:
				fprintf(KL_ERRORFP, "symbol name");
				break;

			case KLE_BAD_SYMADDR:
				fprintf(KL_ERRORFP, "symbol address");
				break;

			case KLE_BAD_FUNCADDR:
				fprintf(KL_ERRORFP, "function address");
				break;

			case KLE_BAD_STRUCT:
				fprintf(KL_ERRORFP, "struct");
				break;

			case KLE_BAD_FIELD:
				fprintf(KL_ERRORFP, "field");
				break;

			case KLE_BAD_PC: 
				fprintf(KL_ERRORFP, "program counter");
				break;

			case KLE_BAD_SP: 
				fprintf(KL_ERRORFP, "stack pointer");
				break;

			case KLE_BAD_EP: 
				fprintf(KL_ERRORFP, "eframe_s pointer");
				break;

			case KLE_BAD_SADDR:
				fprintf(KL_ERRORFP, "stack address");
				break;

			case KLE_BAD_KERNELSTACK:
				fprintf(KL_ERRORFP, "kthread stack address");
				break;

			case KLE_BAD_DEBUG:
				fprintf(KL_ERRORFP, "debug value");
				break;

			case KLE_DEFUNCT_PROCESS:
				if (KL_ERROR & 0xffffffff) {
					fprintf(KL_ERRORFP, "defunct process");
				}
				else {
					fprintf(KL_ERRORFP, "process (defunct)");
				}
				break;

			case KLE_BAD_VERTEX_HNDL:
				fprintf(KL_ERRORFP, "vertex handle");
				break;

			case KLE_BAD_VERTEX_EDGE:
				if (KLIB_ERROR.e_mode == 2) {
					fprintf(KL_ERRORFP, "graph_edge_s pointer");
				}
				else {
					fprintf(KL_ERRORFP, "vertex edge entry");
				}
				break;

			case KLE_BAD_VERTEX_INFO:
				if (KLIB_ERROR.e_mode == 2) {
					fprintf(KL_ERRORFP, "graph_info_s pointer");
				}
				else {
					fprintf(KL_ERRORFP, "vertex info entry");
				}
				break;

			case KLE_BAD_DEFKTHREAD:
				fprintf(KL_ERRORFP, "defkthread");
				break;

			default:
				fprintf(KL_ERRORFP, "Unknown");
				break;
		}
	}

	if (struct_error && base_error) {
		fprintf(KL_ERRORFP, ")");
	}
	fprintf(KL_ERRORFP, "\n");
}

/*
 * Deliver warning only. If errstr is non-null, write errors to stdout, 
 * else quietly return. It might make more sense to use stderr, but the 
 * pcs may have redirected that to /dev/null so we use stdout (left 
 * available by the dbxpcs) instead.  fflush() to get the data out asap.
*/
void 
_kmem_warn(const char* errstr, char* errfmt,...)
{
    va_list args;

    va_start(args,errfmt);

    if (errstr != NULL) { 
	fprintf(stdout,"%s: ", errstr);
	vfprintf(stdout, errfmt, args);
	fprintf(stdout,"\n");
	fflush(stdout);
    }
    va_end(args);
}

/* 
 * For fatal errors, printing warning message if errstr is non-null, and
 * exit program.
 */
void 
_kmem_fatal(char* errstr, char* errmsg)
{
    _kmem_warn(errstr, errmsg);
    exit(2);
}

#ifdef DEBUG
void 
_kmem_debug(char *s,...)
{
    va_list args;

    va_start(args,s);
    vfprintf(stdout, s, args);
    fflush(stdout);
    va_end(args);
}

#endif	/* DEBUG */


