# Generated automatically from makefile.cfg by configure.
# Makefile for Independent JPEG Group's software

# makefile.cfg is edited by configure to produce a custom Makefile.

# Read installation instructions before saying "make" !!

# For compiling with source and object files in different directories.
srcdir = .
incdir = ../include

# Where to install the programs and man pages.
prefix = /usr/local
exec_prefix = ${prefix}
bindir = $(exec_prefix)/bin
libdir = $(exec_prefix)/lib
includedir = $(prefix)/include
binprefix =
manprefix =
manext = 1
mandir = $(prefix)/man/man$(manext)

# The name of your C compiler:
CC= cc

# You may need to adjust these cc options:
CFLAGS= -O  -I$(incdir)
# Generally, we recommend defining any configuration symbols in jconfig.h,
# NOT via -D switches here.
# However, any special defines for ansi2knr.c may be included here:
ANSI2KNRFLAGS= 

# Link-time cc options:
LDFLAGS= 

# To link any special libraries, add the necessary -l commands here.
LDLIBS= 

# Put here the object file name for the correct system-dependent memory
# manager file.  For Unix this is usually jmemnobs.o, but you may want
# to use jmemansi.o or jmemname.o if you have limited swap space.
SYSDEPMEM= jmemnobs.o

# miscellaneous OS-dependent stuff
SHELL= /bin/sh
# linker
LN= $(CC)
# file deletion command
RM= rm -f
# file rename command
MV= mv
# library (.a) file creation command
AR= ar rc
# second step in .a creation (use "touch" if not needed)
AR2= :
# installation program
INSTALL= cp
INSTALL_PROGRAM= ${INSTALL}
INSTALL_DATA= ${INSTALL}

# End of configurable options.


# source files: JPEG library proper
LIBSOURCES= jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c \
        jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c \
        jcphuff.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c \
        jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c \
        jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdphuff.c \
        jdpostct.c jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c \
        jfdctint.c jidctflt.c jidctfst.c jidctint.c jidctred.c jquant1.c \
        jquant2.c jutils.c jmemmgr.c

# memmgr back ends: compile only one of these into a working library
SYSDEPSOURCES= jmemnobs.c

# files included by source files
INCLUDES= jchuff.h jdhuff.h jdct.h jerror.h jinclude.h jmemsys.h jmorecfg.h \
        jpegint.h jpeglib.h jversion.h

# library object files common to compression and decompression
COMOBJECTS= jcomapi.o jutils.o jerror.o jmemmgr.o $(SYSDEPMEM)
# compression library object files
CLIBOBJECTS= jcapimin.o jcapistd.o jctrans.o jcparam.o jdatadst.o jcinit.o \
        jcmaster.o jcmarker.o jcmainct.o jcprepct.o jccoefct.o jccolor.o \
        jcsample.o jchuff.o jcphuff.o jcdctmgr.o jfdctfst.o jfdctflt.o \
        jfdctint.o
# decompression library object files
DLIBOBJECTS= jdapimin.o jdapistd.o jdtrans.o jdatasrc.o jdmaster.o \
        jdinput.o jdmarker.o jdhuff.o jdphuff.o jdmainct.o jdcoefct.o \
        jdpostct.o jddctmgr.o jidctfst.o jidctflt.o jidctint.o jidctred.o \
        jdsample.o jdcolor.o jquant1.o jquant2.o jdmerge.o
# These objectfiles are included in libjpeg.a
LIBOBJECTS= $(CLIBOBJECTS) $(DLIBOBJECTS) $(COMOBJECTS)

SRCFILE= /usr/people/jcb/SGI_3D_LOGO.jpg
OUTFILE= ../../gui/src/backimg.c

all: djpg2mmsc
	$(RM) $(OUTFILE)
	djpg2mmsc < $(SRCFILE) > $(OUTFILE)

djpg2mmsc:  libjpeg.a djpg2mmsc.o
	$(CC) -o djpg2mmsc djpg2mmsc.o libjpeg.a 

libjpeg.a:  $(LIBOBJECTS)
	$(RM) libjpeg.a
	$(AR) libjpeg.a  $(LIBOBJECTS)
	$(AR2) libjpeg.a

clean:
	$(RM) *.o djpg2mmsc libjpeg.a core

# GNU Make likes to know which target names are not really files to be made:
.PHONY: all clean

djpg2mmsc.o: djpg2mmsc.c
jcapimin.o: jcapimin.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jcapistd.o: jcapistd.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jccoefct.o: jccoefct.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jccolor.o: jccolor.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jcdctmgr.o: jcdctmgr.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdct.h
jchuff.o: jchuff.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jchuff.h
jcinit.o: jcinit.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jcmainct.o: jcmainct.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jcmarker.o: jcmarker.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jcmaster.o: jcmaster.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jcomapi.o: jcomapi.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jcparam.o: jcparam.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jcphuff.o: jcphuff.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jchuff.h
jcprepct.o: jcprepct.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jcsample.o: jcsample.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jctrans.o: jctrans.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdapimin.o: jdapimin.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdapistd.o: jdapistd.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdatadst.o: jdatadst.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jerror.h
jdatasrc.o: jdatasrc.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jerror.h
jdcoefct.o: jdcoefct.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdcolor.o: jdcolor.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jddctmgr.o: jddctmgr.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdct.h
jdhuff.o: jdhuff.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdhuff.h
jdinput.o: jdinput.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdmainct.o: jdmainct.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdmarker.o: jdmarker.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdmaster.o: jdmaster.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdmerge.o: jdmerge.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdphuff.o: jdphuff.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdhuff.h
jdpostct.o: jdpostct.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdsample.o: jdsample.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jdtrans.o: jdtrans.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jerror.o: jerror.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jversion.h $(incdir)/jerror.h
jfdctflt.o: jfdctflt.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdct.h
jfdctfst.o: jfdctfst.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdct.h
jfdctint.o: jfdctint.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdct.h
jidctflt.o: jidctflt.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdct.h
jidctfst.o: jidctfst.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdct.h
jidctint.o: jidctint.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdct.h
jidctred.o: jidctred.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jdct.h
jquant1.o: jquant1.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jquant2.o: jquant2.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jutils.o: jutils.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h
jmemmgr.o: jmemmgr.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jmemsys.h
jmemnobs.o: jmemnobs.c $(incdir)/jinclude.h $(incdir)/jconfig.h $(incdir)/jpeglib.h $(incdir)/jmorecfg.h $(incdir)/jpegint.h $(incdir)/jerror.h $(incdir)/jmemsys.h
