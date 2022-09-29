#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

. mrprofrc

# Ignore SIGHUP.  This is needed so that processes that haven't had
# a chance to run long enough to daemonize won't get killed when
# this script exits.
#

trap "" 1

LOGGER=/usr/bsd/logger

case "$1" in

start)

	# check case where old miniroot is being reused.  we do not want
        # modified syslog.conf in place initially
	test -s /etc/syslog.conf.orig && mv /etc/syslog.conf.orig /etc/syslog.conf 2>/dev/null
	cp /etc/syslog.conf /etc/syslog.conf.orig 2>/dev/null
	/usr/etc/syslogd
	;;

restart)

	test -d /root/var/adm || mkdir -p /root/var/adm 2>/dev/null
	/usr/etc/syslogd
	;;

stop)

	killall -TERM syslogd
	;;

move)

	( cat /etc/syslog.conf | sed "s/	\/var\/adm\/SYSLOG/	\/root\/var\/adm\/SYSLOG/" > /etc/syslog.conf.sav ) 2> /dev/null
	if [ -s /etc/syslog.conf.sav ] ; then
	    mv -f  /etc/syslog.conf.sav /etc/syslog.conf
	    test -d /root/var/adm || mkdir -p /root/var/adm 2>/dev/null
	    (cat /var/adm/SYSLOG >> /root/var/adm/SYSLOG) 2>/dev/null
	    rm -f /var/adm/SYSLOG 2>/dev/null
	    killall -HUP syslogd 
	fi
	rm -f /etc/syslog.conf.sav 2>/dev/null

	;;

addhost)
	shift 2>/dev/null
	tmp1=/etc/syslog.conf1.$$
	tmp2=/etc/syslog.conf2.$$
	tmp3=/etc/syslog.conf3.$$

	sed -n 's/^##may be uncommented##//p' /etc/syslog.conf >$tmp1 2>/dev/null
	if [ -s $tmp1 ]; then
	    ( for host in $* ;  do
		  echo '## remote loghost '$host' ##'
		  sed 's/	@.*/	@'$host'/g' $tmp1
		  echo '## done loghost '$host' ##'
	      done ) >$tmp2 2>/dev/null
	fi

	if [ -s $tmp2 ]; then
	    cat /etc/syslog.conf $tmp2 > $tmp3 2>/dev/null
	    mv $tmp3 /etc/syslog.conf
	    killall -HUP syslogd
	fi
	rm -f $tmp1 $tmp2 $tmp3 2>/dev/null
	;;

localoff)       # comment-out the local entries
        nawk ' /## remote loghost .* /,/## done loghost .* ##/ { print;next; }
                { printf "## savelocal ##%s\n", $0 }' /etc/syslog.conf \
                >/etc/syslog.conf.sav 2>/dev/null
        mv /etc/syslog.conf.sav /etc/syslog.conf
        killall -HUP syslogd
        ;;

localon)        # restore the commented-out local entries
        sed 's/## savelocal ##//g' /etc/syslog.conf \
                >/etc/syslog.conf.sav 2>/dev/null
        mv /etc/syslog.conf.sav /etc/syslog.conf
        killall -HUP syslogd
        ;;

*)
    echo "usage: $0 {start|stop|restart|move|addhost ipaddr}"
    ;;

esac
