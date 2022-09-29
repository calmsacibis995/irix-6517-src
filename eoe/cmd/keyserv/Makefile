#!smake
#
# makefile for secure RPC key server and utilities.
#
# NOTE WELL:  This is NOT suitable for export!
#

include	$(ROOT)/usr/include/make/commondefs

#BINS	= keylogout keylogin domainname chkey 
BINS	= keylogout keylogin chkey 
SBINS	= keyserv newkey   
TARGETS	= $(BINS) $(SBINS)

KEYSERV_OBJS = setkey.o detach.o key_generic.o
LIBMPOBJS    = pow.o gcd.o msqrt.o mdiv.o mout.o mult.o madd.o util.o
CHANGE_OBJS  = generic.o update.o

LLDLIBS = -lnsl
LLDOPTS = -Wl,-transitive_link

# define YP if you want to use update public keys via NIS
#LCDEFS	= -DYP
LCDEFS	= -D_SVR4_TIRPC -DPORTMAP
#OPTIMIZER= -g

FILES	= publickey

default all: $(TARGETS) 

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin -m 0555 -u root -g sys $(SBINS)
	$(INSTALL) -F /usr/bin  -m 0555 -u bin  -g bin $(BINS)
	$(INSTALL) -F /etc      -m 0644 -u root -g sys $(FILES)

#LIBRPC	= -lrpcsvc -lnsl

# default rules can handle these
#
#keylogout: keylogout.o 
#	$(CCF) -o $@ $@.o $(LDFLAGS) $(LDLIBS)
#
#keylogin: keylogin.o
#	$(CCF) -o $@ $@.o $(LDFLAGS) $(SHLIBS)
#
#domainname: domainname.o
#	$(CCF) -o $@ $@.o $(LDFLAGS) $(SHLIBS)

keyserv: $(KEYSERV_OBJS) $(LIBMPOBJS) keyserv.o
	$(CCF) -o $@ $@.o $(KEYSERV_OBJS) $(LIBMPOBJS) $(LDFLAGS) 

chkey: $(CHANGE_OBJS) $(LIBMPOBJS) chkey.o
	$(CCF) -o $@ $@.o $(CHANGE_OBJS) $(LIBMPOBJS) $(LDFLAGS) 

newkey:$(CHANGE_OBJS) $(LIBMPOBJS) newkey.o
	$(CCF) -o $@ $@.o $(CHANGE_OBJS) $(LIBMPOBJS) $(LDFLAGS) 

generic:$(LIBMPOBJS) generic.o
	$(CCF) -o $@ $@.o $(LIBMPOBJS) $(LDFLAGS) 

update:$(LIBMPOBJS) update.o
	$(CCF) -o $@ $@.o $(LIBMPOBJS) $(LDFLAGS) 

