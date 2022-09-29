#
# Makefile for Moosehead power-on self-test code
#
MHDEPTH= ..
include $(MHDEPTH)/mhcommondefs

# Define simulation flags if environment variable IP32SIM set to YES
LOCALDEFINES=
NOX=\#
YESX=\#
$(IP32SIM)X=

$(YESX)LOCALDEFINES=-DIP32SIM
ASFILES=\
	sl_csu.s        \
	sl_stackinit.s

CFILES=\
	sloader.c		\
	fw_callback.c

LIBMSK= $(TARGETDIR)/libmsk.a
LIBS=   $(LIBMSK) $(LIBSK) $(LIBSC)

LDVS=	$(TOOLROOT)/usr/bin/ld -B static -r -d -G 0 $(LLDOPTS)

LLDOPTS= -v -T bfc00000 
LLLDOPT= -32 -T bfc00000 $(LIBS)
LLLCDEFS=-D__STDC__=1 -D_KERNEL $(LOCALDEFINES)
CALLBK= $(MHDEPTH)/include/fwcallback.h
SLOADER=$(TARGETDIR)/sloader

TARGETS= $(SLOADER) $(CALLBK)

default: inspect
clean:   mhclean
	rm -f $(SLOADER) $(CALLBK) sloader.bin
clobber: clean rmtargets
	rm -rf $(MKDEPFILE)
rmtargets: $(_FORCE)
	if [ -d $(TARGETDIR) ]; then cd $(TARGETDIR); rm -rf $(TARGETS) sloader.map; fi

$(SLOADER): $(OBJECTS) $(LIBSK)
	cd $(TARGETDIR); $(LDVS) $(OBJECTS) $(LIBS) -o sloader -m > $@.map

	size -x $(TARGETDIR)/$@

$(CALLBK):	sloader.bin
	rm -f $@
	@${NM} -B sloader.bin |\
	oawk '(($$3=="Halt")||($$3=="Restart")||($$3=="Reboot")||\
	       ($$3=="PowerDown")||($$3=="EnterInteractiveMode"))\
	       {printf("#define FWCB_%s (void(*)(void))0x%s\n",$$3,$$1);}' >$@

sloader.bin: $(OBJECTS) $(LIBSK) $(LIBSC)
	rm -f $(CALLBK) sloader.bin
	cd $(TARGETDIR);\
	$(TOOLROOT)/usr/bin/ld -v -rom -G 0 -T bfc00000 $(OBJECTS) $(LIBS) -o sloader.bin
	mv $(TARGETDIR)/sloader.bin .

# Inspect checks that the vectors are properly located in the image.
inspect: $(TARGETS)
	@$(NM) -B $(TARGETDIR)/sloader | grep bev_.*_vector | $(INSPECT)

install: default

include $(MHCOMMONRULES)
