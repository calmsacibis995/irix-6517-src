$
$	(C) COPYRIGHT CRAY RESEARCH, INC.
$	UNPUBLISHED PROPRIETARY INFORMATION.
$	ALL RIGHTS RESERVED.

$  'errmsg' utility message text file
$  $msg format: 
$  	$msg ERRMSG_MSGCODE message... 

#ifndef _ERRMSG_H
#include "errmsg.h"
#endif /* _ERRMSG.H */

$  messages and explanations:
$ access() call failed
$msg CMD_ACCESS Cannot access '%s'.
$nexp CMD_ACCESS 
Cannot access \*Vfile\fP.
.PP
The access system call failed to access the \*Vfile\fP.
The access system call checks a \*Vfile\fP for accessibility.
.PP
Inorder to access a \*Vfile\fP, the access system call requires
search permission to all components of the path prefix for the
\*Vfile\fP.
.PP
For a list of error conditions that will cause the access
system call to fail, see access(2).
.ME
$ ---------------------------------------------------------------------------
$ fdopen() call failed
$msg CMD_FDOPEN Cannot open '%s'.
$nexp CMD_FDOPEN 
Cannot open \*Vfile\fP.
.PP
The fdopen function failed.
The fdopen function associates a stream with the given file descriptor 
\*Vfildes\fP, which was obtained from a successful open, dup, creat, or 
pipe system call.
.PP
Opening a file, with the fdopen function, will fail if the
\*Vtype\fP of the stream does not agree with the \*Vmode\fP of the open 
file associated with the file descriptor.
.ME
$ -------------------------------------------------------------------------
$ fopen() call failed
$msg CMD_FOPEN Cannot open '%s'.
$nexp CMD_FOPEN 
Cannot open \*Vfile\fP.
.PP
The fopen function failed.
The fopen function opens the specified \*Vfile\fP and associates
a stream with it.
Opening a file with write mode will create the file, if it does not
exist, or truncate the file to zero length.
.PP
Opening a file with read mode fails if the file does not exist or 
the read permission is denied.
Opening a file with read/write mode fails if 
the \*Vfile\fP exists and write permission is denied or
the \*Vfile\fP does not exist and the directory in which the
file is to be created does not permit writing.
The fopen function requires
search permission to all components of the path prefix for the
\*Vfile\fP.
.PP
For a list of error conditions that will cause the fopen
function to fail, see open(2).
.ME
$ -------------------------------------------------------------------------
$ fseek() call failed.
$msg CMD_FSEEK Cannot reposition file pointer in '%s'.
$nexp CMD_FSEEK 
Cannot reposition file pointer in \*Vfile\fP.
.PP
The fseek function failed.
The fseek function sets the file pointer of the specified
\*Vstream\fP to the specified byte \*Voffset\fP.
.PP
The fseek function will fail if the resulting position of the
file pointer is negative.
The fseek function will also fail if the \*Vstream\fP is
associated with a terminal.
.ME
$ -------------------------------------------------------------------------
$ open() call failed.
$msg CMD_OPEN Cannot open '%s'.
$nexp CMD_OPEN
Cannot open \*Vfile\fP.
.PP
The open system call failed.
The open system call opens the specified \*Vfile\fP for reading or
writing.  With the O_CREAT flag, the open system call will create
the specified \*Vfile\fP, if it does not exist; otherwise the flag
has no effect.
.PP
Opening a file with the read mode fails if the file does not exist or
the read permission is denied.
Opening a file with the read/write mode fails if
the \*Vfile\fP exists and write permission is denied or
the \*Vfile\fP does not exist and the directory in which the
file is to be created does not permit writing.
The open system call requires
search permission to all components of the path prefix for the
\*Vfile\fP.
.PP
For a list of error conditions that will cause the open
system call to fail, see open(2).
.ME
$ -------------------------------------------------------------------------
$ pclose() call failed
$msg CMD_PCLOSE Cannot close pipe.
$nexp CMD_PCLOSE 
Cannot close pipe.
.PP
The pclose function failed.
.PP
The pclose function closes a pipe stream, that was opened by popen.
The pclose function waits for the associated child process to 
terminate and returns the exit status of the command.
Upon successful return, the pclose function returns the termination
status of the shell that ran the command.  Otherwise, pclose returns
-1 and set errno to indicate the error.
.ME
$ -------------------------------------------------------------------------
$ popen() call failed
$msg CMD_POPEN Cannot open pipe to '%s'.
$nexp CMD_POPEN 
Cannot open pipe to \*Vcommand\fP.
.PP
The popen function creates a pipe
between the calling program and the \*Vcommand\fP to be executed.
The popen function allows the user to open a pipe between a 
process and any other executable file.
.PP
The popen function will fail to create a pipe to the specified 
\*Vcommand\fP, if the specified \*Vcommand\fP is not a valid shell
command line.
.ME
$ -------------------------------------------------------------------------
$ stat() call failed.
$msg CMD_STAT Cannot obtain file status on '%s'.
$nexp CMD_STAT 
Cannot obtain file status on \*Vfile\fP.
.PP
The stat system call failed.
The stat system call obtains information about the specified \*Vfile\fP.
.PP
The stat system call will fail to obtain information about the 
\*Vfile\fP if search permission is denied for a component of the 
path prefix of \*Vfile\fP.
.PP
The stat system call will fail to obtain the information on a 
\*Vfile\fP, if the process is not granted read permission to the 
\*Vfile\fP via the security level and compartments, and the process
does not have the appropriate privilege.
.PP
For a list of error conditions that will cause the stat
system call to fail, see stat(2).
.ME
$ -------------------------------------------------------------------------
$ system(string) call failed.
$msg CMD_SYSTEM Execution of command '%s' failed.
$nexp CMD_SYSTEM 
Execution of command \*Vstring\fP failed.
.PP
The system function failed.
The system function passes the \*Vstring\fP to the host
environment to be executed by a "command processor" (sh(1)) as input,
as if \*Vstring\fP had been typed as a command at a terminal.
.PP
The system function will fail if the vfork system call or
exec system call fails.
.ME
$ -------------------------------------------------------------------------$
$ tempnam() call failed
$msg CMD_TEMPNAM  Failed to obtain a name for temporary file.
$nexp CMD_TEMPNAM  
Failed to obtain a name for temporary file.
.PP
The tempnam function failed.
The tempnam function generates a valid file name that is not
the same as the name of an existing file and that can safely be used
for the name of a temporary file.
.PP
The tempnam function will fail if it is unable to find an
accessible directory for the temporary file.
The tempnam function first tries the directory specified by the
\*Vdir\fP argument, then the directory \*VP_tmpdir\fP
as specified in the header file stdio.h, and finally /tmp will be
tried as a last resort, if the other directories are not accessible.
If the \*VTMPDIR\fP environment variable is set to an accessible
directory, then this directory will override the other possible
directories for the location of the temporary file.
.ME
$ -------------------------------------------------------------------------$
$msg ERRMSG_CATERRUSAGE \n\
  Usage: caterr [-c catfile] [-e] [-Y n,pathname] [-Y c,pathname] \n\
                [-Y m,pathname] [-s [-P cpp_opts]] [msgfile]
$nexp ERRMSG_CATERRUSAGE
Usage error
.PP
Either an incomplete command line or an unrecognized option was entered
with the \*Ccaterr\fR utility.
.PP
For details about the \*Ccaterr\fR utility and options, and the following
command line:
.CS
	man caterr
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_MACROACCESS Cannot access %s or it is a directory.
$nexp ERRMSG_MACROACCESS 
Cannot access \*Vfile\fP or it is a directory.
.PP
The \*Caccess\fR system call determines the accessibility of the path name     |
pointed to by the path argument. 
The \*Caccess\fR system call requires search permission to all components 
of the path prefix for the path.
Either the \*Caccess\fR system call failed to access the message macro specified
by the path or the path specified for the message macro points to a directory.
.PP
For a list of error conditions that will cause the \*Caccess\fR system call to fail,
see access(2).
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_USMID Encounter USMID line, more text is expected for $msg.
$nexp ERRMSG_USMID 
Encounter USMID line, more text is expected for $msg.
.PP
While expecting more text for the $msg currently being 
processed, \*Ccaterr\fR encounter an USMID line.  
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_NROFFEMPTY Nroff command failed, file '%s' is empty.
$nexp ERRMSG_NROFFEMPTY 
Nroff command failed, file \*Vfilename\fP is empty.
.PP
The output file produced by nroff is of zero length. 
Nroff command failed to format the explanations.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_LIBCPP Cannot access '%s'.
$nexp ERRMSG_LIBCPP 
Cannot access \*Vfile\fP.
.PP
The \*Caccess\fR system call failed to access \*Vfile\fP.
In order to access a \*Vfile\fP, the \*Caccess\fR system call require search
permission to all components of the path prefix for the \*Vfile\fP.
.PP
For a list of error conditions that will cause the \*Caccess\fR system 
call to fail, see access(2).
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_SYMNAME Cannot process symbolic names.
$nexp ERRMSG_SYMNAME 
Cannot process symbolic names.
.PP
The \*Ccaterr\fR utility calls the C language preprocessor, cpp to 
preprocess symbolic message names into message numbers.
The system call to process the symbolic names failed.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_FOPENTEXTF Cannot open message text file '%s'.
$nexp ERRMSG_FOPENTEXTF 
Cannot open message text file \*Vmsgfile\fP.
.PP
The \*Ccatxt\fR utility cannot open the input message text file you 
have specified. Check the path name again and make sure that you have 
read permission to the file.
The function call \*Cfopen\fR() failed.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_SEEKLAST Cannot fseek to the last byte of file '%s'.
$nexp ERRMSG_SEEKLAST 
Cannot fseek to the last byte of file \*Vfile\fP.
.PP
As part of the message text file processing, trailing blank lines 
produced by nroff are removed.
An internal error occurred in repositioning the file descriptor 
within the nroff output file.
The function call \*Cfseek\fR() failed.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_SEEKBLANK fseek() error on file '%s'; file contains possibly only blank lines.
$nexp ERRMSG_SEEKBLANK 
fseek() error on file \*Vfile\fP; file contains possibly only blank lines.
.PP
As part of the message text file processing, trailing blank lines 
produced by nroff are removed.
From the information returned by the function call \*Cfseek\fR, the 
output file produced by nroff contains only blanks lines.  
The function call \*Cfseek\fR() failed.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_TRUNCNROFF Error truncating nroff output file '%s'.
$nexp ERRMSG_TRUNCNROFF 
Error truncating nroff output file \*Vfile\fP.
.PP
As part of the message text file processing, trailing blank lines 
produced by nroff are removed.
An internal error occurred in truncating the nroff output file after
the blank lines are removed.
The function call \*Cftruncate\fR() failed.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_FOPENPROCF Cannot open the now processed message file '%s'.
$nexp ERRMSG_FOPENPROCF 
Cannot open the now processed message file \*Vmsgfile\fP.
.PP
An internal error occurred in opening the pre-processed message file.
The function call \*Cfopen\fR() failed.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_EXPLAINUSAGE Usage: explain groupcode-msgnumber
$nexp ERRMSG_EXPLAINUSAGE 
Usage: explain groupcode-msgnumber
.PP
Either an incomplete command line or an unrecognized option was entered
with the \*Cexplain\fR utility.
.PP
For details about the \*Cexplain\fR utility, enter the following command line:
.CS
	man explain
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_MSGIDENT %s-%s is not a valid message identifier.
$nexp ERRMSG_MSGIDENT 
\*Vgroupcode\fP-\*Vmsgnumber\fP is not a valid message identifier.
.PP
The message identifier specified to the \*Cexplain\fR 
utility \*Vgroupcode\fP-\*Vmsgnumber\fP was not valid.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_EXPCATOPEN Cannot open explanation catalog for message group '%s'.
$nexp ERRMSG_EXPCATOPEN 
Cannot open explanation catalog for message group \*Vgroupcode\fP.
.PP
The \*Cexplain\fR utility could not access the explanation catalog
specified by the group \*Vgroup\fR.
Check the value of the NLSPATH environment variable if it is set 
to ensure that the path name of the catalog is in the message system 
search path.  
See the \*Ccatopen\fR(3C) man page for a description of the NLSPATH variable.
The function call \*Ccatopen\fR failed.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_NOEXPMSG No explanation available for %s-%s.
$nexp ERRMSG_NOEXPMSG 
No explanation available for \*Vgroupcode\fP-\*Vmsgnumber\fP.
.PP
The \*Cexplain\fR utility could not access the explanation catalog
specified by the group \*Vgroupcode\fR or the explanation specified by
message number \*Vmsgnumber\fR was not found.
Check the value of the NLSPATH environment variable if it is set
to ensure that the path name of the catalog is in the message system
search path.
See the \*Ccatopen\fR(3C) man page for a description of the NLSPATH variable.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_NL_MALLOC Cannot open explanation catalog, catopen() unable to allocate memory for internal structures.
$nexp ERRMSG_NL_MALLOC 
Cannot open explanation catalog, catopen() unable to allocate memory for internal structures.
.PP
The \*Cexplain\fR utility cannot open the explanation catalog.
The library function call catopen() failed.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_NL_MAXOPEN Cannot open explanation catalog, too many message catalogs are open.
$nexp ERRMSG_NL_MAXOPEN 
Cannot open explanation catalog, too many message catalogs are open.
.PP
The \*Cexplain\fR utility cannot open the explanation catalog. The per
program limit NL_MAX_OPEN of open catalogs has been reached.
NL_MAX_OPENED is defined in /usr/include/nl_types.h.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_NL_MAP Cannot open explanation catalog, mmap() cannot map the explanation catalog.
$nexp ERRMSG_NL_MAP
Cannot open explanation catalog, mmap() cannot map the explanation catalog.
.PP
The \*Cexplain\fR utility cannot open the explanation catalog. The mmap()
system call could not map the explanation catalog into memory.
.ME
$ -----------------------------------------------------------------------
$msg ERRMSG_NL_HEADER Cannot open explanation catalog, incorrect file header.
$nexp ERRMSG_NL_HEADER
Cannot open explanation catalog, incorrect file header.
.PP
The \*Cexplain\fR utility cannot open the explanation catalog. The explanation catalog header has incorrect information and could be corrupt.
$ -----------------------------------------------------------------------
$msg ERRMSG_NROFF The message file contains $nexp symbols, but '%s' cannot be found.
$nexp ERRMSG_NROFF
The message file contains $nexp symbols, but \*Vfile\fP cannot be found.
.PP
\*Ccaterr\fR found $nexp symbols in the message file. nroff is needed to
process these explanations, but could not be found. If the explanations
do not contain nroff formatting, $exp symbols may be used instead of $nexp
symbols.
.ME


