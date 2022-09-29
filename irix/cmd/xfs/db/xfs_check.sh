#!/bin/sh -f
#ident "$Revision: 1.3 $"

OPTS=" "
ISFILE=" "
USAGE="usage: xfs_check [-svf] [-i ino]... [-b bno]... special"


while getopts "b:fi:sv" c
do
	case $c in
	s)	OPTS=$OPTS"-s ";;
	v)	OPTS=$OPTS"-v ";;
	i)	OPTS=$OPTS"-i "$OPTARG" ";;
	b)	OPTS=$OPTS"-b "$OPTARG" ";;
	f)	ISFILE=" -f";;
	\?)	echo $USAGE 1>&2
		exit 2
		;;
	esac
done
shift `expr $OPTIND - 1`
case $# in
	1)	xfs_db$ISFILE -r -p xfs_check -c "check$OPTS" $1
		status=$?
		;;
	*)	echo $USAGE 1>&2
		exit 2
		;;
esac
exit $status
