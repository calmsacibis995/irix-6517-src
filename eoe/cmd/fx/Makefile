#! smake
# Makefile for kernel version of fx, Silicon Graphics' disk formatter/exerciser.
#
#ident "$Revision cmd/fx/Makefile $"

BASEVERSION=n32bit
ALTVERSIONS=BE.O BE64.O
# WANTPARALLEL=yes-please


include ${ROOT}/usr/include/make/cmdcommondefs

# since we'll "never" do another mixed mode 32bit prom, 64 bit kernel,
# this stays "machine indepdenent".
BE_MACH=mach(MODE=32bit || CPUBOARD=IP19)
BE64_MACH=mach(MODE=64bit && !CPUBOARD=IP19)

# menu files 
MFILES = scsidata.M
CFILES= fx.c bb.c lb.c ex.c pt.c db.c scsi.c sgi.c io.c input.c \
	error.c qsort.c $(MFILES:.M=.c) $(XTRA)

LDIRT= ${MFILES:.M=.c} vers.c avers.c

COMMANDS= fx

.SUFFIXES: .M	# menu specifications

default: ${TARGETS}

include $(CMDCOMMONRULES)

# some machines from 3.1 early alphas, and 'all' 3.0 machines had
# fx in the volume header.  remove it because there isn't room anymore,
# and because the user could execute the wrong one.
# for asd, fx was mistakenly installed in vh in mfgr of system disks,
# so we still remove it for a while again, but be sure user sees no
# errors; do it at top level so we only get the one postop
n32bitinstall: default
	${INSTALL} -F /usr/bin fx

# 32 bit fx goes into CDROM volhdr as fx.ARCS, 64bit as fx.64
BE.Oinstall: default
	${INSTALL} -idb '${BE_MACH} postop("if test @$$instmode = @normal;then dvhtool -v d fx $$vhdev>/dev/null 2>&1;exit 0; fi")' -F /stand fx
	${INSTALL} -idb "sa ${NOSTRIP}" -src fx -F / 30fx.ARCS

BE64.Oinstall: default
	${INSTALL} -idb '${BE64_MACH} postop("if test @$$instmode = @normal;then dvhtool -v d fx $$vhdev>/dev/null 2>&1;exit 0; fi")' -F /stand fx 
	${INSTALL} -idb "sa ${NOSTRIP}" -src fx -F / 30fx.64

fx: ${OBJECTS}
	${LD_CMD} $(OBJECTS) -o $@ $(LLD_LIBS)
	$(LD_AUX_CMD)

vers.c: ${OBJECTS:N*vers.o}
	csh -f ./newvers.csh $@

avers.c: ${OBJECTS:N*vers.o}
	sh ./newvers.arcs $(TARGETS) ${RELEASE} "ARCS ${VERSION:S/.O//}" > $@

# this is primarily to handle the case where we want the .c file for
# a lint run.  Otherwise we could just have a .M.o rule
.M.c:
	$(NAWK) -f menucvt.awk $< > $*.c
