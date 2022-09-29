#! /bin/sh

#
#  NAME
#	makewhatis - make manual page "whatis" database for use with apropos
#
#  SYNOPSIS
#	makewhatis [-M manpath] [filename]
#
#  DESCRIPTION
#	makewhatis scans the preformatted manual page trees (/usr/catman,
#	/usr/share/catman, /usr/man, /usr/share/man),
#	parses the manual pages, and strips out the NAME section information
#	to create the "whatis" database used by apropos(1).
#
#	By default, makewhatis creates the file, /usr/share/catman/whatis.  Another
#	file may be created as the database by specifying its filename on
#	the command line.  You must run makewhatis as root to create the
#	whatis file.  See su(1M).
#
#	An alternate manual page tree may be specified by using the -M option to
#	specify a path or paths to a manual page tree or trees.  See the
#	discussion of the -M option in man(1) for more details.
#
#	The format of the whatis file created is based on that used by
#	William Joy's UC Berkeley version of apropos.
#
#  ENVIRONMENT
#	The environment variables used by man(1) apply to makewhatis.
#
#  SEE ALSO
#	apropos(1), man(1), whatis(1)
#  
#  FILES
#	/usr/share/catman/whatis
#
#  CAVEATS
#	This program is S...L...O...W.  Expect execution times of about
#	10-15 minutes.  The reason is that EVERY manual page on the system
#	is read.
#
#	You must run makewhatis as root.
#
#  AUTHOR
#	Dave Ciemiewicz (ciemo)
#
#  COPYRIGHT (C) 1989, Silicon Graphics, Inc., All rights reserved.
#

USAGE="Usage: makewhatis [-M manpath] [-s] [filename]"
WHATIS=/usr/share/catman/whatis

while getopts "M:s" c
do
    case $c in
    	M)  MANPATHFLAG="-M "$OPTARG;;
		s)	silent=1 ;;
    	\?) echo $USAGE 1>&2
	    exit 2
	    ;;
    esac
done
shift `expr $OPTIND - 1`


case $# in
    0)	;;
    1)	WHATIS=$1;;
    *)  echo $USAGE 1>&2
	exit 2
	;;
esac

#
# Only launch if we can create $WHATIS
#
if touch -t 197001010000 $WHATIS > /dev/null 2>&1
then
    trap "rm -f ${WHATIS}.NEW > /dev/null 2>&1; exit 0" 2
    (/usr/lib/manwhat $MANPATHFLAG | sort -f -u > ${WHATIS}.NEW) \
		2>&1 | egrep -v '^awf:.*unknown|^Broken pipe'
    if [ -s ${WHATIS}.NEW ]
    then
        mv ${WHATIS}.NEW ${WHATIS}
	chmod a+r $WHATIS # protect against restrictive umask
    else
	echo $0 failed 1>&2
	rm -f ${WHATIS}.NEW > /dev/null 2>&1
    fi

    # Check to see if /usr/catman/whatis is still around
    if [ -f /usr/catman/whatis ]
    then
	echo
	echo "NOTE: the new version of the whatis file resides in"
	echo "/usr/share/catman rather than /usr/catman.  The file"
	echo "/usr/catman/whatis is obsolete and can be removed."
	echo
    fi 
else
	if [ "$silent" -ne 1 ]
    then echo $0: cannot create $WHATIS 1>&2
	fi
    exit 1
fi
