'\" pcd/chap04.mm 1.9 99/01/20 (C) Copyright 1999 Silicon Graphics, Inc.
.1 4 "Process Environment"
.2 2 "User Identification"
.3 3 "Get Supplementary  Group IDs"
.4 2 "Description"
.uD
The values of array entries with indices larger than or equal to the
value returned by
.f getgroups
are unchanged by the
.f getgroups
call; they retain whatever values were present prior to the call.
.uS
On
\*(sN
systems, the effective group ID of the calling process is 
included in 
the returned list of supplementary group IDs.
.90 4.2.3.2 "This requirement is absent from the 1992 standard."
.3 4 "Get User Name"
.4 3 "Returns"
.mV
On
\*(sN
systems, the return value from
.f getlogin 
points to static data.
.P
.90 4.2.4.1 "This requirement is absent from the 1992 standard."
.4 4 "Errors"
.mV
On
\*(sN
systems,
no 
error conditions are detected
for the 
.f getlogin
function.
.2 3 "Process Groups"
.3 3 "Set Process Group ID for Job Control"
.4 2 "Description"
.jC
\*(sN 
does not define {_POSIX_JOB_CONTROL} and 
.oP
it supports the
.f setpgid
function as described in section 4.3.3.2 / the 
.f setpgid
function fails.  
.oE
.2 4 "System Identification"
.3 1 "System Name"
.4 2 "Description"
.iD 
On
\*(sN
systems, the structure 
.v utsname
contains the following members:
.P
sysname,
.br
nodename
.br
release
.br
version
.br
machine
.P
Each of these is a character array of size 9 bytes.
The possible values of these are described in the uname(2) manpage.
.4 4 "Errors"
.uS
On
\*(sN
systems,
.f uname
will fail, and set errno to EFAULT, if the name parameter does not point to
a valid area in process address space.
.2 5 "Time"
.3 1 "Get System Time"
.4 4 "Errors"
.uS
On
\*(sN
systems
if the argument to the 
.f time
function does not point to a valid area in process address space, a SIGSEGV 
signal will be delivered to the calling process.
.3 2 "Process Times"
.4 3 "Returns"
.mV
\*(sN
allows 
possible return values from
.f times
to overflow the range of type
.k clock_t.
.4 4 "Errors"
.uS
On
\*(sN
systems,
.f times
will fail, and set errno to EFAULT, if the name parameter does not point to
a valid area in process address space.
.2 6 "Environment Variables"
.3 1 "Environment Access"
.4 3 "Returns"
.mV
On 
\*(sN
systems, the return value from
.f getenv
if not null, points to the area on the process stack where the value of the
relevent variable resides. Programs should not make any write references via
the pointer value returned by the
.f getenv
function.
.4 4 "Errors"
.uS
On
\*(sN
systems, no error conditions are detected for the 
.f getenv
function.
.2 7 "Terminal Identification"
.3 1 "Generate Terminal Pathname"
.4 3 "Returns"
.mV
If 
.f ctermid
is called with a \fBNULL\fP
pointer, the string is generated in a static area. 
.4 4 "Errors"
.uS
On 
\*(sN
systems,
no 
error conditions are detected
for the 
.f ctermid
function.
.3 2 "Determine Terminal Device Name"
.4 2 "Description"
.mV
On
\*(sN
systems, the return value of 
.f ttyname
points to static data.
.4 4 "Errors"
.uS
On
\*(sN
systems, no error conditions are detected for the 
.f ttyname
function.
.uS
No error conditions are detected for the 
.f isatty
function.
.2 8 "Configurable System Variables"
.3 1 "Get Configurable System Variables"
.4 2 "Description"
.mV
\*(sN
supports no additional configurable system variables
other than those required by Table 4-2 of
.pN
.nP
.dM 
