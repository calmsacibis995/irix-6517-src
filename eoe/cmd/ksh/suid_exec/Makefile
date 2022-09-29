#!smake
#	Makefile for ksh - suid_exec
#

include $(ROOT)/usr/include/make/commondefs

SUID    = suid_exec
TARGETS = $(SUID)

LCINCS	= -I../include
LCDEFS	= -DKSHELL -DPROTO
LCOPTS  = -signed
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default: $(TARGETS)

include $(COMMONRULES)

install: default 
	$(INSTALL) -F /sbin -m 4655 suid_exec
	$(INSTALL) -F /etc -lns ../sbin/suid_exec suid_exec

$(SUID):
