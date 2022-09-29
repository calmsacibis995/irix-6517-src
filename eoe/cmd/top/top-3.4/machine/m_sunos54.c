/*
 * top - a top users display for Unix
 *
 * SYNOPSIS:  Any Sun running SunOS 5.4 (Solaris 2.4) or later.
 *
 * DESCRIPTION:
 * This is the machine-dependent module for SunOS 5.4 and 5.5.
 * There is some support for MP architectures.
 * This makes top work on the following systems:
 *         SunOS 5.4
 *         SunOS 5.5
 *
 * Tested on a SS20/61 w/ SunPRO C 2.0.1 running Solaris 2.4/SPARC and
 * a P90 running Solaris 2.4/x86.
 *
 * LIBS: -lkvm -lelf -lkstat
 *
 * Need -I. for all the top include files which are searched for in machine/,
 * because of the way include "file" finds files.
 *
 * CFLAGS: -I. -DHAVE_GETOPT -DORDER -DSOLARIS24
 *
 * AUTHORS:      Torsten Kasch 		<torsten@techfak.uni-bielefeld.de>
 *               Robert Boucher		<boucher@sofkin.ca>
 * CONTRIBUTORS: Marc Cohen 		<marc@aai.com>
 *               Charles Hedrick 	<hedrick@geneva.rutgers.edu>
 *	         William L. Jones 	<jones@chpc>
 *               Petri Kutvonen         <kutvonen@cs.helsinki.fi>
 *	         Casper Dik             <casper@fwi.uva.nl>
 */
#include "machine/m_sunos5.c"
