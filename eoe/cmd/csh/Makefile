#!smake
#ident	"$Revision: 2.20 $"

# this is the internationalized csh

OBJECT_STYLE=N32_M3

include	$(ROOT)/usr/include/make/commondefs

TARGETS=csh

CFILES=	sh.printf.c sh.c sh.char.c sh.debug.c sh.dir.c sh.dol.c sh.err.c \
	sh.exec.c sh.exp.c sh.file.c sh.func.c sh.glob.c sh.hist.c sh.init.c \
	sh.lex.c sh.misc.c sh.parse.c sh.print.c sh.proc.c sh.sem.c sh.set.c \
	sh.wchar.c sh.wconst.c sh.time.c \
	compat.c libc.c

LCOPTS	=-fullwarn -non_shared -use_readonly_const -woff 1116
#
# Specify load addrs for text and data to keep them in same segment with libc
# to save memory.  This might have to be bumped up in the future as things
# grow.
#
LLDOPTS	=-Wl,-woff,84,-T,e000000,-D,e080000,-rdata_shared,-LD_LAYOUT:segalign=0x4000
LLDLIBS=-nostdlib -L$(ROOT)/usr/lib32/mips3/nonshared -L$(ROOT)/usr/lib32/nonshared -ltermlib -lw -lc_nomp
OPTIMIZER=-O3 -TARG:platform=ip22_4k
GLDOPTS=
#
# don't define TELL.
# Since csh is using wchar_t internally, there's no longer a 1:1
# relation between file ptr and position in csh's fbuf.
#
LCDEFS	= -D_BSD_COMPAT

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin csh
	$(INSTALL) -F /usr/bin -lns ../../sbin/csh csh

csh: csh.precord
	${CORD} $@.precord $@.fb -o $@

csh.precord:${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@
