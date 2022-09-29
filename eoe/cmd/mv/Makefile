#
# Makefile for mv(1), cp(1), and ln(1), all compiled from the same source.
#
#ident	"$Revision: 1.33 $"

include	$(ROOT)/usr/include/make/commondefs

# NOTE: we do NOT want mv/cp/ln compiled with shared libraries,
# because it makes it impossible to recover from moving shared
# libraries targets, dynamic linkers, etc. to a different place,
# perhaps for testing.  Without at least one program that doesn't
# use shared libraries, you have to boot a miniroot to recover.
# SO, do NOT change this back to shared libs.

CFILES= mv.c
TARGETS= mv cp ln
GLDOPTS=
LCOPTS=-non_shared
# Link text and data close together.
LLDOPTS=-nostdlib -L$(ROOT)/usr/$(LIB_LOCATION)/nonshared -Wl,-T,10000000,-N
LLDLIBS=-lgen -lc_nomp

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -o -F /sbin ln
	$(INSTALL) -ln ln -F /sbin mv
	$(INSTALL) -ln ln -F /sbin cp
	$(INSTALL) -lns ../../sbin/ln -F /usr/bin ln
	$(INSTALL) -lns ln -F /usr/bin cp
	$(INSTALL) -lns ln -F /usr/bin mv

cp ln: mv
	rm -rf $@; ln mv $@
