#
# Makefile for Moosehead power-on self-test code
#
MHDEPTH= ..
include $(MHDEPTH)/mhcommondefs

#
# We need to redefine CPUARCH_MI_LDOPTS macros here to avoid generating
# an coff binary - see bugs 311840 filed by Kuang at 11/07/95, although
# mti closed the bug but they did not provide a fix for it.
#
CPUARCH_MI_LLOPTS=-32 -mips3 -G 0 -allow_jump_at_eop
LLLCINCS=-I$(MHDEPTH)/debugcard/include
LDZ= $(LD) $(LLLDOPT) $(CPUARCH_MI_LLOPTS) $(VLDFLAGS) $(ENDIAN)
ASM= $(MHDEPTH)/tools/asmtool
IMG= $(MHDEPTH)/tools/dumptext
MV = mv -f
LIBmsk= $(TARGETDIR)/libmsk.a $(LIBSK)

# PTCI = $(TARGETDIR)/IP32tci.o

#
# Define simulation local flags.
#
NOX=\#
YESX=\#
$(IP32SIM)X=
$(YESX)LLLCDEFS=-DIP32SIM

#
# We put all post objects into LIBSIM2 for simulation use
# We build post1 to build images
#
# TARGETS= $(LIBSIM) post1 post1.bin
# 	mte_asm.s       <--- moved to ---> lib/libsk/ml
#	mte_copy.c      <--- moved to ---> lib/libsk/ml
#	mte_stubs.c     <--- moved to ---> lib/libsk/ml

TARGETS= post1 post2 post3

ASFILES=\
	post1.s\
	post1csu.s\
	post1asmsupt.s\
	post1memtst.s\
	post1kseg1.s\
	post23.s

CFILES=\
	post1mem.c\
	ser_post.c\
	post1diags.c\
	st16c1451.c\
	DBCuartsim.c\
	DBCuartio.c

POST1OBJS=\
	post1.o\
	DBCuartio.o\
	st16c1451.o\
	DBCuartsim.o\
	post1memtst.o\
	ser_post.o\
	post1mem.o\
	post1kseg1.o

#
# post1asmsupt.o have to be the first one in the POST1KSEG1OBJS
# macro, please do not re-arrange the order.
#	DBCuartsim.o\
#
POST1KSEG1OBJS=\
	post1csu.o\
	post1asmsupt.o\
	ser_post.o\
	st16c1451.o\
	DBCuartsim.o\
	DBCuartio.o\
	post1diags.o


# POST1VA1= bfc04400, 6.3(3) ld will round 0x4400 to 0x5000, we'll use
# post1fill.s to fillup the 0x400 space and this is only for post1.bin
# and its sole purpose is to generate a dis file for debugging.
POST1VA1= bfc04000
POST1VA2= a0004000

VCOPTS= -mips3

default: $(TARGETS)
clean:   mhclean
	rm -fr post1kseg1 post1.bin
clobber: clean rmtargets
	rm -rf $(MKDEPFILE)
rmtargets: $(_FORCE)
	if [ -d $(TARGETDIR) ]; then cd $(TARGETDIR);\
	rm -rf $(TARGETS) post1.map post1kseg1* ; fi

post1kseg1.s: $(POST1KSEG1OBJS) 
	cd $(OBJECTDIR)	;\
	${LDZ} -rom -B static -N -T $(POST1VA2) $(POST1KSEG1OBJS) -o $(@:.s=) ;\
	${IMG}  $(@:.s=) > $(@:.s=.images) ;\
	${ASM}  $(@:.s=.images) > $@
	${MV}	$(OBJECTDIR)/$(@:.s=) .
	${MV}	$(OBJECTDIR)/$@ .

post1:	$(OBJECTS) $(LIBmsk)
	cd $(OBJECTDIR); $(LDV) -m $(POST1OBJS) $(LIBmsk) -o $@ > $@.map
	cd $(OBJECTDIR); $(SIZE) -x $@
	@if test -f post1kseg1.s ; then						 \
		rm -f post1kseg1.s.O  $(OBJECTDIR)/post1kseg1.o	;\
        mv post1kseg1.s post1kseg1.s.O					;\
	fi

post1.bin:	post1fill.o $(POST1OBJS) $(LIBmsk)
	cd $(OBJECTDIR)	;\
	$(LDZ) -rom -B static -N -T $(POST1VA1) post1fill.o $(POST1OBJS) $(LIBmsk) -o $@
	mv $(OBJECTDIR)/$@ .

post2:	post23.o
	echo "No post2 at the moment."

post3:	post23.o
	echo "No post3 at the moment."


# $(LIBSIM): $(OBJECTS)
#	cd $(OBJECTDIR); $(AR) cru $@ $(OBJECTS)

include $(MHCOMMONRULES)
