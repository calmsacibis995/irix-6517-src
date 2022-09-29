#
# Pmake rule for making a lint library. 
# Usage is:
#
# llib-l<library-name>.ln : <sources-in-library> MAKELINT
#
# Output is sent to FLUFF.<library-name> and a lint library is 
# created in $(.TARGET)
#
# $Revision: 1.2 $

LINTFLAGS	+=
MAKELINT	: .USE
	lib=`expr $(.TARGET) : 'llib-l\(.*\).ln'` ; \
	lint -o$${lib} $(LINTFLAGS) $(CFLAGS:M-[ID]*) $(.ALLSRC) \
		> FLUFF.$${lib} 2>&1
