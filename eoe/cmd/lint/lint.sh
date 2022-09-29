#! /bin/sh
# --------------------------------------------------- 
# | Copyright (c) 1989 MIPS Computer Systems, Inc.  | 
# | All Rights Reserved.                            | 
# --------------------------------------------------- 
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/lint/RCS/lint.sh,v 7.2 1991/08/05 10:37:33 davea Exp $
#
#
# New lint shell script.  Changed to make lint(1) act as much as is possible
# like a different version of the cc(1) command.  This includes the notion of
# a ``lint .o'' (.ln) and incremental linting.  Thu Jan 27 10:07:15 EST 1983
#
if test -z "$TMPDIR"; then
  TMPDIR=/usr/tmp
fi
TOUT=$TMPDIR/tlint.$$		# combined input for second pass
HOUT=$TMPDIR/hlint.$$		# header messages file
PATH=/bin:/usr/bin:
LDIR=$TOOLROOT/usr/lib/lint		# where first & second pass are
LLDIR=$TOOLROOT/usr/lib/lint		# where lint libraries are found
CCF="-cckr -E -C -Dlint"		# options for the cc command
CC=$TOOLROOT/usr/bin/cc
LINTF=				# options for the lint passes
LINT1F=				# special flags just for lint1
FILES=				# the *.c and *.ln files in order
NDOTC=				# how many *.c were there
DEFL=$LLDIR/llib-lc.ln		# the default library to use
LLIB=				# lint library file to create
CONLY=				# set for ``compile only''
pre=				# these three variables used for
post=				# handling options with arguments
optarg=				# list variable to add argument to
sawsystype=                     # handling for -systype args
sawwoff=			# handling for -woff list
systype=
sawOlimit=			# handling for -Olimit nnn
verb=0			# echo commands
#
trap "rm -f $TOUT $HOUT; exit 2" 1 2 3 15
#
# First, run through all of the arguments, building lists
#
#	lint's options are "abchl:no:puvx"
#	cc/cpp options are "I:D:U:gOG:"
#	NOTE: -I can have no args
#
usage=0
for OPT in "$@"
do
	if [ "$sawwoff" ]
	then
		sawwoff=
		LINT1F="$LINT1F -Xwoff$OPT"
		continue
	fi
	if [ "$sawsystype" ]
	then
		sawsystype=
		CCF="$CCF -systype $OPT"
		systype=$OPT
		LLDIR="/$OPT/$LLDIR"
		DEFL="/$OPT/$DEFL"
		continue
	fi
	if [ "$optarg" ]
	then
		if [ "$optarg" = "LLIB" ]	# special case...
		then
			OPT=`basename $OPT`
		fi
		if [ "$optarg" = "CCF" -a "$pre" = "-G" ]
		then
			# cc wont take -Gnum (must be -G num)
			eval "$optarg=\"\$$optarg \$pre \$OPT\$post\""
		else
		if [ "$optarg" = "CCF" -a "$pre" = "-D" ]
		then
			# plain -D keep spaces aroud it
			eval "$optarg=\"\$$optarg \$pre \$OPT \$post\""
		else
		eval "$optarg=\"\$$optarg \$pre\$OPT\$post\""
		fi
		fi
		pre=
		post=
		optarg=
		continue
	fi
	if [ "$sawOlimit" ]
	then
		sawOlimit=
		continue
	fi
	case "$OPT" in
	-woff)
			sawwoff=t
			;;
	-A)      LINT1F="$LINT1F -XA" ;;
	-Olimit)  sawOlimit=1;;
	-cckr)   ;; # do nothing
	-ansi)    echo 'lint ignores -ansi. assuming -cckr' ;;
	-xansi)     echo 'lint ignores -xansi. assuming -cckr' ;;
	-ansi_posix) echo 'lint ignores -ansi_posix. assuming -cckr' ;;
	*.c)	FILES="$FILES $OPT"	NDOTC="x$NDOTC";;
	*.ln)	FILES="$FILES $OPT";;
	-systype) sawsystype=$OPT;;
	-*)	OPT=`echo $OPT | sed s/-//`
		while [ "$OPT" ]
		do
			O=`echo $OPT | sed 's/\\(.\\).*/\\1/'`
			OPT=`echo $OPT | sed s/.//`
			case $O in
			W)      
				case $OPT in
				f,*)
				 LV=`echo $OPT | sed 's/\\(f,\\)\\(.*\\)/\\2/'`
					LINTF="$LINTF $LV"
					;;
				*)       # ignore, not relevant
					;;  
		
			        esac 
				break ;;
			s)
				if [ "$O$OPT" = "signed" ]
				then
					LINTF="$LINTF -$O"
					break
				fi
				echo "lint: bad option ignored: $O$OPT"
				break
				;;
			f)	
				if [ "$O$OPT" = "float" ]
				then
					# ignore, not relevant
					break
				fi
				echo "lint: bad option ignored: $O$OPT"
				break
				;;
				
			W)
				CCF="$CCF -$O$OPT" # not really very useful
				break;;
			L)
				CCF="$CCF -$O$OPT"
				if [ "$OPT"x != "x" ]
				then
					LLDIR=$OPT/lint
					DEFL=$LLDIR/llib-lc.ln
				fi
				break;;
			p)	
				if [ "$O$OPT" = "prototypes" ]
				then
					break #  -prototypes is automatic
				else
				  LINTF="$LINTF -p"
				  DEFL=$LLDIR/llib-port.ln
				fi
				;;
			n)	
				if [ "$O$OPT" = "nostdinc" ]
				then
					CCF="$CCF -$O$OPT"
					break
				else
				if [ "$O$OPT" = "nostdlib" ]
				then
					break # ignore -nostdlib
				else
				if [ "$O$OPT" = "noprototypes" ]
				then
					break # ignore -noprototypes
				else
				  LINTF="$LINTF -n"
				  DEFL=
				fi
				fi
				fi
				;;
			c)	CONLY=1;;
			y)	LINT1F="$LINT1F -y" ;;
			z)	LINT1F="$LINT1F -z" ;;
			[abhuvx]) LINTF="$LINTF -$O";;
			[gO])	CCF="$CCF -$O";;
			I)	CCF="$CCF -$O$OPT"
				break;;
			[GDU])	if [ "$OPT" ]
				then
					CCF="$CCF -$O$OPT"
				else
					optarg=CCF
					pre=-$O
				fi
				break;;
			l)	if [ "$OPT" ]
				then
					FILES="$FILES $LLDIR/llib-l$OPT.ln"
				else
					optarg=FILES
					pre=$LLDIR/llib-l
					post=.ln
				fi
				break;;
			o)	if [ "$OPT" ]
				then
					OPT=`basename $OPT`
					LLIB="llib-l$OPT.ln"
				else
					LLIB=
					optarg=LLIB
					pre=llib-l
					post=.ln
				fi
				break;;
			systype) sawsystype=1;
				break;;
			V)	verb=1;
				CCF="$CCF -v"
				break;;
			*)	echo "lint: bad option ignored: $O"
				usage=1;;
			esac
		done;;
	*)	echo "lint: file with unknown suffix ignored: $OPT"
		usage=1;;
	esac
done
if test "$usage" = "1"; then
	echo "Usage: $0 [options] file ..."
	echo '	lint options are "abchlnopuvVxA  -woff number-list -L"'
	echo '	cc/cpp options are "IDUgO"'
	echo '  cc/cpp options accepted but ignored are "-Olimit -signed -float -nostdinc -nostdlib -prototypes -noprototypes"'
fi
#
# Second, walk through the FILES list, running all .c's through
# lint's first pass, and just adding all .ln's to the running result
#
lint1stat=""
if [ "$NDOTC" != "x" ]	# note how many *.c's there were
then
	NDOTC=1
else
	NDOTC=
fi
if [ "$CONLY" ]		# run lint1 on *.c's only producing *.ln's
then
	for i in $FILES
	do
		case $i in
		*.c)	T=`basename $i .c`.ln
			if [ "$NDOTC" ]
			then
				echo $i:
			fi
			if [ $verb = 1 ]
			then
			 echo '(' cc $CCF $i '|' $LDIR/lint1 $LINTF $LINT1F -H$HOUT $i '>' $T ') 2>&1'
			fi
			($CC $CCF $i | $LDIR/lint1 $LINTF $LINT1F -H$HOUT $i >$T) 2>&1
			status=$?
			if [ $status != "0" ]
			then
				lint1stat="$i"
			fi
			if [ $verb = 1 ]
			then
			 echo $LDIR/lint2 -H$HOUT
			fi
			$LDIR/lint2 -H$HOUT
			if [ $verb = 1 ]
			then
			 echo rm -f $HOUT
			fi
			rm -f $HOUT;;
		esac
	done
else			# send all *.c's through lint1 run all through lint2
	if [ $verb = 1 ]
	then
	 echo rm -f $TOUT $HOUT
	fi
	rm -f $TOUT $HOUT
	for i in $FILES
	do
		case $i in
		*.ln)	if [ $verb = 1 ]
			then
			 echo cat '<' $i '>>' $TOUT
			fi
			cat <$i >>$TOUT;;
		*.c)	if [ "$NDOTC" ]
			then
				echo $i:
			fi
			if [ $verb = 1 ]
			then
			 echo '(' cc $CCF $i '|' $LDIR/lint1 $LINTF $LINT1F -H$HOUT $i '>>' $TOUT ')2>&1'
			fi
			($CC $CCF $i|$LDIR/lint1 $LINTF $LINT1F -H$HOUT $i >>$TOUT)2>&1
			status=$?
			if [ $status != "0" ]
			then
				lint1stat="$i"
			fi
			;;
		esac
	done
	if [ "$LLIB" ]
	then
		if [ $verb = 1 ]
		then
		 echo cp $TOUT $LLIB
		fi
		cp $TOUT $LLIB
	fi
	if [ "$DEFL" ]
	then
		if [ $verb = 1 ]
		then
		 echo cat '<' $DEFL '>>' $TOUT
		fi
		cat <$DEFL >>$TOUT
	fi
	if [ x$lint1stat != "x" ]
	then
		echo "Error: lint pass 1 failed on " $lint1stat ". Use cc to see compilation errors."
		echo "Correct the errors before running lint."
		rm -f $TOUT $HOUT
		exit 1
	else
	 if [ -s "$HOUT" ]
	 then
		if [ $verb = 1 ]
		then
		 echo $LDIR/lint2 -T$TOUT -H$HOUT $LINTF
		fi
		$LDIR/lint2 -T$TOUT -H$HOUT $LINTF
	 else
		if [ $verb = 1 ]
		then
		 echo $LDIR/lint2 -T$TOUT $LINTF
		fi
		$LDIR/lint2 -T$TOUT $LINTF
	 fi
	fi
fi
if [ $verb = 1 ]
then
 echo rm -f $TOUT $HOUT
fi
rm -f $TOUT $HOUT
exit 0
