#
# add all new messages to msgs.src AT THE END !
# syntax is 'label:text'
# msgs.src is compiled to <msgs/ux???.h> and msgs
# <msgs/ux???.h> contains definitions of the form:
#	#define _SGI_Dlabel	":nbr"		/* for quick reference */
#	#define _SGI_label	"catalog:nbr"	/* for other catalog */
#	#define _SGI_Mlabel	"catalog:nbr:msg"  /* ref incl. default msg */
#	#define _SGI_Slabel	"msg"		   /* for referencing the default msg */
#
include $(ROOT)/usr/include/make/commondefs

PREF	= uxsgish
TARGETS	= $(PREF).str $(PREF)
LDIRT	= $(PREF).h

default: $(TARGETS)

headers: default
	$(INSTALL) -m 444 -F /usr/include/msgs $(PREF).h 

include $(COMMONRULES)

install: default headers
	#
	# HACK WARNING: to simulate the removal of this file from 
	# the 6.5.6 overlay, we have to install an empty file.
	# The following two lines should be removed in 6.6 and
	# the target file should be removed from the build/idb
	# file instead.
	#
	rm -f $(PREF)
	touch $(PREF)
	$(INSTALL) -m 444 -F /usr/lib/locale/C/LC_MESSAGES $(PREF)

$(PREF).str: msgs.src ../mkinclmsg
	../mkinclmsg msgs.src $(PREF).str $(PREF) $(PREF) 

$(PREF):$(PREF).str
	$(TOOLROOT)/usr/bin/mkmsgs -o $? $@
