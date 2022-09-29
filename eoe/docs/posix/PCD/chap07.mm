'\" pcd/chap07.mm 1.5 99/01/20 (C) Copyright 1999 Silicon Graphics, Inc.
.1 7 "Device- and Class-Specific Functions" "Device & Class Functions"
.2 1 "General Terminal Interface"
.iD
The
\*(sN
system supports asynchronous ports
using the General Terminal Interface defined in
.pN .
.P
Network connections supporting this interface may also be established by
use of pseudo-tty devices: see the pty(7M) man page.
.rC " (choose appropriate members of the above list; we suggest you also list \
which of these you do not support) "
.3 1 "Interface Characteristics"
.90 7.1.1.2 "It is unspecified whether the terminal has a foreground process \
group when there is no longer any process whose process group ID matches \
the process group ID of the foreground process group, but there is a \
process whose process ID matches."
.4 3 "The Controlling Terminal"
.iD
On
\*(sN
systems,
the controlling terminal for a session is allocated by the session leader
in the following manner:
the first terminal device opened by the process becomes the controlling
terminal for the session, unless the O_NOCTTY flag is given in the
.f open
call.
.iD
On
\*(sN
systems,
if a session leader has no controlling terminal, and opens a terminal
device that is not already associated with a session without using the
O_NOCTTY option, the terminal does 
become the controlling terminal of the session leader.
.90 7.1.1.3 "Upon the close of the last file descriptor in the system \
(whether or not it is in the current session) associated with the \
controlling terminal, all processes that had \
the terminal as their controlling terminal (do / do not) cease to have \
any controlling terminal."
.90 7.1.1.3 "A session leader that has lost its controlling terminal \
because all file descriptors associated with that terminal were \
closed can acquire a new controlling terminal by:"
.mV
When the controlling process has terminated, subsequent access
to the terminal by other processes in the earlier session
is denied.
.4 5 "Input Processing and Reading Data"
.mV
\*(sN
does not impose the limit
.Pl MAX_INPUT 
on the number of bytes that may be stored in the input queue: buffer memory is
allocated dynamically by the system as needed.  However, available memory is
finite, and incoming characters may be discarded if memory cannot be allocated
to buffer them. 
Programmers are advised to respect the MAX_INPUT parameter.
.4 6 "Canonical Mode Input Processing"
.iD
.Pl MAX_CANON
is defined for asynchronous tty ports.
.iD
When there are
.Pl MAX_CANON
characters in the input queue for a given terminal device, 
\*(sN
discards subsequent input characters.
.4 7 "Non-Canonical Mode Input Processing"
.iD
In
\*(sN
it is not possible for
the MIN member of the \fIc_cc\fP array to be greater than
.Pl MAX_INPUT , 
since it is actually an unsigned char: its max value is thus equal 
to {MAX_INPUT} .
.90 7.1.1.7 "The preceding paragraph is undefined, not impl-def."
.4 8 "Writing Data and Output Processing"
.iD
\*(sN
provides a buffering mechanism for writing to a terminal device: a
.f write
call may complete before all of the characters it sends have been physically
transmitted.
.4 9 "Special Characters"
.iD
On
\*(sN
systems, the START and STOP characters
may not
be changed.
.uD
If two or more of the special characters 
listed in section  7.1.1.9 of
.pN
have the same value, the behaviour is driver-dependent and may vary from
one version of the system to another.  Programmers should avoid creating
this condition.
.90 7.1.1.10 "(unspecified) If modem control and job control are supported, \
and the [EIO] condition specified in 6.4.1.4 exists, read() \
returns ( the EOF condition / -1 with errno set to [EIO] )."
.3 2 "Settable Parameters"
.4 1 "\fItermios\fP Structure"
.mV
On
\*(sN
systems, 
in addition to the fields required by
.pN ,
the \fItermios\fP structure includes the field c_line, which specifies 
line discipline behaviour.  A zero value of this field selects a line
discipline implementing the old system V behaviour; a value of one
(the default) selects the normal job control line discipline. 
No other values are currently defined.
.iD
.rC " (enter termios structure size here) "
.90 7.1.2.1 "No mention of the size of struct termios."
.4 2 "Input Modes"
.iD
On
\*(sN
systems, in contexts other than asynchronous serial data transmission, a break
condition is defined as follows:
.P
In the case of pseudo-ttys as described in the pty(7M) man page, a break
condition established on the master side by
.f tcsendbreak
is internally propagated by the system so that the slave side will behave
analogously to an asynchronous terminal receiving a break.
.P
A break condition is not defined for devices other than asynchronous ports
and pseudo-ttys; an attempt to send one with
.f tcsendbreak
will fail and set errno to einval.
.iD
If IXON is set,
\*(sN
transmits a STOP character when the number of bytes in the input queue
reaches a limiting value: currently 256.
.P
A START character is sent when a subsequent
.f read
operation removes any characters from the input queue.
.iD
The initial input control value after 
.f open
is ICRNL|IXON|IXANY|BRKINT|IGNPAR|ISTRIP.
.rC "(Supply the value here)"
.4 3 "Output Modes"
.iD
The initial output control value after
.f open
is OPOST|ONLCR|TAB3.
.rC " (insert initial output value) " 
.rF
Additional output control values have been defined which are used 
when OPOST is set. Refer to the termio(7) man page
for further description of these extensions.
.rC "(insert specific documentation references if possible)"
.4 4 "Control Modes"
.mV
On
\*(sN
systems, if an object for which the control modes are set is not
an asynchronous serial port, the following modes are ignored:
.P
CLOCAL
.br
CREAD
.br
CSIZE
.br
CSTOPB
.br
PARENB
.br
PARODD
.iD
On
\*(sN
systems, the initial hardware control value after
.f open
is CS8|HUPCL|CLOCAL.
.rC " (insert initial control value here) "
.4 5 "Local Modes"
.iD
When ECHOE and ICANON are set, and the ERASE character attempts to erase
when there is no character to erase, 
\*(sN
echoes nothing.
.iD
If ECHOK and ICANON are set, the KILL character
causes the terminal to erase the line from the display.
.iD
If IEXTEN is set, 
no additional functions are recognized from the input data; IEXTEN is
not currently used.
.iD
IEXTEN being set has no effect on the behaviour of ICANON, ISIG, IXON, or 
IXOFF. 
.iD
The initial local control value after
.f open
is ISIG|ICANON|ECHO|ECHOK.
.rC " (enter initial control value here) "
.4 6 "Special Control Characters"
.jC
\*(sN
does not support job control and it 
.oP
ignores / does not ignore
.oE
the SUSP character value when it is input.
.iD
The number of elements in the \fIcc_c\fP array is 31.
.rC " (insert number of elements here)"
.90 7.1.2.6 "NCCS is unspecified, not impl-def."
.mV
\*(sN
does not support changing the START and STOP
characters, and it 
ignores
the character values in the \fIc_cc\fP array indexed by the VSTART and
VSTOP subscripts, when
.f tcsetattr
is called.
.iD
An
\*(sN
terminal when initially opened is always in canonical mode. Non-canonical 
mode may be set by appropriate use of the
.f tcsetattr
function.
The initial values of control characters are defined as follows:
.P
.ta .75i
VEOF	control 'd' (004)
.br
VEOL	0 
.br
VERASE	control 'h' (008)
.br
VINTR	delete (0177)
.br
VKILL	control 'u' (025)
.br
VQUIT	control backslash (034)
.br
VSUSP	control 'z' (032)
.br
VSTART	control(q)  (021)
.br
VSTOP	control(s)  (023)
.4 7 "Baud Rates"
.5 2 "Description"
.iD
On
\*(sN
systems, if an attempt is made to set an unsupported baud rate, 
.f cfsetispeed ,
.f cfsetospeed ,
and
.f tcsetattr
return -1 and set errno to EINVAL.
.90 7.1.3.2 "Eliminate tcsetattr() from the list above."
.4 8 "Errors"
.uS
For the
.f cfgetispeed 
and
.f cfgetospeed ,
functions,
\*(sN
detects no error conditions.
.P
For the
.f cfsetispeed ,
and
.f cfsetospeed
functions, an invalid speed parameter will cause a return of -1 and set
errno to EINVAL.
.2 2 "General Terminal Interface Control Functions"
.3 2 "Line Control Functions"
.4 2 "Description"
.iD
On
\*(sN
systems,
the parameter
.a duration
to
.f tcsendbreak 
is ignored; the function causes transmission of a continuous stream of 
zero-valued bits for .25 seconds.
.iD
If a terminal is not using asynchronous serial data transmission, the 
.f tcsendbreak
function sends data to generate a break condition only on a pseudo-tty device.
On any other device types, the
.f tcsendbreak
function will fail, and set errno to EINVAL.
.3 3 "Get Foreground Process Group"
.4 2 "Description"
.jC
\*(sN
.oP
supports / does not support 
.oE
the
.f tcgetpgrp
function.
.3 4 "Set Foreground Process Group ID"
.4 2 "Description"
.jC
\*(sN
.oP
supports / does not support 
.oE
the
.f tcsetpgrp
function.
.nP
.dM
