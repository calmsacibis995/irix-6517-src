#ifndef __IDE_MSG_H__
#define __IDE_MSG_H__

#ident "include/ide_msg.h: $Revision: 1.13 $" 

#define NO_REPORT	0
#define SUM		1
#define ERR		2
#define INFO		3
#define VRB		4
#define DBG		5
#define V1DBG		6
#define V2DBG		7
#define V3DBG		8
#define V4DBG		9
#define VDBG		10
#define PRIMDBG		20
#define PRCPU		4096
#define LOG		8192

/*
 * Added for io type tests where some tests may not be able to run since
 * optional hardware is not installed.  The conventional return values of
 * tests are:
 *	PASSED	0
 *	FAILED #failures
 *	SKIPPED -1
 */
#define TEST_PASSED	0
#define TEST_SKIPPED	(-1)

/* 
 * The upper 16 bits of the (32-bit) msg_level are for ide-internal use.
 */
#define NORM_MMASK	0x0000ffff
#define IDE_DBGMMASK	(~NORM_MMASK)
#define PRINT_IT	0x80000000

/*
 * USER_ERR, IDE_ERR, and JUST_DOIT are special msg_printf() verbosity
 * levels used only by the ide interface.  msg_printf() displays all 
 * of these messages regardless of 'Reportlevel'.  IDE_ERR and USER_ERR
 * messages are prefaced by a special identifying string.  IDE_ERR
 * messages report internal ide interface errors; USER_ERR describe
 * user errors encountered during script execution or interactively.
 * JUST_DOIT messages generally dump internal state, since the routines
 * doing the output are invoked expressly for that purpose, the 
 * Reportlevel checking is overridden.
 */
#define USER_ERR	0x00001000
#define IDE_ERR		0x00002000
#define JUST_DOIT	PRINT_IT

#define OPTDBG		0x00010000
#define OPTVDBG		0x00020000
#define EVAL_DBG	0x00040000
#define GLOB_DBG	0x00080000
#define DISPDBG		0x00100000
#define DISPVDBG	0x00200000
#define NAUSEOUS	0x00400000
#define PDA_DBG		0x00800000



/*	Codes for where output should go.  */
#define	PRW_BUF				0x01	/* Output to putbuf.	*/
#define	PRW_CONS			0x02	/* Output to console.	*/
#define MSG_LOG				0x04	/* Tee to message log */
#define MSG_LOG_OVERFLOW		0x08
#define PRW_SERIAL			0x10	/* Tee to serial console */

/* 
 * in order for msg_printf() to work correctly when in 'message buffering'
 * mode, max message length must be MAXMSGLENGTH bytes.
 */
#define MAXMSGLENGTH    160	/* see uif/msg_printf.c */

#define NON_COHERENT	0xDEAD 
#define COHERENT	0xCACE 

#if !defined(_LANGUAGE_ASSEMBLY)
/* allocation prototypes here for lack of a better place */
int  init_ide_malloc(int);
void *get_chunk(int);
void *ide_malloc(int);
void *ide_align_malloc(int,int);
void *ide_free(void *);

extern int _coherent;
#endif

#endif /* __IDE_MSG_H__ */
