#ident "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/include/RCS/hwregcmd.h,v 1.2 1999/05/18 07:11:56 zs Exp $"

/*
 * Common definitions for hw register commands (hubreg, dirmem, ...)
 * Mostly taken from stand/arcs/tools/hwreg/main.c
 * some added for icrash.
 */

#include <sys/types.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/SN0/hub.h>
#include <sys/SN/addrs.h>
#include <sys/SN/SN0/arch.h>
#include <klib/hwreg.h>

extern	hwreg_set_t		hwreg_hub;
extern	hwreg_set_t		hwreg_router;
extern	hwreg_set_t		hwreg_xbow;
extern	hwreg_set_t		hwreg_c0;
extern	hwreg_set_t		hwreg_dir;

#define MB			0x100000ULL
typedef __uint64_t		i64;

extern int	opt_base;
extern i64	opt_calias;
extern char	opt_addr[80];
extern int	opt_nmode;
extern int	opt_hub;
extern int	opt_router;
extern int	opt_xbow;
extern int	opt_c0;
extern int	opt_dir;
extern int	opt_list;
extern int	opt_node;
extern int	opt_node_given;
extern int	opt_short;
extern int	opt_sn00;
extern int	opt_bit;
extern int	opt_premium;
extern i64	opt_syn;
extern int	opt_syn_given;
extern i64	winsize;		/* 256 MB if N-mode, 512 MB if M-mode */

extern FILE *ofp;

#define P0(x)			fprintf(ofp, x)
#define P1(x,par)		fprintf(ofp, x, par)
#define P2(x,par1,par2)		fprintf(ofp, x, par1, par2)


#define REGION(a)		((a) & 0xffffff0000000000)
#define ADD_REGION(a, r)	(a |= (r))

#define NODE(a)			(int) (opt_nmode ?		\
					   (a) >> 31 & 0x1ff :	\
					   (a) >> 32 & 0xff)

#define ADD_NODE(a, n)		(opt_nmode ?			\
				 (a |= (i64) (n) << 31) :	\
				 (a |= (i64) (n) << 32))

#define REM_NODE(a)		(opt_nmode ?			\
				 (a &= ~((i64) 0xff << 31)) :	\
				 (a &= ~((i64) 0xff << 32)))

#define NODEOFF(a)		(opt_nmode ?			\
				 ((a) & (1ULL << 31) - 1) :	\
				 ((a) & (1ULL << 32) - 1))

#define PHYSADDR(a)		((a) & (1ULL << 40) - 1)

#define BANK(a)			(int) ((a) >> 29 & (opt_nmode ? 7 : 3))
#define BANKOFF(a)		((a) & (1ULL << 29) - 1)

#define BWIN(a)			(int) (opt_nmode ?		\
					   (a) >> 28 & 7 :		\
					   (a) >> 29 & 7)
#define ADD_BWIN(a, w)		(opt_nmode ?			\
				 (a |= (i64) (w) << 28) :	\
				 (a |= (i64) (w) << 29))

#define SWIN(a)			(int) ((a) >> 24 & 0xf)
#define ADD_SWIN(a, w)		(a |= (i64) (w) << 24)

#define REMOTE_BIT(a)		(int) ((a) >> 23 & 1)
#define ADD_REMOTE_BIT(a)	(a |= 1ULL << 23)

#define HUBDIV_PI		0
#define HUBDIV_MD		1
#define HUBDIV_IO		2
#define HUBDIV_NI		3

#define HUBOFF(a)		((a) & 0x7fffff)
#define HUBDIV(a)		(HUBOFF(a) >> 21)
#define HUBDIVOFF(a)		((a) & 0x1fffff)

#define PDIR_STATUS(lo)		((lo & MD_PDIR_STATE_MASK) >> 13)
#define SDIR_STATUS(lo)		((lo & MD_SDIR_STATE_MASK) >>  7)

#define P_BIT_VEC(hi, lo)	(((hi & MD_PDIR_VECLSB_MASK) >> 10 ) | \
				(((lo & MD_PDIR_VECMSB_MASK) >> 22 ) << 38))

#define S_BIT_VEC(hi, lo)	(((hi & MD_SDIR_VECLSB_MASK) >>  5 ) | \
				 ((lo & MD_SDIR_VECMSB_MASK)))

#define P_ONECNT(lo)		((lo & MD_PDIR_ONECNT_MASK) >> 16)
