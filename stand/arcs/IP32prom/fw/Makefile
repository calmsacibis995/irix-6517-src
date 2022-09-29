#
# Makefile for Moosehead power-on self-test code
#
MHDEPTH= ..
include $(MHDEPTH)/mhcommondefs
SHELL=/bin/sh

# Define the LDZ macros.
CPUARCH_MI_LLOPTS=-32 -G 0 -allow_jump_at_eop
LDZ= $(LD) -rom -woff 15 $(LLLDOPT) $(CPUARCH_MI_LLOPTS) $(VLDFLAGS) $(ENDIAN)

# Define IP32SIM flags if environment variable IP32SIM set to YES.
NOX=\#
YESX=\#
$(IP32SIM)X=
$(YESX)LLLCDEFS=-DIP32SIM

# determind the configureation files
CONFIG_CF=IP32conf.cf
CONFIG_C=IP32conf.c
$(YESX)CONFIG_CF=IP32SIMconf.cf
$(YESX)CONFIG_C=IP32SIMconf.c

# TARGETS= fw $(LIBSIM) fw.bin
# Target link address
FW_RAM_ADDR= -T $(FW_TARGET)

TARGETS= fw fw.bin

ASFILES=\
	csu.s

CFILES=\
	main.c\
	finit.c\
	fsconf.c\
	$(CONFIG_C)\
	hello_tune.c\
	video.c\
	stubs.c

CFFILES=\
	$(CONFIG_CF)\
	fsconf.cf

# THIS IS A HACK.  FIX IT
POSTOBJS=\
	 post23.o

# LIBS=	$(LIBMSK) $(LIBSK) $(LIBSC) $(LIBMSK)
LIBMSK=	$(TARGETDIR)/libmsk.a
LIBS=	$(LIBMSK) $(LIBSK) $(LIBSC)

LLDOPTS= -m

default: $(TARGETS)
clean:   mhclean
	rm -rf $(CFFILES:.cf=.h) $(CFFILES:.cf=.c)
clobber: clean rmtargets
	rm -rf $(MKDEPFILE)
rmtargets: $(_FORCE)
	if [ -d $(TARGETDIR) ]; then cd $(TARGETDIR); rm -rf $(TARGETS) fw.map; fi

fw:	$(LIBS) $(OBJECTS)
	cd $(OBJECTDIR);\
		$(LDV) $(FW_RAM_ADDR) $(OBJECTS) $(POSTOBJS) $(LIBS) -o $@ > $@.map
	cd $(OBJECTDIR); $(SIZE) -x $@

fw.bin:	$(LIBS) $(OBJECTS)
	cd $(OBJECTDIR);\
	${LDZ} $(FW_RAM_ADDR) $(OBJECTS) $(POSTOBJS) $(LIBS) -o $@
	mv $(OBJECTDIR)/$@ .

env: env.o
	cd $(OBJECTDIR);\
		$(LDV) -non_shared env.o -o $@ > $@.map
	cd $(OBJECTDIR); $(SIZE) -x $@

env.s: env.tab
	@rm -fr env.s
	${CC} -E env.tab |\
	oawk 'BEGIN {printf(".data\n");}\
		 ($$3=="=YES") {\
			printf(".asciiz \"%s=",$$1);\
			if ($$2!="nullStr") {printf("%s\"\n",$$2);}\
			              else  {printf("\"\n");}\
		 }\
		 END {printf (".asciiz \"\\0\\0\"\n");}' >> env.s

debug:
	echo ${CC}

# $(LIBSIM): $(OBJECTS)
#	cd $(OBJECTDIR); $(AR) cru $@ $(OBJECTS)

include $(MHCOMMONRULES)
