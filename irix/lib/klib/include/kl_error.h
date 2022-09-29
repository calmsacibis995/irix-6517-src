#ident "$Header: "

/**
 **	This header file contains basic definitions and declarations
 **	for the KLIB error handling facility.
 **
 **/

/* Error Classes
 */
#define KLEC_APP        0
#define KLEC_KLIB		1	
#define KLEC_ALLOC		2
#define KLEC_TRACE		3
#define KLEC_CONFIG		4
#define KLEC_MEM		5
#define KLEC_SYM		6
#define KLEC_KERN		7
#define KLEC_UTIL		8

#define KLEC_CLASS_MASK 0x00000000ff000000
#define KLEC_CLASS_SHIFT 24
#define KLEC_TYPE_MASK  0xffffffff00000000
#define KLEC_TYPE_SHIFT 32
#define KLEC_CLASS(e) ((e & KLEC_CLASS_MASK) >> KLEC_CLASS_SHIFT)
#define KLEC_TYPE(e) ((e & KLEC_TYPE_MASK) >> KLEC_TYPE_SHIFT)

typedef void (*kl_print_error_func)();

/* Error structure 
 */
typedef struct error_s {
	FILE       *e_errorfp;  /* Default file deescriptor for error output     */
	char	   *e_program;  /* Name of program                               */
	int         e_flags;    /* Everybody needs some flags (see below)        */
	k_uint_t    e_code;     /* Error code (see below)                        */
	k_uint_t    e_nval;     /* Numeric value that caused error               */
	char       *e_cval;     /* String representation of error value          */
	int         e_mode;     /* Mode indicating how represent numeric value   */
	kl_print_error_func print_error;   	/* print application error msg.      */
} error_t;

extern error_t klib_error;

/** 
 ** Some macros for accessing data in klib_error 
 **/
#define KLIB_ERROR		klib_error
#define KL_ERROR 		(KLIB_ERROR.e_code)
#define KL_ERRORFP 		(KLIB_ERROR.e_errorfp)
#define KL_PROGRAM 		(KLIB_ERROR.e_program)
#define K_PRINT_ERROR() (KLIB_ERROR.print_error)

#define KL_RETURN_ERROR return(KL_ERROR)

#define KL_CHECK_ERROR(code, nval, cval, mode) \
	if (KL_ERROR) { \
		if (code) { \
			kl_set_error(KL_ERROR|(code>>32), nval, cval, mode); \
		} \
		kl_print_error(); \
		return(KL_ERROR); \
	}

#define KL_CHECK_ERROR_RETURN\
	if (KL_ERROR) {\
		KL_RETURN_ERROR;\
	}

#define KL_SET_ERROR(code) \
	kl_set_error(code, 0, (char*)0, -1)

#define KL_SET_ERROR_NVAL(code, nval, mode) \
	kl_set_error(code, nval, (char*)0, mode)

#define KL_SET_ERROR_CVAL(code, cval) \
	kl_set_error(code, 0, cval, -1)

#define KLE_KTHREAD_TYPE(ktp) \
	(IS_UTHREAD(ktp) ? KLE_IS_UPROC : \
	(IS_XTHREAD(ktp) ? KLE_IS_XTHREAD : \
	(IS_STHREAD(ktp) ? KLE_IS_STHREAD : KLE_UNKNOWN_ERROR)))

/* Flags
 */
#define EF_NVAL_VALID           0x001
#define EF_CVAL_VALID           0x002

/* Error codes
 *
 * There are basically two types of error codes -- with each type
 * residing in a single word in a two word error code value. The lower
 * 32-bits contains an error class and code that represents exactly 
 * WHAT error occurred (e.g., non-numeric text in a numeric value 
 * entered by a user, bad virtual address, etc.). 
 * 
 * The upper 32-bits represents what type of data was being referenced 
 * when the error occurred (e.g., bad proc struct). Having two tiers of 
 * error codes makes it easier to generate useful and specific error 
 * messages. Note that is possible to have situations where one or the 
 * other type of error codes is not set. This is OK as long as at least 
 * one type s set.
 */

/** libklib error codes
 **/
#define KLE_KLIB (KLEC_KLIB << KLEC_CLASS_SHIFT)
#define KLE_NO_KLIB_STRUCT 			(KLE_KLIB|1) 
#define KLE_NO_FUNCTION 			(KLE_KLIB|2) 
#define KLE_NOT_INITIALIZED			(KLE_KLIB|3) 
#define KLE_NOT_IMPLEMENTED 		(KLE_KLIB|4) 
#define KLE_NOT_SUPPORTED 			(KLE_KLIB|5) 
#define KLE_AGAIN         			(KLE_KLIB|6) 
#define KLE_UNKNOWN_ERROR			(KLE_KLIB|99) 

/** liballoc error codes
 **/
#define KLE_ALLOC (KLEC_ALLOC << KLEC_CLASS_SHIFT)
#define KLE_ZERO_BLOCK 				(KLE_ALLOC|1) 
#define KLE_NO_MEMORY 				(KLE_ALLOC|2) 

/** libtrace error codes
 **/
#define KLE_TRACE (KLEC_TRACE << KLEC_CLASS_SHIFT)
#define KLE_NO_RA 					(KLE_TRACE|1) 
#define KLE_NO_TRACE 				(KLE_TRACE|2) 
#define KLE_NO_EFRAME 				(KLE_TRACE|3) 
#define KLE_NO_KTHREAD 				(KLE_TRACE|4) 
#define KLE_NO_PDA 					(KLE_TRACE|5) 
#define KLE_STACK_NOT_FOUND 		(KLE_TRACE|6) 

/** libconfig error codes
 **/
#define KLE_CONFIG (KLEC_CONFIG << KLEC_CLASS_SHIFT)

/** libmem error codes
 **/
#define KLE_MEM (KLEC_MEM << KLEC_CLASS_SHIFT)
#define KLE_INVALID_LSEEK 			(KLE_MEM|1) 
#define KLE_INVALID_READ 			(KLE_MEM|2) 
#define KLE_INVALID_CMPREAD 		(KLE_MEM|3) 
#define KLE_SHORT_DUMP 				(KLE_MEM|4) 

/** libsym error codes
 **/
#define KLE_SYM (KLEC_SYM << KLEC_CLASS_SHIFT)

/** libkern error codes
 **/
#define KLE_KERN (KLEC_KERN << KLEC_CLASS_SHIFT)
#define KLE_INVALID_VALUE 			(KLE_KERN|1)  
#define KLE_INVALID_VADDR 			(KLE_KERN|2)  
#define KLE_INVALID_VADDR_ALIGN 	(KLE_KERN|3)  
#define KLE_INVALID_K2_ADDR 		(KLE_KERN|4)  
#define KLE_INVALID_KERNELSTACK 	(KLE_KERN|5)  
#define KLE_INVALID_STRUCT_SIZE 	(KLE_KERN|6)  
#define KLE_BEFORE_RAM_OFFSET	 	(KLE_KERN|8)  
#define KLE_AFTER_MAXPFN 			(KLE_KERN|9)  
#define KLE_AFTER_PHYSMEM  			(KLE_KERN|10)  
#define KLE_AFTER_MAXMEM 			(KLE_KERN|11)  
#define KLE_PHYSMEM_NOT_INSTALLED 	(KLE_KERN|12)  
#define KLE_IS_UPROC 				(KLE_KERN|13)  
#define KLE_IS_STHREAD 				(KLE_KERN|14)  
#define KLE_IS_XTHREAD 				(KLE_KERN|15)  
#define KLE_WRONG_KTHREAD_TYPE 		(KLE_KERN|16)  
#define KLE_NO_DEFKTHREAD 			(KLE_KERN|17)  
#define KLE_PID_NOT_FOUND 			(KLE_KERN|18)  
#define KLE_INVALID_DUMPPROC 		(KLE_KERN|19)  
#define KLE_NULL_BUFF 				(KLE_KERN|20)  
#define KLE_ACTIVE 					(KLE_KERN|21)  
#define KLE_DEFKTHREAD_NOT_ON_CPU 	(KLE_KERN|22)  
#define KLE_NO_CURCPU 				(KLE_KERN|23)  
#define KLE_NO_CPU 					(KLE_KERN|24)  
#define KLE_BAD_NIC_DATA 			(KLE_KERN|25)  
#define KLE_SIG_ERROR 				(KLE_KERN|26)  

/** libutil error codes
 **/
#define KLE_UTIL (KLEC_UTIL << KLEC_CLASS_SHIFT)

/* Error codes that indicate what type of structure was being allocated 
 * when an error occurred.
 */
#define KLE_BAD_AVLNODE	    	(((k_uint_t)1)<<32)
#define KLE_BAD_BHV_DESC        (((k_uint_t)2)<<32)
#define KLE_BAD_BLOCK_S  	    (((k_uint_t)3)<<32)
#define KLE_BAD_BUCKET_S      	(((k_uint_t)4)<<32)
#define KLE_BAD_EFRAME_S	    (((k_uint_t)5)<<32) 
#define KLE_BAD_GRAPH_VERTEX_S  (((k_uint_t)6)<<32)
#define KLE_BAD_GRAPH_EDGE_S  	(((k_uint_t)7)<<32)
#define KLE_BAD_GRAPH_INFO_S  	(((k_uint_t)8)<<32)
#define KLE_BAD_INODE  	    	(((k_uint_t)9)<<32)
#define KLE_BAD_INPCB  	    	(((k_uint_t)10)<<32)
#define KLE_BAD_KTHREAD       	(((k_uint_t)11)<<32)
#define KLE_BAD_STHREAD_S     	(((k_uint_t)13)<<32)
#define KLE_BAD_MBSTAT        	(((k_uint_t)14)<<32)
#define KLE_BAD_MNTINFO        	(((k_uint_t)15)<<32)
#define KLE_BAD_MRLOCK_S       	(((k_uint_t)16)<<32)
#define KLE_BAD_MBUF            (((k_uint_t)17)<<32)
#define KLE_BAD_NODEPDA         (((k_uint_t)18)<<32)
#define KLE_BAD_PDA             (((k_uint_t)19)<<32)
#define KLE_BAD_PDE             (((k_uint_t)20)<<32)
#define KLE_BAD_PFDAT           (((k_uint_t)21)<<32)
#define KLE_BAD_PFDATHASH       (((k_uint_t)22)<<32)
#define KLE_BAD_PID_ENTRY       (((k_uint_t)23)<<32)
#define KLE_BAD_PID_SLOT        (((k_uint_t)24)<<32)
#define KLE_BAD_PREGION         (((k_uint_t)25)<<32)
#define KLE_BAD_PROC            (((k_uint_t)26)<<32)
#define KLE_BAD_QUEUE           (((k_uint_t)27)<<32)
#define KLE_BAD_REGION          (((k_uint_t)28)<<32)
#define KLE_BAD_RNODE           (((k_uint_t)29)<<32)
#define KLE_BAD_SEMA_S          (((k_uint_t)30)<<32)
#define KLE_BAD_LSNODE          (((k_uint_t)31)<<32)
#define KLE_BAD_SOCKET          (((k_uint_t)32)<<32)
#define KLE_BAD_STDATA          (((k_uint_t)33)<<32)
#define KLE_BAD_STRSTAT         (((k_uint_t)34)<<32)
#define KLE_BAD_SWAPINFO        (((k_uint_t)35)<<32)
#define KLE_BAD_TCPCB           (((k_uint_t)36)<<32)
#define KLE_BAD_UNPCB           (((k_uint_t)37)<<32)
#define KLE_BAD_UTHREAD_S       (((k_uint_t)38)<<32)
#define KLE_BAD_VFILE           (((k_uint_t)39)<<32)
#define KLE_BAD_VFS             (((k_uint_t)40)<<32)
#define KLE_BAD_VFSSW           (((k_uint_t)41)<<32)
#define KLE_BAD_VNODE           (((k_uint_t)42)<<32)
#define KLE_BAD_VPROC           (((k_uint_t)43)<<32)
#define KLE_BAD_VSOCKET         (((k_uint_t)44)<<32)
#define KLE_BAD_XFS_INODE       (((k_uint_t)45)<<32)
#define KLE_BAD_XFS_DINODE_CORE (((k_uint_t)46)<<32)
#define KLE_BAD_XTHREAD_S       (((k_uint_t)47)<<32)
#define KLE_BAD_ZONE            (((k_uint_t)48)<<32)
#define KLE_BAD_DIE             (((k_uint_t)49)<<32) /* Bad Dwarf DIE info */
#define KLE_BAD_BASE_VALUE      (((k_uint_t)50)<<32) /* Bad bit offset get */
#define KLE_BAD_CPUID           (((k_uint_t)51)<<32) /* CPUID or pda_s ptr */
#define KLE_BAD_STREAM          (((k_uint_t)52)<<32) /* Not in streamtab   */
#define KLE_BAD_LINENO          (((k_uint_t)53)<<32)
#define KLE_BAD_SYMNAME         (((k_uint_t)54)<<32)
#define KLE_BAD_SYMADDR         (((k_uint_t)55)<<32)
#define KLE_BAD_FUNCADDR        (((k_uint_t)56)<<32)
#define KLE_BAD_STRUCT          (((k_uint_t)57)<<32)
#define KLE_BAD_FIELD           (((k_uint_t)58)<<32)
#define KLE_BAD_PC              (((k_uint_t)59)<<32)
#define KLE_BAD_SP              (((k_uint_t)60)<<32)
#define KLE_BAD_EP              (((k_uint_t)61)<<32)
#define KLE_BAD_SADDR           (((k_uint_t)62)<<32)
#define KLE_BAD_KERNELSTACK     (((k_uint_t)63)<<32)
#define KLE_BAD_DEBUG           (((k_uint_t)64)<<32)
#define KLE_DEFUNCT_PROCESS     (((k_uint_t)65)<<32)
#define KLE_BAD_VERTEX_GRPID    (((k_uint_t)66)<<32)
#define KLE_BAD_VERTEX_HNDL     (((k_uint_t)67)<<32)
#define KLE_BAD_VERTEX_EDGE     (((k_uint_t)68)<<32)
#define KLE_BAD_VERTEX_INFO     (((k_uint_t)69)<<32)
#define KLE_BAD_DEFKTHREAD      (((k_uint_t)70)<<32)
#define KLE_BAD_ANON            (((k_uint_t)71)<<32)
#define KLE_PAGE_HEADER			(((k_uint_t)72)<<32)
#define KLE_BLOCK_HEADER		(((k_uint_t)73)<<32)
#define KLE_BAD_ML_INFO 		(((k_uint_t)74)<<32)
#define KLE_BAD_CSNODE          (((k_uint_t)75)<<32)
#define KLE_BAD_STRING          (((k_uint_t)76)<<32)

/**
 ** Error Function prototypes
 **/

/* Setup the global klib_error structure. A pointer to the error 
 * output file is required. The name of the program, if provided,
 * will be coppied into klib_error. This will allow other all KLIB
 * modules to reference the program name when printing error 
 * messages. In the event of an error, a value of one (1) will be 
 * returned. Otherwise, a value of zero (0) will be returned.
 */
int kl_setup_error(
	FILE*					/* error message output file */, 
	char*					/* pathname of program */, 
	kl_print_error_func		/* pointer to application print error function */);

/* Zeros out all non-permenant fields in the klib_error structure. 
 * Normally, kl_reset_error() will be called by any KLIB function 
 * that might generate an error condition. In the event that the 
 * application is using the klib error facilities, it may be 
 * necessary for kl_reset_error() to be called by the application.
 */
void kl_reset_error();

/* Set the values in klib_error when an error occurs. The klib_error
 * structure can contain the bad value in either numeric or character
 * form (the flag parameter will determine which value to use).
 */
void kl_set_error(
	k_uint_t				/* error code */, 
	k_uint_t				/* bad value */, 
	char*					/* bad value in string form */, 
	int						/* flag value (string or value) */);

/* This function will make a call to the kl_print_errror() function
 * after printing out a debug error string.
 */
void kl_print_debug(
	char*					/* header string */);

/* Prints out a KLIB error string based on the contents of the klib_error
 * structure.
 */
void kl_print_error();
