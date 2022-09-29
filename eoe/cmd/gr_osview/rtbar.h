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

# ifndef _RTBAR_H
# define _RTBAR_H

# include	<gl.h>

/*
 * Color bar control block.
 */

# define	MAXSECTS	10	/* maximum sections in a bar */
# define	MAXBARS		20	/* maximum bars in a window */
# define	MAXCPU		256	/* maximum CPU's we can handle */
# define	MAXDISK		40	/* maximum disks we can handle */
# define	mask(i)		(1<<i)

/*
 * Numeric color description type.
 */

typedef struct {
	Colorindex	back;
	Colorindex	front;
} numcol_t;

/*
 * One of these defines a single "real-time" bar.
 */
# define	T_REL		1	/* relative bar */
# define	T_ABS		2	/* absolute bar */
# define	T_SABS		3	/* stripchart bar */
# define	T_SREL		4	/* stripchart bar */
# define	T_NUM		5	/* numeric bar */

# define	TF_NOBORD	01	/* no bar border */
# define	TF_MAX		02	/* display max value */
# define	TF_REMAX	04	/* max value changed */
# define	TF_NUMDISP	010	/* some numeric changed */
# define	TF_WHERE	020	/* scale change in strip */
# define	TF_NOSCALE	040	/* freeze scaling */
# define	TF_LIMFLT	0100	/* display limit as float */
# define	TF_SCHANGE	0400	/* scale change */
# define	TF_TICK		01000	/* add tick marks to strip chart */
# define	TF_BIGTICK	02000	/* add big tick marks to chart */
# define	TF_AVERAGE	04000	/* compute averages */
# define	TF_STRDR	010000	/* no rectcopy, please */
# define	TF_CREEP	020000	/* scale only increases */
# define	TF_MBSCALE	040000	/* scale factor is in Meg */
# define	TF_EXCEED	0100000	/* larger than locked scale */
# define	TF_MAXRESET	0200000	/* clear max after startup */
# define	TF_ONETRIP	0400000	/* don't draw first sample */

typedef struct colorblock_s {
	/*
	 * Stuff caller has to set up for us.
	 */
	int	cb_flags;		/* control flags */
	int	cb_type;		/* block type */

	float	cb_tlimit;		/* absolute bar limit */
	float	cb_upmove;		/* maximum move */

	int	cb_nsects;		/* number of sections in bar */
	int	cb_nmask;		/* mask of interesting values */

	int	cb_avgtick;		/* ticks for average update */
	int	cb_avgcnt;		/* countdown ticks on average */
	int	cb_maxtick;		/* ticks for max update */
	int	cb_maxcnt;		/* countdown ticks on max */

	int	cb_nsamp;		/* strip chart initializer */

	int	cb_tick;		/* every tick on strip chart */
	int	cb_bigtick;		/* every big tick on strip chart */

	numcol_t	cb_limcol;	/* limit colors */
	numcol_t	cb_maxcol;	/* max value colors */
	numcol_t	cb_sumcol;	/* sum colors */

	int	cb_colors[MAXSECTS];	/* colors of the pieces */
	float	cb_last[MAXSECTS];	/* normalized display values */

	/*
	 * Fields duplicated from the barinfo structure when reading an
	 * export file or restarting a remote.
	 */
	int	cb_interval;
	int	cb_count;
	int	cb_subtype;
	int	cb_disk;
	int	cb_btype;
	char	*cb_info;
	int	cb_infolen;
	char	*cb_name;

	/*
	 * Stuff that is set up, but doesn't need to go across the wire.
	 */
	char	*cb_header;		/* bar header */
	char	*cb_legend[MAXSECTS];	/* legend for each section */

	/*
	 * Stuff set up internally.
	 */
	char	cb_valid;		/* bar is valid */
	Matrix	cb_matrix;		/* viewing matrix */
	float	cb_cposx;		/* x character screen position */
	float	cb_cposy;		/* y character screen position */
	float	cb_cposz;		/* z character screen position */
	float	cb_cposmid;		/* middle character position */
	float	cb_cposbot;		/* bottom character position */
	float	cb_cpostop;		/* top of character positions */
	int	cb_twid;		/* string area # pixels */
	float	cb_ftwid;		/* string area world/pixel in X */
	float	cb_ftsywid;		/* string area world/pixel in Y */
	float	cb_ftywid;		/* world/pixel in Y */
	float	cb_ftxwid;		/* world/pixel in X */
	float	cb_strwid;		/* string area world length */
	float	cb_hdlen;		/* header length in world space */
	float	cb_hdstart;		/* post-header part */
	float	cb_cmaxx;		/* x position of max space */
	float	cb_cmwid;		/* length of max space */
	char	*cb_mess;		/* overlay message */
	float	cb_xwhere;		/* x bar base */
	float	cb_ywhere;		/* y bar base */
	int	cb_curscale;		/* current scale value */
	int	cb_rb;			/* remote bar */
	float	cb_pixcross;
	float	cb_max1;
	float	cb_dispavg1;
	float	cb_lastavg1;
	float	cb_dispmax;
	float	cb_results[MAXSECTS];	/* real last values */
	float	cb_max[MAXSECTS];	/* maximum value seen */
	int	cb_amax[MAXSECTS];
	float	cb_avg[MAXSECTS];	/* average value counts */
	float	cb_aspect;		/* aspect ratio */
	struct strip_s	*cb_stp;	/* strip chart data */
	int	cb_nbelow;		/* for use by clamping function */
	int	cb_nabove;		/* for use by clamping function */
	Screencoord	cb_scrmask[4];	/* screen mask */
} colorblock_t;

/*
 * Strip-chart sublock.
 */
typedef struct strip_s {
	int	st_nsamples;		/* number of samples */
	float	*st_samples;		/* array of sample values */
	int	st_cursamp;		/* current sample in buffer */
	int	st_abswhere;		/* last scale change */
	int	st_tickcnt;		/* tick counter */
	int	st_bigtickcnt;		/* big tick counter */
	float	st_avg[MAXSECTS];	/* average for too many samples */
	int	st_avgcnt;		/* samples in average */
	char	st_forcetick;		/* for serious stripcharting */
	Screencoord	st_irect[4];		/* initial rectangle */
	Screencoord	st_fpos[2];		/* final rectangle */
} strip_t;

/*
 * Colormap management stuff.
 */
# define	heavy_c		0
# define	bad_c		1
# define	waste_c		2
# define	unused_c	3
# define	light_c		4
# define	inuse_c		5
# define	DEFBACKCOLOR	46
# define	DEFPALECOLOR	54

# endif	/* RTBAR_H */
