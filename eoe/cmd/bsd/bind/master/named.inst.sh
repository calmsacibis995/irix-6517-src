#!/bin/sh
#
#Move the named configuration databases if it hasn't been done yet.
#Only do the move if /etc/named.boot does not exist and /usr/etc/named.d.O
#does exist.  If that is not the case and they both exist, then assume
#that the new named stuff is already installed and just remove
#/usr/etc/named.d.O
#
#Look for the old stuff in /usr/etc/named.d.O, because inst
#should have replaced /usr/etc/named.d with a symlink to /var/named
#by the time we execute.
#
#When moving named.boot, pipe it through sed to replace any
#occurences of /usr/etc/named.d with /var/named.
#When moving other files in /usr/etc/named.d.O, don't bother with
#the default installed files.  The new versions of these files
#will be automatically installed in /var/named by inst.
#
#Only remove /usr/etc/named.d.O if we were successful in
#moving everything from there to /var/named.
#
#Once everything is moved and removed, check to see if
#/var/named/named.boot exists.  If it doesn't, then install it as
#a symlink to /etc/named.boot.  We can't do this from inst because
#inst will blow away /var/named/named.boot if it exists.  We can't
#have that because it is likely that administrator's will mess with
#this stuff and we don't want to destroy their customizations.
#
if [ -f $rbase/usr/etc/named.d.O/named.boot -a ! -f $rbase/etc/named.boot ]
then
	cat $rbase/usr/etc/named.d.O/named.boot | \
	sed -e 's#/usr/etc/named.d#/var/named#g' > $rbase/etc/named.boot
	CPWORKED=true
	for i in $rbase/usr/etc/named.d.O/*
	do
		if [ $i != "$rbase/usr/etc/named.d.O/named.boot" -a \
		     $i != "$rbase/usr/etc/named.d.O/Examples" -a \
		     $i != "$rbase/usr/etc/named.d.O/mkdns" -a \
		     $i != "$rbase/usr/etc/named.d.O/README" ]
		then
			if cp -r $i $rbase/var/named 2>&1 >/dev/null
			then
				true
			else
				CPWORKED=false
			fi
		fi
	done
	if $CPWORKED
	then
		if rm -rf $rbase/usr/etc/named.d.O 2>&1 >/dev/null
		then
			true
		else
			echo "named.inst.sh: $rbase/usr/etc/named.d.O \
could not be"
			echo "               removed. Remove it at your \
convenience."
		fi
	else
		echo "named.inst.sh: $rbase/usr/etc/named.d.O could not be"
		echo "               moved. Remove it at your convenience."
	fi
elif [ -d $rbase/usr/etc/named.d.O -a -f $rbase/etc/named.boot ]
then
	if rm -rf $rbase/usr/etc/named.d.O 2>&1 >/dev/null
	then
		true
	else
		echo "named.inst.sh: $rbase/var/etc/named.d.O could not be"
		echo "               removed.  Remove it at your convenience."
	fi
fi

#
#See if we should install the /var/named/named.boot -> /etc/named.boot
#symlink.  Only install the link if it won't overwrite anything.
#
if [ ! -f $rbase/var/named/named.boot -a \
     ! -l $rbase/var/named/named.boot -a \
     ! -d $rbase/var/named/named.boot ]
then
	ln -s ../../../etc/named.boot $rbase/var/named/named.boot \
	 2>&1 >/dev/null
fi
exit 0




