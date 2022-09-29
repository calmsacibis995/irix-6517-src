/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

# include	"grosview.h"

# include	<sys/lock.h>
# include	<sys/schedctl.h>
# include	<sys/prctl.h>
# include	<sys/fpu.h>
# include	<gl.h>
# include	<string.h>
# include	<signal.h>
# include	<invent.h>
# include	<setjmp.h>
# include	<unistd.h>
# include	<stdio.h>

# define	MYPRI		NDPHIMIN

void	(*getdata)(void);

static char	*stackbase;

#ifdef DEBUG
static int	isfpu(void);
#endif
static void	freeze(void);

#ifdef DEBUG
static int	have_fpu = 1;
#endif

int
main(int argc, char *argv[])
{
   int		marker;

	setup(argc, argv);

#ifdef DEBUG
	have_fpu = isfpu();
	if (have_fpu)
		/*set_fpc_csr(0xf00); ignore INEXACT & UNDERFLOW */
		set_fpc_csr(0xe00);
#endif

	schedctl(NDPRI, 0, MYPRI);

	getdata = getinfo;
	stackbase = (char *) &marker;

	if (sigfreeze) {
		sigset(SIGUSR1, freeze);
	}
	runit();
	/*NOTREACHED*/
}

#ifdef DEBUG
static int
isfpu(void)
{
   inventory_t	*ip;

	while ((ip = getinvent()) != NULL) {
		if (ip->inv_class == INV_PROCESSOR)
			if (ip->inv_type == INV_FPUCHIP) {
				if (ip->inv_state != 0) {
					endinvent();
					return(1);
				}
				else {
					endinvent();
					return(0);
				}
			}
	}
	endinvent();
	return(0);
}
#endif /* DEBUG */

int
isgfx(void)
{
    static	int	done = 0;
    static	int	is_gfx = 0;
    inventory_t	*ip;

        if (done) {
	        return(is_gfx);
	}

	while ((ip = getinvent()) != NULL) {
		if (ip->inv_class == INV_GRAPHICS) {
		        is_gfx = 1;
			break;
		}
	}
	endinvent();
        done = 1;

        return(is_gfx);
}
        

jmp_buf		freezebuf;

static void
unfreeze(void)
{
	sigset(SIGUSR1, freeze);
	sigrelse(SIGALRM);
	longjmp(freezebuf, 1);
}

static void
freeze(void)
{
	sigset(SIGUSR1, unfreeze);
	if (setjmp(freezebuf))
		return;
	sighold(SIGALRM);
	sigpause(SIGUSR1);
}

/*
 * Code to lock down specific portions of memory to avoid locking down 
 * much more than we should.
 */

# define	MAXSTACK	(8*1024)

typedef struct {
	long	bstart;
	long	bend;
} memblock_t;

memblock_t	lbmem[] = {	/* local table */
	{ (long) rtbar_text_start,	(long) rtbar_text_end },
	{ (long) drawbar_text_start,	(long) drawbar_text_end },
	{ (long) getinfo_text_start,	(long) getinfo_text_end },
	{ 0,				0 },
};
memblock_t	rbmem[] = {	/* remote table */
	{ (long) drawbar_text_start,	(long) drawbar_text_end },
	{ (long) remote_text_start,	(long) remote_text_end },
	{ 0,				0 },
};
memblock_t	lrbmem[] = {	/* local slave table */
	{ (long) rtbar_text_start,	(long) rtbar_text_end },
	{ (long) drawbar_text_start,	(long) drawbar_text_end },
	{ (long) remote_text_start,	(long) remote_text_end },
	{ (long) getinfo_text_start,	(long) getinfo_text_end },
	{ 0,				0 },
};

/* The 'ifdef NOTDEF' was changed from 'ifdef REDWOOD' during the 5.3/6.0
 * merge. The reason this code differed in 6.0 was that the symbols
 * end and _fdata had disappeared from a.out's. However, at some point,
 * the compiler started putting those symbols back in, so we can revert
 * to relying on those symbols.
 */
#ifdef NOTDEF
#include <elf.h>

void
getdataseginfo(long *bod, long *len)
{
	extern int	 __elf_header[];
	extern int	 __program_header_table[];
	extern int	 __dso_displacement[];
	Elf32_Ehdr	*ep;
	Elf32_Phdr	*pp;
	int		*ip;
	int		 i;


	ep = (Elf32_Ehdr *)__elf_header;
	pp = (Elf32_Phdr *)__program_header_table;
	ip = (int *)__dso_displacement;

	if( debug )
	   fprintf(stderr,"elf header = 0x%x p header = 0x%x dso disp = 0x%x\n",
		ep, pp, ip);

	for( i = 0 ; i < ep->e_phnum ; i++, pp++ ) {
		if( (pp->p_type == PT_LOAD) && (pp->p_flags == (PF_W|PF_R)) )
			break;
	}

	if( i >= ep->e_phnum ) {
		fprintf(stderr,"unable to find dataseg\n");
		exit(1);
	}

	*bod = pp->p_vaddr + (int)ip;
	*len = pp->p_memsz;

	if( debug )
		fprintf(stderr,"bod = 0x%x len = 0x%x\n",*bod,*len);
}
#endif	/* NOTDEF */

void
lockmem(int x)
{
   memblock_t	*mp;
   long		npages = 0;
   long		rv;
#ifdef NOTDEF
   long		bod;
#else /* !NOTDEF */
   extern	long end, _fdata;
#endif /* !NOTDEF */

	if (!pinning)
		return;
	/*
	 * First, pin the text area.
	 */
	switch(x) {
	case 0:
		mp = lbmem;
		break;
	case 1:
		mp = rbmem;
		break;
	case 2:
		mp = lrbmem;
		break;
	}
	for (; mp->bstart != 0; mp++) {
		mpin((char *) mp->bstart, mp->bend - mp->bstart);
		npages += ((mp->bend-mp->bstart)+4095) / 4096;
	}
	
	/*
	 * Next, pin the stack area.
	 */
	mpin(stackbase - MAXSTACK, MAXSTACK);
	npages += (MAXSTACK+4095) / 4096;

	/*
	 * Pin all of the data area.  We figure that it's so small
	 * compared to the rest that it doesn't matter.
	 */
#if NOTDEF
	getdataseginfo(&bod, &rv);
	mpin((char *) bod, rv);
#else /* !NOTDEF */
	mpin((char *) &_fdata, (rv = ((long) &end - (long) &_fdata)));
#endif /* !NOTDEF */
	npages += (rv+4095) / 4096;

	/*
	 * Make sure the OS doesn't swap us out.
	 */
	prctl(PR_RESIDENT);

	if (debug)
		printf("initially locked down %d pages\n", npages);
}
