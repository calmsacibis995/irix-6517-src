#!smake
#
# Makefile for libusio.
#
#ident	"$Revision: 1.1 $"

BASELIB=libusio_32.a
ALTLIBS=libusio_n32.a libusio_64.a

NOSO=no-thank-you
include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(BASELIB) $(ALTLIBS)

default: $(TARGETS)

include $(LIBROOTRULES)

exports install! default
	$(INST_32) -m 444 -src libusio_32.a libusio.a
	$(INST_N32) -m 444 -src libusio_n32.a libusio.a
	$(INST_64) -m 444 -src libusio_64.a libusio.a
