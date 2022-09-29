# Makefile libsc/lib
# 
# This directory contains the core of the standalone C runtine support
# library.  Routines here are generally supposed to conform to ANSI C
# and POSIX.
#
# $Revision: 1.50 $

SCDEPTH=..
include ${SCDEPTH}/libscdefs

#
# Set up to build cpu architecture and endian dependent objects
OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

ASFILES=dummyfunc.s libasm.s setjmp.s
CFILES= large.c arcs_stub.c arcs_stub2.c atob.c atobu.c auto.c bootname.c \
	btoa.c	\
	check_dbg.c cmn_err.c ctype.c errputs.c expand.c getenv.c	\
	getpath.c idbg_stub.c inetaddr.c kernname.c malloc.c		\
	mem.c menu.c mp.c panel.c parser.c gpdata.c			\
	path.c pause.c perror.c privstub.c privstub2.c privstub3.c 	\
	random.c rb.c rbutil.c restart.c screen.c scsaio.c invfind.c	\
	setenv.c stdio.c strcasecmp.c stringlist.c strnstuff.c		\
	strstuff.c spinlock.c spinner.c stdio_lock.c privstub4.c eimode.c

# idbg.o is used if fx.DBG or sash.DBG is built, but it shouldn't be
# put into the libraries.
EXTRAOBJ=idbg.o
LDEFAULT=$(EXTRAOBJ)

include $(LIBSCRULES)
