/* USMID @(#)tcp/usr/etc/nettest/nettest.h	80.2	11/03/92 17:12:29 */

/*
 * Copyright 1992 Cray Research, Inc.
 * All Rights Reserved.
 */
/*
 * Permission to use, copy, modify and distribute this software, in
 * source and binary forms, and its documentation, without fee is
 * hereby granted, provided that:  1) the above copyright notice and
 * this permission notice appear in all source copies of this
 * software and its supporting documentation; 2) distributions
 * including binaries display the following acknowledgement:  ``This
 * product includes software developed by Cray Research, Inc.'' in
 * the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of
 * this software; 3) the name Cray Research, Inc. may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission; 4) the USMID revision line and
 * binary copyright notice are retained without modification in all
 * source and binary copies of this software; 5) the software is
 * redistributed only as part of a bundled package and not as a
 * separate product (except that it may be redistibuted separately if
 * if no fee is charged); and 6) this software is not renamed in any
 * way and is referred to as Nettest.
 *
 * THIS SOFTWARE IS PROVIDED AS IS AND CRAY RESEARCH, INC.
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL CRAY RESEARCH, INC. BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#define	CHUNK	4096
#define	NCHUNKS	100

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/times.h>
#if	defined(CRAY) || defined(SYSV) || defined(sgi)
#include <sys/param.h>
#else
#define	HZ	60
#include <sys/timeb.h>
#endif	/* CRAY */
#define	UNIXPORT	"nt_socket"
#define	UNIXDPORT	"nt_dsocket"
#define	PIPENAME	"nt_npipe"
#define	PORTNUMBER	(IPPORT_RESERVED + 42)
