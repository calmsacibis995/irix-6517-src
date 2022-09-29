
#									
#	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		
#			All Rights Reserved				
#									
#	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		
#	    The copyright notice above does not evidence any		
#	   actual or intended publication of such source code.		
#									

# test the behavior of co-processes
function err_exit
{
	print -u2 -n "\t"
	print -u2 -r $Command: "$@"
	let Errors+=1
}

Command=$0
integer Errors=0

function ping # id
{
	integer x=0
	while ((x < 5))
	do	read -r
		print -r "$1 $REPLY"
	done
}

ping three |&
exec 3>&p
ping four |&
exec 4>&p
ping pipe |&

integer count
for i in three four pipe four pipe four three pipe pipe three pipe
do	case $i in
	three)	to=-u3;;
	four)	to=-u4;;
	pipe)	to=-p;;
	esac
	count=count+1
	print  $to $i $count
done

while	((count > 0))
do	count=count-1
	read -p
#	print -r - "$REPLY"
	set -- $REPLY
	if	[[ $1 != $2 ]]
	then	err_exit "$1 does not match 2"
	fi
	case $1 in
	three);;
	four) ;;
	pipe) ;;
	*)	err_exit "unknown message"
	esac
done

exit $((Errors))
