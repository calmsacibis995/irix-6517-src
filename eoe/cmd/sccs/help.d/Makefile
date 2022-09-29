#!smake
#
#ident	"$Revision: 1.15 $"
#
# Notes:
#

#
# Common Definitions
#
DEPTH=..
include	$(DEPTH)/slocaldefs

#
# Compile Time Options
#

#
# Local Definitions
#
FILES=	ad bd cb cm cmds co de default ge he prs rc un ut vc

#
# Targets/Rules
#

#
# Can't use SGI's standard target of 'default' because there is 
# a file in this directory called default.
#
Default:

include $(COMMONRULES)

#
# Specific Target/Rules follow
#
install: Default
	$(INSTALL) -F $(HELPLIB) -m 444 "$(FILES)"
