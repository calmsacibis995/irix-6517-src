#!${TOOLROOT}/bin/make
# Makefile to generate Moosehead images
#
MHDEPTH=..
include $(MHDEPTH)/mhcommondefs

# TARGETS=  sloader.bin post1.bin sloader.hex flash.hex flashReload.hex
# TARGETS=  sloader.hex flash.hex flashReload.hex post1.bin 

LLDIRT=flash.image flash.elf

# Common flash arguments for FB
FLASHARGS= -s 0x80000 -p 0x200

# Serial loader arguments for FB
SL=				$(TARGETDIR)/sloader
SL_VERSION= 	1.0
SL_LENGTH=		0x4000
SL_ARGS=		-N sloader -V $(SL_VERSION) -x 0 -l $(SL_LENGTH) $(SL)

# Post1 arguments for FB
POST1= 		$(TARGETDIR)/post1
POST1_VERSION= 	1.0
POST1_ARGS= 	-N post1 -V $(POST1_VERSION) -x 0 $(POST1)

# Firmware arguments for FB
FW=				$(TARGETDIR)/fw
FW_RAM_ADDR=	0x$(FW_TARGET)
FW_VERSION=		4.3

# Following macros are used to build a unmapped uncached IP32PROM
YES_X=\#
$(DEBUGR10K)_X=

$(YES_X)FW_RAM_ADDR=	0xa1000000
$(YES_X)FW_VERSION=		MFG-v0.0

FW_ARGS=		-N firmware -V $(FW_VERSION) -r $(FW_RAM_ADDR) $(FW)
VHARDWARE=		O2 R4600/R5000/R10000
VSOFTWARE=		IRIX 6.3 IP32prom IP32PROM-v4

# Environment arguments for FB
#
# Note that we reserve 1KB for the ENV section of the flash using this
# data but it's only a dummy.  The firmware itself initializes the env.
ENV=			/dev/null
ENV_VERSION=	1.0
ENV_ARGS=		-N env -V $(ENV_VERSION) -d -l 1024 $(ENV)

# Firmware version segment
VER=			$(TARGETDIR)/version
VER_VERSION=	$(FW_VERSION)
VER_ARGS=		-N version -V $(VER_VERSION) -d $(VER)

ALL= $(SL) $(ENV) $(POST1) $(FW) $(VER)  
RELOAD= $(ENV) $(POST1) $(FW) $(VER) 


default $(COMMONTARGS): $(COMMONPREF)$$@
mhdefault: $(TARGETS)


# Build the hex file for just the serial loader
sloader.hex: $(SLOADER)
	$(FB) $(FLASHARGS) -v 0xbfc00000 -h $@ $(SL_ARGS)

# Build the hex file for a full FLASH image.
flash.hex: $(ALL)
	$(FB) $(FLASHARGS) -v 0xbfc00000 -h $@ -i flash.image -e flash.elf\
		$(SL_ARGS) $(ENV_ARGS) $(POST1_ARGS) $(FW_ARGS) $(VER_ARGS)

# Build the hex file for a reload image, i.e. an image that reloads everything
# except the serial loader.
flashReload.hex: $(RELOAD)
	$(FB) $(FLASHARGS) -v 0xbfc04000 -b 0xbfc04000 -h $@\
		$(ENV_ARGS) $(POST1_ARGS) $(FW_ARGS) $(VER_ARGS)

# Go ahead build a elf version of post1 image, its safe to do it here.
post1.bin: flash.hex $(POST1)
	cd $(MHDEPTH)/post ; make $@

# Go ahead build a elf version of sloader image, its safe to do it here.
sloader.bin: sloader.hex $(SL)
	cd $(MHDEPTH)/sloader ; make $@

# Go ahead build a elf version of Fireware image, its safe to do it here.
$(TARGETDIR)/version: version.s
	$(TOOLROOT)/usr/bin/as version.s -o $@.o
	$(TOOLROOT)/usr/bin/ld -r $@.o -o $@

#	                  printf("VERSION %s\\n",$$2);
version.s: Makefile $(SL) $(POST1) $(FW)
	rm -f version.s;\
	echo ".set noreorder\n.set nomacro\n.rdata\n" > $@;\
	/sbin/date | oawk '{printf(".asciiz \"VSEGOK\0\0%s \\n", $$0);}' >>$@
	cat ./Makefile | oawk '($$1=="FW_VERSION=") {\
	                  printf("VERSION $(FW_VERSION)\\n");\
	                  printf("$(VHARDWARE)\\n");\
	                  printf("$(VSOFTWARE)\\n");\
	                  printf("\"\n");}' >>$@

ip32prom.image: flash.golden
	rm -f ip32prom.image
	cd $(MHDEPTH)/tools/flashcreate; make default
	$(MHDEPTH)/tools/flashcreate/flashbuild -c R4600,R4600SC,R5000,R5000SC,R10000 -o ip32prom.image flash.golden

install: ip32prom.image
	$(INSTALL) -idb 'mach(CPUBOARD=IP32)' \
		   -idb 'exitop("if [ X$$instmode=Xnormal ]; then $$rbase/sbin/flashinst -T -y -q $$rbase/usr/cpu/firmware/ip32prom.image; fi")' \
			-m 644 -F /usr/cpu/firmware ip32prom.image

include $(MHCOMMONRULES)
