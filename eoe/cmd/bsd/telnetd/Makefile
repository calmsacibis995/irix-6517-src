#!smake
#
# Make the telnetd daemon
#
# $Revision: 1.9 $

include $(ROOT)/usr/include/make/commondefs

CFILES=	global.c slc.c setenv.c state.c sys_term.c \
	telnetd.c termstat.c utility.c
	
TARGETS	=telnetd

LCOPTS	=-signed -woff 1116
LCDEFS	=-D_BSD_SIGNALS -DUSE_TERMIO -DNO_GETTYTAB -DNO_SETSID -DTSD \
	-DNEED_GETTOS 
LLDOPTS =-Wl,-T,7FC00000,-D,7FC40000
LLDLIBS	=-ltermlib

default:$(TARGETS)

install: default
	$(INSTALL) -F /usr/etc $(TARGETS)

include $(COMMONRULES)

telnetd: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

#---------------------------------------------------------------------------
# LINEMODE		Turns on support for the linemode option.
#
# KLUDGELINEMODE	The old linemode code.
#
# USE_TERMIO		Use a termios structure.
#
# SYSV_TERMIO		Use the System V termio structure. (implies USE_TERMIO)
#
# NEWINIT		Turns on the new init code for UNICOS systems.
#
# NO_GETTYTAB		This system does not have gettytab source.
#
# STREAMS		This system needs <sys/stream.h> for <sys/tty.h>
#			(Sun 4.0.3)
#
# FILIO_H		This system should use <sys/fileo.h> instead
#			of <sys/ioctl.h> (Sun 4.0.3)
#
# NO_CC_T		This system does not have a typedef for "cc_t"
#
# HAVE_fd_set		This system has a typedef for fd_set, but does
#			not have FDSET() defined.
#
# NO_SETENV		If you don't have setenv(3) and unsetenv(3)
#			in your library.
#
# NO_SETSID		If you don't have the POSIX setsid() call
#
# NO_STRERROR		If you don't have strerror(3)
#
# NO_STRING_H		If you don't have <string.h>, but have <strings.h>
#
# NO_LOGIN_P		If /bin/login doesn't understand the "-p"
#			(preserve environment) option.
#
# DIAGNOSTICS		Turns on diagnostic code; adds extra logic and
#			checks and debuging output if started with the
#			-D option.
#
# STREAMS		If the system has streams; causes <sys/stream.h>
#			to be included instead of <sys/tty.h>
#
# HAS_IP_TOS		Define this if you have the setsockopt() option for
#			setting the IP Type of Service bit (IP_TOS) and you
#			have the gettosbyname() function.
#
# NEED_GETTOS		Define this if you have the setsockopt() option for
#			setting the IP Type Of Service bits, but you don't
#			have the gettosbyname() function.  If you define
#			this, you don't need to define HAS_IP_TOS.
#
# TSD			Use telnetd streams driver. Only supported with 
#			all of the following options set: USE_TERMIO, 
#			NO_GETTYTAB.
#
# LIBS		Libraries to be included.  This will include
#		either -lcurses or -ltermcap, -lutil for 4.4bsd,
#		and -lnet for UNICOS5.0 and earlier.
#
# GETTYSRC	source picked up from getty(1). (get_date.c and gettytab.c)
# VPATH		Where to find ${GETTYSRC}
# GETTYOBJ	get_date.o and gettytab.o if ${GETTYSRC} is set.
# LIBEXEC	Where to install the executable
# LIBC		Where the C library is, for dependencies.
# 
#
# 4.3bsd:
# 	make -f Makefile.generic ${WHAT} \
# 		LIBS=-ltermcap \
# 		GETTYOBJ="get_date.o gettytab.o" \
# 		GETTYSRC="get_date.c gettytab.c" \
# 		VPATH=/usr/src/etc/getty
# 		LIBEXEC=${DESTDIR}/etc
# 		DEFINES="-DDIAGNOSTICS"
# 		LIBC=/lib/libc.a
# 
# 
# sun3.5:
# 	make -f Makefile.generic ${WHAT} \
# 		LIBS=-ltermcap \
# 		LIBEXEC=${DESTDIR}/usr/etc/in.telnetd \
# 		DEFINES="-DHAVE_fd_set -DNO_GETTYTAB -DDIAGNOSTICS \
# 			-DNO_SETENV -DNO_SETSID -DNO_STRERROR -DNO_LOGIN_P" \
# 		LIBC=/lib/libc.a
# 
# sun4.0.3c sun4.0:
# 	make -f Makefile.generic ${WHAT} \
# 		LIBS=-ltermcap \
# 		LIBEXEC=${DESTDIR}/usr/etc/in.telnetd \
# 		DEFINES="-DNO_GETTYTAB -DUSE_TERMIO -DNO_CC_T -DFILIO_H \
# 			-DSTREAMS -DDIAGNOSTICS -DNO_SETENV -DNO_SETSID \
# 			-DNO_STRERROR" \
# 		LIBC=/lib/libc.a
# 
# sun4.1:
# 	make -f Makefile.generic ${WHAT} \
# 		LIBS=-ltermcap \
# 		LIBEXEC=${DESTDIR}/usr/etc/in.telnetd \
# 		DEFINES="-DNO_GETTYTAB -DUSE_TERMIO -DFILIO_H -DSTREAMS\
# 			-DDIAGNOSTICS -DNO_SETENV -DNO_STRERROR" \
# 		LIBC=/lib/libc.a
