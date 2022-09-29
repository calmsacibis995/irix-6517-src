'\" pcd/chap06.mm 1.8 99/01/20 (C) Copyright 1999 Silicon Graphics, Inc.
.1 6 "Input and Output Primitives"
.2 3 "File Descriptor Deassignment
.3 1 "Close a File."
.uS
If the
.f close
function is interrupted by a signal that is to be caught
.k fildes
does not remain usable; further attempts to access it will fail and
set errno to EBADF.
.2 4 "Input and Output"
.3 1 "Read from a File"
.4 2 "Description"
.uD
The value of the file offset associated with a file not capable of
seeking after a call to
.f read
is incremented from its previous value by the number of bytes successfully
read by the 
.f read
call.  This is not, however, a guaranteed mechanism and programmers are
enjoined against writing code which relies on it.
.mV
On
\*(sN
systems, if a 
.f read
is interrupted by a signal, it returns -1 with
.a errno
set to EINTR. 
.iD
If the 
.a fildes 
argument refers to a device special file, subsequent
.f read 
requests may return further data if available; however there is no guarantee
that no data will be lost.
.iD
On
\*(sN
systems, it is not possible for the
value of the 
.a nbyte 
argument to 
.f read 
to exceed
.Pl INT_MAX , 
since size_t is in fact a signed long: the largest integer type supported.
.90 6.4.1.2 "Use {SSIZE_MAX}, not {INT_MAX}."
.4 4 "Errors"
.uS
On
\*(sN
systems,
an 
.Pe EIO
error may be generated in response to hardware errors on the physical device.
.3 2 "Write to a File"
.4 2 "Description"
.iD
On
\*(sN
systems, if the file is not a regular file, when the
.a nbyte
argument is zero,
.f write
succeeds, returning a value of zero.  No data is transferred.
.90 6.4.2.2 "The preceding paragraph becomes unspecified, not impl-def."
.uD
The value of the file offset associated with a file not capable of
seeking after a call to
.f write
is incremented from its previous value by the number of bytes successfully
transferred by the 
.f write
call. This is not, however, a guaranteed mechanism and programmers are
enjoined against writing code which relies on it.
.mV
On
\*(sN
systems, if a 
.f write
is interrupted by a signal, it returns -1 with 
.a errno
set to EINTR. 
.iD
On
\*(sN
systems, it is not possible for the
value of the 
.a nbyte 
argument to 
.f read 
to exceed
.Pl INT_MAX , 
since size_t is in fact a signed long: the largest integer type supported.
.90 6.4.2.2 "Use {SSIZE_MAX}, not {INT_MAX}."
.4 4 "Errors"
.uS
On
\*(sN
systems, for a 
.f write 
call, an
.Pe EIO
error may be generated in response to hardware errors on the physical device.
.2 5 "Control Operations on Files"
.3 2 "File Control"
.4 2 "Description"
.iD
On
\*(sN
systems, if
.f fcntl
is called with the 
.a cmd
argument set to F_SETFL, and if any bits are set in
.a arg
other than those associated with the file status flags,
certain bits will cause changes in file behaviour which are beyond the scope
of POSIX definitions.  See the fcntl(2) manpage for details.
.mV
\*(sN 
supports advisory record locking
for all file types.
.uD
If the \fIl_len\fP member of the \fIflock\fP structure 
is negative when 
.f fcntl
is called, the call returns -1 and errno is set to EINVAL.
.4 4 "Errors"
.rF
For the
.f fcntl
function,
\*(sN
detects the conditions and returns the corresponding errno value for
.Pe EDEADLK .
.3 3 "Reposition Read/Write File Offset"
.4 2 "Description"
.iD
On
\*(sN
systems, the
.f lseek
function behaves as follows on devices incapable of seeking:
an offset may be reported and modified.  There is no effect on actual data
transfer.
.nP
.dM
