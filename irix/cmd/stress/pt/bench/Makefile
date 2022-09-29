#!smake

TESTROOT	= ..

include localdefs

TESTLIST	= $(TARGS_sproc) $(TARGS_pthread)

COMMANDS	= $(TESTLIST) $(LIBTHRSPROC) $(LIBTHRPTHREAD)

TESTNAME	= bench

include versiondefs

include $(TESTROOTPATH)/mk.common

print::
	echo $(TESTLIST)
