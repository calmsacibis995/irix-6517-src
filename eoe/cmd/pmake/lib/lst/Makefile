#!smake
# $Revision: 1.4 $

CFILES	= lstAppend.c lstAtEnd.c lstAtFront.c lstConcat.c \
     	lstClose.c lstCur.c lstDatum.c lstDeQueue.c \
  	lstDestroy.c lstDupl.c lstEnQueue.c lstFind.c \
 	lstFindFrom.c lstFirst.c lstForEach.c lstForEachFrom.c \
	lstIndex.c lstInit.c lstInsert.c lstIsAtEnd.c lstIsEmpty.c \
	lstLast.c lstLength.c lstMember.c lstMove.c lstNext.c \
	lstOpen.c lstPred.c lstPrev.c lstRemove.c lstReplace.c \
	lstSetCirc.c lstSucc.c

LIBNAME = liblst.a

include ../Mk.common

# XXX pmake can fail when this file is optimized
lstInit.o: lstInit.c
	$(CCF) -O0 -c $*.c


# Use pmake's MAKELINT macro to create a lint library. 
# The extraneous MAKELINT keeps old make happy.

MAKELINT:

#include <makelint.mk>
llib-llst.ln : $(CFILES) MAKELINT

LDIRT = llib-llst.ln FLUFF.lst
