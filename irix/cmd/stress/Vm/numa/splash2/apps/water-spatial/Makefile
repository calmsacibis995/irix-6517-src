#!smake
OBJECT_STYLE=64_M3
SPLASH_ROOT=../..
include $(ROOT)/usr/include/make/commondefs
include $(SPLASH_ROOT)/macros/defs

APPL = water-spatial
OBJS = water.o mdmain.o syscons.o kineti.o bndry.o cnstnt.o initia.o \
       interf.o intraf.o cshift.o poteng.o predcor.o
SHFILES = runtests.sh 
DATFILES = input.dat random.in.dat
CFLAGS = -O2  -mips3 -64 -DNUMA
LDFLAGS = -lm 
LDIRT = *.c *.h *~

TARGETS= $(APPL) ${SHFILES:.sh=} ${DATFILES:.dat=}

default: $(TARGETS)

$(APPL): $(OBJS)
	$(CCF) $(OBJS) -o $(APPL) $(GLDOPTS) $(LDFLAGS)

install: $(TARGETS)
	$(INSTALL) -idb "nostrip" -F  \
	 /usr/stress/Vm/numa/splash2/apps/water-spatial $(TARGETS)
	$(INSTALL) -idb "nostrip" -F  \
	 /usr/stress/Vm/numa/splash2/apps/water-spatial README.water-spatial

include $(SPLASH_ROOT)/macros/rules
include $(COMMONRULES)

cnstnt.c: cnstnt.C water.h wwpot.h cnst.h frcnst.h fileio.h parameters.h
water.c: water.C parameters.h mdvar.h water.h wwpot.h cnst.h mddata.h \
         fileio.h frcnst.h randno.h global.h split.h
mdmain.c: mdmain.C  parameters.h mdvar.h water.h wwpot.h cnst.h \
          mddata.h fileio.h frcnst.h randno.h global.h split.h
syscons.c: syscons.C parameters.h mdvar.h water.h wwpot.h cnst.h mddata.h
kineti.c: kineti.C global.h split.h mdvar.h parameters.h mddata.h
bndry.c: bndry.C split.h mdvar.h parameters.h mddata.h
initia.c: initia.C split.h mdvar.h water.h cnst.h fileio.h \
           parameters.h mddata.h
predcor.c: predcor.C split.h mdvar.h parameters.h mddata.h
intraf.c: intraf.C frcnst.h mdvar.h water.h wwpot.h parameters.h \
           mddata.h global.h split.h
interf.c: interf.C mdvar.h water.h wwpot.h cnst.h global.h split.h \
           parameters.h mddata.h
cshift.c: cshift.C water.h
poteng.c: poteng.C global.h split.h mdvar.h frcnst.h water.h wwpot.h \
           parameters.h mddata.h
