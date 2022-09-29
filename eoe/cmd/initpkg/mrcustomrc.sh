#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

# This script is a no op unless executed with nvram mrmode set
# to either custom or customdebug, or unless the force argument
# is given on the command line.

# Contains functions used to implement roboinst hooks.
# Use "iscustom" argument to determine if executing in custom mode.
# Other arguments described at the end.  If preceded by "force", the
# function is executed regardless of mrmode.


. mrprofrc

mrmode=`nvram mrmode 2>/dev/null`
if [ "$mrmode" != custom -a "$mrmode" != debug -a "$mrmode" != customdebug -a "$1" != force ]; then
    exit 1
fi
if [ "$1" = force ]; then
    shift 2>/dev/null
fi
if [ "$1" = iscustom ]; then
    exit 0  # test for custom mode
fi

INSTRBASE=/root
BPCLT="/usr/etc/bootpc -dr"
PROCLAIM=/usr/etc/proclaim
PROCONFIG=/etc/config/proclaim.options
PROLEASE=/var/adm/proclaim.data
CUSTOM=/custom
MRNETRC=/etc/mrnetrc
MRMOUNTRC=/etc/mrmountrc
MRLOGRC=/etc/mrlogrc
MRCONF=mrconfig
MRCONFIG=$CUSTOM/$MRCONF
MREXPORTS=$CUSTOM/$MRCONF.exports
INDEXFILE=".index"
LOGOK="logmsg ok"
ERROR="logmsg error"
WARNING="logmsg warning"
INFO="logmsg info"
DEBUG=":"
DEFCUSTOM=/usr/local/boot/roboinst/custom
DEFSERVER=configserver
ROBOINSTLOG=/var/inst/.roboinstlog
ROBOSTATUS=$INSTRBASE/var/inst/.roboinst_status
ROBOREBOOT=$INSTRBASE/etc/rc2.d/S99roboinst
ROUTESFILE=/tmp/static-route.options
PRO_CONFIGDIR=pro_roboinstdir
TMPFILE=/tmp/mrc.$$
TEMPHOST=IRIS
DBGFD=-
LOGGER="logger -t roboinst"
IFCONFIG=/usr/etc/ifconfig
# The highest version of mrconfig we understand.
# DEFVERS is assumed if mrconfig contains no "version" keyword.
MAXVERS=1
DEFVERS=1

# Enable debugging if requested

if [ "$mrmode" = debug -o "$mrmode" = customdebug -o "$mrmode" = test ]; then
    DEBUG="logmsg info"
    DBGFD=1
fi


# Determine which network interface to use

ifname()
{
    # this works even if there are interfaces without
    # miniroot drivers:
    hinv -c network | sed -ne 's/^Integr.* \([a-z][a-z]*0\),.*$/\1/p'
}

# Determine the hardware address

systemid()
{
    $BPCLT -f `ifname` 2>/dev/null | grep '^chaddr=' | sed 's/^chaddr=//'
}


# Send a log message to various places

logmsg()
{
    if [ "$1" = "ok" ]; then
        shift 2>/dev/null ; mstate="$1" ; shift 2>/dev/null
        msg="state=$mstate status=0 $*"
    elif [ "$1" = "error" ]; then
        shift 2>/dev/null ; mstate="$1" ;
        shift 2>/dev/null ; mstatus="$1" ; shift 2>/dev/null
        msg="state=$mstate status=$mstatus error: $*"
    elif [ "$1" = "warning" ]; then
        shift 2>/dev/null
	msg="warning: $*"
    elif [ "$1" = "info" ]; then
        shift 2>/dev/null
	msg="$*"
    else
        msg="$*"
    fi

    $LOGGER "$msg"
    echo "roboinst: $msg"
    if [ -f $ROBOINSTLOG ]; then
        # queue remote msgs until loghost is known
	echo "$msg" >> $ROBOINSTLOG
    fi
}

dbgmsg()
{
    while read dbline; do
        $DEBUG "$dbline"
    done
}

dirname()
{
    ans=`/usr/bin/expr "${1:-.}/" : '\(/\)/*[^/]*//*$' `
    if [ -n "$ans" ];then
	    echo $ans
    else
	    ans=`/usr/bin/expr "${1:-.}/" : '\(.*[^/]\)//*[^/][^/]*//*$' `
	    if [ -n "$ans" ];then
		    echo $ans
	    else
		    echo "."
	    fi
    fi
}


# Generate statements of the form:  "A=val ; export A ; B=val ; export B ; "
# to represent SGI_CAPACITY and SGI_CAP_<device>

disk_capacity()
{
    # Set SGI_CAP_<device>
    (cd /dev/scsi && ls -1|grep '^sc'|xargs -n10 scsicontrol -c \
	   | sed -n 's/^sc\([0-9]*\)d\([0-9]*\)l\([0-9]*\):.*capacity=\([0-9]*\)[^0-9]*\([0-9]*\)[^0-9]*/SGI_CAP_dks\1d\2vol=\4 ; export SGI_CAP_dks\1d\2vol ; /p')

    # Set SGI_CAPACITY for root device

    ( cd /dev/rdsk && \
      rootdev=`stat -rq root` && \
      test "$rootdev" && \
          for f in dks* ; do
	      test "$f" && \
		  test "`stat -rq $f`" = "$rootdev" && {
		      ff=`echo $f|sed -n 's/^dks\([0-9]*\)d\([0-9]*\)s\([0-9]*\)/dks\1d\2/p'`
		      echo "SGI_CAPACITY=\$SGI_CAP_${ff}vol ; export SGI_CAPACITY ; "
		      break;
		  }
	  done )
}

# Build the export file $MREXPORTS.  This holds environment
# variables that will be exported over the course of the miniroot
# session.  We only construct the file once, then . it in later
# invocations of mrcustomrc.

build_exports()
{
    inst -H > $TMPFILE 2>&-
    SGI_CPUBOARD=`sed -n 's/^CPUBOARD=//p' $TMPFILE`
    SGI_CPUARCH=`sed -n 's/^CPUARCH=//p' $TMPFILE|grep -iv MIPS`
    SGI_VIDEO=`sed -n 's/^VIDEO=//p' $TMPFILE`
    SGI_ABI=`sed -n 's/^CPUARCH=//p' $TMPFILE|grep -i MIPS`
    SGI_GFXBOARD=`sed -n 's/^GFXBOARD=//p' $TMPFILE`
    SGI_SUBGR=`sed -n 's/^SUBGR=//p' $TMPFILE`
    SGI_MODE=`sed -n 's/^MODE=//p' $TMPFILE`
    SGI_MACHINE=`sed -n 's/^MACHINE=//p' $TMPFILE`
    rm -f $TMPFILE 2>&-

    SGI_ROOT=$INSTRBASE
    SGI_CUSTOM=$CUSTOM
    SGI_SYSID=`systemid`
    SGI_IPADDR=`nvram netaddr 2>&-`
    SGI_HOSTNAME=`sed -n p /etc/sys_id 2>&-`
    SGI_MEMSIZE=`hinv | sed -n 's/.*Main memory size: \([0-9]*\) Mbytes.*/\1/p'`

    tapedevice=`nvram tapedevice 2>&-`
    bootfile=`echo "$tapedevice" | sed -e 's;.*bootp();;' -e 's;\([^ ()]*\).*;\1;'`
    SGI_BOOTSERVER=`echo "$bootfile" | sed -n 's;:.*;;p'`
    SGI_BOOTDIR=`echo "$bootfile" | sed -e 's;.*:;;' -e 's;^/sa$;/;' -e 's;/sa;;' -e 's;^sa$;.;'`

    configfile=`nvram mrconfig 2>&-`

    # determine whether to use DHCP or skip it.
    # default is to use DHCP, so that empty $configfile does the right thing
    # NOTE: the special character check is in multiple locations!
    case "$configfile" in
	!*)
	    # don't do any DHCP stuff
	    SGI_NETINQUIRY="NONE"
	    configfile=`echo "$configfile" | sed -e 's/^!//'`
	    ;;
	+*)
	    # DHCP assigns IP address if it is available
	    SGI_NETINQUIRY="DHCP"
	    configfile=`echo "$configfile" | sed -e 's/^+//'`
	    ;;
	%*|*)
	    # use extended BOOTP protocol
	    # (use IP address from nvram, but ask for network parameters)
	    SGI_NETINQUIRY="BOOTP1533"
	    configfile=`echo "$configfile" | sed -e 's/^%//'`
	    ;;
    esac

    SGI_CONFIGSERVER=`echo "$configfile" | sed -n 's;:.*;;p'`
    SGI_CONFIGDIR=`echo "$configfile" | sed -e 's;.*:;;'`

    SGI_SYSTEMPART=`devnm / | sed 's/. .*/0/' | xargs basename`
    SGI_SYSTEMDISK=`echo "$SGI_SYSTEMPART" | sed 's/s[0-9]*$//'`
    rm -f $MREXPORTS 2>&-

    ( for var in SGI_CPUBOARD SGI_GFXBOARD SGI_SUBGR SGI_VIDEO \
		SGI_CPUARCH SGI_ABI SGI_MODE SGI_MACHINE \
		SGI_ROOT SGI_CUSTOM \
		SGI_SYSID SGI_IPADDR SGI_HOSTNAME \
		SGI_BOOTSERVER SGI_BOOTDIR SGI_CONFIGSERVER SGI_CONFIGDIR \
		SGI_SYSTEMPART SGI_SYSTEMDISK \
		SGI_MEMSIZE SGI_NETINQUIRY ; do
	eval "echo \"\$$var\"" | \
	    sed -e 's/"/\\"/g' \
		-e 's/\(.*\)/'$var'="\1"; export '$var'/'
      done
      disk_capacity 2>&-
    ) > $MREXPORTS
}

# tftp remote file ($1) to local file ($2) using
# mode ($3).  Suppress msg if quiet is 1 ($4).

dotftp()
{
    tstat=0
    if [ "$3" != "" ]; then
	MODE=$3
    else
	MODE=binary
    fi
    test "$4" != 1 && $DEBUG "tftp $1 $2"
    tftp <<- EOF >$TMPFILE 2>&1
	    mode $MODE
	    get $1 $2
	    quit
	EOF
    test "$?" != 0 -o ! -f "$2" && {
	cat $TMPFILE
	tstat=1
    }

    rm -f $TMPFILE >/dev/null 2>&1
    return $tstat
}

# test whether a file ($1) has the expected size ($2)

chksize()
{
    test "$2" = "" && return 0
    s=`/sbin/stat -qs $1`
    test "$s" != "$2" && \
	{ $ERROR init 1 "Size mismatch for $1 $s (expected $2)";
	  $ERROR init 1 "Try re-running roboinst_config on the configuration directory";
	  return 1; }
    return 0
}

# test whether a file ($1) has the expected checksum ($2)

chksum()
{
    test "$2" = "" && return 0
    s=`sum -r $1 | nawk '{print $1}'`
    test "$s" != "$2" && \
	{ $ERROR init 1 "Checksum mismatch for $1 $s (expected $2)";
	  $ERROR init 1 "Try re-running roboinst_config on the configuration directory";
	  return 1; }
    return 0
}

# $1 = remote pathname, $2 = local pathname, $3 = tftp(0)/rcp(1), $4= attrs

filecopy()
{
    type=""
    size=""
    sum=""
    mode=""

    # parse valid attrs into local vars
    eval ` echo "$4" | \
       nawk \
	' { for (i=1; i <= NF; ++i)
	      if (match($i,"^size=")) {
		 s=substr($i,RSTART+RLENGTH,length($i)-RLENGTH);
		 if (match(s,"^[0-9][0-9]*$"))
		    printf("size=%s;",s);
	      } else if (match($i,"^sum=")) {
		 s=substr($i,RSTART+RLENGTH,length($i)-RLENGTH);
		 if (match(s,"^[0-9][0-9]*$"))
		    printf("sum=%s;",s);
	      } else if (match($i,"^type=")) {
	         s=substr($i,RSTART+RLENGTH,length($i)-RLENGTH);
		 if (match(s,"^[fd]*$"))
		    printf("type=%s;",s);
	      } else if (match($i,"^mode=")) {
		 s=substr($i,RSTART+RLENGTH,length($i)-RLENGTH);
		 if (match(s,"^[0-7][0-7]*$"))
		    printf("mode=%s;",substr(s,length(s)-3,3));
	      }
	    } ' `

    if [ "$type" = "" -o "$type" = "f" ]; then

	if [ $3 -eq 0 ]; then
	    dotftp $1 $2
	else
	    rcp -v $1 $2 >&$DBGFD 2>&$DBGFD
	fi

	( test $? -eq 0 &&
	  chksize $2 $size &&
	  chksum $2 $sum &&
	  chmod $mode $2 ) || return 1

    elif [ "$type" = "d" ]; then

	$DEBUG "mkdir $2"

	( mkdir -p $2 &&
	  chmod $mode $2 ) || return 1

    else

	$ERROR init 1 "Unknown filetype $type"
	return 1

    fi
    return 0
}

# $1 = remote dir (from), $2 = local dir (to), $3 = tftp(0) or rcp(1)

dircopy()
{
    mkdir -p $2 || return 1
    rm -f $2/$INDEXFILE >/dev/null 2>&1
    if [ $3 -eq 0 ]; then
	dotftp $1/$INDEXFILE $2/$INDEXFILE ascii 1
    else
	rcp -v $1/$INDEXFILE $2/$INDEXFILE >&$DBGFD 2>&$DBGFD
    fi
    ( test $? -eq 0 && test -s $2/$INDEXFILE) || {
	$DEBUG "dircopy: no index file $1/$INDEXFILE"
	rm -f $2/$INDEXFILE >/dev/null 2>&1  # in case it was empty

	if [ $3 -eq 0 ]; then
	    dotftp $1/$MRCONF $2/$MRCONF ascii 1
	else
	    rcp -v $1/$MRCONF $2/$MRCONF >&$DBGFD 2>&$DBGFD
	fi

	( test $? -eq 0 && test -s $2/$MRCONF) || {
	    $DEBUG "dircopy: no configuration file $1/$MRCONF"
	    rm -f $2/$MRCONF >/dev/null 2>&1  # in case it was empty
	    return 1
	}

	return 0
    }
    ( ret=0;
      while read pathname attrs ; do
        echo "$pathname" | grep '^#' >/dev/null || \
	    filecopy $1/$pathname $2/$pathname $3 "$attrs" || ret=1
      done;
      return $ret ) < $2/$INDEXFILE || return 1

    return 0
}

# Copy the remote scripts ($1) to a local dir ($2),
# $3 is the hardware address, and $4 is the IP address
# ($3 and $4 and not currently used.)

getcustom()
{
    remote="$1"
    local="$2"

    $DEBUG "getcustom $1 $2 $3 $4"

    for src in $remote
    do
	rm -fr $local >/dev/null 2>&1
	dircopy $src $local 0 && {
	    $LOGOK custom "using configuration $src"
	    return 0
        }
    done

    echo "$remote" | grep "^.*@.*:" > /dev/null || \
	remote="guest@$remote"

    for src in $remote
    do
	rm -fr $local >/dev/null 2>&1
	dircopy $src $local 1 && {
	    $LOGOK custom "using configuration $src"
	    return 0
        }
    done

    rm -fr $local >/dev/null 2>&1

    return 1
}

# Preprocess the setenv and if/then/else conditionals
# in the mrconfig file, and generate separate files such
# as /custom/mrconfig.preinst for each phase.

preprocess()
{
    test -f $MRCONFIG && \
    nawk -v p=1 -v top=-1 -v cfg=$MRCONFIG \
	'function unexpected(kwd,line)
	 { printf "ERROR: unexpected \"%s\" keyword at line %d\n", kwd, line; }

	 BEGIN { # the files for these phases are written
		 # raw, without the #!/bin/sh heading and
		 # opening environment section.
	         special["partition"] = 1;
	         special["onerror"] = 1;
	         special["loghost"] = 1;
	         special["version"] = 1;
	         special["nokernel"] = 1;
	         special["inst"] = 1;
		 # for these phases, write a separate side-file
		 # that contains the environment.
	         sepenv["inst"] = 1;
		 # slines[scount] is an array containing all
		 # the setenv commands we have seen thus far.
		 scount =0;
		 # name of temp file for variable expansions
		 srand;
		 envfile = "/tmp/roboenv" rand;
	       }
	{  if (match($1, "^#")) {
		next;
	   } else if ($1 == "if") {
		sub($1,"",$0);
		if ($NF == "then")
		    sub("then[ \t]*$","",$0);
		t = p ? system(envargs " " $0) : 0;
		pstack[++top] = p;
		ifstack[top] = "if";
		tstack[top] = t;
		p = p && (t == 0);
		next;
	   } else if ($1 == "elsif" || $1 == "elif") {
		if (ifstack[top] != "if" && ifstack[top] != "elsif")
		    { unexpected($1, FNR); next; }
		sub($1,"",$0);
		if ($NF == "then")
		    sub("then[ \t]*$","",$0);
		ifstack[top] = "elsif";
		if ((top < 0 || pstack[top]) && tstack[top] != 0)
		    t = system(envargs " " $0);
		else
		    t = 1;
		if (tstack[top] == 0)
		    t = 1;
		else
		    tstack[top] = t;
		p = (top < 0 || pstack[top]) && (t == 0);
		next;
	   } else if ($1 == "else") {
		if (ifstack[top] != "if" && ifstack[top] != "elsif")
		    { unexpected($1, FNR); next; }
		ifstack[top] = "else";
		p = (top < 0 || pstack[top]) && (tstack[top] != 0);
		next;
	   } else if ($1 == "endif" || $1 == "fi") {
		if (ifstack[top] != "if" && ifstack[top] != "elsif" && ifstack[top] != "else")
		    { unexpected($1, FNR); next; }
		p = pstack[top--];
		next;
	   }
	   if (!p) next;
	   if (match($1, "setenv>?") && length($2) > 0) {
	       # expand the envargs string for later use in
	       # this nawk script (when we run condition
	       # subprocesses above), and also save the environment
	       # setting in the file mrconfig.setenv.
	       var = $2;
	       val = $0;
	       sub("^[ \t]*setenv>?[ \t]+" var "[ \t]*", "", val);
	       if (!match(val, "^[A-Za-z0-9_\.\-]*$")) {
		   # do shell expansion
		   syscmd=sprintf(envargs " echo %s > %s", val, envfile);
		   system(syscmd);
		   getline val < envfile;
		   close(envfile);
	       }
	       ENVIRON[var] = val;
	       gsub("\"", "\\\"", val);
	       envargs = envargs var "=\"" val "\" ; export " var " ; ";
	       outfile = cfg ".setenv";
	       setcmd = sprintf ("%s=\"%s\"; export %s", var, val, var);
	       printf "%s\n", setcmd > outfile;
	       slines[scount++] = setcmd;
	   } else if (NF > 0) {
	       # save this line in corresponding output file
	       line = $0;
	       phase = $1;
	       sub(">$","",phase);
	       outfile = cfg "." phase;
	       if (!seenfile[phase]) {
		   seenfile[phase] = 1;
		   if (!special[phase]) {
		       # unless this phase is 'special', write the
		       # interpreter line, and any lines from the
		       # mrconfig.setenv file we have accumulated thus far.
		       printf "#!/bin/sh\n" > outfile
		       for (s=0; s<scount; ++s)
		           printf "%s\n", slines[s] > outfile;
		       system("chmod 755 " outfile);
		   }
		   if (sepenv[phase]) {
		       # some phases needed a separate environment file
		       # so remember the current contents of mrconfig.setenv
		       # as mrconfig.setenv.inst, for example.
		       system("cp " cfg ".setenv " cfg ".setenv." phase " 2>&-");
		   }
	       }
	       sub("^[ \t]*" phase ">?($|[ \t])", "", line);
	       if (special[phase]) {
		   # do envvar substitution for phases that
		   # are not interpreted by /bin/sh
		   while (1) {
		       if (match(line, "^\\$[A-Za-z_]+"))
			   ;
		       else if (match(line, "[^\\\\]\\$[A-Za-z_]+"))
			   { RSTART += 1; RLENGTH -=1 }
		       else
			   break;
		       line = substr(line,1,RSTART-1) \
			      ENVIRON[substr(line, RSTART+1, RLENGTH-1)] \
			      substr(line,RSTART+RLENGTH,length(line));
		   }
	       }
	       if (phase == "partition") {
		   # substitute systemdisk keywords in partition statements
		   w="";
		   if (match(line, "^[ \t]*")) w=substr(line, RSTART, RLENGTH);
		   if (sub("^[ \t]*systemdisk", "", line)) {
		       # if partition was omitted assume 0
		       if (match(line, "^[0-9]|^vh|^vol"))
			   line = w ENVIRON["SGI_SYSTEMDISK"] "s" line;  # dks0d1 + s + line
		       else
			   line = w ENVIRON["SGI_SYSTEMPART"] line;      # dks0d1s0
		   }
	       }
	       printf "%s\n", line > outfile;
	   }
	 }
	 END { system(sprintf("rm %s 2>&-", envfile)) }' $MRCONFIG
}




# Derive the fstab from the partition statements in the mrconfig
# file, and print results on stdout

buildfstab()
{
    nawk ' { if ($3 != "swap" && $4 != "nomount") {
	       dev = match($1,"^/") ? $1 : "/dev/dsk/" $1;
	       type = $3;
	       dir = $4;
	       sub("/.*","",type);
	       if (type == "option" || type == "root")
	           type = "xfs";
	       if (type != "xfs" && type != "nfs" && type != "efs")
		   next;
	       opt = ""
	       # optional mount options are terminated
	       # by optional semicolon.
	       for (i=5; i <= NF && $i != ";"; ++i)
		   if (opt == "")
		       opt = $i;
		   else
		       opt = opt " " $i;
	       if (opt == "") {
	           if (type == "nfs") {
		      opt="ro,soft,bg"
		   } else {
		      rdev=dev;
		      sub("^/dev/dsk", "/dev/rdsk", rdev);
		      opt = "rw,raw=" rdev;
		  }
	       }
	       if (dir != "")
		   printf "%s %s %s %s 0 0\n", dev, dir, type, opt;
	     }
	   } ' $MRCONFIG.partition 2>&-
}



# Run user sh commands in the mrconfig file labeled with
# pattern ($1).  Additional args ($2, $3, ...) are passed
# as positional parameters.

runuser()
{
    phase=$1
    runfile=$MRCONFIG.$phase
    shift
    if [ -x $runfile ]; then
	$runfile $*
	istat=$?
	if [ $istat = 0 ]; then
	    $LOGOK $phase ok
	else
	    $ERROR $phase "$istat" "failed"
	fi
    fi
}

# converts $1 IP address (or netmask) argument to hex format
tohex()
{
    echo $1 | awk -F. '
	/^0x/ && NF == 1 {a=substr($1,3);printf "%s\n",a; exit 0}
	NF == 2 {printf "%02x%06x\n",$1,$2; exit 0}
	NF == 3 {printf "%02x%02x%04x\n",$1,$2,$3; exit 0}
	NF == 4 {printf "%02x%02x%02x%02x\n",$1,$2,$3,$4; exit 0}
	{exit 1}
    '
}

# Initial client discovery, determine hostname, ipaddress,
# and fetch contents of /custom directory.

startcustom()
{
    # Create a file that queues state message to be delivered to
    # loghost after we know it's IP address.
    touch $ROBOINSTLOG

    # Log the fact that we've reached miniroot state.
    $LOGOK miniroot "miniroot booted"

    # determine which interface to use and configure
    # enough networking to use broadcast

    /sbin/ioconfig -f /hw
    cd /dev; ./MAKEDEV MAXPTY=10 MAXGRO=4 MAXGRI=4 tape scsi > /dev/null

    chkconfig -f network on
    interface=`ifname`

    # determine whether/what dhcp mode to do
    configfile=`nvram mrconfig 2>/dev/null`
    $DEBUG "using nvram configfile= $configfile"
    # NOTE: the special character check is in multiple locations!
    case "$configfile" in
	!*)
	    # skip DHCP totally, use nvram netaddr for IP address
	    doproclaim=n
	    tmpaddr=`nvram netaddr`
	    tmpid=0x`tohex $tmpaddr`
	    configfile=`echo $configfile | sed -e 's/^!//'`
	    ;;
	+*)
	    # DHCP mode == assign IP and network parameters
	    doproclaim=y
	    pro_opts="-i"
	    configfile=`echo $configfile | sed -e 's/^+//'`
	    # generate a pseudo random address on the test network
	    eval `/usr/etc/bootpc -t | nawk \
		   ' { srand($2); a=int(rand()*253)+2;
			if (a>254) a=254;
			printf("tmpaddr=192.0.2.%d\n",a);
			printf("tmpid=0xc00002%02x\n",a); }' `
	    ;;
	%*|*)
	    # extended BOOTP 1533 mode == ask for network parameters only
	    # use IP address from nvram netaddr
	    doproclaim=y
	    tmpaddr=`nvram netaddr`
	    tmpid=0x`tohex $tmpaddr`
	    pro_opts="-B"
	    configfile=`echo $configfile | sed -e 's/^%//'`
	    ;;
    esac
    $DEBUG "after cleanup: configfile= $configfile, doproclaim= $doproclaim, pro_opts= $pro_opts"

    if [ "$interface" = "" ]; then
	$ERROR custom 1 "unable to determine network interface"
    else
	$DEBUG "using temp addr $tmpaddr and hostid $tmpid for client discovery"
	$IFCONFIG $interface inet $tmpaddr up netmask 255.255.255.0
	hostid $tmpid
    fi
    $DEBUG "networking for proclaim: `$IFCONFIG -a`"

    # See what parameters we can get from bootp
    # Derive -bbootfile from tapedevice, or use "" for local boot
    # -i0 tells bootp to tell us our ip address
    # -x6 yields about a 12 second timeout

    tmp=` nvram tapedevice 2>/dev/null || echo 0 `
    bootopt=` echo "$tmp" | sed -n 's;.*bootp()\([^ ()]*\).*;-b\1;p' `
    $BPCLT -i0 -x6 $bootopt > $TMPFILE 2>&$DBGFD

    $DEBUG "trace: after call to $BPCLT -i0 -x6 $bootopt"

    if [ -s $TMPFILE ]; then
	bootpnetaddr=`sed -n 's/^yiaddr=//p' $TMPFILE`
	bootpserver=`sed -n 's/^bootpserver=//p' $TMPFILE`
	bootpaddr=`sed -n 's/^bootpaddr=//p' $TMPFILE`
	chaddr=`sed -n 's/^chaddr=//p' $TMPFILE`
    fi
    rm -f $TMPFILE > /dev/null 2>&1


    if test "$doproclaim" = "y" ; then

	# See what configuration parameters DHCP provides
	# dhcp option 18 - extensions file (see rfc for registered numbers)
	# dhcp option 40 - nis domain name (see rfc for registered numbers)
	# dhcp option 3  - router list     (see rfc for registered numbers)
	# dhcp option 15 - dns domain name (see rfc for registered numbers)
	# dhcp option 33 - static routes   (see rfc for registered numbers)
	# dhcp option 1  - subnet mask     (see rfc for registered numbers)

	mkdir -p /etc/config
	cat > $PROCONFIG <<-!!
	    ShutdownOnExpiry: 0
	    DHCPoptionsToGet: 1,3,15,18,33,40
	!!
	$PROCLAIM $pro_opts -q >&$DBGFD 2>&$DBGFD &
	propid=$!
	sleep 20
	kill $propid 2>&-		# proclaim timeout is not reliable

	$DEBUG "trace: after call to $PROCLAIM $pro_opts -q"

	if test -s $PROLEASE ; then

	    # TAKE CARE:  the following expressions contain true TABs.

	    dhcpserver=`sed -n 's/^[ 	]*DHCPserverName:[ 	]*//p' $PROLEASE`
	    dhcpaddr=`sed -n 's/^[ 	]*ServerIPaddress:[ 	]*//p' $PROLEASE`
	    dhcpnetaddr=`sed -n 's/^[ 	]*HostIPaddress:[ 	]*//p' $PROLEASE`
	    hostname=`sed -n 's/^[ 	]*HostName:[ 	]*//p' $PROLEASE`
	    netmask=`sed -n 's/^[ 	]*NetworkMask:[ 	]*//p' $PROLEASE`
	    extfile=`sed -n 's/^[ 	]*ExtensionsPathname:[ 	]*//p' $PROLEASE`
	    domain=`sed -n 's/^[ 	]*DNSdomainName:[ 	]*//p' $PROLEASE`
	    nisdomain=`sed -n 's/^[ 	]*NISdomainName:[ 	]*//p' $PROLEASE`
	    routers=`sed -n 's/^[ 	]*Routers:[ 	]*//p' $PROLEASE`
	    staticroutes=`sed -n 's/^[ 	]*StaticRoutes:[ 	]*//p' $PROLEASE`

	    $DEBUG "proclaim received hostname=$hostname"
	    $DEBUG "proclaim received netaddr=$dhcpnetpaddr netmask=$netmask"
	    $DEBUG "proclaim received extfile=$extfile domain=$domain nisdomain=$nisdomain"
	    $DEBUG "proclaim received routers=$routers staticroutes=$staticroutes"
	fi
    fi

    $DEBUG "trace: after possible proclaim call"
    # remember the current IP address and set the IP address
    # from DHCP or BOOTP.

    oldaddr=`nvram netaddr 2>/dev/null`
    if $MRNETRC validaddr "$dhcpnetaddr" ; then

	$INFO "ip address $dhcpnetaddr (from DHCP server $dhcpserver)"
	netaddr=$dhcpnetaddr
	nvram netaddr $netaddr
	if [ -n "$netmask" ] ; then
	    # if we've got it, use it! (same below)
	    nvram netmask "$netmask" 2>/dev/null
	fi

    elif $MRNETRC validaddr "$bootpnetaddr" ; then

	$INFO "ip address $bootpnetaddr from bootp server $bootpserver"
	netaddr=$bootpnetaddr
	nvram netaddr $netaddr
	if [ -n "$netmask" ] ; then
	    nvram netmask "$netmask" 2>/dev/null
	fi

    else

        $INFO "assuming existing IP address $oldaddr"
	netaddr=$oldaddr

    fi

    # Determine hostname from DHCP if possible, otherwise from
    # /root/etc/sys_id, otherwise pick something.

    $DEBUG "trace: before mount /root"

    $MRMOUNTRC rootonly >&$DBGFD 2>&$DBGFD
    rm $ROBOSTATUS 2>&-		# remove the status script

    if [ "$hostname" ]; then

        fullname="$hostname"
        if [ "$domain" ] ; then
	    fullname="$fullname.$domain"
	fi
	$INFO "hostname $hostname (from DHCP server $dhcpserver)"

    else
	hostname=`nawk '{ if (length($0)) print ; exit }' /root/etc/sys_id 2>&-`
	if [ "$hostname" != "" ]; then
	    $INFO "using most recent hostname $hostname"
	else
	    hostname=$TEMPHOST
	    $INFO "using default hostname $hostname for now"
	fi
    fi

    # Start minimal networking.

    echo "$hostname" > /etc/sys_id
    $MRNETRC startdefaultinterface "" /root
    $DEBUG "mrcustomrc minimal networking: `$IFCONFIG -a`"

    /bin/rm -f $ROUTESFILE
    # setup static and default routes
    if [ "$staticroutes" != "" ] ; then
	# process all static routes
	echo "$staticroutes" | /usr/bin/nawk '{for (i=1; i<NF; i=i+3){
		j=i+1; if ($j != "-") {
			system ("echo static route format botch! \"$0\" 1>&2")
			exit 1;}
		j=i+2;
		print "route add net", $i, $j;
	    }}' > $ROUTESFILE
    fi
    if [ "$routers" != "" ] ; then
	# use only the first pingable router as default gateway
	for r in $routers ; do
	    $DEBUG "Trying gateway $r ..."
	    if /usr/etc/ping -nqQr -c 1 -w 3 $r >&$DBGFD 2>&$DBGFD ; then
		$DEBUG "Setting default route to $r"
		echo route add net default $r >> $ROUTESFILE
		break;
	    fi
	done
    fi

    test -s "$ROUTESFILE" && . $ROUTESFILE

    umount /root >&$DBGFD 2>&$DBGFD

    echo Waiting for routed initialization >&$DBGFD ; sleep 10
    $MRNETRC netisup || $ERROR custom 2 "network configuration problem"

    if [ "$bootpaddr" = "" ]; then
        # if server has no bootptab, it will not respond to any bootp
        # request that has client ip addr 0 (tell me my ip addr).
        # So, try again w/o -i0 flag to at least get server's ip addr
        # for our host file.  This is very useful info.
	$BPCLT -x6 $bootopt > $TMPFILE 2>&$DBGFD
	if [ -s $TMPFILE ]; then
	    bootpserver=`sed -n 's/^bootpserver=//p' $TMPFILE`
	    bootpaddr=`sed -n 's/^bootpaddr=//p' $TMPFILE`
	fi
    fi

    test "$bootpaddr" != "" -a "$bootpserver" != "" && \
	$MRNETRC sethostaddr $bootpserver $bootpaddr
    test "$dhcpaddr" != "" -a "$dhcpserver" != "" -a  \
	"$dhcpserver" != "$bootpserver" && \
	$MRNETRC sethostaddr $dhcpserver $dhcpaddr


    # Determine the location of the client's configuration directory,
    # In the following order of preference:
    #	nvram mrconfig variable
    #	dhcp variable pro_roboinstdir from the vendor-extensions file
    #	default directory on the dhcpserver
    #	default directory on the bootpserver
    #	default directory on a host called "configserver"

    # cleanup nvram mrconfig.
    # NOTE: the special character check is in multiple locations!
    configdir=`nvram mrconfig 2>/dev/null | sed -e 's/^[%!+]//'`
    $DEBUG "trace: before Find Configuration Directory: configdir= $configdir"

    if [ "$configdir" = "" -a "$extfile" != "" ]; then
	(echo "$extfile" | grep "..*:" >/dev/null ) || {
	    # fallback on dhcpserver
	    extfile="$dhcpserver:$extfile"
	}
	rm -f $TMPFILE >/dev/null 2>&1
	tftp <<- EOF >/dev/null 2>&1
		get $extfile $TMPFILE
		quit
	EOF
	if [ -s $TMPFILE ]; then

	    # dhcp_bootp config file format
	    configdir=`grep '^[ \t]*'$PRO_CONFIGDIR'[ \t]*:[ \t]*' $TMPFILE | \
			sed 's/^[ 	]*'$PRO_CONFIGDIR'[ 	]*:[ 	]*//'`
	    test "$configdir" \
		&& (echo "$configdir" | grep -v "..*:" > /dev/null) \
		&& configdir="$dhcpserver:$configdir"

	else

	    $WARNING "unable to find DHCP extensions file $extfile"

	fi
	rm -f $TMPFILE >/dev/null 2>&1
    fi
    if [ "$configdir" = "" ]; then
	if [ "$dhcppserver" ]; then
	    configdir="$dhcpserver:$DEFCUSTOM"
	elif [ "$bootpserver" ]; then
	    configdir="$bootpserver:$DEFCUSTOM"
	else
	    configdir="$DEFSERVER:$DEFCUSTOM"
	fi
    fi

    # Copy the client configuration directory onto the miniroot

    $DEBUG "trace: after Find Configuration Directory: configdir= $configdir"
    getcustom "$configdir" $CUSTOM "$chaddr" "$netaddr"

    if [ ! -d $CUSTOM ]; then
	$ERROR custom 3 "unable to retrieve configuration from $configdir"
	return 1
    fi

    # Compute the values of the SGI_ roboinst variables once,
    # and store them in a file for use during the remainder
    # of the miniroot session.
    build_exports
    test -s $MREXPORTS && . $MREXPORTS

    # pre-process the mrconfig file to create a series
    # of mrconfig.<phase> files that are access later in
    # the miniroot session.
    preprocess

    # Verify that we understand this version of mrconfig
    mrversion=`nawk '{if ($1 != "") { print $1; exit }}' $MRCONFIG.version 2>&-`
    if [ "$mrversion" = "" ]; then
        mrversion=$DEFVERS
    fi

    $DEBUG "mrversion= $mrversion"

    if [ "$mrversion" -gt $MAXVERS ]; then
        rm -rf $CUSTOM 2>&-
	$ERROR custom 4 "mrconfig with invalid version $mrversion.  This miniroot only understands mrconfig versions up through $MAXVERS."
	return 1
    fi

    # Start remote logging if requested

    loghosts=`cat $MRCONFIG.loghost 2>&-`

    # filter out attempts to remote log to this host

    thisipaddr="$netaddr"
    loghost=""
    for host in $loghosts ; do
	if [ "$host" = "localhost" ]; then
	    ipaddr=$thisipaddr
	else
	    ipaddr=`$MRNETRC gethostaddr $host`
	    if [ $? != 0 ]; then
		ipaddr=$host
	    fi
	fi
        if [ "$ipaddr" != "$thisipaddr" ]; then
	    loghost="$loghost $host"
	fi
    done

    if [ "$loghost" ]; then
	$INFO "loghost $loghost"
	echo "$loghost" > $MRCONFIG.loghost 2>&$DBGFD
	$MRLOGRC addhost $loghost
	if [ -f $ROBOINSTLOG ]; then
	    # Send any queued messages only to remote hosts
	    # since they are already in local syslog
	    $MRLOGRC localoff
	    $LOGGER -f $ROBOINSTLOG
	    $MRLOGRC localon
	    rm -f $ROBOINSTLOG
	fi
    fi

    # We are done with the initial processing.
    # Run user init commands immediately.
    runuser init
}



#-------------- main procedures -----------------------


cmd="$1"
shift 2>/dev/null
test -s $MREXPORTS && . $MREXPORTS
case "$cmd" in

start)
    startcustom
    ;;

stop)
    $LOGOK stop "restarting system"
    
    # Write the .roboinst_status script that sends "reboot complete"
    # message to loghost after client reboots.

    loghost=`cat $MRCONFIG.loghost 2>&-`
	cat 2>&- >$ROBOSTATUS <<EOF
#! /bin/sh
#Tag 0x00000f00

# /var/inst/.roboinst_status
# created by /etc/mrcustomrc

LOGHOST="$loghost"

cp /etc/syslog.conf /etc/syslog.conf.new.\$\$ 2>&-

    for HOST in \$LOGHOST ; do
        echo '*.debug;kern,syslog.none	@'\$HOST >> /etc/syslog.conf.new.\$\$
    done

cp /etc/syslog.conf /etc/syslog.conf.old.\$\$
mv /etc/syslog.conf.new.\$\$ /etc/syslog.conf
killall -HUP syslogd
logger -t roboinst "state=reboot status=0 reboot completed"
mv /etc/syslog.conf.old.\$\$ /etc/syslog.conf
killall -HUP syslogd
EOF
	chmod +x $ROBOSTATUS 2>&-

	cat 2>&- >$ROBOREBOOT <<EOF 
#! /sbin/sh
#Tag 0x00000f00

# On startup, execute any roboinst operations pending in the
# file /var/inst/.roboinst_status.  This will send a summary
# message (either system rebooted, or system never reached miniroot)
# to the remote syslog.

set `who -r`
new_runlevel=\$3

if [ "\$new_runlevel" = "2" ] ; then
    if [ -x /var/inst/.roboinst_status ]; then
	/var/inst/.roboinst_status
	mv /var/inst/.roboinst_status /var/inst/.roboinst_status.O 2>&-
    fi
fi
EOF
    chmod +x $ROBOREBOOT 2>&-

    sleep 2 # wait for msg before shutting down
    ;;

run)
    runuser $*
    ;;

mounthook)
    # Before the disks are mounted, update the fstab on
    # the root drive according to mrconfig.
    buildfstab > /tmp/mrfs$$
    if [ -s /tmp/mrfs$$ ]; then
	cat /tmp/mrfs$$ | dbgmsg
	mkdir -p /root/etc
        cp /tmp/mrfs$$ /root/etc/fstab
    fi
    rm -f /tmp/mrfs$$ 2>/dev/null
    ;;

fx)
    # Execute any scripted fx commands before mounting any disks.
    # If an fx script has been supplied, use that instead.

    # Perform any partitioning
    test -s $MRCONFIG.partition && \
	nawk '{if ($3 != "nfs") print}' $MRCONFIG.partition > $TMPFILE 2>&-
    if [ -s $TMPFILE ]; then
	    (echo  "fx -s" ; cat $TMPFILE) | dbgmsg
	    fx -x -s $TMPFILE
	    $LOGOK autofx ok	# fx -s does not report status
    fi
    rm -f $TMPFILE >/dev/null 2>&1

    # Run any user-requested fx operations
    runuser fx || fxstat=$?
    ;;

mkfs)
    # Make any filesystems requested with partition keywords
    # Input lines are of the form:
    #   partition size type mountpoint mount-opt ... ; mkfs-opt ... ;
    # Semicolons are only required to separate option types, or to
    # specify mkfs options when there are no mount options.
    # The mount options begin at word 5, up to but not including the
    # the next semicolon (if present).  The mkfs options follow, up to
    # but not including the next semicolon (if present).

    nawk ' BEGIN {
	       types[0] = "efs";
	       types[1] = "xfs";
	       types[2] = "root";
	       types[3] = "option";
	       ntypes = 4;
	   }

	   { blksz = "";
	     device = $1;
	     for (i=0; i < ntypes; ++i) {
		 s = $3;
		 if (s == types[i])
		    break;
		 else if (sub("^" types[i] "/", "", s)) {
		    blksz = s
		    break;
		 }
	      }
	      if (i < ntypes) {
		  if (types[i] == "efs")
		      blksz = "512";
		  else if (blksz == "")
		      blksz = "4096";
		  type = types[i];
		  if (type != "efs")
		      type = "xfs";
		  # skip mount options
		  j = 4
		  while (++j <= NF && $j != ";")
		      ;
		  # remember mkfs options
		  opt = ""
		  while (++j <= NF && $j != ";")
		      opt = opt " " $j
		  printf "/dev/dsk/%s %s %s %s\n", device, type, blksz, opt
	      }
	    }' $MRCONFIG.partition > $TMPFILE 2>&-

    if [ -s $TMPFILE ]; then
	    mkstat=0
	    while read dev fstype bsize mkfsopts; do
		    $DEBUG "mrmkfsrc -a $dev $fstype $bsize $mkfsopts"
		    mrmkfsrc -a $dev $fstype $bsize $mkfsopts || mkstat=$?
	    done < $TMPFILE
	    if [ $mkstat = 0 ]; then
		$LOGOK automkfs ok
	    else
		$ERROR automkfs $mkstat "error occurred during filesystem creation"
	    fi
    fi
    rm -f $TMPFILE >/dev/null 2>&1

    # Run any user-requested fx operations
    runuser mkfs
    ;;

preinst)		# Run the pre-inst phase
    # try to start standard networking if we can find the "live"
    # configuration.  Try to revert to what we had if we get any errors
    # This is as late as possible, while still providing a chance to
    # override networking setup in the mrconfig file before installing

    $MRNETRC safestart
    test -s "$ROUTESFILE" && . $ROUTESFILE

    runuser preinst $*
    ;;

inst)			# Run the inst phase
    # Pass the appropriate value of abort_cmdfile_on_error depending
    # on the onerror keyword, so that inst goes interactive in
    # case of conflicts or other errors.
    onerror=`nawk '{ if ($1 != "") { print $1; exit} }' $MRCONFIG.onerror 2>&-`

    if [ "$onerror" = wait ]; then
        # inst will stop and give a prompt on error
        errflag="-Vabort_cmdfile_on_error:on"
    else
        # inst will continue despite any errors
	errflag="-Vabort_cmdfile_on_error:off"
    fi

    if [ -s $MRCONFIG.inst ]; then
	echo quit >> $MRCONFIG.inst
	# execute inst, using the mrconfig.setenv.inst file
	# that was determined during preprocessing
	( test -s $MRCONFIG.setenv.inst && . $MRCONFIG.setenv.inst;
	    inst -c $MRCONFIG.inst -Vinteractive:off $errflag -Vsyslog:on $* )
	if [ $? = 0 ]; then
	    $LOGOK inst ok
	else
	    $ERROR inst $? "Error occurred during software installation"
	fi
    fi
    rm -f $TMPFILE 2>/dev/null
    ;;

postinst)		# Run the post-inst phase
    runuser postinst $*
    ;;

autoconfig)		# Run autoconfig and report status, unless
			# nokernel keyword is present

    noauto=0
    test -f $MRCONFIG.nokernel && noauto=1

    if [ "$noauto" = 1 ]; then
        $LOGOK kernel "skipping kernel build"
	autostatus=0
    else
	mrconfigrc
	autostatus=$?
	if [ "$autostatus" = 0 ]; then
	    $LOGOK kernel "kernel is configured"
	else
	    $ERROR kernel 1 "failed configuring kernel"
	fi
    fi
    exit $autostatus
    ;;

*)
    echo "Usage: $0 {start|run phase|configure|etc...}"
    ;;

esac
