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

# include	<sys/utsname.h>
# include	<sys/param.h>
# include	<sys/fs/efs_ino.h>
# include	<sys/fs/efs_fs.h>
# include	<sys/ioctl.h>
# include	<sys/socket.h>
# include	<net/if.h>
# include	<stdlib.h>
# include	<string.h>
# include	<unistd.h>

# define	UPMOVE		.4	/* maximum move per tick */
# define	SUPMOVE		1.0	/* up move on strip */
# define	INTERVAL	2	/* in tenths of a second */
# define	NSAMPLES	150	/* default samples in bar */

int		interval = INTERVAL;

/*
 * Bar initialization information.
 * NOTE: the 'mask' values are those valuse NOT to be included in the tracksum
 *	and avg values.
 */

/*
 * Collated information about bars for initialization.  When adding new
 * bars, this is the central data structure to update.
 */
typedef struct {
	drfunc_t	i_draw;			/* draw routine */
	void		(*i_init)(barlist_t *);	/* init routine */
	int		i_preftype;		/* preferred type */
	int		i_prefintv;		/* preferred interval */
	int		i_prefsintv;		/* preferred strip interval */
	int		i_prefnintv;		/* pref numeric interval */
	int		i_prefsamps;		/* pref strip-chart samples */
	int		i_preflim;		/* preferredd limit value */
} barinit_t;

static void	initcpubar(barlist_t *);
static void	initwaitbar(barlist_t *);
static void	initmembar(barlist_t *);
static void	initmemcbar(barlist_t *);
static void	initwaitbar(barlist_t *);
static void	initgfxbar(barlist_t *);
static void	initsyscallbar(barlist_t *);
static void	initiothrubar(barlist_t *);
static void	initdiskbar(barlist_t *);
static void	initbdevbar(barlist_t *);
static void	initnetudpbar(barlist_t *);
static void	initnetiprbar(barlist_t *);
static void	initnetbar(barlist_t *);
static void	initnetipbar(barlist_t *);
static void	initintrbar(barlist_t *);
static void	initfaultbar(barlist_t *);
static void	initswapbar(barlist_t *);
static void	inittlbbar(barlist_t *);
static void	initswpsbar(barlist_t *);
static void	expandcheck(void);
static void	expandcpu(barlist_t *, int);
static void	expandnetif(barlist_t *);
static void	setmovep(barlist_t *);
static void	setintervalp(barlist_t *, int);
static void	setheader(barlist_t *, char *);
static void	setnumcolors(barlist_t *);

static barinit_t	bardata[] = {
/*  0 */{ drawcpubar,     initcpubar,	T_REL, 0, 2, 5, NSAMPLES, 0},
/*  1 */{ drawmembar,     initmembar,	T_ABS, 0, 2, 5, NSAMPLES, 0},
/*  2 */{ drawwaitbar,    initwaitbar,	T_REL, 0, 2, 5, NSAMPLES, 0},
/*  3 */{ drawgfxbar, 	  initgfxbar,	T_ABS, 0, 2, 5, NSAMPLES, 50},
/*  4 */{ drawsyscallbar, initsyscallbar,	T_ABS, 0, 2, 5, NSAMPLES, 50},
/*  5 */{ drawiothrubar,  initiothrubar,	T_ABS, 0, 2, 5, NSAMPLES, 50},
/*  6 */{ drawdiskbar,    initdiskbar,	T_ABS, 10, 2, 5, NSAMPLES, 0},
/*  7 */{ drawbdevbar,    initbdevbar,	T_ABS, 0, 2, 5, NSAMPLES, 50},
/*  8 */{ drawintrbar,    initintrbar,	T_ABS, 0, 2, 5, NSAMPLES, 50},
/*  9 */{ drawfaultbar,   initfaultbar,	T_NUM, 0, 2, 5, NSAMPLES, 50},
/* 10 */{ drawtlbbar,	  inittlbbar,	T_NUM, 50, 2, 5, NSAMPLES, 0},
/* 11 */{ drawswapbar,    initswapbar,  T_ABS, 5, 2, 5, NSAMPLES, 0},
/* 12 */{ drawnetbar,     initnetbar,   T_ABS, 0, 2, 5, NSAMPLES, 0},
/* 13 */{ drawnetipbar,   initnetipbar, T_ABS, 0, 2, 5, NSAMPLES, 0},
/* 14 */{ drawmemcbar,    initmemcbar,	T_ABS, 0, 2, 5, NSAMPLES, 0},
/* 15 */{ drawswpsbar,    initswpsbar,  T_ABS, 5, 2, 5, NSAMPLES, 0},
/* 16 */{ drawnetudpbar,  initnetudpbar,	T_ABS, 0, 2, 5, NSAMPLES, 0},
/* 17 */{ drawnetiprbar,  initnetiprbar,	T_ABS, 0, 2, 5, NSAMPLES, 0},
	{ 0 },
};

/*
 * Routines to initialize the various bars in their custom state.
 */
# define	cb	(*bp->s_cb)

static void
setdirectcolors(barlist_t *bp)
{
	register int i;

	for (i = 0; i < bp->s_ndcolors; i++)
		cb.cb_colors[i] = bp->s_colors[i];
}

static void
initcpubar(barlist_t *bp)
{
   char		mbuf[100];

	if (bp->s_subtype != CPU_SUM && ncpu > 1) {
		sprintf(mbuf, "CPU %d Usage: ", bp->s_subtype);
		cb.cb_header = strdup(mbuf);
	} else
		cb.cb_header = "CPU Usage: ";

	bp->s_nsects = cb.cb_nsects = 6;

	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[bad_c];
	cb.cb_colors[2] = bp->s_colors[inuse_c];
	cb.cb_colors[3] = bp->s_colors[light_c];
	cb.cb_colors[4] = bp->s_colors[waste_c];
	cb.cb_colors[5] = bp->s_colors[unused_c];
	setdirectcolors(bp);
	if (cb.cb_type == T_SREL)
		cb.cb_colors[5] = backcolor;
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 0;
	cb.cb_last[3] = 0;
	cb.cb_last[4] = 0;
	cb.cb_last[5] = 1.0;
	cb.cb_legend[0] = "user  ";
	cb.cb_legend[1] = "sys  ";
	cb.cb_legend[2] = "intr  ";
	cb.cb_legend[3] = "gfxf  ";
	cb.cb_legend[4] = "gfxc  ";
	cb.cb_legend[5] = "idle ";
}

static void
initwaitbar(barlist_t *bp)
{

	cb.cb_header = "CPU Wait: ";
	bp->s_nsects = cb.cb_nsects = 4;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[inuse_c];
	cb.cb_colors[2] = bp->s_colors[light_c];
	setdirectcolors(bp);
	cb.cb_colors[3] = backcolor;
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 0;
	cb.cb_last[3] = 1.0;
	cb.cb_legend[0] = "io ";
	cb.cb_legend[1] = "swap ";
	cb.cb_legend[2] = "pio ";
	cb.cb_legend[3] = "";
	cb.cb_nmask = mask(3);
}

static void
initmembar(barlist_t *bp)
{
	cb.cb_header = "Memory:   ";
	bp->s_nsects = cb.cb_nsects = 6;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[light_c];
	cb.cb_colors[2] = bp->s_colors[bad_c];
	cb.cb_colors[3] = bp->s_colors[waste_c];
	cb.cb_colors[4] = bp->s_colors[unused_c];
	cb.cb_colors[5] = bp->s_colors[heavy_c];
	setdirectcolors(bp);
	cb.cb_legend[0] = "kernel  ";
	cb.cb_legend[1] = "fs ctl  ";
	cb.cb_legend[2] = "fsdirty  ";
	cb.cb_legend[3] = "fsclean  ";
	cb.cb_legend[4] = "free  ";
	cb.cb_legend[5] = "user ";
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 0;
	cb.cb_last[3] = 0;
	cb.cb_last[4] = 0;
	cb.cb_last[5] = 1.0;
	cb.cb_flags |= TF_NOSCALE;
	cb.cb_nmask = mask(4);
}

static void
initmemcbar(barlist_t *bp)
{
	cb.cb_header = "Memory:   ";
	bp->s_nsects = cb.cb_nsects = 7;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[light_c];
	cb.cb_colors[2] = bp->s_colors[bad_c];
	cb.cb_colors[3] = bp->s_colors[waste_c];
	cb.cb_colors[4] = bp->s_colors[inuse_c];
	cb.cb_colors[5] = bp->s_colors[unused_c];
	cb.cb_colors[6] = bp->s_colors[heavy_c];
	setdirectcolors(bp);
	cb.cb_legend[0] = "kernel  ";
	cb.cb_legend[1] = "fs ctl  ";
	cb.cb_legend[2] = "fsdirty  ";
	cb.cb_legend[3] = "fsclean  ";
	cb.cb_legend[4] = "freec  ";
	cb.cb_legend[5] = "freeu  ";
	cb.cb_legend[6] = "user ";
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 0;
	cb.cb_last[3] = 0;
	cb.cb_last[4] = 0;
	cb.cb_last[5] = 0;
	cb.cb_last[6] = 1.0;
	cb.cb_flags |= TF_NOSCALE;
	cb.cb_nmask = mask(5);
}

static void
initgfxbar(barlist_t *bp)
{
	bp->s_nsects = cb.cb_nsects = 7;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[light_c];
	cb.cb_colors[2] = bp->s_colors[bad_c];
	cb.cb_colors[3] = bp->s_colors[inuse_c];
	cb.cb_colors[4] = bp->s_colors[waste_c];
	cb.cb_colors[5] = bp->s_colors[unused_c];
	setdirectcolors(bp);
	cb.cb_colors[6] = backcolor;
	cb.cb_legend[0] = "intr    ";
	cb.cb_legend[1] = "swch   ";
	cb.cb_legend[2] = "ioctl  ";
	cb.cb_legend[3] = "swap   ";
	cb.cb_legend[4] = "fiwt    ";
	cb.cb_legend[5] = "finowt  ";
	cb.cb_legend[6] = "";
	cb.cb_nmask = mask(6);
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 0;
	cb.cb_last[3] = 0;
	cb.cb_last[4] = 0;
	cb.cb_last[5] = 0;
	cb.cb_last[6] = 1.0;
	setheader(bp, "Gfx");
}

static void
initsyscallbar(barlist_t *bp)
{

	bp->s_nsects = cb.cb_nsects = 6;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[light_c];
	cb.cb_colors[2] = bp->s_colors[bad_c];
	cb.cb_colors[3] = bp->s_colors[inuse_c];
	cb.cb_colors[4] = bp->s_colors[waste_c];
	setdirectcolors(bp);
	cb.cb_colors[5] = backcolor;
	cb.cb_legend[0] = "sycall   ";
	cb.cb_legend[1] = "switch   ";
	cb.cb_legend[2] = "fork   ";
	cb.cb_legend[3] = "exec   ";
	cb.cb_legend[4] = "iget   ";
	cb.cb_legend[5] = "";
	cb.cb_nmask = mask(5);
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 0;
	cb.cb_last[3] = 0;
	cb.cb_last[4] = 0;
	cb.cb_last[5] = 1.0;
	setheader(bp, "SysAct");
}

static void
initiothrubar(barlist_t *bp)
{

	bp->s_nsects = cb.cb_nsects = 3;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[light_c];
	setdirectcolors(bp);
	cb.cb_colors[2] = backcolor;
	cb.cb_legend[0] = "read ";
	cb.cb_legend[1] = "write ";
	cb.cb_legend[2] = "";
	cb.cb_nmask = mask(2);
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 1.0;
	setheader(bp, "Read/Write Throughput");
}

# define	MAXDNAME	20

static void
initdiskbar(barlist_t *bp)
{
   char			dname[MAXDNAME];
   dstat_t		*dp = &dstat[bp->s_disk];
   struct statfs	*fp = &dp->d_stat;

	if (dp->d_flags & DF_EFS) {
		cb.cb_nsects = 4;
		cb.cb_colors[0] = bp->s_colors[inuse_c];
		cb.cb_colors[1] = bp->s_colors[light_c];
		cb.cb_colors[2] = bp->s_colors[heavy_c];
		cb.cb_colors[3] = bp->s_colors[unused_c];
		setdirectcolors(bp);
		if (cb.cb_type == T_SREL)
			cb.cb_colors[3] = backcolor;
		cb.cb_legend[0] = "Iused  ";
		cb.cb_legend[1] = "Ifree  ";
		cb.cb_legend[2] = "Bused  ";
		cb.cb_legend[3] = "Bfree";
		cb.cb_nmask = mask(0)|mask(1)|mask(3);
		cb.cb_tlimit = blktomb(fp->f_blocks, fp->f_bsize);
		cb.cb_results[0] =
			blktomb(((double)(fp->f_files-fp->f_ffree)/EFS_INOPBB),
			fp->f_bsize);
		cb.cb_results[1] = blktomb(((double)fp->f_ffree/EFS_INOPBB),
			fp->f_bsize);
		cb.cb_results[2] = blktomb(fp->f_blocks-fp->f_bfree,
			fp->f_bsize);
		cb.cb_results[3] = blktomb(fp->f_bfree, fp->f_bsize);
	}
	else {
		cb.cb_nsects = 2;
		cb.cb_colors[0] = bp->s_colors[heavy_c];
		cb.cb_colors[1] = bp->s_colors[unused_c];
		setdirectcolors(bp);
		if (cb.cb_type == T_SREL)
			cb.cb_colors[1] = backcolor;
		cb.cb_legend[0] = "Bused  ";
		cb.cb_legend[1] = "Bfree";
		cb.cb_nmask = mask(1);
		cb.cb_tlimit = blktomb(fp->f_blocks, fp->f_bsize);
		cb.cb_results[0] = blktomb(fp->f_blocks-fp->f_bfree,
			fp->f_bsize);
		cb.cb_results[1] = blktomb(fp->f_bfree, fp->f_bsize);
	}
	cb.cb_flags |= TF_NOSCALE|TF_LIMFLT|TF_MBSCALE;
	cb.cb_upmove = 1.0;

	bp->s_nsects = cb.cb_nsects;
	dname[0] = '[';
	dname[1] = '\0';
	strncat(dname, bp->s_name, MAXDNAME-4);
	strcat(dname, "]");
	setheader(bp, dname);
}

static void
initbdevbar(barlist_t *bp)
{
	if (cb.cb_type == T_NUM) {
		cb.cb_nsects = 8;
		cb.cb_colors[0] = bp->s_colors[heavy_c];
		cb.cb_colors[1] = bp->s_colors[light_c];
		cb.cb_colors[2] = bp->s_colors[bad_c];
		cb.cb_colors[3] = bp->s_colors[inuse_c];
		cb.cb_colors[4] = bp->s_colors[waste_c];
		cb.cb_colors[5] = bp->s_colors[waste_c];
		cb.cb_colors[6] = bp->s_colors[unused_c];
		setdirectcolors(bp);
		cb.cb_colors[7] = backcolor;
		cb.cb_last[0] = 0;
		cb.cb_last[1] = 0;
		cb.cb_last[1] = 0;
		cb.cb_last[2] = 0;
		cb.cb_last[3] = 0;
		cb.cb_last[4] = 0;
		cb.cb_last[5] = 0;
		cb.cb_last[6] = 0;
		cb.cb_last[7] = 1.0;
		cb.cb_legend[0] = "lread     ";
		cb.cb_legend[1] = "bread  ";
		cb.cb_legend[2] = "%rhit  ";
		cb.cb_legend[3] = "lwrite    ";
		cb.cb_legend[4] = "bwrite  ";
		cb.cb_legend[5] = "wcncl  ";
		cb.cb_legend[6] = "%whit ";
		cb.cb_legend[7] = "";
		cb.cb_nmask = mask(2)|mask(5)|mask(6);
		cb.cb_flags &= ~TF_AVERAGE;
		bp->s_flags &= ~SF_AVERAGE;
	}
	else {
		cb.cb_nsects = 3;
		cb.cb_colors[0] = bp->s_colors[heavy_c];
		cb.cb_colors[1] = bp->s_colors[light_c];
		setdirectcolors(bp);
		cb.cb_colors[2] = backcolor;
		cb.cb_legend[0] = "bread ";
		cb.cb_legend[1] = "bwrite ";
		cb.cb_legend[2] = "";
		cb.cb_nmask = mask(2);
		cb.cb_last[0] = 0;
		cb.cb_last[1] = 0;
		cb.cb_last[2] = 1.0;
	}
	bp->s_nsects = cb.cb_nsects;
	setheader(bp, "BufAct");
}

static void
initnetudpbar(barlist_t *bp)
{
	if (cb.cb_type == T_NUM) {
		cb.cb_nsects = 3;
		cb.cb_colors[0] = bp->s_colors[heavy_c];
		cb.cb_colors[1] = bp->s_colors[light_c];
		cb.cb_colors[2] = bp->s_colors[bad_c];
		setdirectcolors(bp);
		cb.cb_legend[0] = "INdgram ";
		cb.cb_legend[1] = "OUTdgram ";
		cb.cb_legend[2] = "Dropped ";
		cb.cb_nmask = mask(2);
	}
	else {
		cb.cb_nsects = 4;
		cb.cb_colors[0] = bp->s_colors[inuse_c];
		cb.cb_colors[1] = bp->s_colors[heavy_c];
		cb.cb_colors[2] = bp->s_colors[light_c];
		setdirectcolors(bp);
		cb.cb_colors[3] = backcolor;
		cb.cb_legend[0] = "Dropped ";
		cb.cb_legend[1] = "INdgram ";
		cb.cb_legend[2] = "OUTdgram ";
		cb.cb_legend[3] = "";
		cb.cb_nmask = mask(0)|mask(3);
		cb.cb_last[3] = 1.0;
	}
	bp->s_nsects = cb.cb_nsects;
	setheader(bp, "NetUDP Act");
}

static void
initnetiprbar(barlist_t *bp)
{
	if (cb.cb_type == T_NUM) {
		cb.cb_nsects = 5;
		cb.cb_colors[0] = bp->s_colors[heavy_c];
		cb.cb_colors[1] = bp->s_colors[light_c];
		cb.cb_colors[2] = bp->s_colors[bad_c];
		cb.cb_colors[3] = bp->s_colors[inuse_c];
		cb.cb_colors[4] = bp->s_colors[waste_c];
		setdirectcolors(bp);
		cb.cb_legend[0] = "INpack ";
		cb.cb_legend[1] = "OUTpack ";
		cb.cb_legend[2] = "Forward ";
		cb.cb_legend[3] = "Delivered ";
		cb.cb_legend[4] = "Dropped ";
		cb.cb_nmask = mask(2)|mask(3)|mask(4);
	}
	else {
		cb.cb_nsects = 4;
		cb.cb_colors[0] = bp->s_colors[inuse_c];
		cb.cb_colors[1] = bp->s_colors[heavy_c];
		cb.cb_colors[2] = bp->s_colors[light_c];
		setdirectcolors(bp);
		cb.cb_colors[3] = backcolor;
		cb.cb_legend[0] = "Dropped ";
		cb.cb_legend[1] = "INpack ";
		cb.cb_legend[2] = "OUTpack ";
		cb.cb_legend[3] = "";
		cb.cb_nmask = mask(0)|mask(3);
		cb.cb_last[3] = 1.0;
	}
	bp->s_nsects = cb.cb_nsects;
	setheader(bp, "NetIP Act");
}

static void
initnetbar(barlist_t *bp)
{
	if (cb.cb_type == T_NUM) {
		cb.cb_nsects = 2;
		cb.cb_colors[0] = bp->s_colors[heavy_c];
		cb.cb_colors[1] = bp->s_colors[light_c];
		setdirectcolors(bp);
		cb.cb_last[0] = 0;
		cb.cb_last[1] = 0;
		cb.cb_legend[0] = "TCPinKb ";
		cb.cb_legend[1] = "TCPoutKb ";
	}
	else {
		cb.cb_nsects = 3;
		cb.cb_colors[0] = bp->s_colors[heavy_c];
		cb.cb_colors[1] = bp->s_colors[light_c];
		setdirectcolors(bp);
		cb.cb_colors[2] = backcolor;
		cb.cb_legend[0] = "TCPinKb ";
		cb.cb_legend[1] = "TCPoutKb ";
		cb.cb_legend[2] = "";
		cb.cb_nmask = mask(2);
		cb.cb_last[0] = 0;
		cb.cb_last[1] = 0;
		cb.cb_last[2] = 1.0;
	}
	bp->s_nsects = cb.cb_nsects;
	setheader(bp, "NetTCP Act");
}

static void
initnetipbar(barlist_t *bp)
{
   int			localif = 0;
   char			headmsg[30];

	if (bp->s_subtype != IP_SUM)
		if (strncmp(nifreq[bp->s_subtype].ifr_name, "lo", 2) == 0)
			localif = 1;
	if (cb.cb_type == T_NUM && !localif) {
		cb.cb_nsects = 6;
		cb.cb_colors[0] = bp->s_colors[heavy_c];
		cb.cb_colors[1] = bp->s_colors[light_c];
		cb.cb_colors[2] = bp->s_colors[bad_c];
		cb.cb_colors[3] = bp->s_colors[inuse_c];
		cb.cb_colors[4] = bp->s_colors[waste_c];
		setdirectcolors(bp);
		cb.cb_colors[5] = backcolor;
		cb.cb_last[0] = 0;
		cb.cb_last[1] = 0;
		cb.cb_last[2] = 0;
		cb.cb_last[3] = 0;
		cb.cb_last[4] = 0;
		cb.cb_last[5] = 1.0;
		cb.cb_legend[0] = "INpack ";
		cb.cb_legend[1] = "OUTpack ";
		cb.cb_legend[2] = "INerr ";
		cb.cb_legend[3] = "OUTerr ";
		cb.cb_legend[4] = "coll ";
		cb.cb_legend[5] = "";
		cb.cb_nmask = mask(2)|mask(3)|mask(4)|mask(5);
		cb.cb_flags &= ~TF_AVERAGE;
		bp->s_flags &= ~SF_AVERAGE;
	} else {
		cb.cb_nsects = 3;
		cb.cb_colors[0] = bp->s_colors[heavy_c];
		cb.cb_colors[1] = bp->s_colors[light_c];
		setdirectcolors(bp);
		cb.cb_colors[2] = backcolor;
		cb.cb_legend[0] = "INpack  ";
		cb.cb_legend[1] = "OUTpack  ";
		cb.cb_legend[2] = "";
		cb.cb_nmask = mask(2);
		cb.cb_last[0] = 0;
		cb.cb_last[1] = 0;
		cb.cb_last[2] = 1.0;
		if (cb.cb_type == T_NUM) {
			/* must be a local interface */
			cb.cb_flags &= ~TF_AVERAGE;
			bp->s_flags &= ~SF_AVERAGE;
		}
	}
	bp->s_nsects = cb.cb_nsects;
	if (bp->s_subtype == IP_SUM) {
		if (nif > 1)
			setheader(bp, "NetIFAct");
		else {
			sprintf(headmsg, "NetIF[%s]", nifreq->ifr_name);
			setheader(bp, headmsg);
		}
	}
	else {
		sprintf(headmsg, "NetIF[%s]", nifreq[bp->s_subtype].ifr_name);
		setheader(bp, headmsg);
	}
}

static void
initintrbar(barlist_t *bp)
{
	bp->s_nsects = cb.cb_nsects = 3;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[light_c];
	setdirectcolors(bp);
	cb.cb_colors[2] = backcolor;
	cb.cb_legend[0] = "other  ";
	cb.cb_legend[1] = "vme  ";
	cb.cb_legend[2] = "";
	cb.cb_nmask = mask(2);
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 1.0;
	setheader(bp, "Interrupts");
}

static void
initfaultbar(barlist_t *bp)
{

	bp->s_nsects = cb.cb_nsects = 9;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[light_c];
	cb.cb_colors[2] = bp->s_colors[bad_c];
	cb.cb_colors[3] = bp->s_colors[waste_c];
	cb.cb_colors[4] = bp->s_colors[inuse_c];
	cb.cb_colors[5] = bp->s_colors[unused_c];
	cb.cb_colors[6] = bp->s_colors[heavy_c];
	cb.cb_colors[7] = bp->s_colors[light_c];
	setdirectcolors(bp);
	cb.cb_colors[8] = backcolor;
	cb.cb_legend[0] = "cpw    ";
	cb.cb_legend[1] = "mod    ";
	cb.cb_legend[2] = "dmd    ";
	cb.cb_legend[3] = "cache  ";
	cb.cb_legend[4] = "file   ";
	cb.cb_legend[5] = "swap   ";
	cb.cb_legend[6] = "double ";
	cb.cb_legend[7] = "pgref  ";
	cb.cb_legend[8] = "";
	cb.cb_nmask = mask(8);
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 0;
	cb.cb_last[3] = 0;
	cb.cb_last[4] = 0;
	cb.cb_last[5] = 0;
	cb.cb_last[6] = 0;
	cb.cb_last[7] = 0;
	cb.cb_last[8] = 1.0;
	setheader(bp, "Page faults");
}

static void
initswapbar(barlist_t *bp)
{

	bp->s_nsects = cb.cb_nsects = 3;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[light_c];
	setdirectcolors(bp);
	cb.cb_colors[2] = backcolor;
	cb.cb_legend[0] = "swpin ";
	cb.cb_legend[1] = "swpout ";
	cb.cb_legend[2] = "";
	cb.cb_nmask = mask(2);
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 1.0;
	setheader(bp, "Pages Swapped");
}

static void
inittlbbar(barlist_t *bp)
{

	bp->s_nsects = cb.cb_nsects = 8;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[light_c];
	cb.cb_colors[2] = bp->s_colors[bad_c];
	cb.cb_colors[3] = bp->s_colors[waste_c];
	cb.cb_colors[4] = bp->s_colors[inuse_c];
	cb.cb_colors[5] = bp->s_colors[unused_c];
	cb.cb_colors[6] = bp->s_colors[heavy_c];
	setdirectcolors(bp);
	cb.cb_colors[7] = backcolor;
	cb.cb_legend[0] = "vmwrap  ";
	cb.cb_legend[1] = "mpsync ";
	cb.cb_legend[2] = "flush ";
	cb.cb_legend[3] = "idwrap ";
	cb.cb_legend[4] = "idget ";
	cb.cb_legend[5] = "idpurge ";
	cb.cb_legend[6] = "vapurge";
	cb.cb_legend[7] = "";
	cb.cb_nmask = mask(7);
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 0;
	cb.cb_last[3] = 0;
	cb.cb_last[4] = 0;
	cb.cb_last[5] = 0;
	cb.cb_last[6] = 0;
	cb.cb_last[7] = 1.0;
	setheader(bp, "TLBAct");
}

static void
initswpsbar(barlist_t *bp)
{
	bp->s_nsects = cb.cb_nsects = 9;
	cb.cb_colors[0] = bp->s_colors[heavy_c];
	cb.cb_colors[1] = bp->s_colors[bad_c];
	cb.cb_colors[2] = bp->s_colors[inuse_c];
	cb.cb_colors[3] = bp->s_colors[light_c];
	cb.cb_colors[4] = bp->s_colors[waste_c];
	cb.cb_colors[5] = bp->s_colors[unused_c];
	cb.cb_colors[6] = bp->s_colors[bad_c];
	cb.cb_colors[7] = bp->s_colors[heavy_c];

	setdirectcolors(bp);
	cb.cb_legend[0] = "mem-r  ";
	cb.cb_legend[1] = "mem   ";
	cb.cb_legend[2] = "fswp-r ";
	cb.cb_legend[3] = "fswp   ";
	cb.cb_legend[4] = "uswp-r ";
	cb.cb_legend[5] = "uswp   ";
	cb.cb_legend[6] = "vswp-r ";
	cb.cb_legend[7] = "vswp  ";
	cb.cb_legend[8] = "";
	cb.cb_nmask = mask(1)|mask(3)|mask(5)|mask(7);
	cb.cb_last[0] = 0;
	cb.cb_last[1] = 0;
	cb.cb_last[2] = 0;
	cb.cb_last[3] = 0;
	cb.cb_last[4] = 0;
	cb.cb_last[5] = 0;
	cb.cb_last[6] = 0;
	cb.cb_last[7] = 0;
	cb.cb_last[8] = 1.0;
	cb.cb_flags |= TF_NOSCALE;
	setheader(bp, "Swap");
}

drfunc_t
getdrawfunc(int type)
{
	return(bardata[type].i_draw);
}

void
reinitbars(void)
{

	if (slave)
		pushoptions(stdout);
	else if (logfd)
		pushoptions(logfd);
	if (slave) {
		if (debug)
			fprintf(stderr, "reinitbars: push back\n");
		pushbackcb(0);
	}
	else if (logfd)
		pushbackcb(logfd);

	/*
	 * Now that all bars are initialized, open up a window and lay
	 * them out in it.
	 */
	if (logfd)
		fflush(logfd);
	if (slave) {
	   char		namebuf[100];

		if (sname == 0) {
			gethostname(namebuf, 100);
			namebuf[99] = '\0';
			CBLAYOUT(namebuf);
		}
		else
			CBLAYOUT(sname);
	}
	else if (do_border) {
		if (sname != 0)
			CBLAYOUT(sname);
		else
			CBLAYOUT("gr_osview");
	}
	else
		CBLAYOUT((char *) 0);
	if (!slave)
		logcblayout(sname);
}

static int
tickpersec(int int1, int int2)
{
   int		tb;

	tb = int1 * int2;
	if (tb <= 10)
		return((int) 10.0 / tb);
	else
		return(1);
}

void
setinitproc(void)
{
   barlist_t		*bp, *tbp;
   colorblock_t		*cbp;

	if (debug)
		fprintf(stderr, "setinitproc called\n");
	expandcheck();
	for (bp = barlist; bp != 0; tbp = bp, bp = bp->s_next, free(tbp)) {
		bp->s_cb = cbp = CBINIT();
		if (bp->s_name)
			cbp->cb_name = strdup(bp->s_name);
		bp->s_cb->cb_flags |= TF_ONETRIP;

		/*
		 * Pre-ops.
		 */
		cbp->cb_type = bardata[bp->s_btype].i_preftype;

		/*
		 * If a strip chart, modify the type.
		 */
		if (bp->s_flags & SF_STRIP) {
			switch (cbp->cb_type) {
			case T_REL:
				cbp->cb_type = T_SREL;
				bp->s_flags &= ~SF_NUM;
				break;
			case T_NUM:
			case T_ABS:
				cbp->cb_type = T_SABS;
				bp->s_flags &= ~SF_NUM;
				break;
			}
			if (bp->s_flags & SF_TICK) {
				cbp->cb_flags |= TF_TICK;
				cbp->cb_tick = bp->s_stevery;
			}
			if (bp->s_flags & SF_BIGTICK) {
				cbp->cb_flags |= TF_BIGTICK;
				cbp->cb_bigtick = bp->s_stbig;
			}
		}

		/*
		 * Numeric display is only valid for absolute number
		 * bars.
		 */
		if (bp->s_flags & SF_NUM) {
			if (cbp->cb_type == T_ABS)
				cbp->cb_type = T_NUM;
			else
				bp->s_flags &= ~SF_NUM;
		}

		/*
		 * Suppress the border if desired.
		 */
		if ((bp->s_flags & SF_NOBORD) || barborder == 0)
			cbp->cb_flags |= TF_NOBORD;

		/*
		 * Set up the scaling limits for an absolute bar, and
		 * lock the scale if requested.
		 */
		if (bp->s_limit == 0)
			cbp->cb_tlimit = bardata[bp->s_btype].i_preflim;
		else
			cbp->cb_tlimit = bp->s_limit;
		if (bp->s_flags & SF_SLOCK)
			cbp->cb_flags |= TF_NOSCALE;

		/*
		 * Set the update interval information.
		 */
		switch (cbp->cb_type) {
		case T_SREL:
		case T_SABS:
			setintervalp(bp, bardata[bp->s_btype].i_prefsintv);
			break;
		case T_NUM:
			setintervalp(bp, bardata[bp->s_btype].i_prefnintv);
			break;
		default:
			setintervalp(bp, bardata[bp->s_btype].i_prefintv);
		}

		/*
		 * Copy critical data into the bar structure.
		 */
		cbp->cb_interval = bp->s_interval;
		cbp->cb_count = bp->s_count;
		cbp->cb_subtype = bp->s_subtype;
		cbp->cb_disk = bp->s_disk;
		cbp->cb_btype = bp->s_btype;

		/*
		 * Do specific setup.
		 */
		(*bardata[bp->s_btype].i_init)(bp);

		/*
		 * Post-ops.
		 */
		switch (cbp->cb_type) {
		case T_REL:
			break;
		case T_SABS:
		case T_ABS:
		case T_NUM:
			if (bp->s_flags & SF_AVERAGE) {
				/* 
				 * Average count over time.
				 */
				cbp->cb_flags |= TF_AVERAGE;
				if (bp->s_average == 0)
					cbp->cb_avgtick = tickpersec(interval,
						bp->s_interval);
				else
					cbp->cb_avgtick = bp->s_average;
				cbp->cb_avgcnt = cbp->cb_avgtick;
			}
			if (bp->s_flags & SF_MAX)
				cbp->cb_flags |= TF_MAX;
			if (cbp->cb_type == T_NUM)
				cbp->cb_flags |= (TF_NUMDISP|TF_REMAX);
			if (cbp->cb_flags & (TF_MAX|TF_NUMDISP)) {
				if (bp->s_max == 0) {
					cbp->cb_maxtick = 0;
					cbp->cb_flags |= TF_MAXRESET;
				}
				else
					cbp->cb_maxtick = bp->s_max;
				cbp->cb_maxcnt = tickpersec(interval,
					bp->s_interval);
			}
			if (bp->s_flags & SF_CREEP)
				cbp->cb_flags |= TF_CREEP;
			if (!(cbp->cb_type == T_SABS))
				break;
		case T_SREL:
			cbp->cb_nsamp = (bp->s_nsamps ? bp->s_nsamps :
				bardata[bp->s_btype].i_prefsamps);
			break;
		}
		setmovep(bp);
		setnumcolors(bp);
		bp->s_draw = bardata[bp->s_btype].i_draw;
	}
	barlist = 0;
}

static void
expandcheck(void)
{
   barlist_t	*bp;

	for (bp = barlist; bp != 0; bp = bp->s_next)
		if (bp->s_btype == bcpu && bp->s_subtype == CPU_ALL)
			expandcpu(bp, ncpu);
		else if (bp->s_btype == bnetip && bp->s_subtype == IP_ALL)
			expandnetif(bp);
}

static void
expandcpu(barlist_t *bp, int cnt)
{
   barlist_t	*nbp;
   barlist_t	*obp;
   int		i;
   int		savebp;

	bp->s_subtype = 0;
	if (barend == bp)
		savebp = 1;
	else
		savebp = 0;
	obp = bp;
	for (i = 1; i < cnt; i++) {
		nbp = newbar();
		*nbp = *bp;
		nbp->s_next = obp->s_next;
		obp->s_next = nbp;
		nbp->s_subtype = i;
		obp = nbp;
	}
	if (savebp)
		barend = nbp;
}

static void
expandnetif(barlist_t *bp)
{
   barlist_t	*nbp;
   barlist_t	*obp;
   int		i;
   int		savebp;

	i = 0;
	while (i < nif && strncmp(nifreq[i].ifr_name, "lo", 2) == 0) i++;
	if (i == nif) {
		bp->s_subtype = IP_SUM;
		return;
	}
	bp->s_subtype = i;
	bp->s_name = strdup(nifreq[i].ifr_name);
	i++;
	if (barend == bp)
		savebp = 1;
	else
		savebp = 0;
	obp = bp;
	for (; i < nif; i++) {
		if (strncmp(nifreq[i].ifr_name, "lo", 2) == 0)
			continue;
		nbp = newbar();
		*nbp = *bp;
		nbp->s_next = obp->s_next;
		obp->s_next = nbp;
		nbp->s_subtype = i;
		nbp->s_name = strdup(nifreq[i].ifr_name);
		obp = nbp;
	}
	if (savebp)
		barend = nbp;
}

# define	XOVER	1.0	/* where we go to instantaneous change */
# define	XUNDER	0.4	

static float
speedarc(int loc) {
	if (loc*interval >= 10)
		return(1.0);
	if ((interval/10.0) > XOVER)
		return(1.0);
	else if ((interval/10.0) < XUNDER)
		return(0.4);
	else
		return(loc);
}

static void
setmovep(barlist_t *bp)
{
   colorblock_t	*cbp = bp->s_cb;
   int		strip = (cbp->cb_type == T_SREL || cbp->cb_type == T_SABS);

	if (bp->s_upmove == 0)
		cbp->cb_upmove = bp->s_upmove = 
			(strip ? SUPMOVE : speedarc(bp->s_interval));
	else
		cbp->cb_upmove = bp->s_upmove;
}

static void
setintervalp(barlist_t *bp, int iv)
{
	if (bp->s_interval == 0) {
		if (iv == 0)
			bp->s_interval = 1;
		else
			bp->s_interval = iv;
	}
	bp->s_cb->cb_interval=bp->s_cb->cb_count=bp->s_count=bp->s_interval;
}

static void
setheader(barlist_t *bp, char *n)
{
   char		mbuf[100];
   colorblock_t	*cbp = bp->s_cb;
   float	x;

	switch (cbp->cb_type) {
	case T_REL:
		strcpy(mbuf, n);
		break;
	case T_ABS:
	case T_NUM:
		x = ((float) interval / 10) * bp->s_interval;
		if (x > 10) {
			x /= 60;
			if (x > 10) {
				x /= 60;
				sprintf(mbuf, "%s %0.1fhour: ", n, x);
			}
			else
				sprintf(mbuf, "%s %0.1fmin: ", n, x);
		}
		else
			sprintf(mbuf, "%s %0.1fsec: ", n, x);
		break;
	case T_SABS:
	case T_SREL:
		x = ((float) interval / 10) * bp->s_interval *
			(bp->s_nsamps ? bp->s_nsamps :
				bardata[bp->s_btype].i_prefsamps);
		if (x > 99) {
			x /= 60;
			if (x > 99) {
				x /= 60;
				sprintf(mbuf, "%s %dhour: ", n, (int) x);
			}
			else
				sprintf(mbuf, "%s %dmin: ", n, (int) x);
		}
		else
			sprintf(mbuf, "%s %dsec: ", n, (int) x);
		break;
	}
	cbp->cb_header = strdup(mbuf);
}

static void
setnumcolors(barlist_t *bp)
{
   colorblock_t		*cp = bp->s_cb;

	if (bp->s_limcol.back == CNULL)
		cp->cb_limcol.back=(limcol.back==CNULL?backcolor:limcol.back);
	else
		cp->cb_limcol.back = bp->s_limcol.back;
	if (bp->s_limcol.front == CNULL)
	    cp->cb_limcol.front=(limcol.front==CNULL?frontcolor:limcol.front);
	else
		cp->cb_limcol.front = bp->s_limcol.front;

	if (bp->s_maxcol.back == CNULL)
		cp->cb_maxcol.back=
			(maxcol.back==CNULL?backcolor:maxcol.back);
	else
		cp->cb_maxcol.back = bp->s_maxcol.back;
	if (bp->s_maxcol.front == CNULL)
	    cp->cb_maxcol.front=
		(maxcol.front==CNULL?frontcolor:maxcol.front);
	else
		cp->cb_maxcol.front = bp->s_maxcol.front;

	if (bp->s_sumcol.back == CNULL)
		cp->cb_sumcol.back=(sumcol.back==CNULL?backcolor:sumcol.back);
	else
		cp->cb_sumcol.back = bp->s_sumcol.back;
	if (bp->s_sumcol.front == CNULL)
	    cp->cb_sumcol.front=(sumcol.front==CNULL?frontcolor:sumcol.front);
	else
		cp->cb_sumcol.front = bp->s_sumcol.front;
}
