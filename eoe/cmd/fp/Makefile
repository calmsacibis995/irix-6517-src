#
# Makefile for cmd version of mkfp fpck, 
# Silicon Graphics' floppy disk formatter/exerciser.
#
#ident "$Revision cmd/fp/Makefile $"
include ${ROOT}/usr/include/make/commondefs

DRIVERDEFS = 
LCDEFS= $(DRIVERDEFS)
CVERSION = -xansi -fullwarn -woff 1174,1692,1209,1552,1506

#OPTIMIZER=-g
LFMTLIBS = -ldisk -lds         # for overlap checking
LCHKLIBS = -ldisk -lds         # for overlap checking

# floppy format files, the files handle low level
# format were pored from format utility fx
CFILES1= mkfp.c smfd.c error.c chkmount.c util.c scsi.c \
	 fpdevice.c dosfs.c device.c part.c dos.c misc.c

# file system checking files
CFILES2= fpck.c fatchk.c error.c chkmount.c smfd.c util.c \
	 dosutil.c fpdevice.c hfschk.c part.c misc.c

# These file were ported from DIT distributions
MACFILES= dm.c macPort.c macVolume.c macTree.c macVIB.c macBitmap.c \
          macExtents.c macCatalog.c macDate.c macPack.c macUtil.c \
	  macFile.c macLibrary.c

OBJECTS1= ${CFILES1:.c=.o} ${MACFILES:.c=.o}

OBJECTS2= ${CFILES2:.c=.o} ${MACFILES:.c=.o}

TARGETS= mkfp fpck

default: ${TARGETS}

include $(COMMONRULES)

mkfp:	${OBJECTS1} ${MACOBJECTS}
	${CCF} $(OBJECTS1) -o $@ $(LDFLAGS) $(LFMTLIBS)

fpck:	${OBJECTS2} 
	${CCF} $(OBJECTS2) -o $@ $(LDFLAGS) $(LCHKLIBS) 

install: default 
	${INSTALL} -F /usr/bin -m 755 mkfp
	${INSTALL} -F /usr/bin -m 755 fpck
