#!smake
#ident	"$Revision: 1.12 $"

DEPTH=../..
include $(DEPTH)/Makedefs
include $(ROOT)/usr/include/make/librootdefs
LLDOPTS= -rpath ${INSTXFSMLIB}
LLDDSOLIBS= -ltclMotif -ltcl ../libxfsmclnt.so -lXt -lXm -lc 

GLDDSOOPTS= $(MKDEPOPT) $(LD_NO_UNRESOLVED) $(LD_QUICKSTART_INFO) $(DSONAMEOPT) $(DSOVERSIONOPT)

LCDEFS	= $(DEBUG)
LCOPTS	= -I../../include -I${TCLINCDIR}
TARGETS	= libxfsmtcl.so libxfsmtcl.a
CFILES	= xfsInit.c xfsObjects.c xfsInfo.c \
	  xfsFs.c xfsXlv.c xfsPartition.c xfsHost.c \
	  xfsmSort.c xfsmSortUtils.c xfsUtil.c transform.c

default: $(TARGETS)

include $(ROOT)/usr/include/make/commonrules

#Original 5.3 load line
#	$(LD) $(LDOPTS) -L${TCLLIBDIR} $(LDDSOOPTS) -o $@ libxfsmtcl.a $(LLDDSOLIBS) $(DEBUG_LIB)


libxfsmtcl.so: libxfsmtcl.a
	@rm -f $@;
	$(LD) $(LDOPTS) -L${TCLLIBDIR} -elf -shared -all -MDupdate Makedepend -no_unresolved -quickstart_info -soname libxfsmtcl.so -set_version sgi1.0 -o $@ libxfsmtcl.a $(LLDDSOLIBS) $(DEBUG_LIB)

libxfsmtcl.a: $(OBJECTS)
	@rm -f $@;
	$(AR) crls $@ $(OBJECTS)

install: default
	$(INSTALL) -u root -g sys -m 0644 -F ${INSTXFSMLIB} libxfsmtcl.so
