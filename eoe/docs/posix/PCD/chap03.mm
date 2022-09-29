'\" pcd/chap03.mm 1.8 99/01/20 (C) Copyright 1999 Silicon Graphics, Inc.
.1 3 "Process Primitives"
.2 1 "Process Creation and Execution"
.3 1 "Process Creation"
.4 2 "Description"
.mV
On
\*(sN
systems, after a
.f fork ,
directory stream positioning is not shared between parent and child processes.
However, the underlying file descriptor (and its offset) is shared.
This offset advances when the library code providing the directory stream needs 
to refill its buffer. This could happen in either parent or child, depending
on patterns of access: thus the process which did not cause the advance
would be likely, on later accesses, to experience an unexpected discontinuity
in the order of directory entries.
.P
Programmers should stick to the POSIX recommendation of allowing only one member
of the pair to use the directory stream after a
.f fork .
.uS
On
\*(sN
systems, after a 
.f fork ,
the new child process inherits certain process characteristics,
concerned with
\*(sN
extensions, 
not specified by 
.pN .
See the fork(2) manpage for details.
.4 4 Errors
In addition to the EAGAIN conditions,
if a process is using the SGI parallel C facilities in library libmpc.a, a
.f fork
can fail with the ENOSPC or ENOLCK conditions. See the fork(2) 
manpage for details.
.3 2 "Execute a File"
.4 2 "Description"
.iD
On
\*(sN
systems, when constructing a pathname that identifies a new process image
file, if the 
.a file
argument does not contain a slash and the \fBPATH\fR environmental variable
is not present, the 
.a file
argument is searched for in the following default list of directories for 
normal users:
.sp
/usr/sbin
.br
/usr/bsd
.br
/sbin
.br
/usr/bin
.br
/usr/bin/X11
.sp
and for super users is searched for in the following default list of 
directories:
.sp
/usr/sbin
.br
/usr/bsd
.br
/sbin
.br
/usr/bin
.br
/etc
.br
/usr/etc
.br
/usr/bin/X11
.sp
These default paths are obtained from the file: \fB/etc/default/login\fR
at login time.  These default paths for normal and super users are inserted
into the \fB/etc/default/login\fR from \fB<paths.h>\fR.
.uS
On
\*(sN
systems,
the number of bytes available for the new process's combined argument and
environment lists is {ARG_MAX}.
Null terminators for each argument or environment string are 
included in this total.
.mV
On
\*(sN
systems, the new process image inherits from the calling process image
the process attributes defined by 
.pN 
as well as some additional attributes. 
See the exec(2) manpage for details.
.uS
On
\*(sN
systems, if the 
.f exec
function failed but was able to locate the 
\fIprocess image file\fR, the \fIst_atime\fR field
is not
marked for update.
.4 4 Errors
.rF
For the
.f exec
functions, \*(sN
detects the conditions and returns the corresponding errno value for
.Pe ENOMEM .
.2 2 "Process Terminations"
.3 1 "Wait for Process Termination"
.4 2 "Description"
.uS
If status information is available for two or more child processes,
the order in which their status is reported is undefined.
.iD
On 
\*(sN
systems, in addition to the circumstances specified by
.pN 
.f wait
or
.f waitpid
reports status when a child process traced with ptrace encounters a breakpoint.
See the ptrace(2) manpage.
Note that the ptrace mechanism is obsolescent and deprecated.
.iD
On
\*(sN 
systems,
when a parent process terminates without waiting for all of its child
processes to terminate, the remaining child processes are assigned
process 1 (init) as their parent process.
.uD
An application should not use the value of
.k stat_loc
if the process is interrupted by a signal after calling
.f wait
or
.f waitpid .
.3 2 "Terminate a Process"
.4 2 "Description"
.iD  
Children of a terminated process are assigned 
process 1 (init) as their parent process.
.2 3 "Signals"
.3 1 "Signal Concepts"
.4 1 "Signal Names"
.mV 
The following additional signals
occur in the \*(sN system:
.P
SIGTRAP 
.br
SIGEMT 
.br
SIGBUS
.br
SIGSYS
.br
SIGPWR
.br
SIGPOLL
.br
SIGIO
.br
SIGURG
.br
SIGWINCH
.br
SIGVTALRM
.br
SIGPROF
.br
SIGXCPU
.br
SIGXFSZ
.4 2 "Signal Generation and Delivery"
.uS
On \*(sN systems, if the action associated with a blocked signal is to ignore
the signal and that signal is generated for the process, the signal
is discarded immediately. 
.iD
On
\*(sN
systems, if a subsequent occurrence of a pending signal is generated, 
the signal is not delivered more than once.
.uS
On
\*(sN
systems,
the order in which multiple, simultaneously pending signals are delivered
to a process is undefined, and no assumptions should be made about it.
.iD
On
\*(sN
systems,
the following signals may be
generated which are not defined in
.pN :
.P
.ta 1i
SIGTRAP	Trace trap 
.br
SIGBUS	Bus error 
.br
SIGSYS	Bad argument to system call 
.br
SIGPOLL	Pollable event occurred 
.br
SIGIO	Input/output possible signal
.br
SIGURG	Urgent condition on IO channel 
.br
SIGWINCH	Window size changes 
.br
SIGVTALRM	Virtual time alarm 
.br
SIGPROF	Profiling alarm 
.br
SIGXCPU	Cpu time limit exceeded 
.br
SIGXFSZ	Filesize limit exceeded 
.P
See the signal(2) manpage for information about these signals.
.P
In addition, the following signals are defined and recognised but are never
internally generated by the current version of \*(sN, though they may be sent 
to a process by the
.f kill
function:
.P
SIGEMT        EMT instruction 
.br
SIGPWR        Power-fail restart 
.4 3 "Signal Actions"
The following actions occur in response to signals under
circumstances that are unspecified or undefined in 
.pN .
.P
(2) \fBSIG_IGN\fR--ignore signal
.uD
Applications should not ignore SIGFPE, SIGILL, or SIGSEGV signals. 
.uS
If a process sets the action for the SIGCHLD signal to SIG_IGN,
child processes of the process ignoring SIGCHLD do not create zombie 
processes when they exit, and a
.f wait
will return no information about them.
.uD
Applications should not return from a signal-catching function
for a SIGFPE, SIGILL, or SIGSEGV signal. 
.uS
If a process establishes a signal-catching function for the SIGCHLD
signal while it has a terminated child process for which it has not
waited, a SIGCHLD signal
is not
generated to indicate that the child process has terminated.
.3 2 "Send a Signal to a Process"
.4 2 "Description"
'\"On
'\"\*(sN
'\"systems,  
'\".c kill "pid, sig"
'\"has the following implementation-specific behavior:
.mV
When a receiving process's effective user ID has been altered 
through use of the S_ISUID mode bit (see \fB<sys/stat.h>\fR
sec. 5.6.1), 
\*(sN 
does 
permit the application to receive a signal sent by the parent process or
by a process with the same real user ID.
.90 3.3.2.2 "Delete the preceding paragraph."
.iD
If 
.a pid
is zero, 
.a sig
is sent to all processes whose process group ID is equal to the
process group ID of the sender, and for which the process has permission
to send a signal, excluding the following set of system processes:
.P
process 0 (sched)
.br
process 1 (init).
.br
.uS
If 
.a pid
is -1, 
.f kill
behaves as follows:
.br
If the effective user ID of the sender is not super-user,
.a sig 
will be sent to all processes excluding proc0 and proc1 whose 
real user ID is equal to the effective user ID of the sender.
.P
If the effective user ID of the sender is super-user, 
.a sig 
will be sent to all processes excluding proc0 and proc1.
.iD
If 
.a pid
is negative but not -1,
.a sig
will be sent to all processes whose process group ID is equal to the absolute 
value of pid.
.3 3 "Manipulate Signal Sets"
.uD
Applications should initialize the value of objects of type
.k sigset_t
before use.
If an object of type
.k sigset_t
is passed to
.f sigaddset ,
.f sigdelset ,
.f sigismember ,
.f sigaction ,
.f sigprocmask,
.f sigpending ,
or
.f sigsuspend
without first being initialized, the outcome is undefined. A random set of
signals may be indicated by the sigset_t object, depending on whatever bit 
pattern is in the uninitialized object.
.4 4 "Errors"
.rF
For the 
.f sigaddset ,
.f sigdelset ,
and
.f sigismember 
functions, 
\*(sN
detects 
the condition and
does 
return the corresponding errno value for
.Pe EINVAL .
.4 2
.uS
If the previous action for
.k sig
had been established by the
.f signal
function, defined in the C Standard, the values of the fields returned in
the structure pointed to by
.k oact
are:
.P
The sa_handler field is either one of the constants SIG_DFL or SIG_IGN, or
the address of the handler function installed by the 
.f signal
function.
.P
The other fields contain no information; their value in the current
implementation is zero.
.90 3.3.4.2 "An attempt to set the action for a signal that cannot \
be caught or ignored to SIG_DFL causes an error to be \
returned with errno set to [EINVAL]."
.3 5 "Examine and Change Blocked Signals"
.4 2 "Description"
.uD
On
\*(sN
systems,
if any of the SIGFPE, SIGILL, or SIGSEGV signals are generated while they
are blocked, unless the signal was generated by a call to the 
.f kill
function or the 
.f raise
function defined by the C Standard, the behaviour is undefined.  Applications
should not block these signals.
.3 6 "Examine Pending Signals"
.4 4 "Errors"
.iD
On \*(sN systems,
the following error conditions
are detected for the 
.f sigpending 
function:
.P
If the argument points to memory that is not a part of the calling process's 
valid address space,
.f sigpending
fails, and errno is set to EFAULT.
.90 3.3.6.4 "Change the preceding paragraph to .uS, not .iD."
.2 4 "Timer Operations"
.3 3 "Delay Process Execution"
.4 2 "Description"
.uS
If a SIGALARM signal is generated for the calling process during execution
of the 
.f sleep
function, and if the SIGALARM signal is being ignored 
or blocked from delivery,
.f sleep
does not 
return when the SIGALARM signal is scheduled. If the signal is being
blocked, the signal 
remains pending 
after the 
.f sleep
function returns. 
.uS
If a SIGALARM signal is generated for the calling process during execution
of the 
.f sleep
function except as a result of a prior call to the 
.f alarm
function, and if the SIGALARM signal is not being ignored or blocked from
delivery,
the signal terminates the process.
.uS
If a signal-catching function interrupts the 
.f sleep
function and examines or changes the time a SIGALRM is scheduled
to be generated, the action associated with the SIGALRM signal, or
whether the SIGALRM signal is blocked, the changes take effect at the
time the signal handler executes, overriding any previous settings
for SIGALRM.
.uS
If a signal-catching function interrupts the 
.f sleep
function 
and calls the 
.f siglongjmp
or
.f longjmp
function to restore an environment saved prior to the 
.f sleep
call, any SIGALRM signal pending from an
.f alarm
call will be delivered at the time specified by the 
.alarm
call.
.uS
If a signal-catching function interrupts the 
.f sleep
function 
and calls the 
.f siglongjmp
or
.f longjmp
function to restore an environment saved prior to the 
.f sleep
call and the
the process's signal mask is not restored as part of the
environment, the SIGALARM signal 
is not
blocked.
.nP
.dM
