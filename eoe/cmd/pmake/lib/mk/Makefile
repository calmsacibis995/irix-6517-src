# $Revision: 1.6 $

include $(ROOT)/usr/include/make/commondefs

IDB_TAG	= -idb "config(update)"
DIR = /usr/include/make

FILES = makelib.mk makelint.mk po.mk shx.mk system.mk

default: $(FILES)

install: $(FILES)
	$(INSTALL) $(IDB_TAG) -m 444 -F $(DIR) $(FILES)

clean:
clobber:
