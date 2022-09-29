
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

# test shell builtin commands
Command=$0
integer Errors=0
: ${foo=bar} || err_exit ": failed"
[[ $foo = bar ]] || err_exit ": side effects failed"
set -- -x foobar
[[ $# = 2 && $1 = -x && $2 = foobar ]] || err_exit "set -- -x foobar failed"
getopts :x: foo || err_exit "getopts :x: returns false"
[[ $foo = x && $OPTARG = foobar ]] || err_exit "getopts :x: failed"
false ${foo=bar} &&  err_exit "false failed"
read <<!
hello world
!
[[ $REPLY = 'hello world' ]] || err_exit "read builtin failed"
exit $((Errors))
