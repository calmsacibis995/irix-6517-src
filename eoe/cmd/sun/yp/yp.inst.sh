#!/bin/sh
# set -xv
# This shell script moves all the configuration scripts and
# YP databases from /usr/etc/yp to their new location, in
# /var/yp.  It is executed as part of the inst for YP.
#
# This script also deletes all the binaries (e.g., ypxfr) that 
# are now located in /usr/sbin.  Note that shell scripts and
# executables that remain (e.g., make.script) may be overwritten 
# during the inst.
#
# This script will not do the move if if /var/yp/ypdomain exists.
#
# Note:
#   inst will create the directory /var/yp and copy this script
#   there before executing it.
#
#   The install also creates a symlink from /usr/etc/yp to /var/yp.
#   When that happens, the existing /usr/etc/yp will be renamed
#   to /usr/etc/yp.O.  Thus, this script looks there for existing
#   configuration files.
#   We know that the symlink will be created before this file
#   executes since the entries in the idb file are processed
#   alphabetically, (and /usr/etc/yp is before /var/yp.)
# 
if [ -f $rbase/var/yp/ypdomain ]
then :
else
    if [ -d $rbase/usr/etc/yp.O -o -l $rbase/usr/etc/yp.O ]
    then
        echo "moving contents of" $rbase/usr/etc/yp.O "to" $rbase/var/yp
        cd $rbase/usr/etc/yp.O

#	Copy any subdirectories that may be in /usr/etc/yp.O
#	and then move the files.
        for i in `find . -type d -print`; do
            mkdir -p $rbase/var/yp/$i > /dev/null 2>&1
        done
        for i in `find . ! -type d -print`; do
	    mv $i $rbase/var/yp/$i
	done

#	Now delete the subdirectories and /usr/etc/yp.O itself.
	rmdir * > /dev/null 2>&1
	cd /
	rmdir $rbase/usr/etc/yp.O > /dev/null 2>&1

        if [ -f $rbase/var/yp/makedbm ] 
	then rm $rbase/var/yp/makedbm 
	fi
        if [ -f $rbase/var/yp/mkalias ] 
	then rm $rbase/var/yp/mkalias 
	fi
        if [ -f $rbase/var/yp/mknetid ] 
	then rm $rbase/var/yp/mknetid 
	fi
        if [ -f $rbase/var/yp/registrar ] 
	then rm $rbase/var/yp/registrar 
	fi
        if [ -f $rbase/var/yp/revnetgroup ] 
	then rm $rbase/var/yp/revnetgroup 
	fi
        if [ -f $rbase/var/yp/stdhosts ] 
	then rm $rbase/var/yp/stdhosts 
	fi
        if [ -f $rbase/var/yp/updbootparam ] 
	then rm $rbase/var/yp/updbootparam 
	fi
        if [ -f $rbase/var/yp/stdhosts ] 
	then rm $rbase/var/yp/stdhosts 
	fi
        if [ -f $rbase/var/yp/updbootparam ] 
	then rm $rbase/var/yp/updbootparam 
	fi
        if [ -f $rbase/var/yp/yp_bootparam ] 
	then rm $rbase/var/yp/yp_bootparam 
	fi
        if [ -f $rbase/var/yp/yp_host ] 
	then rm $rbase/var/yp/yp_host 
	fi
        if [ -f $rbase/var/yp/ypmapname ] 
	then rm $rbase/var/yp/ypmapname 
	fi
        if [ -f $rbase/var/yp/yppoll ] 
	then rm $rbase/var/yp/yppoll 
	fi
        if [ -f $rbase/var/yp/yppush ] 
	then rm $rbase/var/yp/yppush 
	fi
        if [ -f $rbase/var/yp/ypset ] 
	then rm $rbase/var/yp/ypset 
	fi
        if [ -f $rbase/var/yp/ypxfr ] 
	then rm $rbase/var/yp/ypxfr 
	fi
    fi
fi
rm $rbase/var/yp/.yp.inst.sh
