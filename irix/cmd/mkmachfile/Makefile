#!smake 

## Copyright 1997, Silicon Graphics, Inc.
## All Rights Reserved.
##
## This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
## the contents of this file may not be disclosed to third parties, copied or
## duplicated in any form, in whole or in part, without the prior written
## permission of Silicon Graphics, Inc.
##
## RESTRICTED RIGHTS LEGEND:
## Use, duplication or disclosure by the Government is subject to restrictions
## as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
## and Computer Software clause at DFARS 252.227-7013, and/or in similar or
## successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
## rights reserved under the Copyright Laws of the United States.

#

include $(ROOT)/usr/include/make/commondefs

TARGETS=mkmachfile machfile

LDIRT=machtable machfile

default: $(TARGETS) 

install: default headers
	$(INSTALL) -m 444 -F /var/inst machfile

include $(COMMONRULES)

headers: 
	$(INSTALL) -m 444 -idb noship -F /usr/include iMachTypes.h

machfile: mkmachfile custom
	./mkmachfile machfile.tmp
	echo "# Version ${RELEASE}" > machfile
	cat machfile.tmp custom >> machfile
	rm machfile.tmp

#mkmachfile.o: machtable mkmachfile.c++ 
#	$(C++) -c mkmachfile.c++ -I$(ROOT)/usr/include

#mkmachfile: mkmachfile.o
#	$(C++) -o $@ mkmachfile.o 

machtable: machtable.KUDZU
	rm -f machtable > /dev/null 2>&1 ; \
	   ln -s machtable.KUDZU machtable

mkmachfile: machtable $$@.c++
	$(C++F) -o $@ $@.c++ $(LDFLAGS)

