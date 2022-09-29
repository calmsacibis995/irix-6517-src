/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  File: error_force.h Pertains to SN0 errors that can be forced
 *	  into the OS. These structures will be included by user level and kernel 
 *        programs.
 */

#if defined (SN)  && defined (FORCE_ERRORS)

/* 
 * The user can inject various types of errors into the kernel
 * system. These break down into one of the four categories of hub
 * interfaces (NI, MD, II, PI). In order to organize thes parameters
 * appropriately they are defined individually for each type and then
 * organizes as a union of all the types so they can be both handled by
 * the kernel uniformly and also referenced accordingly to each error
 * type.  
 */

/* Error involving memory operations */
typedef struct md_error_params_s {
	__uint64_t sub_code; 		/* Error to generate*/
	__uint64_t mem_addr; 		/* Address of mem to reference */
	__uint64_t read_write;		/* IS the op, read, write, or both? */
	__uint64_t access_val; 		/* What value to write to mem */
        __uint64_t flags; 	   	/* misc | with user vs. kernel*/
	__uint64_t corrupt_target; 	/* If we are corrupting what is the target*/
	__uint64_t usr_def; 		/* Left for user */
} md_error_params_t;

/* Paramters for errors involving the network */
typedef struct ni_error_params_s {
	__uint64_t sub_code; /* sub_code */
	__uint64_t node;
	__uint64_t net_vec;
	__uint64_t port;
	__uint64_t usr_def1; 
	__uint64_t usr_def2; 
	__uint64_t usr_def3; 
} ni_error_params_t;

/* Paramters for errors involving io */
typedef struct io_error_params_s {
	__uint64_t sub_code; /* sub_code */
	__uint64_t node;
	__uint64_t widget; 
	__uint64_t module; 
	__uint64_t slot; 
	__uint64_t usr_def1; 
	__uint64_t usr_def2; 
} io_error_params_t;

/* The union that encapsulates all the interfaces into one type. */
typedef union error_params_u {
	ni_error_params_t net_params;
	md_error_params_t mem_params;
	io_error_params_t io_params;
} error_params_t;

#define MD_ERROR_BASE		0x20000000
#define NI_ERROR_BASE		0x40000000
#define IO_ERROR_BASE		0x80000000

#define MD_ERR_CODE(_code)	((_code) | MD_ERROR_BASE)
#define NI_ERR_CODE(_code)	((_code) | NI_ERROR_BASE)
#define IO_ERR_CODE(_code)	((_code) | IO_ERROR_BASE)

#define IS_MD_ERROR(_code) 	((_code) & MD_ERROR_BASE)
#define IS_IO_ERROR(_code) 	((_code) & IO_ERROR_BASE)
#define IS_NI_ERROR(_code) 	((_code) & NI_ERROR_BASE)


/* Below are the types of errors that the SGI_FORCE_ERROR sys call can generate */

/* MD Errors */
#define ETYPE_DACCESS  		MD_ERR_CODE(0x1)     /* Error DWord Access */                
#define ETYPE_WACCESS  		MD_ERR_CODE(0x2)     /* Error Word Access */                 
#define ETYPE_DIRERR   		MD_ERR_CODE(0x3)     /* Error Directory Entry */             
#define ETYPE_MEMERR   		MD_ERR_CODE(0x4)     /* Secondary memory error */
#define ETYPE_SCACHERR 		MD_ERR_CODE(0x5)     /* Secondary Cache Entry */             
#define ETYPE_PICACHERR 	MD_ERR_CODE(0x6)     /* Primary Instr. Cache Error */        
#define ETYPE_PDCACHERR 	MD_ERR_CODE(0x7)     /* Primary Data Cache Error */          
#define ETYPE_PIOERR 		MD_ERR_CODE(0x8)       
#define ETYPE_PROT		MD_ERR_CODE(0x9)     /* Memory Protection Error */           
#define ETYPE_RERR		MD_ERR_CODE(0xa)     /* Node Region Error */                 
#define ETYPE_BRIDGEDEV 	MD_ERR_CODE(0xb)     /* Bridge Error */                      
#define ETYPE_PROTOCOL		MD_ERR_CODE(0xc)     /* Mem protocol error */                
#define ETYPE_NACK		MD_ERR_CODE(0xd)     /* NACK TimeOut error */                
#define ETYPE_POISON		MD_ERR_CODE(0xe)     /* Poison page then access it error */  
#define ETYPE_FLUSH		MD_ERR_CODE(0xf)     /* Flush the cache */  
#define ETYPE_AERR_BTE_PULL     MD_ERR_CODE(0x10)   
#define ETYPE_DERR_BTE_PULL     MD_ERR_CODE(0x11)   
#define ETYPE_PERR_BTE_PULL     MD_ERR_CODE(0x12)   
#define ETYPE_CORRUPT_BTE_PULL  MD_ERR_CODE(0x13)
#define ETYPE_WERR_BTE_PUSH     MD_ERR_CODE(0x14)   
#define ETYPE_PERR_BTE_PUSH     MD_ERR_CODE(0x15)   
#define ETYPE_NO_NODE_BTE_PUSH  MD_ERR_CODE(0x16)
#define ETYPE_SET_DIR_STATE  	MD_ERR_CODE(0x17)

/* NI Errors */
#define ETYPE_RTR_LINK_DISABLE 	NI_ERR_CODE(0x1)     /* Disable a router link */       
#define ETYPE_LINK_DN_N_PANIC	NI_ERR_CODE(0x2)     /* Take a link down then panic */  
#define ETYPE_LINK_RESET	NI_ERR_CODE(0x3)

/* IO Errors */
#define ETYPE_SET_READ_ACCESS 	   	IO_ERR_CODE(0x1)
#define ETYPE_CLR_READ_ACCESS	   	IO_ERR_CODE(0x2)
#define ETYPE_XTALK_HDR_RSP	   	IO_ERR_CODE(0x3)
#define ETYPE_XTALK_HDR_REQ	   	IO_ERR_CODE(0x4)
#define ETYPE_ABSENT_WIDGET 	   	IO_ERR_CODE(0x5) 
#define ETYPE_FORGE_ID_WRITE 	   	IO_ERR_CODE(0x6) 
#define ETYPE_FORGE_ID_READ 	   	IO_ERR_CODE(0x7) 
#define ETYPE_XTALK_SIDEBAND	   	IO_ERR_CODE(0x8) 
#define ETYPE_CLR_OUTBND_ACCESS    	IO_ERR_CODE(0x9)
#define ETYPE_PIO_N_PRB_ERR	   	IO_ERR_CODE(0xA)
#define ETYPE_CLR_LOCAL_ACCESS	   	IO_ERR_CODE(0xB)
#define ETYPE_XTALK_CREDIT_TIMEOUT 	IO_ERR_CODE(0xC)
#define ETYPE_SPURIOUS_MSG	   	IO_ERR_CODE(0xD)
#define ETYPE_SET_CRB_TIMEOUT	   	IO_ERR_CODE(0xE)
#define ETYPE_XTALK_USES_NON_POP_MEM	IO_ERR_CODE(0XF)
#define ETYPE_XTALK_USES_ABSENT_NODE	IO_ERR_CODE(0X10)

/* READ-WRITE is to illustrate whether the operation should be a read
 * access, write access, or both. 
 */

#define ERRF_READ  	0x1             /* Error-Force Read */
#define ERRF_WRITE 	0x2             /* Error force write */
#define ERRF_NO_ACTION	0x4		/* Do neither action */

/* The corruption flags direct the target of some of the errors. SO an
 * ECC error can be focused on the DATA, section of memory or a page
 * TAG, etc. 
 */

#define CORRUPT_DATA	0x1             /* Corrupt the data */
#define CORRUPT_TAG	0x2             /* Corrupt the tag */
#define CORRUPT_PTR	0x4            /* Corrupt the ptr */
#define CORRUPT_STATE	0x8            /* Corrupt the state */

/* Misc. Flags grouped together only because they cannot be
 * grouped. They cover the small speical flags that many of these
 * calls need on an individual basis. 
 */

#define ERRF_SHOW	0x1           	/* Turn on printfs for call */
#define ERRF_USER       0x2		/* This implies the user has
					   altered memory to be used
					   for the test and the kernel
					   should take that into
					   account, when modifying it. */

#define ERRF_PROT_RO	0x4     	/* READ-ONLY protection error */
#define ERRF_PROT_RW	0x8          	/* READ-WRITE protection error */
#define ERRF_PROT_NO	0x10     	/* READ-NO, turn off protection */
#define ERRF_ALL_NODES	0x20         	/* To have a command affect all nodes */
#define ERRF_SBE	0x40		/* Single-bit error if ECC */
#define ERRF_USER_DEF	0x80            /* Curtain #1 */
#define ERRF_USER_DEF1	0x100           /* Curtain #2 */

#endif
