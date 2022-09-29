#!smake
#
#ident  "$Revision: 1.33 $"
#
# lib/libcurses/screen makefile
#

BASELIB=libcurses_32.a
ALTLIBS=libcurses_32_abi.a libcurses_32_ns.a libcurses_64.a \
	 libcurses_64_ns.a libcurses_n32_ns.a libcurses_n32.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

PRIVATEFILES=-x Makedepend
BASELIBA=libcurses.a
BASELIBSO=libcurses.so 
TOOLS= tic tput captoinfo infocmp dumpti chgtinfo llib-lcurses.ln
SRCDIR=src


include	$(ROOT)/usr/include/make/librootdefs

LLDDSOOPTS=-LD_LAYOUT:segalign=0x4000
DSONAMEOPT=-soname libcurses.so
LLDLIBS=-lc

default: $(TARGETS) tools

include	$(LIBROOTRULES)

install: headers exports

exports: default $(_FORCE)
	$(INSTALL) -F /usr/lib/lint -src src/llib-lcurses llib-lcurses
	$(INSTALL) -F /usr/lib/lint -src src/llib-lcurses.ln llib-lcurses.ln
	$(INSTALL) -F /usr/bin -src src/tic tic
	$(INSTALL) -F /usr/bin -src src/captoinfo captoinfo
	$(INSTALL) -F /usr/bin -src src/infocmp infocmp
	$(INSTALL) -F /usr/bin -src src/tput tput
	# Install libraries
	$(INST_32) -m 444 -src libcurses_32.a libcurses.a
	$(INST_32) -m 444 -src libcurses_32.so libcurses.so
	$(INST_32) -lns $(BASELIBSO) "libtermcap.so libtermlib.so"
	$(INST_32) -lns $(BASELIBA)  "libtermcap.a libtermlib.a"
	$(INST_32_NS) -m 444 -src libcurses_32_ns.a libcurses.a
	$(INST_32_NS) -lns $(BASELIBA) "libtermcap.a libtermlib.a"
	# ABI version
	$(INST_32_ABI) -m 444 -src libcurses_32_abi.a libcurses.a
	# 64bit shared libcurses
	$(INST_64) -m 444 -src libcurses_64.a libcurses.a
	$(INST_64) -m 444 -src libcurses_64.so libcurses.so
	$(INST_64) -lns $(BASELIBSO) "libtermcap.so libtermlib.so"
	$(INST_64) -lns $(BASELIBA) "libtermcap.a libtermlib.a"
	# 64bit non_shared libcurses
	$(INST_64_NS) -m 444 -src libcurses_64_ns.a libcurses.a
	$(INST_64_NS) -lns $(BASELIBA) "libtermcap.a libtermlib.a"
	# n32bit shared libcurses
	$(INST_N32) -m 444 -src libcurses_n32.a libcurses.a
	$(INST_N32) -m 444 -src libcurses_n32.so libcurses.so
	$(INST_N32) -lns $(BASELIBSO) "libtermcap.so libtermlib.so"
	$(INST_N32) -lns $(BASELIBA) "libtermcap.a libtermlib.a"
	# n32bit non_shared libcurses
	$(INST_N32_NS) -m 444 -src libcurses_n32_ns.a libcurses.a
	$(INST_N32_NS) -lns $(BASELIBA) "libtermcap.a libtermlib.a"

headers: $(_FORCE)
	cd src; ${MAKE} $@
	$(INSTALL) -F /usr/include -m 444 -src src/curses.h curses.h
	$(INSTALL) -F /usr/include -m 444 -src src/term.h term.h
	$(INSTALL) -F /usr/include -m 444 -src src/unctrl.h unctrl.h

$(TOOLS) tools:
	(cd ${SRCDIR}; ${MAKE} $${RULE:=$@})
