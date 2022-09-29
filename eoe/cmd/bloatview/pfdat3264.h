/*
 * pfdat3264.h
 *
 */

#ifndef PFDAT3264_H
#define PFDAT3264_H

#include <sys/types.h>
#include <sgidefs.h>

typedef __int32_t Pointer32;
typedef __int64_t Pointer64;

typedef struct pfdat32 {
	Pointer32	pf_next;       /* Next free pfdat.     */
	union {
		Pointer32	prev;	/* Previous free pfdat. */
		sm_swaphandle_t	swphdl; /* Swap hdl for anon pages */
	} p_swpun;
	int		pf_vcolor:8,	/* Virtual cache color. */
					/* bit 0: PE_RAWWAIT rawwait */
					/* bits 1..7: PE_COLOR  */
			pf_flags:24;	/* Page flags.		*/
	cnt_t		pf_use;		/* Share use count.	*/
	unsigned short	pf_rawcnt;	/* Count of processes	*/
					/* doing raw I/O to page*/
	unsigned	pf_pageno;	/* Object page number	*/
	union {
		Pointer32	vp;	/* Page's incore vnode. */
		Pointer32	tag;	/* Generic hash tag.	*/
	} p_un;
	Pointer32	pf_hchain;	/* Hash chain link.	*/

	union pde	*pf_pdep1;		/* Primary pde ptr	*/
	union {
		Pointer32	*pf_revmapp;	/* Reverse map pointer	*/
		union	pde	*pf_pdeptr;	/* Page tbl entry ptr	*/
	} p_rmapun;

} pfd32_t;

typedef struct pfdat64 {
	Pointer64	pf_next;       /* Next free pfdat.     */
	union {
		Pointer64	prev;	/* Previous free pfdat. */
		sm_swaphandle_t	swphdl;	/* Swap hdl for anon pages */
	} p_swpun;
	int		pf_vcolor:8,	/* Virtual cache color. */
					/* bit 0: PE_RAWWAIT rawwait */
					/* bits 1..7: PE_COLOR  */
			pf_flags:24;	/* Page flags.		*/
	cnt_t		pf_use;		/* Share use count.	*/
	unsigned short	pf_rawcnt;	/* Count of processes	*/
					/* doing raw I/O to page*/
	unsigned	pf_pageno;	/* Object page number	*/
	union {
		Pointer64	vp;	/* Page's incore vnode. */
		Pointer64	tag;	/* Generic hash tag.	*/
	} p_un;
	Pointer64	pf_hchain;	/* Hash chain link.	*/

	union pde	*pf_pdep1;		/* Primary pde ptr	*/
	union {
		Pointer64	*pf_revmapp;	/* Reverse map pointer	*/
		union	pde	*pf_pdeptr;	/* Page tbl entry ptr	*/
	} p_rmapun;

} pfd64_t;


#endif

