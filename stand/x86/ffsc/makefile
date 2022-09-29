#!smake -f Makefile.sgi
#	(Above applies to IRIX only - this makefile is for the PC environment)
#
# Top level makefile for FFSC firmware image.
#
# The following environment variables are assumed to be set:
#
#	WIND_HOST_TYPE: Wind River-defined value describing host environment
#			(typically x86-win32)
#
#	WIND_BASE: Top-level VxWorks directory (usually C:\TORNADO)
#
#	VX_SRC: Top-level directory containing VxWorks source
#		(often the same as WIND_BASE, but may be different
#		for source-control reasons, etc.)
#
#	VX_LIB: Directory containing VxWorks libraries
#		(typically C:\TORNADO\TARGET\LIB)
#
#       MW_HDR: Directory containing MetaWindows headers
#
#       MW_LIB: Directory containing MetaWindows libraries
#
#	SGI_SRC: Top-level directory containing SGI-specific source code
#
#	CDI_SRC: Top-level directory containing CDI-provided source code
#
#	BUILD_DIR: Build directory. The directory containing this file.
#
# Just to be annoying, the *_SRC variables need to be in UNC format
# ("\\host\share\dir\subdir...") so they can be used with GNUmake's VPATH,
# which apparently cannot cope with colons in filenames, while WIND_BASE
# and BUILD_DIR need to be in normal MSDOS format ("C:\dir\subdir") so
# they can work with the MSDOS "cd" command.
#

CPU		= i80486
TOOL		= gnu

include $(VX_SRC)/target/h/make/defs.bsp
include $(VX_SRC)/target/h/make/make.$(CPU)$(TOOL)
include $(VX_SRC)/target/h/make/defs.$(WIND_HOST_TYPE)

#
# WARNING: VxWorks-specific variables defined before this point
#	   may be overridden by the above includes!
#


#
# Compute the build target and related parameters based on incoming vars
#
ifdef PRODUCTION
	STANDALONE = 1
endif

ifdef BOOTROM

	ifdef FFSC_NUM
		OBJ_DIR		= $(BUILD_DIR)\bootrom.$(FFSC_NUM)
		END_PRODUCT	= bootrom_uncmp.$(FFSC_NUM)
	else
		OBJ_DIR		= $(BUILD_DIR)\bootrom
		END_PRODUCT	= bootrom_uncmp
	endif

	TARGET			= do_bootrom
	HIGHADR			= 00008000
	SYM_FILE	  	=

else	# !BOOTROM

	ifdef STANDALONE

		ifdef NETBOOT
			TARGET  = do_netbootsa
			HIGHADR	= 00008000
			OBJ_DIR = $(BUILD_DIR)\netbootsa
			END_PRODUCT = vxworks
		else
			ifdef PRODUCTION
				TARGET  = do_production
				HIGHADR	= 00108000
				OBJ_DIR = $(BUILD_DIR)\production
				END_PRODUCT = vxworks.st
			else
				TARGET  = do_standalone
				HIGHADR	= 00108000
				OBJ_DIR = $(BUILD_DIR)\standalone
				END_PRODUCT = vxworks.st
			endif
		endif

		SYM_FILE	=

	else 	# !STANDALONE

		TARGET		= do_normal
		HIGHADR		= 00008000
		OBJ_DIR		= $(BUILD_DIR)\normal
		END_PRODUCT 	= vxworks
		SYM_FILE	= $(END_PRODUCT).sym

	endif
endif


##
## Make variables that are local to this makefile
##
HEX_FLAGS	= -a 0

MACH_EXTRA	=

VXDIR		= $(VX_SRC)\target\config
VXOBJ		= kernel.o
PADOBJ		= datasegpad.o

SGIDIR		= $(SGI_SRC)
SGIOBJ		= sgi.o

CDIDIR		= $(CDI_SRC)
CDIOBJ		= cdi.o

GUIDIR		= $(GUI_SRC)
GUIOBJ		= gui.o
#GUIOBJ		= $(GUI_OBJ)\gui.o

LOCALDEFS	= $(OBJ_DIR)\localdefs.h

SUBDIRS		= vxdir sgidir cdidir guidir


##
## Make variables to propagate to subdirectories
##

ROM_BASE_ADRS	= fff20000	# ROM entry address
ROM_SIZE	= 0007fe00	# number of bytes of ROM space
RAM_LOW_ADRS	= 00108000	# RAM text/data address
RAM_HIGH_ADRS	= $(HIGHADR)	# RAM text/data address

#
# Unfortunately, DOS-hosted GNUmake does not seem capable of propagting
# make variables to recursively invoked make's, so we must propagate them
# manually. Do so here by adding any important variables to "MAKE_VARS"
#

MAKE_VARS	= \
		  CPU=$(CPU)			\
		  LOCALDEFS=$(LOCALDEFS)	\
		  MAKEFLAGS=$(MAKEFLAGS)	\
		  TOOL=$(TOOL)

ifdef BOOTROM
	MAKE_VARS += BOOTROM=$(BOOTROM)
endif

ifdef STANDALONE
	MAKE_VARS += STANDALONE=$(STANDALONE)
endif

ifdef NETBOOT
	MAKE_VARS += NETBOOT=$(NETBOOT)
endif

ifdef PRODUCTION
	MAKE_VARS += PRODUCTION=$(PRODUCTION)
endif


#
# Pull in local boot info if present, otherwise try the prototype file
#
ifneq (,$(wildcard bootdefs))
	include bootdefs
else
	ifneq (,$(wildcard protobootdefs))
		include protobootdefs
	endif
endif



#
# Targets
#

.PHONY:	do_normal do_netbootsa do_standalone do_bootrom \
	standalone_image vxdir cdidir sgidir guidir localdefs  \
	clean clobber rmtarget sterile deltree srcclean

default: $(TARGET)

#
# These targets are for backward compatibility. The Better way to do
# things is to invoke make with no target at all and let the makefile
# figure out what to do from STANDALONE, BOOTROM and NETBOOT
#
vxworks:
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE= BOOTROM=

vxworks.st:
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=1 BOOTROM=

bootrom_uncmp bootrom:
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) BOOTROM=1 STANDALONE=


#
# Phony targets for forcing a descent into a subdirectory
#
vxdir: localdefs
	$(MAKE) -C $(OBJ_DIR) -f $(VXDIR)\makefile	\
		  $(MAKE_VARS)				\
		  RAM_LOW_ADRS=$(RAM_LOW_ADRS)		\
		  RAM_HIGH_ADRS=$(RAM_HIGH_ADRS)	\
		  ROM_BASE_ADRS=$(ROM_BASE_ADRS)	\
		  ROM_SIZE=$(ROM_SIZE)			\
		  OUTPUT=$(VXOBJ)			\
		  SRC_DIR=$(VXDIR)

cdidir: localdefs
	$(MAKE) -C $(OBJ_DIR) -f $(CDIDIR)\makefile	\
		$(MAKE_VARS) OUTPUT=$(CDIOBJ) SRC_DIR=$(CDIDIR)

sgidir: localdefs
	- $(RM) $(OBJ_DIR)\sgiversion.o
	$(MAKE) -C $(OBJ_DIR) -f $(SGIDIR)\makefile \
		$(MAKE_VARS) OUTPUT=$(SGIOBJ) SRC_DIR=$(SGIDIR)

guidir: localdefs
	$(MAKE) -C $(OBJ_DIR) -f $(GUIDIR)\makefile \
		$(MAKE_VARS) OUTPUT=$(GUIOBJ) SRC_DIR=$(GUIDIR)

#
# Exercise caution when changing these targets: the order of the
# object files is often important
#

#
# normal - build a normal network-bootable vxWorks system and symbol table
#
do_normal: $(OBJ_DIR) vxdir cdidir guidir sgidir $(LIBS)
	- @ $(RM) $(END_PRODUCT) $(SYM_FILE)
	- @ $(RM) $(OBJ_DIR)\version.c 
	- @ $(RM) $(OBJ_DIR)\version.o
	$(MKVERSION) > $(OBJ_DIR)\version.c
	$(CC) $(CFLAGS) -c -o $(OBJ_DIR)\version.o $(OBJ_DIR)\version.c
	$(LD) $(LDFLAGS) -e $(SYS_ENTRY) $(LD_LOW_FLAGS) \
		-o $(END_PRODUCT) \
		$(OBJ_DIR)\$(PADOBJ) \
		$(OBJ_DIR)\$(VXOBJ)  \
		$(OBJ_DIR)\$(CDIOBJ) \
		$(OBJ_DIR)\$(SGIOBJ) \
		$(OBJ_DIR)\$(GUIOBJ) \
		$(OBJ_DIR)\version.o \
		$(LIBS)
	$(BINXSYM) < $(END_PRODUCT) > $(SYM_FILE)
	$(VXSIZEPROG) -v $(RAM_HIGH_ADRS) $(RAM_LOW_ADRS) $(END_PRODUCT)


#
# standalone - standalone VxWorks with symbol table linked in
#
do_standalone: standalone_image
	$(ADDSIG) $(END_PRODUCT)

do_netbootsa: standalone_image
	@rem That is all

standalone_image: $(OBJ_DIR) vxdir cdidir guidir sgidir $(LIBS)
	- @ $(RM) $(END_PRODUCT)
	- @ $(RM) $(OBJ_DIR)\symtbl.c 
	- @ $(RM) $(OBJ_DIR)\symtbl.o
	- @ $(RM) $(OBJ_DIR)\tmp.o
	- @ $(RM) $(OBJ_DIR)\version.c 
	- @ $(RM) $(OBJ_DIR)\version.o
	$(MKVERSION) > $(OBJ_DIR)\version.c
	$(CC) $(CFLAGS) -c -o $(OBJ_DIR)\version.o $(OBJ_DIR)\version.c
	$(LD) $(LD_PARTIAL_FLAGS) \
		-o $(OBJ_DIR)\tmp.o \
		$(OBJ_DIR)\$(PADOBJ) \
		$(OBJ_DIR)\$(VXOBJ)  \
		$(OBJ_DIR)\$(CDIOBJ) \
		$(OBJ_DIR)\$(SGIOBJ) \
		$(OBJ_DIR)\$(GUIOBJ) \
		$(OBJ_DIR)\version.o \
		$(LIBS)
	$(MKSYMTBL) $(OBJ_DIR)\tmp.o > $(OBJ_DIR)\symtbl.c
	$(CC) $(CFLAGS) -c -o $(OBJ_DIR)\symtbl.o $(OBJ_DIR)\symtbl.c
	$(LD) $(LDFLAGS) -e $(SYS_ENTRY) $(LD_LOW_FLAGS) \
		-o $(END_PRODUCT) \
		$(OBJ_DIR)\tmp.o  \
		$(OBJ_DIR)\symtbl.o
	- @ $(RM) $(OBJ_DIR)\tmp.o
	$(VXSIZEPROG) -v $(RAM_HIGH_ADRS) $(RAM_LOW_ADRS) $(END_PRODUCT)


#
# do_production - production image: no symbols or debugging
#
do_production: $(OBJ_DIR) vxdir cdidir guidir sgidir $(LIBS)
	- @ $(RM) $(END_PRODUCT)
	- @ $(RM) $(OBJ_DIR)\symtbl.c 
	- @ $(RM) $(OBJ_DIR)\symtbl.o
	- @ $(RM) $(OBJ_DIR)\tmp.o
	- @ $(RM) $(OBJ_DIR)\version.c 
	- @ $(RM) $(OBJ_DIR)\version.o
	$(MKVERSION) > $(OBJ_DIR)\version.c
	$(CC) $(CFLAGS) -c -o $(OBJ_DIR)\version.o $(OBJ_DIR)\version.c
	$(LD) $(LD_PARTIAL_FLAGS) \
		-o $(OBJ_DIR)\tmp.o \
		$(OBJ_DIR)\$(PADOBJ) \
		$(OBJ_DIR)\$(VXOBJ)  \
		$(OBJ_DIR)\$(CDIOBJ) \
		$(OBJ_DIR)\$(SGIOBJ) \
		$(OBJ_DIR)\$(GUIOBJ) \
		$(OBJ_DIR)\version.o \
		$(LIBS)
	$(CC) $(CFLAGS) -c -o $(OBJ_DIR)\symtbl.o dummySymTbl.c
	$(LD) $(LDFLAGS) -e $(SYS_ENTRY) $(LD_LOW_FLAGS) \
		-o $(END_PRODUCT) \
		$(OBJ_DIR)\tmp.o  \
		$(OBJ_DIR)\symtbl.o
	- @ $(RM) $(OBJ_DIR)\tmp.o
	$(VXSIZEPROG) -v $(RAM_HIGH_ADRS) $(RAM_LOW_ADRS) $(END_PRODUCT)
	$(ADDSIG) $(END_PRODUCT)


#
# bootrom - uncompressed bootrom image
#
do_bootrom: $(OBJ_DIR) vxdir $(LIBS)
	- @ $(RM) $(END_PRODUCT)
	- @ $(RM) bootrom.tmp
	- @ $(RM) $(OBJ_DIR)\version.c 
	- @ $(RM) $(OBJ_DIR)\version.o
	$(MKVERSION) > $(OBJ_DIR)\version.c
	$(CC) -c $(CFLAGS) -o $(OBJ_DIR)\version.o $(OBJ_DIR)\version.c
	$(LD) $(LDFLAGS) -e $(ROM_ENTRY) $(LD_HIGH_FLAGS) \
		-o bootrom.tmp \
		$(OBJ_DIR)\$(VXOBJ) \
		$(OBJ_DIR)\version.o \
		$(LIBS)
	$(ROMSIZEPROG) -b $(ROM_SIZE) bootrom.tmp
	$(ADDSIG) bootrom.tmp
	$(RENAME) bootrom.tmp $(END_PRODUCT)	# vxaddsgi must have 8.3 names


#
# localdefs - build a header file of local constants
#
localdefs:
	@echo Generating local definintions...
	- @ $(RM) $(LOCALDEFS)
	@echo /* AUTOMATICALLY GENERATED FILE - DO NOT EDIT */ > $(LOCALDEFS)
	@echo /* Local definitions for FFSC software */	>> $(LOCALDEFS)
	@echo #define RAM_LOW_ADRS   0x$(RAM_LOW_ADRS)	>> $(LOCALDEFS)
	@echo #define RAM_HIGH_ADRS  0x$(RAM_HIGH_ADRS)	>> $(LOCALDEFS)
	@echo #define ROM_SIZE       0x$(ROM_SIZE)	>> $(LOCALDEFS)
	@echo #define ROM_BASE_ADRS  0x$(ROM_BASE_ADRS)	>> $(LOCALDEFS)
	@echo #define FFSC_NUM       $(FFSC_NUM)	>> $(LOCALDEFS)
	@echo #define HOST_NAME      $(HOST_NAME)	>> $(LOCALDEFS)
	@echo #define HOST_IP_ADDR   $(HOST_IP_ADDR)	>> $(LOCALDEFS)
	@echo #define TARGET_NAME    $(TARGET_NAME)	>> $(LOCALDEFS)
	@echo #define TARGET_IP_ADDR $(TARGET_IP_ADDR)	>> $(LOCALDEFS)
	@echo #define IMAGE_PATH     $(IMAGE_PATH)	>> $(LOCALDEFS)
	@echo #define FTP_NAME       $(FTP_NAME)	>> $(LOCALDEFS)
	@echo #define FTP_PASSWD     $(FTP_PASSWD)	>> $(LOCALDEFS)
	@echo #define BOOT_FLAGS     $(BOOT_FLAGS)	>> $(LOCALDEFS)

#
# Miscellaneous targets
#
depend: $(OBJ_DIR) localdefs
	$(MAKE) -C $(OBJ_DIR) -f $(VXDIR)\makefile \
		$(MAKE_VARS) SRC_DIR=$(VXDIR) depend
	$(MAKE) -C $(OBJ_DIR) -f $(CDIDIR)\makefile \
		$(MAKE_VARS) SRC_DIR=$(CDIDIR) depend
	$(MAKE) -C $(OBJ_DIR) -f $(SGIDIR)\makefile \
		$(MAKE_VARS) SRC_DIR=$(SGIDIR) depend
	$(MAKE) -C $(OBJ_DIR) -f $(GUIDIR)\makefile \
		$(MAKE_VARS) SRC_DIR=$(GUIDIR) depend

dependall:
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=  BOOTROM=  depend
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=1 BOOTROM=  NETBOOT=   depend
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=1 BOOTROM=  NETBOOT=1  depend
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=  BOOTROM=1 FFSC_NUM=  depend
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=  BOOTROM=1 FFSC_NUM=1 depend
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=  BOOTROM=1 FFSC_NUM=2 depend

$(OBJ_DIR):
	+mkdir $(OBJ_DIR)


#
# Cleanup rules
#	clean: 	 clean up object files and generated source code files
#		 for the current object directory
#	clobber: same as clean, but also deletes final targets and
#		 dependency files
#	deltree: delete the object directory
#	rmtarget: remove the final target file(s) only
#	srcclean: erase cruft from *source* directories, such as *~ files
#	sterile: like srcclean followed by a clobber and rmdir of each possible
#		 object directory. After a "make sterile" a "p_check -w"
#		 should only show bootdefs and ffscvars.bat (if present)
#
clean: $(OBJ_DIR)
	$(MAKE) -C $(OBJ_DIR) -f $(VXDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(VXDIR) OUTPUT=$(VXOBJ) clean
	$(MAKE) -C $(OBJ_DIR) -f $(CDIDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(CDIDIR) OUTPUT=$(CDIOBJ) clean
	$(MAKE) -C $(OBJ_DIR) -f $(SGIDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(SGIDIR) OUTPUT=$(SGIOBJ) clean
	$(MAKE) -C $(OBJ_DIR) -f $(GUIDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(GUIDIR) OUTPUT=$(GUIOBJ) clean
	- $(RM) $(OBJ_DIR)\symtbl.*
	- $(RM) $(OBJ_DIR)\tmp.*
	- $(RM) $(OBJ_DIR)\version.*
	- $(RM) $(LOCALDEFS)

clobber: $(OBJ_DIR) rmtarget
	$(MAKE) -C $(OBJ_DIR) -f $(VXDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(VXDIR) OUTPUT=$(VXOBJ) clobber
	$(MAKE) -C $(OBJ_DIR) -f $(CDIDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(CDIDIR) OUTPUT=$(CDIOBJ) clobber
	$(MAKE) -C $(OBJ_DIR) -f $(SGIDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(SGIDIR) OUTPUT=$(SGIOBJ) clobber
	$(MAKE) -C $(OBJ_DIR) -f $(GUIDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(GUIDIR) OUTPUT=$(GUIOBJ) clobber
	- $(RM) $(OBJ_DIR)\symtbl.*
	- $(RM) $(OBJ_DIR)\tmp.*
	- $(RM) $(OBJ_DIR)\version.*
	- $(RM) $(LOCALDEFS)
	- $(RMDIR) $(OBJ_DIR)

deltree:
	$(DELTREE) $(OBJ_DIR)

rmtarget:
	- $(RM) $(END_PRODUCT) $(SYM_FILE)

srcclean:		# Clean cruft from source directories
	$(MAKE) -f $(VXDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(VXDIR) OUTPUT=$(VXOBJ) srcclean
	$(MAKE) -f $(CDIDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(CDIDIR) OUTPUT=$(CDIOBJ) srcclean
	$(MAKE) -f $(SGIDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(SGIDIR) OUTPUT=$(SGIOBJ) srcclean
	$(MAKE) -f $(GUIDIR)\makefile \
		 $(MAKE_VARS) SRC_DIR=$(GUIDIR) OUTPUT=$(GUIOBJ) srcclean
	- $(RM) $(VX_SRC)\h\make\*~
	- $(RM) *~

sterile: srcclean
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=  BOOTROM=  \
		rmtarget deltree
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=1 BOOTROM=  NETBOOT=  \
		rmtarget deltree
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=1 BOOTROM=  NETBOOT=1 \
		rmtarget deltree
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=  BOOTROM=1 FFSC_NUM=  \
		rmtarget deltree
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=  BOOTROM=1 FFSC_NUM=1 \
		rmtarget deltree
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) STANDALONE=  BOOTROM=1 FFSC_NUM=2 \
		rmtarget deltree

