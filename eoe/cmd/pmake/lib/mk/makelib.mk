#
# Pmake rules for making libraries. The object files which make up 
# the library are removed once they are archived.
#
#
# To use, do something like this:
#
# OBJECTS = <files in the library>
#
# fish.a: fish.a($(OBJECTS)) MAKELIB
#
#
# $Revision: 1.3 $

#ifndef _MAKELIB_MK
_MAKELIB_MK	=

#include	<po.mk>

ARFLAGS		?= cr

#
# Re-archive the out-of-date members.
#
MAKELIB		: .USE .PRECIOUS
	$(AR) $(ARFLAGS) $(.TARGET) $(.OODATE)
	rm $(.OODATE)

#endif _MAKELIB_MK
