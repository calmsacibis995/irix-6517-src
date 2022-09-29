
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
set -A x zero one two three four 'five six'
if	[[ $x != zero ]]
then	err_exit '$x is not element 0'
fi
if	[[ ${x[0]} != zero ]]
then	err_exit '${x[0] is not element 0'
fi
if	(( ${#x[0]} != 4 ))
then	err_exit "length of ${x[0]} is not 4"
fi
if	(( ${#x[@]} != 6  ))
then	err_exit 'number of elements of x is not 6'
fi
if	[[ ${x[2]} != two  ]]
then	err_exit ' element two is not 2'
fi
set -A y -- ${x[*]}
if	[[ $y != zero ]]
then	err_exit '$x is not element 0'
fi
if	[[ ${y[0]} != zero ]]
then	err_exit '${y[0] is not element 0'
fi
if	(( ${#y[@]} != 7  ))
then	err_exit 'number of elements of y is not 7'
fi
if	[[ ${y[2]} != two  ]]
then	err_exit ' element two is not 2'
fi
set +A y nine ten
if	[[ ${y[2]} != two  ]]
then	err_exit ' element two is not 2'
fi
if	[[ ${y[0]} != nine ]]
then	err_exit '${y[0] is not nine'
fi
unset y[4]
if	(( ${#y[@]} != 6  ))
then	err_exit 'number of elements of y is not 6'
fi
if	(( ${#y[4]} != 0  ))
then	err_exit 'string length of unset element is not 0'
fi
unset foo
if	(( ${#foo[@]} != 0  ))
then	err_exit 'number of elements of unset variable foo is not 0'
fi
foo=''
if	(( ${#foo[0]} != 0  ))
then	err_exit 'string length of null element is not 0'
fi
if	(( ${#foo[@]} != 1  ))
then	err_exit 'number of elements of null variable foo is not 1'
fi
set -A foo -- "${x[@]}"
if	(( ${#foo[@]} != 6  ))
then	err_exit 'number of elements of foo is not 6'
fi
if	(( ${#PWD[@]} != 1  ))
then	err_exit 'number of elements of PWD is not 1'
fi
unset x
x[2]=foo x[4]=bar
if	(( ${#x[@]} != 2  ))
then	err_exit 'number of elements of x is not 2'
fi
exit $((Errors))
