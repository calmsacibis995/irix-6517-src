#!smake
#
# Makefile for libconfigmon.a 
#
#ident "$Header: "

BASELIB = libconfigmon.a
ALTLIBS = libconfigmon_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

HEADERS_SUBDIRS = include

TARGETS = libconfigmon_n32_M3.a

include $(LIBROOTRULES)

default : $(TARGETS)

install : headers exports

headers : $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports : default
	$(INSTALL) -F /usr/lib32/internal -src libconfigmon_n32_M3.a \
	libconfigmon.a
