
#									
#	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		
#			All Rights Reserved				
#									
#	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		
#	    The copyright notice above does not evidence any		
#	   actual or intended publication of such source code.		
#									

function err_exit
{
	print -u2 -n "\t"
	print -u2 -r $Command: "$@"
	let Errors+=1
}

integer Errors=0
Command=$0
integer foo=33
bar=bye
# check for global variables and $0
function foobar
{
	case $1 in
	1) 	print -r - "$foo" "$bar";;
	2)	print -r - "$0";;
	3)	typeset foo=foo
		integer bar=10
	 	print -r - "$foo" "$bar";;
	4)	trap 'foo=36' EXIT
		typeset foo=20;;
	esac
}

if	[[ $(foobar 1) != '33 bye' ]]
then	err_exit 'global variables not correct'
fi

if	[[ $(foobar 2) != 'foobar' ]]
then	err_exit '$0  not correct'
fi

if	[[ $(bar=foo foobar 1) != '33 foo' ]]
then	err_exit 'environment override not correct'
fi
if	[[ $bar = foo ]]
then	err_exit 'scoping error'
fi

if	[[ $(foobar 3) != 'foo 10' ]]
then	err_exit non-local variables
fi

foobar 4
if	[[ $foo != 36 ]]
then	err_exit EXIT trap in wrong scope
fi
unset -f foobar || err_exit "cannot unset function foobar"
typeset -f foobar>/dev/null  && err_exit "typeset -f has incorrect exit status"

function foobar
{
	(return 0)
}
> /tmp/shtests$$.1
{
foobar
if	[ -r /tmp/shtests$$.1 ]
then	rm -r /tmp/shtests$$.1
else	err_exit 'return within subshell inside function error'
fi
}
exit $((Errors))
