#! /sbin/sh
#Tag 0x00000f00
#ident "$Revision: 1.9 $"

#	"Run Commands" executed when the system is changing to init state 3,
#	same as state 2 (multi-user) but with remote file sharing.
set `who -r`
if [ -d /etc/rc3.d ]
then
	for f in /etc/rc3.d/K*
	{
		if [ -s ${f} ]
		then
			/bin/sh ${f} stop
		fi
	}

	for f in /etc/rc3.d/S*
	{
		if [ -s ${f} ]
		then
			/bin/sh ${f} start
		fi
	}
fi
if [ $9 = 'S' ]
then
	echo '
The system is ready.'
fi
