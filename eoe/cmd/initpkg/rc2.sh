#! /sbin/sh
#Tag 0x00000f00
#ident "$Revision: 1.23 $"

#	"Run Commands" executed when the system is changing to init state 2,
#	traditionally called "multi-user".


# Mount /proc if it is not already mounted.
/etc/mntproc

#	Pickup start-up packages for mounts, daemons, services, etc.
set `who -r`
if [ $9 = "S" ]
then
	#
	# Coming from single-user
	#
	BOOT=yes
	if [ -f /etc/rc.d/PRESERVE ]	# historical segment for vi and ex
	then
		mv /etc/rc.d/PRESERVE /etc/init.d
		ln /etc/init.d/PRESERVE /etc/rc2.d/S02PRESERVE
	fi

elif [ $7 = "2" ]
then
	#
	# Coming from some other state (ie: not single user)
	# Shut down any services available in the other state.
	#
	echo 'Changing to state 2.'
	if [ -d /etc/rc2.d ]
	then
		for f in /etc/rc2.d/K*
		{
			if [ -s ${f} ]
			then
				/sbin/sh ${f} stop
			fi
		}
	fi
fi

#
# Execute all package initialization scripts
# (ie: mount the filesystems, start the daemons, etc)
#
if [ -d /etc/rc2.d ]
then
	for f in /etc/rc2.d/S*
	{
		if [ -s ${f} ]
		then
			/sbin/sh ${f} start
		fi
	}
fi

#
# Historical - run any scripts in /etc/rc.d
#
if [ "${BOOT}" = "yes" -a -d /etc/rc.d ]
then
	for f in `ls /etc/rc.d`
	{
		if [ ! -s /etc/init.d/${f} ]
		then
			/sbin/sh /etc/rc.d/${f} 
		fi
	}
fi

#
# Make the SysAdmin warm and fuzzy
#
if [ "${BOOT}" = "yes" -a $7 = "2" ]
then
	if chkconfig verbose
	then
		echo "The system is ready."
	fi
elif [ $7 = "2" ]
then
	echo 'Change to state 2 has been completed.'
fi
