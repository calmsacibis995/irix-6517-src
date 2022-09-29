#!smake
#
# Makefile for fam(1m).
#
#	`$Revision: 1.5 $'

include $(ROOT)/usr/include/make/commondefs

CLIENT   = Client InternalClient MxClient TCP_Client RemoteClient
SERVER   = ServerHost ServerHostRef
FILESYS  = FileSystem LocalFileSystem NFSFileSystem FileSystemTable
INTEREST = Interest ClientInterest File Directory DirEntry DirectoryScanner
NETWORK  = NetConnection ClientConnection ServerConnection RPC_TCP_Connector
SUPPORT  = main BTB IMon Listener Log Pollster Scanner Scheduler timeval
SIMPLE   = Activity ChangeFlags Cred Event
ALL      = $(INTEREST) $(CLIENT) $(SERVER) $(FILESYS) $(NETWORK) \
           $(SUPPORT) $(SIMPLE)

CXXFILES = $(ALL:=.c++)
LCXXDEFS = -DNDEBUG 			# Comment out to make fam assertive.
# LCXXDEFS = -DNO_LEAKS			# Uncomment for watertight operation.
LCXXOPTS = -nobool # -woff 3577 -woff 3509 -woff 3262
LDIRT    = ii_files Makedepend fam.precord

TARGETS  = fam

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -idb eoe1.sw.unix -F /usr/etc $(TARGETS)

fam:	fam.precord
	# $(CORD) -o $@ $@.precord $@.fb
	cp $? $@

fam.precord:	$(OBJECTS)
	$(CXXF) $(OBJECTS) $(LDFLAGS) -o $@
	
test:
	./utest `grep -l UNIT_TEST *.c++`
