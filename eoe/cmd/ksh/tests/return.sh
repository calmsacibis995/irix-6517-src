
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
# test the behavior of return and exit with functions
Command=$0
integer Errors=0
foo=NOVAL bar=NOVAL
file=/tmp/shtest$$
trap "rm -f $file" EXIT INT
function foo
{
	typeset foo=NOEXIT
	trap "foo=EXIT;rm -f $file" EXIT
	> $file
	if	(( $1 == 0 ))
	then	return $2
	elif	(( $1 == 1 ))
	then	exit $2
	else	bar "$@"
	fi
}

function bar
{
	typeset bar=NOEXIT
	trap 'bar=EXIT' EXIT
	if	(( $1 == 2 ))
	then	return $2
	elif	(( $1 == 3 ))
	then	exit $2
	fi
}

function funcheck
{
	[[ $foo = EXIT ]] || err_exit "foo "$@" : exit trap not set"
	if	[[ -f $file ]]
	then	rm -r $file
		err_exit "foo $@: doesn't remove $file"
	fi
	foo=NOVAL bar=NOVAL
}

(exit 0) || err_exit "exit 0 is not zero"
(return 0) || err_exit "return 0 is not zero"
(exit) || err_exit "default exit value is not zero"
(return) || err_exit "default return value is not zero"
(exit 35)
ret=$?
if	(( $ret != 35 ))
then	err_exit "exit 35 is $ret not 35"
fi
(return 35)
ret=$?
if	(( $ret != 35 ))
then	err_exit "return 35 is $ret not 35"
fi

foo 0 0 || err_exit "foo 0 0: incorrect return"
funcheck 0 0
foo 0 3
ret=$?
if	(( $ret != 3 ))
then	err_exit "foo 0 3: return is $ret not 3"
fi
funcheck 0 3
foo 2 0 || err_exit "foo 2 0: incorrect return"
[[ $bar = EXIT ]] || err_exit "foo 2 0: bar exit trap not set"
funcheck 2 0
foo 2 3
ret=$?
if	(( $ret != 3 ))
then	err_exit "foo 2 3: return is $ret not 3"
fi
[[ $bar = EXIT ]] || err_exit "foo 2 3: bar exit trap not set"
funcheck 2 3
(foo 3 3)
ret=$?
if	(( $ret != 3 ))
then	err_exit "foo 3 3: return is $ret not 3"
fi
foo=EXIT
funcheck 3 3
cat > $file <<!
return 3
exit 4
!
( . $file )
ret=$?
if	(( $ret != 3 ))
then	err_exit "return in dot script is $ret should be 3"
fi
chmod 755 $file
(  $file )
ret=$?
if	(( $ret != 3 ))
then	err_exit "return in script is $ret should be 3"
fi
cat > $file <<!
: line 1
# next line should fail and cause an exit 
: > /
exit 4
!
( . $file ; exit 5 ) 2> /dev/null
ret=$?
if	(( $ret != 1 ))
then	err_exit "error in dot script is $ret should be 1"
fi
(  $file; exit 5 ) 2> /dev/null
ret=$?
if	(( $ret != 5 ))
then	err_exit "error in script is $ret should be 5"
fi
exit $((Errors))
