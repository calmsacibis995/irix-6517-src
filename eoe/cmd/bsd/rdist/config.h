/*
 * Copyright (c) 1993 Michael A. Cooper
 * Copyright (c) 1993 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: config.h,v 1.3 1995/09/22 07:37:09 ack Exp $
 * @(#)config.h
 */

#ifndef __config_h__
#define __config_h__

/*
 * Configuration parameters
 */

#include OS_H

/*
 * Include system pathname header file.  Usually this is <paths.h>.  
 * If your system doesn't have such a file, use "paths.h"
 */
#if	defined(PATHS_H)
#	include PATHS_H
#else
#	include "paths.h"
#endif

/*
 * Define _PATH_OLDRDIST to be the name of the original rdist that
 * was distributed with 4.3BSD.  
 * 
 * If you want to be backwards compability with the old rdist, uncomment
 # the "#define" line.  If you don't want to be backwards compability or 
 * don't have the old rdist, then uncomment the "#undef" line.
 */
#define _PATH_OLDRDIST	"/usr/bsd/ordist"	/* Enable compat */
/*#undef  _PATH_OLDRDIST*/				/* Disable compat */

/*
 * Check to see if file is on a NFS.  If it is, the file is
 * skipped unless the hostname specified in the Distfile has
 * a trailing "+".  e.g. "foobar+".  This feature is enabled by
 * the -N option.  If your system does not support NFS or you don't
 * want the -N option, undefine this.
 */
#define NFS_CHECK

/*
 * Check to see if file on a Read-Only filesystem.  If it is, no
 * attempt is made to update the file.  This feature is enabled by
 * the -O option.
 */
#define RO_CHECK

/*
 * Default value for the maximum number of clients to update at once.
 * Can be changed with the -M option.
 */
#define MAXCHILDREN 	4

/*
 * Response Time Out interval (in seconds).
 * Should be long enough to allow transfer of large files.
 * The -t option can be used to override this value.
 */
#define RTIMEOUT 	900

/*
 * Define LOG_OPTS to be the syslog/openlog() logging options you
 * wish to use.  Define to be 0 if you don't want any options.
 * Define LOG_FACILITY to be the syslog/openlog() facility to log
 * to.  Both LOG_OPTS and LOG_FACILITY values are defined in <syslog.h>
 * If you don't have syslog, then undefine both values.
 */
#define LOG_OPTS		LOG_PID
#if	defined(LOG_DAEMON)
#	define LOG_FACILITY	LOG_DAEMON
#endif

/*
 * Syslog levels.  Define these to match the levels you want to log
 * via syslog().  These are defined in <syslog.h>.  If you don't want
 * a particuliar level logged _ever_, undefine it.  What is logged is
 * usually controlled via command line options, so you normally should
 * not need to undefine these.
 */
#define SL_FERROR	LOG_INFO		/* Fatal errors */
#define SL_NERROR	LOG_INFO		/* Normal errors */
#define SL_WARNING	LOG_INFO		/* Warnings */
#define SL_CHANGE	LOG_INFO		/* Things that change */
#define SL_INFO		LOG_INFO		/* General info */
#define SL_NOTICE	LOG_NOTICE		/* General notices */
#define SL_DEBUG	LOG_DEBUG		/* Debugging */

#endif	/* __config_h__ */
