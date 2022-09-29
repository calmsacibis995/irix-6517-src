#!/bin/csh -f
#
# Copyright (c) 1980 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that this notice is preserved and that due credit is given
# to the University of California at Berkeley. The name of the University
# may not be used to endorse or promote products derived from this
# software without specific prior written permission. This software
# is provided ``as is'' without express or implied warranty.
#
#	which : tells you which program you get
#
# SGI changes:
#   Always uses current $path.
#   -a = "all" -- keep looking, even after a match is found.
#   -f = "fast" -- don't read ~/.cshrc.

set noglob
set _curpath = ($path)		# inherited via $PATH
foreach arg ( $argv )
    switch ("$arg")
	case "-a" :
	    set _findall
	    shift
	    breaksw
	case "-f" :
	    set _fast
	    shift
	    breaksw
    endsw
end
unset noglob
if ( ! $?_fast && -r ~/.cshrc ) source ~/.cshrc
set noglob

foreach arg ( $argv )
    set alius = `alias $arg`
    switch ( $#alius )
	case 0 :
	    breaksw
        default :
	    echo ${arg} aliased to \"$alius\"
	    set arg = $alius[1]
	    breaksw
    endsw
    unset found
    if ( "$arg:h" != "$arg:t" ) then
	if ( -e "$arg" ) then
	    echo $arg
	    set found
	else
	    echo $arg not found
	endif
	continue
    endif
    foreach i ( $_curpath )
	if ( -x $i/$arg && ! -d $i/$arg ) then
	    echo $i/$arg
	    set found
	    if ( ! $?_findall ) break
	endif
    end
    if ( ! $?found ) then
	echo $arg not in $_curpath
    endif
end
if ( $?found ) then
    exit 0
else
    exit 1
endif
