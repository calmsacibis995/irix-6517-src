#ident "$Header: "

#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <klib/klib.h>

error_t klib_error;     /* Global error structure  */ 
k_uint_t klib_debug;	/* Global debug value */

/*
 * kl_setup_error()
 */
int
kl_setup_error(FILE *errorfp, char *program, kl_print_error_func print_error) 
{

	KL_ERRORFP = errorfp;
	if (KL_ERRORFP == NULL) {
		return(1);
	}

	/* Copy the name of the program into klib_error. This will allow
	 * other modules to reference the program name when printing error
	 * messages.
	 */
	if (program) {
		KL_PROGRAM = (char *)malloc(strlen(program) + 1);
		if (KL_PROGRAM == NULL) {
			fprintf(KL_ERRORFP, "out of memory mallocing %ld bytes for name\n",
				(long)strlen(program) + 1);
			return(1);
		}
		strcpy(KL_PROGRAM, program);
	}
	if (print_error) {
		K_PRINT_ERROR() = print_error;
	}
	return(0);
}
 
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
kl_print_debug(char* m)
{
	fprintf(KL_ERRORFP, "%s: ", m);
	kl_print_error();
}

/*
 * kl_print_error()
 */
void
kl_print_error()
{
	k_uint_t base_error, type_error;

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
	if (base_error & ((base_error & KLEC_CLASS_MASK) == KLEC_APP)) {
		if (K_PRINT_ERROR()) {
			K_PRINT_ERROR()();
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
				fprintf(KL_ERRORFP, "not found in memory image");
				break;

			case KLE_INVALID_CMPREAD:
				fprintf(KL_ERRORFP, "not found in compressed vmcore image");
				break;

			case KLE_INVALID_STRUCT_SIZE:
				fprintf(KL_ERRORFP, "bad struct size");
				break;

			case KLE_BEFORE_RAM_OFFSET:
				fprintf(KL_ERRORFP, "physical address proceeds start of RAM");
				break;

			case KLE_AFTER_MAXPFN:
				fprintf(KL_ERRORFP, "exceeds maximum possible PFN"); 
				break;

			case KLE_PHYSMEM_NOT_INSTALLED:
				fprintf(KL_ERRORFP, "physical memory not installed\n");
				break;

			case KLE_AFTER_PHYSMEM:
				fprintf(KL_ERRORFP, "exceeds physical memory");
				break;

			case KLE_AFTER_MAXMEM:
				fprintf(KL_ERRORFP, "exceeds maximum physical address");
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
				fprintf(KL_ERRORFP, "dumpproc is invalid"); 
				break;

			case KLE_NULL_BUFF:
				fprintf(KL_ERRORFP, "no buffer pointer");
				break;

			case KLE_ACTIVE:
				fprintf(KL_ERRORFP, "operation not supported on a live "
					"system");
				break;

			case KLE_DEFKTHREAD_NOT_ON_CPU:
				fprintf(KL_ERRORFP, "default kthread not running on a cpu");
				break;

			case KLE_NO_CURCPU:
				fprintf(KL_ERRORFP, "current cpu could not be determined");
				break;

			case KLE_NO_CPU:
				fprintf(KL_ERRORFP, "CPU not installed");
				break;

			case KLE_SHORT_DUMP:
				fprintf(KL_ERRORFP, "compressed vmcore image is short");
				break;

			case KLE_NO_KLIB_STRUCT:
				fprintf(KL_ERRORFP, "KLIB control structure not initialized");
				break;

			case KLE_NO_FUNCTION:
				fprintf(KL_ERRORFP, "function not found in symbol table");
				break;

			case KLE_NO_MEMORY:
				fprintf(KL_ERRORFP, "not enough memory to allocate block");
				break;

			case KLE_ZERO_BLOCK:
				fprintf(KL_ERRORFP, "tried to allocate a zero-sized block");
				break;

			case KLE_BAD_NIC_DATA:
				fprintf(KL_ERRORFP, "invalid data in NIC string");
				break;

			case KLE_NO_RA:
				fprintf(KL_ERRORFP, "ra not saved in function\n");
				break;

			case KLE_NO_TRACE:
				fprintf(KL_ERRORFP, "no valid traces");
				break;

			case KLE_NO_EFRAME:
				fprintf(KL_ERRORFP, "no eframe found");
				break;

			case KLE_NO_KTHREAD:
				fprintf(KL_ERRORFP, "no kthread pointer");
				break;

			case KLE_NO_PDA:
				fprintf(KL_ERRORFP, "no pda_s pointer");
				break;

			case KLE_STACK_NOT_FOUND:
				fprintf(KL_ERRORFP, "no stack found");
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

	if (type_error = ((KL_ERROR >> 32) << 32)) {
		if (base_error) {
			fprintf(KL_ERRORFP, " (");
		}
		else {
			fprintf(KL_ERRORFP, "invalid ");
		}

		switch (type_error) {

			case KLE_BAD_AVLNODE:
				fprintf(KL_ERRORFP, "avlnode pointer");
				break;

            case KLE_BAD_BHV_DESC:
                fprintf(KL_ERRORFP, "bhv_desc pointer");
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

			case KLE_BAD_STHREAD_S:
				fprintf(KL_ERRORFP, "sthread pointer");
				break;

			case KLE_BAD_MBSTAT:
				fprintf(KL_ERRORFP, "mbstat pointer");
				break;

			case KLE_BAD_MNTINFO:
				fprintf(KL_ERRORFP, "mntinfo pointer");
				break;

			case KLE_BAD_MRLOCK_S:
				fprintf(KL_ERRORFP, "mrlock_s pointer");
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

			case KLE_BAD_PFDAT:
				fprintf(KL_ERRORFP, "pfdat pointer");
				break;

			case KLE_BAD_PFDATHASH:
				fprintf(KL_ERRORFP, "pfdat hash entry");
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

			case KLE_BAD_PID_SLOT:
				fprintf(KL_ERRORFP, "pidtab slot number");
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

			case KLE_BAD_VFILE:
				fprintf(KL_ERRORFP, "vfile pointer");
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

			case KLE_BAD_XFS_INODE:
				fprintf(KL_ERRORFP, "xfs_inode pointer");
				break;

			case KLE_BAD_XFS_DINODE_CORE:
				fprintf(KL_ERRORFP, "xfs_dinode_core pointer");
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

			case KLE_BAD_ANON: /* XXX - out of order */
				fprintf(KL_ERRORFP, "anon pointer");
				break;

			case KLE_PAGE_HEADER: 
				fprintf(KL_ERRORFP, "page header");
				break;

			case KLE_BLOCK_HEADER: 
				fprintf(KL_ERRORFP, "block header");
				break;

			case KLE_BAD_ML_INFO: /* XXX - out of order */
				fprintf(KL_ERRORFP, "ml_info pointer");
				break;

			case KLE_BAD_CSNODE: /* XXX - out of order */
				fprintf(KL_ERRORFP, "csnode pointer");
				break;

			default:
				fprintf(KL_ERRORFP, "Unknown");
				break;
		}
	}

	if (type_error && base_error) {
		fprintf(KL_ERRORFP, ")");
	}
	fprintf(KL_ERRORFP, "\n");
}

