'\" pcd/chap02.mm 
.bp 0\"used to set the page number accurately
.nr P 0
.1 2 "Definitions and General Requirements" "Definitions, Requirements"
.2 2 "Conformance"
.3 1 "Implementation Conformance"
.rP
The \*(sN
operating system conforms to the IEEE Std 2003.1-1992
(\*(pN) with
Common Usage C Language-Dependent System Support.
.90 1.3.1 "Standard name is ISO/IEC 9945-1: 1992 (IEEE Std2003.1-1992)"
.rP
The
\*(sN
kernel is POSIX conformant as shipped. If FIPS-1 conformance is required,
the following changes should be made via the command \fBsystune\fR as
superuser:
.P
# systune -i
.br
Updates will be made to running system and /unix.install
.br
systune-> restricted_chown 1
.br
     restricted_chown = 0
.br
     Do you really want to change restricted_chown to 1 (0x1)? (y/n)  y
.br
systune->  posix_tty_default 1
.br
     posix_tty_default = 0
.br
     Do you really want to change posix_tty_default to 1 (0x1)? (y/n)  y
.br
systune->  q
.P
A new kernel will be built: see autoconfig(1M),
and the system rebooted to run with the new kernel.
.P
Also, to obtain Common Usage C behaviour, the compiler must be configured 
appropriately. There are two ways in which this can be done: either the command
line option -cckr may be given, or the environment variable SGI_CC may be set
to the string -cckr.
.2 3 "General Terms"
The following paragraphs briefly define terms specific to the 
\*(sN
implementation of \*(pN.
For more details, see the 
\*(sN
system documentation.
.br
.iD
\fBappropriate privileges\fR: On  
\*(sN 
systems, appropriate privileges are associated with the superuser; the
superuser is defined as a user with a uid of zero.  To become
superuser, or run a command as superuser, from the command prompt or from
a shellscript, the command "su" is used.  See su(1M).
.P
It should be noted that the \fBAccess Control Lists\fR feature
[See \fBacl(4)\fR] and the \fBuser capability database\fR feature
[See \fBcapabilities(4)\fR and \fBcapability(4)\fR] were not used
while certifying POSIX.
.iD
\fBcharacter special file\fR: On 
\*(sN 
systems, other than \fIterminal device files\fR,
the
following \fIcharacter special files\fR are available:
.P
Disk device special files and tape device special files: see intro(7).
.P
Pseudo tty device files: see the pty(7M) man page, which provide a terminal
interface independent of physical hardware.
.P
Several other types of character special files exist.  They are listed here 
for information; however, many of them are intended for the use of specialised
system programs and should not be considered a general-purpose user resource.
.P
Subject to optional software, special files for DECnet link level interface:
see dn_ll(7M).
.P
A special file for system software access to filesystems: see fsctl(7M).
.P
A special file for interface to the kernel error logging mechanism: see 
klog(7M).
.P
A special file for access to system memory: see kmem(7M).
.P
On the personal IRIS only, a special file for accees to a parallel printer port:
see plp(7M).
.P
Special files for user-mode access to the SCSI bus: see ds(7M).
.P
Subject to hardware support, special files for 3270 interface:
see t3270(7M).
.P
Special files for providing user-level semaphore facilities: see usema(7M).
.90 2.2.2.9 "Structure is unspecified, not implementation-defined"
.mV
\fBfile\fR: Other than \fIregular, character special, block special, FIFO
special\fR, and \fIdirectory files\fR, 
the following file types are available:
.P
Symbolic links. See ln(1), symlink(2).
.P
UNIX domain sockets. See socket(2).
.iD
\fBparent process ID\fR: On
\*(sN
sytems, if a child process continues to exist 
after its creator process ceases to exist, 
the \fIparent process ID\fR becomes the \fIprocess ID\fR of the 
init process, process id 1.
.mV
\fBpathname\fR:
A \fIpathname\fR that begins with two slashes is interpreted as 
if only one slash were present: ie as an absolute pathname.
.rF
\fBread-only file system\fR: \*(sN
restricts modifications to \fIread-only file systems\fR in the following ways: 
files may not be created or deleted; no modification of file contents,
ownership, permissions, or times may occur.
.uS
\fBsupplementary group ID\fR:
A \fIprocess's effective group ID\fR
is included in 
its list of \fIsupplementary group IDs\fR.
.2 4 "General Concepts"
.iD
\fBfile times update\fR:
Fields that are marked for update are updated
immediately in the incore copy of the inode, so the update will be visible
immediately to any process accessing the file. For performance reasons,
the on-disk copy is refreshed only periodically, however the updated 
information is synchronized to disk when the last process referencing the file
closes it.
.2 5 "Error Numbers"
.iD
\*(sN
detects the
.Pe EFAULT
error condition for some system calls.  For others, noted in
later sections of this document, an invalid address parameter will cause
a segmentation violation.
.iD
On
\*(sN
systems, the
.Pe EFBIG
error occurs when the size of a file exceeds
the maximum file size.
.2 6 "Primitive System Data Types"
.uS
In addition to those primitive system data types listed in Table 2-1, the
\*(sN 
system defines
the following data types 
whose names end with 
\fI_t\fR, in POSIX.1 headers: 
.br
.sp .3v
.ta 2i
In <types.h>:
.br
typedef long daddr_t;	/* <disk address> type */
.br
typedef char * caddr_t;	/* <core address> type */
.br
typedef short cnt_t;	/* <count> type */
.br
typedef unsigned long paddr_t;	/* <physical address> type */
.br
typedef int key_t;	/* IPC key type */
.br
typedef unsigned char use_t;	/* use count for swap.  */
.br
typedef short sysid_t;
.br
typedef short index_t;
.br
typedef unsigned int lock_t;	/* <spinlock> type */
.br
typedef signed char cpuid_t;	/* cpuid */
.br
typedef long swblk_t;
.90 2.5 "The additional types are unspecified in the 1992 standard, not \
implementation-defined."
.2 7 "Environment Description"
.mV
\*(sN
permits all characters in the 8-bit ASCII character set except NULL
in environment variable names.
.2 8 "C Language Definitions"
.3 2 "POSIX Symbols"
.2 9 "Numerical Limits"
.iD
On
\*(sN
systems, \fB<limits.h>\fR contains
the following implementation-specific names and values:
.TS
tab(|), allbox;
cw(1.5i) cw(1.5i) 
l l.
Name | Value           
NGROUPS_MAX|16
LINK_MAX|30000
MAX_CANON|255
MAX_INPUT|255
NAME_MAX|255
PATH_MAX|1024
PIPE_BUF|10240
.TE
Note: ARG_MAX, CHILD_MAX and OPEN_MAX are not defined in limits.h
when _POSIX_SOURCE is in effect, since these values are not invariant; it
is possible to create systems with differing values. They are controlled
by the values \fBmaxup\fR, and \fBrlimit_nofile_max\fR, respectively.
Changes to these kernel variables must be made using the command 
\fBsystune\fR see systune(1M) and a new kernel must be booted:
see autoconfig(1M)..
.90 2.9 "NGROUPS_MAX must always be present; others from <limits.h>, \
only if they're in the file.  Add: SSIZE_MAX, STREAM_MAX, TZNAME_MAX."
.2 10 "Symbolic Constants"
.iD
On
\*(sN
systems, \fB<unistd.h>\fR contains the following values:
.TS
tab(|), allbox;
cw(2.5i) cw(1.0i)
l l.
Symbolic Constant | Value       
R_OK|4
W_OK|2 
X_OK|1
F_OK|0
SEEK_SET|0
SEEK_CUR|1
SEEK_END|2
_POSIX_JOB_CONTROL|1
_POSIX_SAVED_IDS|1
_POSIX_VERSION|199506L
_POSIX_VDISABLE|0xff
.TE
No definition is present for _POSIX_CHOWN_RESTRICTED
since versions of the system with differing behaviours may be created.
The behaviour is controlled by the value of the kernel variable 
"restricted_chown".  A kernel with values other than the default may be 
created by changing this value via the command \fBsystune\fR: see systune(1M)
and reconfiguring: see autoconfig(1M).
.90 2.9 "NGROUPS_MAX must always be present; others from <limits.h>, \

.90 2.9 "Only limits need be reported from <limits.h> and <unistd.h>."
.nP
.dM
