#! /sbin/sh
# NAME
#	mkdepend - compute header file dependencies
# SYNOPSIS
#	mkdepend [-c compiler] [-e sedprog] [-f force] [-ipr] [-s sentinel]
#		depfile file ...
# DESCRIPTION
#	See mkdepend(1).
#
me=$0
usage="usage: $me [-c compiler] [-e sedprog] [-f force] [-ipr]\n\t[-s sentinel] depfile [file ...]"
depgen="cc -M"
incremental=no
rawdepinput=no

#
# Process options and arguments.  We put quotes around the edit (-e) arguments
# and use eval "sed -e ..." later, to preserve spaces in the edit command.
#
while getopts c:e:f:ip:rs: OPT; do
	case $OPT in
	  c)	depgen=$OPTARG;;
	  e)	sedprog="$sedprog -e '$OPTARG'";;
	  f)	force="$force $OPTARG";;
	  i)	incremental=yes;;
	  p)	depgen=collapse pcount=$OPTARG rawdepinput=yes;;
	  r)	depgen=cat rawdepinput=yes;;
	  s)	sentinel=$OPTARG;;
	  *)	echo $usage; exit 2
	esac
done
shift `expr $OPTIND - 1`
case $# in
  0)	echo $usage; exit 2
esac

newdepfile=$1
depfiledir=`dirname $1`
olddepfile=$depfiledir/#`basename $1`
shift

#
# A shell function to collapse dependencies of targets in subdirectories
# on common rightsides.
#
collapse() {
	sed -e 's@/\([^/:]*\): @/%\1: @' $* |
	sort -t% +1 |
	uniq |
	nawk -F: '
	BEGIN {
		total = '$pcount'
	}
	function flush(group, lastdir, dirname, count) {
		for (dirname in group)
			count++
		if (count == total) {
			print group[lastdir]
		} else {
			for (dirname in group)
				print dirname group[dirname]
		}
		for (dirname in group)
			delete group[dirname]
	}
	{
		percent = index($1, "%")
		name = substr($1, percent + 1)
		if (name != basename || $2 != rightside) {
			flush(group, dirname)
			basename = name
			rightside = $2
		}
		dirname = substr($1, 1, percent - 1)
		group[dirname] = basename ":" rightside
	}
	END {
		flush(group, dirname)
	}' |
	sort
}

#
# An awk script to compress and pretty-print dependencies.
#
awkprog='
BEGIN {
	FS = ":"
	INDENT = "\t"
	INDENTSIZE = 8
	MAXLINE = 72
	MAXINDENTEDLINE = MAXLINE - INDENTSIZE - 1
}

#
# For each line of the form "target1 ... targetN: dependent", where the
# spaces are literal blanks, do the following:
#
{
	if ($1 != target) {
		if (depline) print depline
		target = $1
		depline= $0 "'"$force"'"
		lim = MAXLINE
	} else {
		if (length(depline) + length($2) > lim) {
			print depline " \\"
			depline = INDENT
			lim = MAXINDENTEDLINE
		}
		depline = depline $2
	}
}

END {
	if (depline) print depline 
}'

#
# Save the old file in case of problems.  Define some temporary files for
# incremental make depend.  Clean up if interrupted.
#
olddeps=/tmp/olddeps.$$
newdeps=/tmp/newdeps.$$
tmpdeps=$depfiledir/tmpdeps.$$
if test $incremental = yes; then
	trapcmds="rm -f $olddeps $newdeps $tmpdeps;"
fi
if test -f $newdepfile -o -l $newdepfile; then
	trapcmds="$trapcmds mv -f \\$olddepfile \\$newdepfile;"
	ln -f $newdepfile $olddepfile
	echo "# placeholder in case make includes me" > $tmpdeps
	mv -f $tmpdeps $newdepfile
fi
trap "$trapcmds exit 0" 1 2 15

#
# Remove any old dependencies from the file.
#
firstline="#$sentinel DO NOT DELETE THIS LINE -- make depend uses it"
lastline="#$sentinel DO NOT DELETE THIS 2nd LINE -- make depend uses it"

if test -f $olddepfile -o -l $olddepfile; then
	sed -e "/^$firstline/,/^$lastline/d" $olddepfile
fi > $newdepfile

#
# Delimit the beginning of the dependencies.  Save the old dependencies
# in incremental mode.
#
echo $firstline >> $newdepfile
if test $incremental = yes; then
	#
	# XXX Workaround for this sed bug: 1,/pat/d deletes 1,$ if line 1
	# XXX and only line 1 matches pat.
	#
	(echo '# food for sed bug to eat'; cat $olddepfile 2> /dev/null) |
	    sed -e "1,/^$firstline/d" -e "/^$lastline/"',$d' > $olddeps

	#
	# Compose an awk program that deletes pretty-printed dependencies
	# from $olddeps that involve basenames of objects (in the raw input)
	# or sources (arguments) whose dependencies are being updated.
	#
	awkinc='/^[^	 ]/ { deleting = 0; }'
	if test $rawdepinput = yes; then
		cat $* > $newdeps
		for f in `sed -e 's@^\([^:/]*\)\.[^:/]*: .*@\1@' \
			      -e 's@^[^:]*/\([^:/]*\)\.[^:/]*: .*@\1@' \
			      $newdeps |
			  sort -u`; do
			list="$list $f"
		done
		set -- $newdeps
	else
		for f in $*; do
			list="$list `expr ./$f : '.*/\(.*\)\..*'`"
		done
	fi
	for f in $list; do
		awkinc="$awkinc /^$f\./ || /^[^	 :]*\/$f\./ { deleting = 1; }"
	done
	awkinc="$awkinc { if (!deleting) print \$0; }"
	awk "$awkinc" $olddeps >> $newdepfile
	rm -f $olddeps
fi

#
# Process source files - the sed commands and their functions are: 
#	s:[^\./][^\./]*/\.\./::g	rewrite "dir/../path" to be "path"
#	s:\([/ ]\)\./:\1:g		rewrite " ./path" to be " path" and
#					remove the "./" from  ".././h/x.h"
#
$depgen $* |
    eval "sed \
	-e :loop -e 's:[^\./][^\./]*/\.\./::g' -e tloop \
	-e 's:\([/ ]\)\./:\1:g' \
	$sedprog" |
    sort -u |
    awk "$awkprog" >> $newdepfile

echo $lastline >> $newdepfile
rm -f $olddepfile $newdeps
