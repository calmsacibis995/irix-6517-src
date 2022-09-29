/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module machtype.h, release 1.43 dated 96/07/30
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.machtype.h
 *sccs
 *sccs    1.43	96/07/30 15:39:16 sshanbha
 *sccs	PEER_SOLARIS2_5_PORT needs PEER_SIGNAL_INT_PARM with c++ compilers (PR#733)
 *sccs
 *sccs    1.42	96/07/25 15:11:20 sshanbha
 *sccs	Added PEER_SOLARIS2_5_PORT for solaris 2.5 (PR#733)
 *sccs
 *sccs    1.41	96/07/12 14:25:24 sshanbha
 *sccs	Define some additional flags for solaris port (PR#733) (PR#748)
 *sccs
 *sccs    1.40	96/06/07 18:17:40 sshanbha
 *sccs	Updated flags to allow compilation on dgux 5.4 (PR#645)
 *sccs
 *sccs    1.39	96/06/07 16:25:06 sshanbha
 *sccs	Defined some flags to allow compilation on DGUX 5.4 (PR#645)
 *sccs
 *sccs    1.38	96/06/06 11:54:16 sshanbha
 *sccs	Added PEER_NO_STATIC_FORWARD_DECL flag for DEC alpha ports
 *sccs
 *sccs    1.37	96/06/04 17:06:23 sshanbha
 *sccs	gcc on HPUX requires definition of PEER_NO_STATIC_FORWARD_DECL (PR#730)
 *sccs
 *sccs    1.36	96/05/30 11:19:00 rpresuhn
 *sccs	added preprocessor handling of incorrect sigaction declarations, solaris and irix (PR#730) (PR#732) (PR#733)
 *sccs
 *sccs    1.35	96/05/29 19:25:45 rpresuhn
 *sccs	added PEER_FDSET_IS_INT to support HPUX 10 (PR#730)
 *sccs
 *sccs    1.34	96/04/30 14:54:56 rpresuhn
 *sccs	added PEER_NEED_TIME_H for systems that need it (PR#388) (PR#660)
 *sccs
 *sccs    1.33	96/04/30 14:32:26 rpresuhn
 *sccs	added PEER_GETOWNIP_HAS_MEMORY to assist performance optimization (PR#689)
 *sccs
 *sccs    1.32	96/03/21 10:28:29 santosh
 *sccs	Added PEER_NO_STATIC_FORWARD_DECL flag for AIX port (PR#694)
 *sccs
 *sccs    1.31	96/02/12 13:16:09 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.30	95/12/15 16:12:16 sam
 *sccs	Minor additions for the OS/2 port (PR#619)
 *sccs
 *sccs    1.29	95/10/26 14:59:55 santosh
 *sccs	Added missing #endif
 *sccs
 *sccs    1.28	95/10/26 14:52:17 santosh
 *sccs	Added PEER_NO_STATIC_FORWARD_DECL for c++ compilers (PR#170)
 *sccs
 *sccs    1.27	95/09/15 10:22:30 sam
 *sccs	Clean up ifdefs for READONLY and USE_PROTOTYPES
 *sccs
 *sccs    1.26	95/05/26 12:24:27 santosh
 *sccs	Added PEER_NO_STATIC_FORWARD_DECL flag. (PR#362)
 *sccs
 *sccs    1.25	95/02/06 16:18:47 sam
 *sccs	Modified to force PEER_WINSOCK_PORT to define USE_PROTOTYPES and READONLY. See PR 159,161,162 and 205
 *sccs
 *sccs    1.24	95/01/24 13:00:28 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.23	95/01/18 12:40:06 randy
 *sccs	default to NOT use getdomainname (PR#32), change address (PR#183)
 *sccs
 *sccs    1.22	95/01/16 16:20:09 randy
 *sccs	verified 64-bit support on Alpha (PR#173)
 *sccs
 *sccs    1.21	94/11/30 17:47:58 randy
 *sccs	main is not static under windows (PR#141)
 *sccs
 *sccs    1.20	94/11/02 11:20:21 randy
 *sccs	added BSDI port construct
 *sccs
 *sccs    1.19	94/07/11 16:55:28 randy
 *sccs	added Winsock port
 *sccs
 *sccs    1.18	94/06/07 12:23:37 randy
 *sccs	added hooks for acer port
 *sccs
 *sccs    1.17	94/05/25 11:52:52 randy
 *sccs	support for API tracing
 *sccs
 *sccs    1.16	94/05/03 22:34:24 randy
 *sccs	VRTX port
 *sccs
 *sccs    1.15	94/03/24 14:00:09 randy
 *sccs	added support for SCO
 *sccs
 *sccs    1.14	93/11/02 15:44:00 randy
 *sccs	support for NCR environment
 *sccs
 *sccs    1.13	93/10/26 15:44:30 randy
 *sccs	added manifests to control compilation with Sequent
 *sccs
 *sccs    1.12	93/06/02 22:13:26 randy
 *sccs	added include paths for AIX
 *sccs
 *sccs    1.11	93/05/27 22:45:23 randy
 *sccs	tolerate version 7 of microsoft compiler
 *sccs
 *sccs    1.10	93/04/26 18:14:33 randy
 *sccs	psos support
 *sccs
 *sccs
 *sccs    1.9	93/04/15 22:04:31 randy
 *sccs	support compilation with g++
 *sccs
 *sccs
 *sccs    1.8	93/03/05 09:34:42 randy
 *sccs	added manifests for target-specific versions
 *sccs
 *sccs    1.7	92/11/12 16:33:37 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.6	92/10/26 22:48:55 randy
 *sccs	added support for non-volatile configuration
 *sccs
 *sccs    1.5	92/10/23 15:28:52 randy
 *sccs	final cosmetics for release 1.5
 *sccs
 *sccs    1.4	92/09/11 19:08:16 randy
 *sccs	merged DOS/Unix versions
 *sccs
 *sccs    1.3	92/01/21 10:48:18 randy
 *sccs	added dependency for sys/time.h
 *sccs
 *sccs    1.2	91/12/30 13:59:16 randy
 *sccs	added VOID control for dos port
 *sccs
 *sccs    1.1	91/10/25 14:57:55 randy
 *sccs	date and time created 91/10/25 14:57:55 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef MACHTYPEH			/* avoid recursion		*/
#define MACHTYPEH

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


/*
 *	The following manfests are used to generate platform-
 *	specific versions from the master release.
 *
 *		PEER_GENERIC_PORT	- SunOs 4.1.1 / standard product
 *		PEER_ACER_PORT		- ported to Acer/Altos
 *		PEER_AIX_PORT		- ported to AIX
 *		PEER_BSDI_PORT		- ported to 4.3 BSD BSDI BSD386 v. 1.1
 *		PEER_DGUX_PORT		- ported to HPUX 5.4
 *		PEER_HPUX_PORT		- ported to HPUX 10.x
 *		PEER_IRIX_PORT		- ported to IRIX 5.3
 *		PEER_LYNX_PORT		- ported to Lynx
 *		PEER_MSDOS_PORT		- MS-DOS TSR version
 *		PEER_NCR_PORT		- ported to NCR
 *		PEER_OS2_PORT		- ported to os/2
 *		PEER_PSOS_PORT		- ported to PSOS
 *		PEER_QNX_PORT		- ported to QNX
 *		PEER_SCO_PORT		- ported to SCO
 *		PEER_SEQUENT_PORT	- ported to Dynix
 *		PEER_SOLARIS_PORT	- ported to Solaris 2.4
 *		PEER_SOLARIS2_5_PORT	- ported to Solaris 2.5
 *		PEER_VRTX_PORT		- ported to VRTX
 *		PEER_VXWORKS_PORT	- ported to VxWorks
 *		PEER_WINSOCK_PORT	- ported to windows with winsock
 */


/*
 *	the following manifests, provide finer control over the process:
 *
 *	Packet Tracing:
 *		SMUX_SERVER_TRACE - define if you want the server (agent)
 *			to print out information about all SMUX packets
 *		SMUX_CLIENT_TRACE - define if you want the client to
 *			print out information about all SMUX packets
 *		SNMP_PACKET_TRACE - define if you want a trace of all
 *			snmp packet activity
 *		UDP_TRACE - display all datagram activity
 *		UDP_READ_TRACE - display all incoming datagram activity
 *		UDP_WRITE_TRACE - display all outgoing datagram activity
 *		TCP_TRACE - display all TCP activity
 *		TCP_READ_TRACE - display all incoming TCP activity
 *		TCP_WRITE_TRACE - display all outgoing TCP activity
 *
 *	Capability Control:
 *		SNMPV2_SUPPORTED - define if SNMPv2 syntaxes are supported
 *		MD5_SUPPORTED - define if MD5 authentication is supported
 *		DES_SUPPORTED - define is DES encryption is used
 *		NON_VOLATILE_AVAILABLE - define if you want to support non-
 *			volatile agent configuration parameters
 *		ALLOW_NCR_SMUX - allow NCR's peculiar SMUX variant
 *		SMUX_KEEPALIVES - to request that keepalives be used over
 *			SMUX connections
 *		ALLOW_MANGLED_SOUT - allow Acer's mangled SOUT pdus in SMUX
 *
 *	Machine Peculiarities:
 *		BIT64 - define if there is native hardware support for
 *			64-bit integers
 *		LITTLE_ENDIAN - define if this is a VAX / 80X86-ish
 *			machine which requires integers be stored backwards
 *
 *	Compiler Oddities:
 *		VOID_NOT_SUPPORTED - define if the (void *) idiom is not
 *			understood by this compiler (e.g., Microsoft 3.0)
 *		USE_PROTOTYPES - defined if ANSI-style prototypes are allowed
 *			by your compiler
 *
 *	Environmental Characteristics:
 *		NEED_OWN_TIME - define if the local <sys/time.h> is not
 *			appropriate for defining struct timeval.
 *		PEER_NEED_TIME_H - define is <time.h> is needed (in addition
 *			to the usual <sys/time.h>
 *		PEER_GETDOMAINNAME_MISSING - the function getdomainname() is
 *			not available for this target system or not appropriate
 *			(this function returns an NIS domain name, which may
 *			be inappropriate for constructing the ip host name)
 *		PEER_GETPROTOBYNAME_MISSING - the function getprotobyname() is
 *			not available for this target system
 *		PEER_GETSERVBYNAME_MISSING - the function getservbyname() is
 *			not available for this target system
 *		PEER_FDSETSIZE_IS_MAXFILES - the sysconf() function or ulimit()
 *			equivalent is absent or not working.  We rely on the
 *			size of the fd_set data type as an estimate of the
 *			how many file descriptors should be our maximum for
 *			invocations of the select() system call.
 *		PEER_FDSET_IS_INT - references to fd_set * need to be int *
 *		PEER_SIGNALS_MISSING - the sigaction / signal support is
 *			missing or not usable
 *		PEER_SIGNAL_INT_RETURN - signal handlers return int instead
 *			of the usual void
 *		PEER_SIGNAL_VOID_PARM - signal handlers have no parameters
 *			instead of the usual (int, ...)
 *		PEER_MAIN_STATIC - the "main" function has to be static
 *			in many embedded systems with shared symbol spaces
 *		BLOCKING_CONNECT - connect needs to be done with O_NDELAY
 *		PEER_GETOWNIP_HAS_MEMORY - reduces cost of repeated calls to
 *			getownip().
 *		PEER_NO_STATIC_FORWARD_DECL - no forward declaration allowed 
 *			for static functions.
 *
 *	Debug Tracing:
 *		PEER_MGMT_API_TRACE - trace invocation of mgmt_* functions
 *			in the common/ode library
 */


/*
 *	If this is an ANSI compiler (__STDC__ defined and non-zero) or if
 *	told to use prototypes through -D definitions, we'll use them.
 *      Also the same with 'const', some early almost ansi compilers
 *      support const but not prototyes or vice versa.
 *
 *	The only unusual case is that of the Microsoft compilers.
 *      If you enable the MS language extensions for Windows or Win32
 *      then, since they are not strictly ansi, they do not define
 *	(/Ze ) __STDC__ even though the language is a superset, so we do an
 *      explicit check for MSDOS.
 *
 *	Some very old compilers have this whole area bolluxed up.
 *      E.g MS compiler 3.x defines __STDC__, but gives it a value of 0.
 *      (and it was not even close to ansi c).
 *	If this is the case, just add an #undef for one or both of these
 *      as necessary.
 *
 *      Note that there are two versions of this file, one for the
 *      compiler and one for the rest of the code. This is to provide
 *      for the cross compilation case. You can have different machine
 *      options for the compiler executable and the agent etc.
 *      If you make changes to this file, be sure to check on the one
 *      in the ..\gdmoc\ directory also.
 */

#ifndef USE_PROTOTYPES
#ifdef __STDC__                 /* a real ANSI compiler	 */
#define USE_PROTOTYPES
#else
#ifdef MSDOS                    /* probably microsoft		*/
#define USE_PROTOTYPES
#endif /* MSDOS */
#endif /* __STDC__ */
#ifdef __cplusplus
#ifndef USE_PROTOTYPES
#define USE_PROTOTYPES
#endif
#endif /* __cplusplus */
#endif	/* USE_PROTOTYPES */

#ifndef READONLY
#ifdef __STDC__                 /* a real ANSI compiler	 */
#define READONLY	const
#else
#ifdef MSDOS                    /* probably microsoft		*/
#define READONLY	const
#else
#define READONLY		/* clearly not ansi		*/
#endif /* MSDOS */
#endif /* __STDC__ */
#ifdef __cplusplus
#ifndef READONLY
#define READONLY const
#endif
#endif /* __cplusplus */
#endif /* READONLY */


/*
 *	If this is C++, we'll need to mutate certain externs to get access
 *	to the ordinary C library.
 */
#ifndef C_EXTERN
#ifdef __cplusplus
#define C_EXTERN	extern "C"
#define EXTERN_BEGIN	extern "C" {
#define EXTERN_END	}
#else
#define C_EXTERN	extern
#define EXTERN_BEGIN
#define EXTERN_END
#endif
#endif

/* 
 * For C++ compilers like g++ and CC, forward declarations are allowed only 
 * for non-static functions.
 */
#ifndef PEER_NO_STATIC_FORWARD_DECL 
#ifdef __cplusplus
#define PEER_NO_STATIC_FORWARD_DECL
#endif
#endif

/*
 *	For all targets, disable the use of getdomainname().  If you really
 *	want it, delete the following three lines, and it will be disabled
 *	only on the systems that lack the capability,
 */
#ifndef PEER_GETDOMAINNAME_MISSING
#define PEER_GETDOMAINNAME_MISSING
#endif

/*
 *	MS-DOS is an environment where we need to supply a few of the pieces
 *	ourselves.  The key ones relevant here are:
 *		- yywrap
 *		- time of day support
 *	This structure assumes that if we are compiling under DOS, we are
 *	going for a DOS target.	 If this is not so, remove the PEER_MSDOS_PORT
 *	definition.
 */
#ifdef MSDOS
#define NEED_OWN_TIME
#define LITTLE_ENDIAN
#define PEER_MSDOS_PORT
#ifndef PEER_GETSERVBYNAME_MISSING
#define PEER_GETSERVBYNAME_MISSING
#endif
#ifndef PEER_NO_STATIC_FORWARD_DECL  
#define PEER_NO_STATIC_FORWARD_DECL 
#endif
#endif

#ifdef _MSDOS
#define NEED_OWN_TIME
#define LITTLE_ENDIAN
#define PEER_MSDOS_PORT
#ifndef PEER_GETSERVBYNAME_MISSING
#define PEER_GETSERVBYNAME_MISSING
#endif
#endif

/*
 *	Do we provide basic support for non-volatile storage of read-write
 *	parameters in the system and snmp groups?
 */
#ifndef PEER_MSDOS_PORT
#define NON_VOLATILE_AVAILABLE
#endif


/*
 *	The windows environment makes it look like we're a dos port up to
 *	this point.  Here we make corrections.
 */
#ifdef PEER_WINSOCK_PORT
#ifdef PEER_MSDOS_PORT
#undef PEER_MSDOS_PORT
#endif
#ifdef NEED_OWN_TIME
#undef NEED_OWN_TIME
#endif
#ifndef PEER_FDSETSIZE_IS_MAXFILES
#define PEER_FDSETSIZE_IS_MAXFILES
#endif
#ifndef PEER_GETDOMAINNAME_MISSING
#define PEER_GETDOMAINNAME_MISSING
#endif
#ifndef PEER_SIGNALS_MISSING
#define PEER_SIGNALS_MISSING
#endif
#ifndef BLOCKING_CONNECT
#define BLOCKING_CONNECT
#endif
/*
 *	Since __STDC__ is not defined in the windows environment (windows
 *	requires some MS extensions) we must make sure that the function
 *      prototypes and const are used.
 */
#ifndef USE_PROTOTYPES
#define USE_PROTOTYPES
#endif
#ifndef READONLY
#define READONLY	const
#endif

#endif


/*
 *	An Acer port is really just a SCO port, but with mangled SOUT PDUs
 */
#ifdef PEER_ACER_PORT
#ifndef PEER_SCO_PORT
#define PEER_SCO_PORT
#endif
#ifndef ALLOW_MANGLED_SOUT
#define ALLOW_MANGLED_SOUT
#endif
#endif


/*
 *	If this is an alpha (and we are not targetting something else) make
 *	note of the native 64-bit capability.
 */
#ifdef __alpha
#ifndef PEER_NO_STATIC_FORWARD_DECL
#define PEER_NO_STATIC_FORWARD_DECL
#endif
#ifdef PEER_GENERIC_PORT
#ifndef BIT64
#define BIT64
#endif
#endif
#endif


/*
 *	If this is HPUX 10.x, it's a generic port except for the following:
 *		- the declaration of select in /usr/include/sys/time.h 
 *		  uses the incorrect type for parameters that should be 
 *		  pointers to fd_set.
 * 	Also, gcc does not like forward declarations of static functions.
 */
#ifdef PEER_HPUX_PORT
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef PEER_FDSET_IS_INT
#define PEER_FDSET_IS_INT
#endif

	/*
	 *	The following horrible condition is from the HPUX signal.h
	 *	and determines whether signal handlers have an int parameter
	 *	or none at all.
	 */
#if defined(__STDC__) && !defined(_INCLUDE_HPUX_SOURCE) || defined(__cplusplus)
#else
#define PEER_SIGNAL_VOID_PARM
#endif

#ifndef PEER_NO_STATIC_FORWARD_DECL
#define PEER_NO_STATIC_FORWARD_DECL
#endif

#endif


/*
 *	If this is for SGI IRIX, we'll need a few other manifests...
 *		- PEER_GENERIC_PORT, since this is relatively generic
 *		- PEER_SIGNAL_VOID_PARM, since the system headers are hosed
 */
#ifdef PEER_IRIX_PORT
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef PEER_SIGNAL_VOID_PARM
#define PEER_SIGNAL_VOID_PARM
#endif
#endif


/*
 *	If this is for Sun Solaris, we'll need a few other manifests...
 *		- PEER_NO_STATIC_FORWARD_DECL, since forward declaration
 *					      for statics is not allowed
 *		-  __EXTENSIONS__, to define structures such as sigaction
 *		- PEER_GENERIC_PORT, since this is relatively generic
 *		- PEER_SIGNAL_VOID_PARM, since the system headers are hosed
 */
#ifdef PEER_SOLARIS_PORT
#ifndef PEER_NO_STATIC_FORWARD_DECL
#define PEER_NO_STATIC_FORWARD_DECL
#endif
#ifndef __EXTENSIONS__
#define __EXTENSIONS__
#endif
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef PEER_SIGNAL_VOID_PARM
#define PEER_SIGNAL_VOID_PARM
#endif
#endif

/* If this is for Sun Solaris 2.5, we'll need a few other manifests...
 *		- PEER_NO_STATIC_FORWARD_DECL, since forward declaration
 *					      for statics is not allowed
 *		-  __EXTENSIONS__, to define structures such as sigaction
 *		- PEER_GENERIC_PORT, since this is relatively generic
 *		- PEER_SIGNAL_INT_PARM, since the system headers are hosed
 */ 

#ifdef PEER_SOLARIS2_5_PORT
#ifndef PEER_NO_STATIC_FORWARD_DECL
#define PEER_NO_STATIC_FORWARD_DECL
#endif
#ifndef __EXTENSIONS__
#define __EXTENSIONS__
#endif
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef PEER_SIGNAL_INT_PARM
#define PEER_SIGNAL_INT_PARM
#endif
#endif

/*
 *	If this is a VxWorks port, we'll need a few other manifests...
 *		From the last version of VxWorks we saw:
 *		- shared symbol tables mean main() should be private
 *		- time support was missing
 *		- getdomainname() was missing 
 *		- sysconf() and ulimit() were missing 
 *		- getservbyname() was missing
 *		- getprotobyname() was missing
 *		- signal support was missing
 *	Since the VxWorks backplane protocol had problems detecting connection
 *	loss in the last version we saw, default to using TCP keepalives on
 *	SMUX sessions.
 */
#ifdef PEER_VXWORKS_PORT
#ifndef PEER_MAIN_STATIC
#define PEER_MAIN_STATIC
#endif
#ifndef NEED_OWN_TIME
#define NEED_OWN_TIME
#endif
#ifndef PEER_GETDOMAINNAME_MISSING
#define PEER_GETDOMAINNAME_MISSING
#endif
#ifndef PEER_FDSETSIZE_IS_MAXFILES
#define PEER_FDSETSIZE_IS_MAXFILES
#endif
#ifndef PEER_GETSERVBYNAME_MISSING
#define PEER_GETSERVBYNAME_MISSING
#endif
#ifndef PEER_GETPROTOBYNAME_MISSING
#define PEER_GETPROTOBYNAME_MISSING
#endif
#ifndef PEER_SIGNALS_MISSING
#define PEER_SIGNALS_MISSING
#endif
#ifndef SMUX_KEEPALIVES
#define SMUX_KEEPALIVES
#endif
#endif


/*
 *	If this is a PSos port, we'll need a few other things...
 *		From the last version we saw...
 *		- shared symbol tables mean main() should be private
 *		- getdomainname() was missing 
 *		- sysconf() and ulimit() were missing 
 *		- getservbyname() was missing
 *		- getprotobyname() was missing
 *		- support for signals was missing
 */
#ifdef PEER_PSOS_PORT
#ifndef PEER_MAIN_STATIC
#define PEER_MAIN_STATIC
#endif
#ifndef PEER_GETDOMAINNAME_MISSING
#define PEER_GETDOMAINNAME_MISSING
#endif
#ifndef PEER_FDSETSIZE_IS_MAXFILES
#define PEER_FDSETSIZE_IS_MAXFILES
#endif
#ifndef PEER_GETSERVBYNAME_MISSING
#define PEER_GETSERVBYNAME_MISSING
#endif
#ifndef PEER_GETPROTOBYNAME_MISSING
#define PEER_GETPROTOBYNAME_MISSING
#endif
#ifndef PEER_SIGNALS_MISSING
#define PEER_SIGNALS_MISSING
#endif
#endif


/*
 *	If the AIX environment, we also need to let the compiler know we'll
 *	want access to the BSD type definitions (for things like sockaddr)
 *	We will also need time.h in order to do tracing.
 */
#ifdef PEER_AIX_PORT
#ifndef PEER_NO_STATIC_FORWARD_DECL 
#define PEER_NO_STATIC_FORWARD_DECL 
#endif
#define _BSD 1
#ifndef PEER_NEED_TIME_H
#define PEER_NEED_TIME_H
#endif
#endif


/*
 *	If this is for lynx, we need to define some additional symbols
 */
#ifdef PEER_LYNX_PORT
#ifndef PEER_GETDOMAINNAME_MISSING
#define PEER_GETDOMAINNAME_MISSING
#endif
#ifndef PEER_NO_STATIC_FORWARD_DECL 
#define PEER_NO_STATIC_FORWARD_DECL 
#endif
#endif


/*
 *	If this is for SCO, we can consider this to be generic with only
 *	minor deviations.
 */
#ifdef PEER_SCO_PORT
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef PEER_GETDOMAINNAME_MISSING
#define PEER_GETDOMAINNAME_MISSING
#endif
#ifndef PEER_NEED_TIME_H
#define PEER_NEED_TIME_H
#endif
#endif


/*
 *	If this is for VRTX, we can consider this to be generic with only
 *	minor deviations.
 *		From the last version we saw...
 *		- shared symbol tables mean main() should be private
 *		- getdomainname() was missing 
 *		- sysconf() and ulimit() were missing 
 *		- getservbyname() was missing
 *		- support for signals was missing
 */
#ifdef PEER_VRTX_PORT
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef PEER_MAIN_STATIC
#define PEER_MAIN_STATIC
#endif
#ifndef PEER_GETDOMAINNAME_MISSING
#define PEER_GETDOMAINNAME_MISSING
#endif
#ifndef BLOCKING_CONNECT
#define BLOCKING_CONNECT
#endif
#ifndef PEER_FDSETSIZE_IS_MAXFILES
#define PEER_FDSETSIZE_IS_MAXFILES
#endif
#ifndef PEER_GETSERVBYNAME_MISSING
#define PEER_GETSERVBYNAME_MISSING
#endif
#ifndef PEER_SIGNALS_MISSING
#define PEER_SIGNALS_MISSING
#endif
#endif


/*
 *	If this is for NCR machines, we need to set things
 *	up as a generic version, but with the addition of blocking connect
 */
#ifdef PEER_NCR_PORT
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef BLOCKING_CONNECT
#define BLOCKING_CONNECT
#endif
#ifndef ALLOW_NCR_SMUX
#define ALLOW_NCR_SMUX
#endif
#endif


/*
 *	If this is for QNX, we need to set things up as a generic version,
 *	but with a few minor work-arounds
 *		From the last version we saw...
 *		- getdomainname() was missing 
 *		- sysconf() and ulimit() were missing 
 *	This list is not complete!
 */
#ifdef PEER_QNX_PORT
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef PEER_GETDOMAINNAME_MISSING
#define PEER_GETDOMAINNAME_MISSING
#endif
#ifndef PEER_FDSETSIZE_IS_MAXFILES
#define PEER_FDSETSIZE_IS_MAXFILES
#endif
#endif


/*
 *	If this is for Sequent (Dynix) machines, we need to set things
 *	up as a generic version, but with the addition of blocking connect
 */
#ifdef PEER_SEQUENT_PORT
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef BLOCKING_CONNECT
#define BLOCKING_CONNECT
#endif
#endif


/*
 *	If this is for 4.3 BSD BSDI BSD386 version 1.1, we need a few minor
 *	differences from the generic version.
 */
#ifdef PEER_BSDI_PORT
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef PEER_GETDOMAINNAME_MISSING
#define PEER_GETDOMAINNAME_MISSING
#endif
#endif


/*
 *	If this is for OS/2, we will set things up as a generic version,
 *	but the IBM compiler does not define _STDC_ so we will over-ride
 *	and define USE_PROTOTYPES and READONLY and a few others here.
 */
#ifdef PEER_OS2_PORT
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#ifndef USE_PROTOTYPES
#define USE_PROTOTYPES
#endif
#ifndef READONLY
#define READONLY
#endif
#ifndef PEER_FDSETSIZE_IS_MAXFILES
#define PEER_FDSETSIZE_IS_MAXFILES
#endif
#ifndef PEER_NO_STATIC_FORWARD_DECL
#define PEER_NO_STATIC_FORWARD_DECL
#endif
#ifndef PEER_SIGNALS_MISSING
#define PEER_SIGNALS_MISSING
#endif
#endif /* PEER_OS2_PORT */


/*
 *	Handle consequences of the following:
 *		PEER_SIGNAL_INT_RETURN - signal handlers return int instead
 *			of the usual void
 *		PEER_SIGNAL_VOID_PARM - signal handlers have no parameters
 *			instead of the usual (int, ...)
 */
#ifdef PEER_SIGNAL_INT_RETURN
	/*
	 *	Unusual case: signal handlers return int
	 */
#ifndef PEER_SIGNAL_HANDLER_TYPE
#define PEER_SIGNAL_HANDLER_TYPE	int
#endif
#ifndef PEER_SIGNAL_HANDLER_RETURN
#define PEER_SIGNAL_HANDLER_RETURN	return(0);
#endif
#else
	/*
	 *	Normal case: signal handlers return void
	 */
#ifndef PEER_SIGNAL_HANDLER_TYPE
#define PEER_SIGNAL_HANDLER_TYPE	void
#endif
#ifndef PEER_SIGNAL_HANDLER_RETURN
#define PEER_SIGNAL_HANDLER_RETURN	return
#endif
#endif

#ifdef PEER_SIGNAL_VOID_PARM
	/*
	 *	On systems that define signal handlers this way, it's
	 *	usually a result of a botched conversion of the system
	 *	header files to support prototypes.  The typical form
	 *	is to use (); if this does not work, try (void).
	 */
#ifndef PEER_SIGNAL_PROTO_ARG
#define PEER_SIGNAL_PROTO_ARG	
#endif
#else
	/*
	 *	Normally, signal handlers are really varargs functions.  We do
	 *	not use the parameters, so we really don't care.
	 */
#ifndef PEER_SIGNAL_PROTO_ARG
#define PEER_SIGNAL_PROTO_ARG	int sig
#endif
#endif


/*
 *	Packet tracing of any kind requires that the packet display library
 *	be included.
 */
#ifdef SNMP_PACKET_TRACE
#ifndef TRACE_ENABLED
#define TRACE_ENABLED
#endif
#endif

#ifdef SMUX_CLIENT_TRACE
#ifndef TRACE_ENABLED
#define TRACE_ENABLED
#endif
#endif

#ifdef SMUX_SERVER_TRACE
#ifndef TRACE_ENABLED
#define TRACE_ENABLED
#endif
#endif

#ifdef UDP_TRACE
#ifndef UDP_READ_TRACE
#define UDP_READ_TRACE
#endif
#endif

#ifdef UDP_TRACE
#ifndef UDP_WRITE_TRACE
#define UDP_WRITE_TRACE
#endif
#endif

#ifdef TCP_TRACE
#ifndef TCP_READ_TRACE
#define TCP_READ_TRACE
#endif
#endif

#ifdef TCP_TRACE
#ifndef TCP_WRITE_TRACE
#define TCP_WRITE_TRACE
#endif
#endif

#ifdef UDP_READ_TRACE
#ifndef TRACE_ENABLED
#define TRACE_ENABLED
#endif
#endif

#ifdef UDP_WRITE_TRACE
#ifndef TRACE_ENABLED
#define TRACE_ENABLED
#endif
#endif

#ifdef TCP_READ_TRACE
#ifndef TRACE_ENABLED
#define TRACE_ENABLED
#endif
#endif

#ifdef TCP_WRITE_TRACE
#ifndef TRACE_ENABLED
#define TRACE_ENABLED
#endif
#endif

#ifdef PEER_MGMT_API_TRACE
#ifndef TRACE_ENABLED
#define TRACE_ENABLED
#endif
#endif


/*
 *	Handle consequences of SNMPv2 options:
 *		DES implies MD5 support
 *		MD5 and DES both imply SNMPv2 support
 */
#ifdef DES_SUPPORTED
#ifndef MD5_SUPPORTED
#define MD5_SUPPORTED
#endif
#endif

#ifdef MD5_SUPPORTED
#ifndef SNMPV2_SUPPORTED
#define SNMPV2_SUPPORTED
#endif
#endif

/* 
 *	These flags are needed for compilation in a DGUX 5.4 environment. 
 */

#ifdef PEER_DGUX_PORT
#ifndef PEER_GENERIC_PORT
#define PEER_GENERIC_PORT
#endif
#define __using_BSD 1 
#define __using_DGUX 1 
#endif


#endif

