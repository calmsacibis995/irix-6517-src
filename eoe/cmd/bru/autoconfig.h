/*
 *	This autoconfiguration file was build automatically by make.sh.
 *	Any handmade changes will get overwritten the next time autoconfiguration
 *	is done, thus it is best to change make.sh as necessary.
 */

/*
 *	Define whether or not mkdir() can be found in libc.a.  If not, then
 *	we need to build directory nodes the hard way.  Most BSD derived 
 *	systems, or USG systems extended to use NFS, will have mkdir().
 */

#ifndef HAVE_MKDIR
#define HAVE_MKDIR 1
#endif

/*
 *	Define whether or not memset() can be found in libc.a.  If not, then
 *	we do our block memory zero'ing using a portable C loop, which can
 *	be significantly slower.
 */

#ifndef HAVE_MEMSET
#define HAVE_MEMSET 1
#endif

/*
 *	Define whether or not the BSD style directory access routines can
 *	be found in libc.a.  If not, we need to use the internal versions
 *	(set HAVE_SEEKDIR to 0).  It is preferable to use the system
 *	supplied versions if they are available.
 */

#ifndef HAVE_SEEKDIR
#define HAVE_SEEKDIR 1
#endif

/*
 *	Define AUX to 1 if the host is running Apple's A/UX unix
 *	implementation.
 */

#ifndef AUX
#define AUX 0
#endif

/*
 *	Define xenix if it looks like the host is a xenix system and
 *	xenix is not already defined.  Most xenix systems don't define
 *	unix, so one of these MUST be defined.
 */

#ifndef sgi
#ifndef xenix
#define xenix 0
#endif
#endif

/*
 *	If Sun's NFS implementation is available then define NFS to 1.
 */

#ifndef NFS
#define NFS 1
#endif

/*
 *	If any of the source files have been checked out for editing
 *	from SCCS, then define TESTONLY to handle special cases of
 *	non-expansion of sccs keywords.
 */

#ifndef sgi
#ifndef TESTONLY
#define TESTONLY 0
#endif
#endif

/*
 *	Define exactly one of SIGTYPEINT or SIGTYPEVOID, to indicate
 *	whether or not signal returns a pointer to a function returning
 *	int, or a pointer to a function return void, respectively.
 */

#if (!SIGTYPEINT && !SIGTYPEVOID)
#define SIGTYPEINT 1
#define SIGTYPEVOID 0
#endif

/*
 *	Define RMT to 1 if we have support for remote shells, on the
 *	assumption that some system out there may have /etc/rmt on it,
 *	thus allowing us to support remote tape drives.
 */

#ifndef RMT
#define RMT 1
#endif

/*
 *	Define HAVE_VARARGS to 1 if support is provided for 
 *	the varargs macros.  Otherwise, we will fake the varargs
 *	stuff with a local copy, which should work on any system
 *	that doesn't strictly require varargs.
 */

#ifndef HAVE_VARARGS
#define HAVE_VARARGS 1
#endif

/*
 *	Define HAVE_TERMIO to 1 if we have <termio.h> style terminal
 *	handling.
 */

#ifndef HAVE_TERMIO
#define HAVE_TERMIO 1
#endif

/*
 *	Define HAVE_SYMLINKS to 1 if symbolic links are available.
 */

#ifndef HAVE_SYMLINKS
#define HAVE_SYMLINKS 1
#endif

/*
 *	Define HAVE_FIFOS to 1 if fifos are available.
 */

#ifndef HAVE_FIFOS
#define HAVE_FIFOS 1
#endif

/*
 *	Define HAVE_SHM to 1 if System V style shared memory is available.
 *	If it is, we can use use double buffering to greatly increase
 *	throughput in some situations (actual improvement is hardware 
 *	dependent).
 */

#ifndef HAVE_SHM
#define HAVE_SHM 1
#endif

/*
 *	Define FAST_CHKSUM to 1 if you are willing to support a private
 *	machine dependant checksum routine (usually written in assembler)
 *	that replaces the simple checksum loop in the function "chksum".
 *	Your routine must be named sumblock, and takes two arguments,
 *	a pointer to the first byte and a byte count.  Be careful to
 *	test your custom version of bru archive read/write compatibility
 *	with the version that uses the C code.  If you know enough to
 *	write this, you are assumed to know enough to figure out how
 *	to change the makefile appropriately.
 */

#ifndef FAST_CHKSUM
#define FAST_CHKSUM 1
#endif

/*
 *	The following defines are holdovers from the old style
 *	autoconfig mechanism where a system was neatly classified
 *	as USG/BSD USG5/BSD4, etc, rather than having certain
 *	attributes as above.  They will gradually go away as
 *	they are weeded out of the source base.
 */

#define USG5 1
#define BSD 0
#define BSD4_1 0
#define BSD4_2 0

/*
 *	Define CONFIG_DATE as a string of the form given by
 *	the unix "date" command with no arguments".  This is the
 *	date that autoconfig was last run.  It also usually ends
 *	up being the installation date.
 */

#define CONFIG_DATE "Thu May 17 08:28:00 PDT 1988"

