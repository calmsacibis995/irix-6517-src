include ${ROOT}/usr/include/make/commondefs

OPTIMIZER=-O3

TESTLIB_OBJS=sub_hosts.o \
	sub_groups.o \
	sub_nets.o \
	sub_protos.o \
	sub_pws.o \
	sub_rpcs.o \
	sub_servs.o 

TARGETS= \
	allgroups allhosts allnets allproto allpw allrpc allserv allsp \
	getgrent getgrent_r getgrnam getgrnam_r getgrgid getgrgid_r \
	getgrmember getgroups \
	gethostent gethostent_r gethostbyname gethostbyname_r \
	gethostbyaddr gethostbyaddr_r \
	getnetent getnetent_r getnetbyname getnetbyname_r \
	getnetbyaddr getnetbyaddr_r \
	getprotoent getprotoent_r getprotobyname getprotobyname_r \
	getprotobynumber getprotobynumber_r \
	getpwent getpwent_r getpwnam getpwnam_r getpwuid getpwuid_r \
	getrpcent getrpcent_r getrpcbyname getrpcbyname_r \
	getrpcbynumber getrpcbynumber_r \
	getservent getservent_r getservbyname getservbyname_r \
	getservbyport getservbyport_r \
	getspent getspent_r getspnam getspnam_r \
	ns_lookup ns_list domany 

M_TARGETS=\
	allhostsP allgroupsP allnetsP allprotoP allpwP allrpcP \
	allservP allxentP allXbyY

MDBMLIB_OBJS=mdbmUtils.o

CSTYLE=-n32 -mips3 -O3
MDBM_TESTS=\
	mdbmWalk mdbmBMark mdbmBMark2 benchMark

BMARK_TESTS=\
	bHostLookup bPasswdLookup bGroupLookup \
	bRpcLookup bServLookup bProtoLookup 

default: libtestLib.a ${TARGETS} ${M_TARGETS} ${MDBM_TESTS} ${BMARK_TESTS}

libtestLib.a: $(TESTLIB_OBJS)
	@rm -f $@;
	$(AR) crls $@ $(TESTLIB_OBJS)

COMMONPREF=tests
include ${COMMONRULES}

$(M_TARGETS):$$@.o libtestLib.a
	$(CC) $(CSTYLE) -o $@ -g $@.o libtestLib.a -lpthread

$(MDBM_TESTS): $$@.o $(MDBMLIB_OBJS) libtestLib.a libmdbm.a
	$(CC) $(CSTYLE) -o $@ -g  $@.o $(MDBMLIB_OBJS) libmdbm.a -lpthread

bGenLookup bHostLookup1 bServLookup1:$$@.o
	$(CC) $(CSTYLE) -o $@ $@.o 
$(BMARK_TESTS):$$@.o
	$(CC) $(CSTYLE) -o $@ $@.o -lpthread

$(COMMONTARGS) clobber : $(COMMONPREF)$$@
	rm -f $(TESTLIB_OBJS);
	rm -f libtestLib.a;
	rm -f $(M_TARGETS);

