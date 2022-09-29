#!/bin/csh -f

set tmp=`grep RELEASE' *=' $ROOT/usr/include/make/releasedefs`
set REL=`sh -c $tmp'; echo $RELEASE'`
# stand version gets cpu type as well.
if($#argv == 2) set REL=($REL\\040$2)
echo '#include <stdio.h>\n\nchar *fx_version = \c' > $1
/bin/date +'"fx version '$REL', %h %e, %Y";'  >> $1
