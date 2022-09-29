/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	flags.h    command line flags structure definition
 *
 *  SCCS
 *
 *	@(#)flags.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	#include "flags.h"
 *
 *  DESCRIPTION
 *
 *	Rather than make all flags global, they are collected into
 *	a single structure which is then made global.  This simplifies
 *	the bookkeeping.
 */


struct cmd_flags {		/* Flags set on command line */
    BOOLEAN aflag;		/* -a option given */
    BOOLEAN Aflag;		/* -A option given */
    BOOLEAN Acflag;		/* -A option with c arg */
    BOOLEAN Aiflag;		/* -A option with i arg */
    BOOLEAN Arflag;		/* -A option with r arg */
    BOOLEAN Asflag;		/* -A option with s arg */
    BOOLEAN bflag;		/* -b option given */
    BOOLEAN Bflag;		/* -B option given */
    BOOLEAN cflag;		/* -c option given */
    BOOLEAN Cflag;		/* -C option given */
    int dflag;			/* -d option given (multilevel) */
    BOOLEAN Dflag;		/* -D option given */
    BOOLEAN eflag;		/* -e option given */
    BOOLEAN fflag;		/* -f option given */
    BOOLEAN Fflag;		/* -F option given */
    BOOLEAN gflag;		/* -g option given */
    BOOLEAN hflag;		/* -h option given */
    BOOLEAN iflag;		/* -i option given */
    BOOLEAN jflag;		/* -j option given */
    BOOLEAN lflag;		/* -l option given */
    BOOLEAN Kflag;		/* -K option given */
    BOOLEAN Lflag;		/* -L option given */
    BOOLEAN mflag;		/* -m option given */
    BOOLEAN nflag;		/* -n option given */
    BOOLEAN oflag;		/* -o option given */
    BOOLEAN pflag;		/* -p option given */
    BOOLEAN Rflag;		/* -R option given */
    BOOLEAN Sflag;		/* -S option given */
    BOOLEAN sflag;		/* -s option given */
    BOOLEAN tflag;		/* -t option given */
    BOOLEAN Tflag;		/* -T option given */
    BOOLEAN uflag;		/* -u option given */
    BOOLEAN ubflag;		/* -u option with b arg */
    BOOLEAN ucflag;		/* -u option with c arg */
    BOOLEAN udflag;		/* -u option with d arg */
    BOOLEAN ulflag;		/* -u option with l arg */
    BOOLEAN upflag;		/* -u option with p arg */
    BOOLEAN urflag;		/* -u option with r arg */
    int vflag;			/* -v option given (multilevel verbosity) */
    BOOLEAN wflag;		/* -w option given */
    BOOLEAN xflag;		/* -x option given */
    BOOLEAN Xflag;		/* -X option given */
    BOOLEAN Zflag;		/* -Z option; use file compression */
};
