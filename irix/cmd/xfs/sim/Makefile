#!smake
#
# Makefile for libsim (xfs)
#
#ident	"$Revision: 1.53 $"

BASELIB=libsim_n32.a
ALTLIBS=libsim_n32d.a libsim_n32d_rep.a libsim_n32ns.a libsim_n32_rep.a \
	libsim_64.a libsim_64d.a
OTHERLIBS=libsim_o32d.a libsim_o32ns.a libsim_o32.a
include $(ROOT)/usr/include/make/librootdefs
TARGETS=$(LIBS)
default: $(TARGETS)

include $(LIBROOTRULES)

install: $(TARGETS)
