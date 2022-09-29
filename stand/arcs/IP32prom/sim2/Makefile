#
# Makefile for IP32 simulation
#
MHDEPTH=..
include $(MHDEPTH)/mhcommondefs

OBJECTDIR=.

LCINCS= -I$(ROOT)/usr/include\
	-I$(MHDEPTH)/include\
	-I$(DEPTH)/include

# Override CPU_ARCH_MD_xxOPTS as they define -mips3 operation.
CPUARCH_MD_COPTS=
CPUARCH_MD_ASOPTS=

LLDIRT= *.map SIMARENA

LIBS= $(LIBSIM) $(LIBMSK) $(LIBSK) $(LIBSC)

TARGETS= uprom simServer simEtherXR

CL_CFILES=\
	clMain.c\
	clSim.c\
	clForward.c
SVR_CFILES=\
	svrCrime.c\
	svrMain.c\
	svrCop0.c\
	svrFlash.c\
	svrK0K1.c\
	svrMaceEther.c\
	svrMaceSerial.c\
	svrMacePCI.c\
	svrRtc.c\
	svrUST.c\
	svrSim.c
XR_CFILES=\
	simEtherXR.c
COMMON_CFILES=\
	simCommon.c

CFILES=	$(CL_CFILES)\
	$(SVR_CFILES)\
	$(COMMON_CFILES)

CLOBJS=  $(CL_CFILES:.c=.o) $(COMMON_CFILES:.c=.o)
FLASHOBJS=\
	$(TARGETDIR)/flash.o\
	$(TARGETDIR)/flashwrite.o
SVROBJS= $(SVR_CFILES:.c=.o) $(COMMON_CFILES:.c=.o) $(FLASHOBJS)


default $(COMMONTARGS): $(COMMONPREF)$$@

mhdefault: $(TARGETS)

# The -u post1 forces the post1 code in.
# The -u us_delay forces stubs.o in early, otherwise delay.o is loaded
# and it depends on other unimplemented code.

uprom:	$(CLOBJS) $(LIBMSK) $(LIBSIM) $(DEPLIB)
	$(CCF) $(CLOBJS)\
	 -u post1 -u us_delay -u _main -u standcfg -u _fs_table\
	 -u cpu_get_tod \
		$(LIBS) -o $@ -m > $@.map
	$(FORCE_MIPS2) $@
	size -x $@

simServer: $(SVROBJS)  
	$(CCF) -g $(SVROBJS) -o $@ -m > $@.map

simEtherXR: simEtherXR.c
	$(CCF) -g simEtherXR.c -o $@

include $(MHCOMMONRULES)





