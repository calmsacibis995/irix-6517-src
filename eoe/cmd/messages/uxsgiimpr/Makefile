#
#  ATTENTION: this file is different from the others.
#  Please read the README file in this directory.
#  
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

PREF	= uxsgiimpr
TARGETS	= $(PREF).str $(PREF)
LDIRT	= $(PREF).h

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -m 444 -F /usr/lib/locale/C/LC_MESSAGES $(PREF)

$(PREF).str: msgs.src ../mkinclmsg
	../mkinclmsg msgs.src $(PREF).str $(PREF) $(PREF) 

$(PREF):$(PREF).str
	$(TOOLROOT)/usr/bin/mkmsgs -o $? $@
