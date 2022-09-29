'\" pcd/chap05.mm 1.9 99/01/20 (C) Copyright 1999 Silicon Graphics, Inc.
.1 5 "Files and Directories"
.2 1 "Directories"
.3 1 "Format of Files and Directories"
.iD
On
\*(sN
systems, the file system used for POSIX cerification is the \fBxfs\fR 
filesystem see xfs(4) for details.
.3 2 "Directory Operations"
.4 2 "Description"
.mV 
On
\*(sN
systems, the pointer returned by 
.f readdir 
points to data which
is not
overwritten by another call to 
.f readdir 
on the same directory
stream.
.mV
On
\*(sN
systems, the 
.f readdir 
function
buffers several directory entries per actual read operation.
.uS
On
\*(sN
systems, 
if a file is removed from or added to the directory after the most recent
call to 
.f opendir 
or 
.f rewinddir , 
a subsequent call to
.f readdir
does not
accurately reflect the contents of the directory.
.mV
On
\*(sN
systems,
.f dir
no longer points
to an accessible object of type
.k DIR
upon return from the 
.f closedir 
function.
.uD
On
\*(sN
systems, if the 
.a dirp 
argument passed to any of the file operation functions
does not refer to a currently-open directory stream, the following occurs:
.P
If the 
.a dirp
argument had formerly been an open directory stream but has been closed before 
the call, the function will return an error result, indicated by -1
and errno will be set to EBADF.
.P
If the
.a dirp 
argument points to something other than a directory stream the result of 
the function is undefined, and the calling process may be
terminated by bus error or segmentation violation signals.
.uD
On
\*(sN
systems, it is not possible to continue using a directory stream after
one of the
.f exec
family of functions, since the stream handle is not present in the new
process image. Note that the underlying buffering of the stream is discarded
at the time of the
.f exec ,
so no attempt should be made to propagate 
the stream handle to the new process image.
.uD
On
\*(sN
systems
if both the parent and child
processes use a combination of the 
.f readdir 
and 
.f rewinddir 
functions on a directory stream that was open during
a call to
.f fork ,
both processes may encounter unexpected discontinuities in the order of
entries read by 
.f readdir .
Programmers should conform to the POSIX recommendation of using the
directory stream in either the parent or the child, but not both.
.4 4 "Errors"
.rF
For the
.f opendir
function,
\*(sN
detects the conditions and returns the corresponding errno values for
.Pe EMFILE
and
.Pe ENFILE .
.rF
For the
.f readdir
function,
\*(sN
detects the conditions and returns the corresponding errno value for
.Pe EBADF .
.rF
For the
.f closedir
function,
\*(sN
detects the conditions and returns the corresponding errno value for
.Pe EBADF .
.2 2 "Working Directory"
.3 2 "Working Directory Pathname"
.4 2 "Description"
.uD
On
\*(sN
systems, if the argument 
.a buf 
to 
.f getcwd
is a \fBNULL\fP pointer,
.f getcwd 
will use 
.f malloc
to obtain a buffer of the size specified by the
.a size
argument.
.4 3 "Returns"
.uD
On
\*(sN
systems,
the contents of 
.a buf 
are unchanged from their previous value 
after an error on a call to 
.f getcwd .
.4 4 "Errors"
.rF
For the
.f getcwd
function,
\*(sN
detects the conditions and returns the corresponding errno value for
.Pe EACCES .
.2 3 "General File Creation"
.3 1 "Open a File"
.4 2 "Description"
.uD
On
\*(sN
systems, if 
O_RDWR is set in
.a oflag ,
when
.a path
refers to a FIFO, the resulting file descriptor may
be used for both writing to, and reading from, the FIFO.
.uS 
On
\*(sN
systems, when O_CREAT is set in
.a oflag 
and bits in 
.a mode
other than the file permission bits are set, bits which do not correspond
to file permission bits are ignored; the system masks them off before using
.a mode .
.uD
On
\*(sN
systems, if O_EXCL is set and O_CREAT is not set in
.a oflag , 
the 
.f open 
behaves as if O_EXCL were not set; O_EXCL has effect only if 
O_CREAT is set.
.uS
On
\*(sN
systems, when O_NONBLOCK is set in
.a oflag 
for files other than FIFO's with O_RDONLY or O_WRONLY, block special
files, or character special files, the 
.f open 
behaves as if O_NONBLOCK
were not set: O_NONBLOCK has no effect on opening of regular files.
.iD
On
\*(sN
systems,
if O_TRUNC is set in
.a oflag
and
.a path
refers to a file type other than a regular file, a
FIFO special file, or a directory, the 
.f open 
behaves as if O_TRUNC were not set: the O_TRUNC flag has no 
effect.
.90 5.3.1.2 "Previous paragraph becomes unspecified instead of impl-def."
.uD
On
\*(sN
systems, setting both O_TRUNC and O_RDONLY in
.a oflag
causes file truncation to occur in the same way as if either O_WRONLY or
O_RDWR were set if the calling process has write permission for the file.
.P
If the calling process does not have write permission for the file, the 
.f open 
will fail and errno will be set to EACCES.
.3 3 "Set File Creation Mask"
.4 2 "Description"
.iD
On
\*(sN
systems,
bits other than the file permission bits of
.a cmask 
are not used and are not set.
.P
The value of these bits in the return from
.f umask
is zero.
.2 4 "Special File Creation" 
.3 1 "Make a Directory"
.4 2 "Description"
.iD
On
\*(sN
systems, when bits in the 
.a mode 
argument to the 
.f mkdir 
function other than the file permission bits are set, 
they are ignored and have no effect on the mode of the created directory.
.mV
A newly-created directory's group ID depends on whether the S_ISGID bit
is set on the parent directory.
If this is set, the new directory's group is set to the group id of the
parent directory; if not, it is set to
the effective group ID of the calling process
.3 2 "Make a FIFO Special File"
.4 2 "Description"
.iD
On
\*(sN
systems, when bits in the 
.a mode 
argument to the 
.f mkfifo 
function other than the file permission bits are set, 
they have the following effect: 
.P
S_ISUID or S_ISGID bits are set in the mode of the resulting FIFO,
but have no effect on subsequent access to, or use of, the FIFO.
.P
If the value of the file type definition group of bits (S_IFMT in <sys/stat.h>)
is other than 0 or S_IFIFO, 
.f mkfifo
fails and errno is set to EPERM.
.P
Any other bits in the argument have no effect on the behaviour of the
.f mkfifo 
function, or on the mode of the created FIFO.
.P
Applications should not depend on this behaviour, since future versions of 
\*(sN
may be modified so that only the permission bits have significance.
.2 5 "File Removal"
.3 2 "Remove a Directory"
.4 1  "Description"
.iD
On
\*(sN
systems, if the directory indicated in the call to 
.f rmdir
is the root directory or the current working directory of any process,
.f rmdir
succeeds. Subsequent attempts to use the directory by
the process which was referencing it will fail.
.3 3 "Rename a File"
.4 2 "Description"
.mV
On
\*(sN
systems, in a call to 
.c rename "old, new" ,
if the 
.a old 
argument points to the pathname of a
directory, write access permission
is not
required for the directory named by 
.a old , 
nor, if it exists, for the directory named by 
.a new .
.2 6 "File Characteristics"
.3 1 "File Characteristics: Header and Data Structure"
If
.k file
is a file type other than a regular file, the
.k st_size
field returned by stat(file, buf) has the following meaning:
.P
If the file is a directory, the
.k st_size
field indicates the directory size in bytes.
.P
For any other file type, this field contains no significant information.
.4 2 "<sys/stat.h> File Modes"
.mV
On 
\*(sN
systems,
no other 
bits are ORed into S_IRWXU, S_IRWXG, and S_IRWXO.
.4 3 "<sys/stat.h> Time Entries"
.iD
On
\*(sN
systems, 
no other
functions in addition to those specified in 
.pN
mark for update the time-related fields,
\fIst_atime, st_mtime,\fP or \fIst_ctime\fP, of \fIstruct stat\fP.
.3 2 "Get File Status"
.4 2 "Description"
.3 3 "File Accessibility"
.4 2 "Description "
.mV
On
\*(sN
systems, the 
.f access
function
does 
indicate success for X_OK if the process has appropriate privileges yet
none of the file's execute permission bits are set.
.4 4 "Errors"
.rF
For the
.f access
function,
\*(sN
detects the conditions and returns the corresponding errno value for
.Pe EINVAL .
.3 4 "Change File Modes"
.4 2 "Description"
.mV
On
\*(sN
systems,
no additional 
restrictions cause the S_ISUID and S_ISGID bits in 
.f mode
to be ignored.
.iD
On
\*(sN
systems, 
.f chmod
has no effect on the file
descriptors for files open at the time of the function call; permission
checking is done only at
.f open
time.
.3 5 "Change Owner and Group of a File"
.4 2 "Description"
.iD
On
\*(sN
systems, if the 
.a path
argument to 
.f chown 
refers to a regular file and
if the call is made by a process with appropriate privileges, the
set-user-ID (S_ISUID) and set-group-ID (S_ISGID) bits of the file mode
are not
cleared upon successful return from 
.f chown .
.4 4 "Errors"
.rF
For the
.f chown
function,
\*(sN
detects the conditions and returns the corresponding errno value for
.Pe EINVAL .
.ne 6
.90 5.6.6.2 "Extensions to the utimbuf structure:"
.2 7 "Configurable Pathname Variables"
.3 1 "Get Configurable Pathname Variables"
.4 2 "Description"
.iD
\*(sN
supports no other variables
in addition to the configurable pathname variables 
listed in Table 5-2 of
.pN . 
.uD
On
\*(sN
systems, if the
.a path
or 
.a fildes 
parameters to 
.f pathconf
or
.f fpathconf
refer to any type of file other than
defined cases involving FIFOs, pipes, directories, or terminal
special devices, the appropriate values for the system are returned. 
These values are at present constant for the system and do not depend
on file type. Note that this may change in future releases.
.4 4 "Errors"
.rF
For the
.f pathconf
function,
\*(sN
does 
detect the conditions and return the corresponding errno values for
.Pe EINVAL .
.P
No other errors will arise, since in the current system the returned values
do not depend on the value of
.a path .
Note that this may change in future releases.
.rF
For the
.f fpathconf
function,
\*(sN
does 
detect the condition and returns the corresponding errno values for
.Pe EINVAL .
No other errors will arise, since in the current system the returned values
do not depend on the value of
.a fildes .
Note that this may change in future releases.
.nP
.dM
