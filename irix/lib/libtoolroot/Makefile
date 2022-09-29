#!smake
#ident "$Revision: 1.3 $"

BASELIB=libtoolroot_32.a
ALTLIBS=libtoolroot_64.a libtoolroot_n32.a

PRIVATEFILES=

include $(ROOT)/usr/include/make/librootdefs
DSONAMEOPT=-soname libtoolroot.so

default!$(TARGETS)

include ${LIBROOTRULES}

install!headers exports

headers!
	echo "Nothing exported"

exports!default
	$(INST_32) -m 444 -src libtoolroot_32.so libtoolroot.so
	# 64bit shared libtoolroot
	$(INST_64) -m 444 -src libtoolroot_64.so libtoolroot.so
	# n32 shared libtoolroot
	$(INST_N32) -m 444 -src libtoolroot_n32.so libtoolroot.so
	$(INSTALL) -m 755 -F /usr/sbin toolrootsafe

