#!smake
#ident "$Revision: 1.12 $"

DEPTH=.
SM=:
include $(ROOT)/usr/include/make/libleafdefs

#if $(LIBRARY:Mlib*stub*) == ""

RTMOND64_SOURCES = \
	rtmond64_clnt.c \
	rtmond64_xdr.c \
	rtmond64_connect.c \
	$(NULL)

CFILES = \
	rtmon_log_user_tstamp.c \
	rtmon_client.c \
	$(RTMOND64_SOURCES) \
	decode.c \
	print.c \
	syscall.c \
	$(NULL)
	 
ASFILES = \
	atomicIncWithWrap.s \
	atomicDec.s \
	atomicInc.s \
	$(NULL)

HFILES = \
	rtmon.h \
	$(NULL)

LDIRT = \
	rtmond64.h \
	rtmond64_clnt.c \
	rtmond64_xdr.c \
	$(NULL)

RPCGEN = ${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen
RPCGENFLAGS = -P

#else

CFILES = rtlib-stub.c

#endif

LCDEFS+=-D_KMEMUSER
OPTIMIZER=-g

include $(LIBLEAFRULES)

headers!
	$(INSTALL) -m 444 -F /usr/include rtmon.h

exports install!
	$(INSTALL) -m 444 -idb "noship" -F /usr/include rtmon.h

# rtmond RPC ``source'' generation and dependencies.  We generate all of the
# rpcgen files separately because the only way to restrict the generated RPC
# protocol to UDP is to use the ``-s udp'' option which generates the server
# stubs ...  We don't bother generating the server interfaces since these are
# only here for clients which wish to talk to IRIX 6.4 (and earlier) rtmond's.
#
rtmond64.h: rtmond64.x
	$(RPCGEN) $(RPCGENFLAGS) -h $> > $@

rtmond64_clnt.c: rtmond64.x
	$(RPCGEN) $(RPCGENFLAGS) -l $> > $@

rtmond64_xdr.c: rtmond64.x
	$(RPCGEN) $(RPCGENFLAGS) -c $> > $@

$(RTMOND64_SOURCES:.c=.o): rtmond64.h
