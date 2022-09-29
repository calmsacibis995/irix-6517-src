#
#!smake
#
# Makefile for the rpc.lockd subdirectory
#
# $Revision: 1.2 $

include	${ROOT}/usr/include/make/commondefs

LINTFLAGS= -hbax -woff 91,102

CTAGOPTS=-tw

TARGETS = nprocess nprocess_svc locktest

COMMON_FILES = ../common/util.c nprocess_xdr.c lockprocess.c
COMMON_OBJECTS = ${COMMON_FILES:.c=.o}

SVC_FILES = nprocess_svc.c server_procs.c
SVC_OBJECTS = ${SVC_FILES:.c=.o} $(COMMON_OBJECTS)

CLIENT_FILES = nprocess.c nprocess_clnt.c
CLIENT_OBJECTS = ${CLIENT_FILES:.c=.o} $(COMMON_OBJECTS)

GENERATED_FILES = nprocess.h nprocess_clnt.c nprocess_svc.c nprocess_xdr.c

LOCKTEST_FILES = locktest.c
LOCKTEST_OBJECTS = ${LOCKTEST_FILES:.c=.o}

# define NDEBUG to turn off assertions
# define _IRIX for building on IRIX
# define _SUNOS for building on SunOS
#CFLAGS = -DNDEBUG
LCDEFS = -D_IRIX
LCOPTS = -g
LCINCS = -I../common
LIBS = 

default:	$(TARGETS)

include	${COMMONRULES}

locktest:	$(LOCKTEST_OBJECTS)
	$(CCF) $(LCOPTS) $(LCDEFS) $(LCINCS) -o $@ $(LOCKTEST_OBJECTS) $(LDFLAGS) $(LIBS)

nprocess:	$(GENERATED_FILES) $(CLIENT_OBJECTS)
	$(CCF) $(LCOPTS) $(LCDEFS) $(LCINCS) -o $@ $(CLIENT_OBJECTS) $(LDFLAGS) $(LIBS)

nprocess_svc:	$(GENERATED_FILES) $(SVC_OBJECTS)
	$(CCF) $(LCOPTS) $(LCDEFS) $(LCINCS) -o $@ $(SVC_OBJECTS) $(LDFLAGS) $(LIBS)

$(GENERATED_FILES):	nprocess.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -P nprocess.x

install:	default
