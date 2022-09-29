/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module fixpub.h, release 1.4 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.fixpub.h
 *sccs
 *sccs    1.4	96/02/12 13:15:54 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.3	95/01/24 13:00:23 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.2	93/06/22 14:44:29 randy
 *sccs	more twiddling for microsoft environment
 *sccs
 *sccs    1.1	93/06/04 15:23:11 randy
 *sccs	date and time created 93/06/04 15:23:11 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef FIXPUB_H				/* avoid re-inclusion	*/
#define FIXPUB_H				/* prevent re-inclusion */

/************************************************************************
 *                                                                      *
 *          PEER Networks, a division of BMC Software, Inc.             *
 *                   CONFIDENTIAL AND PROPRIETARY                       *
 *                                                                      *
 *	This product is the sole property of PEER Networks, a		*
 *	division of BMC Software, Inc., and is protected by U.S.	*
 *	and other copyright laws and the laws protecting trade		*
 *	secret and confidential information.				*
 *									*
 *	This product contains trade secret and confidential		*
 *	information, and its unauthorized disclosure is			*
 *	prohibited.  Authorized disclosure is subject to Section	*
 *	14 "Confidential Information" of the PEER Networks, a		*
 *	division of BMC Software, Inc., Standard Development		*
 *	License.							*
 *									*
 *	Reproduction, utilization and transfer of rights to this	*
 *	product, whether in source or binary form, is permitted		*
 *	only pursuant to a written agreement signed by an		*
 *	authorized officer of PEER Networks, a division of BMC		*
 *	Software, Inc.							*
 *									*
 *	This product is supplied to the Federal Government as		*
 *	RESTRICTED COMPUTER SOFTWARE as defined in clause		*
 *	55.227-19 of the Federal Acquisitions Regulations and as	*
 *	COMMERCIAL COMPUTER SOFTWARE as defined in Subpart		*
 *	227.401 of the Defense Federal Acquisition Regulations.		*
 *									*
 * Unpublished, Copr. PEER Networks, a division of BMC	Software, Inc.  *
 *                     All Rights Reserved				*
 *									*
 *	PEER Networks, a division of BMC Software, Inc.			*
 *	1190 Saratoga Avenue, Suite 130					*
 *	San Jose, CA 95129-3433 USA					*
 *									*
 ************************************************************************/


/*************************************************************************
 *
 *	fixpub.h  -  run-time library replacements for use with MS-DOS
 *
 *	These functions supplement or replace defective library functions
 *	in the MS-DOS environment.  Note that for standard library functions,
 *	the prototypes in the system include files are just fine.
 *
 ************************************************************************/

#include "ame/machtype.h"

/*
 *	This file is really useless outside the DOS environment.
 *	The various structures and functions declared here are only
 *	used in DOS code.
 */
#ifdef PEER_MSDOS_PORT

/*
 *	define a compiler-independent structure for representing all the
 *	registers we might need when accessing DOS, packet drivers, etc.
 *	The REGPACK structure is a hack to deal with MS-DOS software
 *	interrupts.  This is really ugly but it is general and works.
 */
#define	 CARRY		1	     /* carry bit in flags register */

struct REGPACK
{
	unsigned int	r_ax;
	unsigned int	r_bx;
	unsigned int	r_cx;
	unsigned int	r_dx;
	unsigned int	r_si;
	unsigned int	r_di;
	unsigned int	r_flags;
	unsigned int	r_es;
	unsigned int	r_ds;
};



#ifdef USE_PROTOTYPES

/*
 *	addr_dif - determine the differece in bytes between two addresses
 */
extern long	addr_dif(void *upper, void *lower);

/*
 *	doskeep - terminate and stay resident
 */
extern void	doskeep(unsigned int	pages);

/*
 *	init_bkg - set up context-switching structures
 */
extern void	init_bkg(void		(*handler) (void),
			 char		*stack_space,
			 unsigned int	stack_size);

/*
 *	intr - do an 80X86 interrupt
 */
extern void	intr(int int_id, struct REGPACK *regs);

/*
 *	make_ptr - construct a pointer form segment and offset
 */
extern void	*make_ptr(unsigned short segment, unsigned short offset);

/*
 *	mux_install - bind procedure to multiplex interrupt function number
 *	May only be called ONCE by a given TSR / program.
 */
extern void	mux_install(int function_number, void *base);


/*
 *	mux_installed - determine whether a multiplex interrupt function number
 *	is already in use (by this or any other process)
 */
extern int	mux_installed(int function_number);

/*
 *	mux_getbase - retrieve information associated by someone's (not
 *	necessarily this program's) mux_install
 */
extern void	*mux_getbase(int function_number);

/*
 *	offset_of - extract the offset from a pointer
 */
extern unsigned short	offset_of(void *ptr);

/*
 *	outch - output a single character
 */
extern void	outch(char c);

/*
 *	outhex - convert a byte to hex for display
 */
void		outhex(char	b);

/*
 *	outhexes - dump n hex bytes to stdio
 */
void		outhexes(READONLY char *p, int	n);

/*
 *	outs - output a string
 */
extern void	outs(READONLY char *s);

/*
 *	p_bzero - clear a block of memory
 */
extern void	p_bzero(void *dest, unsigned int length);

/*
 *	p_gettimeofday - retrieve time of day information
 */
extern void	p_gettimeofday(void *tv, void *tz);

/*
 *	p_memcmp - compare two blocks of memory
 */
extern int	p_memcmp(READONLY void *left,
			 READONLY void *right,
			 unsigned int length);

/*
 *	p_memset - set a block of memory to an eight-bit constant
 */
extern void	p_memset(void *dest, int value, unsigned int length);

/*
 *	p_movmem - movem a block of memory
 */
extern void	p_movmem(void *src, void *dest, unsigned int length);

/*
 *	ram_mgr_init - tell the hap manager about a block of memory it can use
 */
extern void	ram_mgr_init(char *mem, unsigned int mem_size);

/*
 *	strupr - map a string to upper case
 */
extern char	*strupr(char *m);

/*
 *	segment_of - extract the segment from a pointer
 */
extern unsigned short	segment_of(void *ptr);

/*
 *	swap_bkg - swap to the next process hanging off this interrupt, if any
 *	Returns on next interrupt.
 */
extern void	swap_bkg(void);

#else

extern long	addr_dif();
extern void	doskeep();
extern void	init_bkg();
extern void	intr();
extern void	*make_ptr;
extern void	mux_install();
extern int	mux_installed();
extern unsigned short	offset_of();
extern void	outch();
void		outhex();
void		outhexes();
extern void	outs();
extern void	p_bzero();
extern void	p_gettimeofday();
extern void	p_memset();
extern void	p_movmem();
extern void	ram_mgr_init();
extern unsigned short	segment_of();
extern void	swap_bkg();

#endif

#endif

#endif
