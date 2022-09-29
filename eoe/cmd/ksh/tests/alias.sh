
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

Command=$0
integer Errors=0
alias foo='print hello'
if	[[ $(foo) != hello ]]
then	err_exit 'foo, where foo is alias for "print hello" failed'
fi
if	[[ $(foo world) != 'hello world' ]]
then	err_exit 'foo world, where foo is alias for "print hello" failed'
fi
alias foo='print hello '
alias bar=world
if	[[ $(foo bar) != 'hello world' ]]
then	err_exit 'foo bar, where foo is alias for "print hello " failed'
fi
if	[[ $(foo \bar) != 'hello bar' ]]
then	err_exit 'foo \bar, where foo is alias for "print hello " failed'
fi
alias bar='foo world'
if	[[ $(bar) != 'hello world' ]]
then	err_exit 'bar, where bar is alias for "foo world" failed'
fi
if	[[ $(alias bar) != 'bar=foo world' ]]
then	err_exit 'alias bar, where bar is alias for "foo world" failed'
fi
unalias foo  || err_exit  "unalias foo failed"
alias foo 2> /dev/null  && err_exit "alias for non-existent alias foo returns true"
exit $((Errors))
