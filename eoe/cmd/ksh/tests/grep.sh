
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

function grep
{
	#
	#	SHELL VERSION OF GREP
	#
	vflag= xflag= cflag= lflag= nflag=
	set -f
	while	((1))				# look for grep options
	do	case	"$1" in
		-v*)	vflag=1;;
		-x*)	xflag=1;;
		-c*)	cflag=1;;
		-l*)	lflag=1;;
		-n*)	nflag=1;;
		-b*)	print 'b option not supported';;
		-e*)	shift;expr="$1";;
		-f*)	shift;expr=$(< $1);;
		-*)	print $0: 'unknown flag';return 2;;
		*)
			if	test "$expr" = ''
			then	expr="$1";shift
			fi
			test "$xflag" || expr="*${expr}*"
			break;;
		esac
		shift				# next argument
	done
	noprint=$vflag$cflag$lflag		# don't print if these flags are set
	integer n=0 c=0 tc=0 nargs=$#		# initialize counters
	for i in "$@"				# go thru the files
	do	if	((nargs<=1))
		then	fname=''
		else	fname="$i":
		fi
		test "$i"  &&  exec 0< $i	# open file if necessary
		while	read -r line		# read in a line
		do	let n=n+1
			case	"$line" in
			$expr)			# line matches pattern
				test "$noprint" || print -r -- "$fname${nflag:+$n:}$line"
				let c=c+1 ;;
			*)			# not a match
				if	test "$vflag"
				then	print -r -- "$fname${nflag:+$n:}$line"
				fi;;
			esac
		done
		if	test "$lflag" && ((c))
		then	print -r -- "$i"
		fi
		let tc=tc+c n=0 c=0
	done
	test "$cflag" && print $tc		#  print count if cflag is set
	let tc					#  set the return value
}

trap 'rm -f /tmp/grep$$' EXIT
cat > /tmp/grep$$ <<\!
this is a food bar test
to see how many lines find both foo and bar.
Some line contain foo only,
and some lines contain bar only.
However, many lines contain both foo and also bar.
A line containing foobar should also be counted.
There should be six lines with foo and bar.
There are only two line with out foo but with bar.
!

if	(( $(grep -c 'foo*bar' /tmp/grep$$ ) != 6))
then	err_exit
fi
exit $((Errors))
