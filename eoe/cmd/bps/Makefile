##########################################################################
# This is a basic Makefile to take a buck of the work out of compiling
# our stuff.
# Just change  OFILES or OFILE_TEST depending on what you want to do.
#
###########################################################################

.KEEP_STATE:
.SUFFIXES: .C .H

#some basic defines

BPSHOME = . 
BPSINC = $(BPSHOME)
BPSLIB = $(BPSHOME)
BPSMOTIF = $(BPSHOME)/src/Motif
BPSSTL	= ./stl/stl
#
# Change EXEC to the name of the executable which you wish to
# create. Make OFILES be the name of all the .C files in your program,
# but change the .C to .o.
#

# Compiler specifications - we use g++
CCC     = CC -pta -O2 -DNDEBUG -DLIBNDEBUG

EXEC    = bps_server
EXEC_CLIENT_TEST = client_test
EXEC_SERV_TEST = serv_test
EXEC_TEST = policy_test

LIBOFILES= FirstThreadInstance.o BPSJob.o  BPSJobSchedule.o BPSPolicy.o BPSDefaultPolicy.o BPSPolicyManager.o BPSSSPartManager.o BPSServerThread.o BPSSocket.o BPSSSPartition.o BPSSSPartitionIterator.o BPSSSPartitionUnit.o BPSPrivate.o Thread.o bps.o BPSAuthenticator.o bps_server.o BPSServer.o

OFILES = 
OFILES_SERV_TEST = socket_serv_test.o
OFILES_CLIENT_TEST = socket_client_test.o
OFILES_TEST =  policy_test.o


HEADERS = 
LIBNAME = libBPS.a

#include directories
IFLAGS  =       -I$(BPSINC) -I$(BPSSTL)

#Link directories

LFLAGS  =       -L$(BPSLIB) 
#Run-time library directories
RFLAGS  =     

#Libraries to include
LIB =  -lBPS 
LIBS = $(LIB) 

all:    $(EXEC)

$(EXEC) : $(OFILES) $(LIBNAME)
	echo "linking $<"
	$(CCC)   -o $(EXEC) $(OFILES) -I$(BPSINC) -I$(BPSSTL) $(LFLAGS) $(LIBS) libpthread_d.a 
	echo "make finished at "; date

test	: $(EXEC_SERV_TEST) $(EXEC_CLIENT_TEST) 
$(EXEC_TEST) : $(OFILES) $(LIBNAME) $(OFILES_TEST) 
	echo "linking $<"
	$(CCC)   -o $(EXEC_TEST) $(OFILES) $(OFILES_TEST) -I$(BPSSTL) -I$(BPSINC)$(LFLAGS) $(LIBS) libpthread_d.a 
	echo "make finished at "; date

$(EXEC_SERV_TEST) : $(OFILES) $(LIBNAME) $(OFILES_SERV_TEST)
	echo "linking $<"
	$(CCC)   -o $(EXEC_SERV_TEST) $(OFILES) $(OFILES_SERV_TEST) $(LFLAGS) $(LIBS) libpthread_d.a
	echo "make finished at "; date

$(EXEC_CLIENT_TEST): $(OFILES) $(LIBNAME) $(OFILES_CLIENT_TEST)
	echo "linking $<"
	$(CCC)   -o $(EXEC_CLIENT_TEST) $(OFILES) $(OFILES_CLIENT_TEST) libBPS.a

	echo "make finished at "

# All .C files depend on their headers
%.C:    %.H

# Define a rule for building .o from .C files
.C.o:
	$(CCC) $(IFLAGS) -c  $<
# Define a rule for building .o from .C files
.c.o:
	echo "I wish I could understand file $<"
	cc $(IFLAGS) -O3 -c  $<


libBPS.a:	$(LIBOFILES)
	ar rv $(LIBNAME) $(LIBOFILES)

#make and install the library file
install:  $(LIBOFILES)
#       install -m 664 -f $(BPSINC) $(HEADERS) 
	ar rv $(LIBNAME) $(LIBOFILES)
# Clean up after making. 
clean:
	rm *.o
	rm -r Templates.DB/*
	$(RM) *.o

# Make tags for emacs and vi. The result will be stored in a text 
# file 'TAGS' in your current directory.
tags:

# Time your make.
time:





