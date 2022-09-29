#!/bin/sh

if [ $mr = true -a ! -d $rbase/var/statmon/sm ]
then
	mkdir -p $rbase/var/statmon/sm >/dev/null 2>&1;
	cp $rbase/usr/etc/statd.d/sm/* $rbase/var/statmon/sm >/dev/null 2>&1;
	mkdir $rbase/var/statmon/sm.bak >/dev/null 2>&1;
	cp $rbase/usr/etc/statd.d/sm.bak/* $rbase/var/statmon/sm.bak >/dev/null 2>&1;
	cp $rbase/usr/etc/statd.d/state $rbase/var/statmon >/dev/null 2>&1;
	rm -r $rbase/usr/etc/statd.d >/dev/null 2>&1;
fi
rm $rbase/var/statmon/.rpc.statd.inst.sh >/dev/null 2>&1;
exit 0
