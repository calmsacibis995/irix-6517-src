#!/bin/csh
#
# This test determines whether Bug #200702 has been corrected
#

set foo = (alpha beta)
set foo[1] = `echo $foo[1]`
if (x$foo[1] == xalpha) then
	exit 0;
else
	exit 1;
