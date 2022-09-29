#!/bin/perl
#
# $Modified: Mon Dec  8 15:23:32 1997 by cwilson $
#
# kwho, ala perl
#
# Usage: kwho [ -noyp ] [ -d ] [ -c class ] [-n] [ -m mapname ] [ machine... ] [ @connector ]
#
#        -noyp		don't look for ktools nis map, use 
#			$PORTSFILE for data
#	 -d <level>	debug level
#	 -c <class>	supply class
#	 -n		don't lookup ip addresses
#	 -l		show long information; cpu type, gfx, hw, memory, desc
#	 -a	        show all possible information and fully expand all fields.
#	 -m		NIS map to use.
#	machine		machine to query.
#       @connector      connector (terminal server) to query directly.
#
# non obvious Globals:
#
#	%k_connector:	connector information by connector name.
#			value is nis packed format.
#       %k_object	object information.  value is nis packed format.
#
#	%k_who		who information.  key is object, value is ` packed format
#			user`when`idle`from
#
# 21st century, here we come.

require 5;

$REVISION	= '$Revision: 1.2 $'; #'";" <-- this fixes indenting and highlighting
# extract just the revision # from the string.
#
$REVISION =~ s/.*(\d\.\d+).*/$1/;

$KTOOLS_HOME	= '/usr/annex';	                          # here lie dragons
$KWHO		= "$KTOOLS_HOME/kwho";			  # don't rely on PATH
$PORTSFILE 	= "$KTOOLS_HOME/ktools.map";		  # when NIS isn't here.

$| = 1; $= = 9999999;		# dammit.

# number of seconds around finger calls to connectors
#
$DELAY = 2;
$FINGER_DELAY = 6;
$FINGER_PORT = 79;		# standard service, no need to do a getservbyname.

# we can now require this, since the ktools are an inst image
# that has pre-reqs on eoe2.sw.gifts_perl or perl.sw.perl
require 'newgetopt.pl';

use Socket;

&NGetOpt( "c:s", "debug", "d:i", "noyp", 'm=s', 'n', 'a', 'l' );

# NIS map to look at.  here so debugging is easier with a test map.
#
$NIS_MAP = $opt_m || 'ktools';


$opt_debug += $opt_d;

# if running w/o nis,  do a versions check to make sure that we can
# still parse the data correctly.
#
&checkversion;

$SIG{'ALRM'} = 'hander';

chop($hostname = `hostname`);

# Check the machines here
# 
if (@ARGV) {
 
    foreach $machine (@ARGV) {

	if ( $machine =~ s/^@//) {
	    $O_connector = $machine;
	    shift @ARGV;	# get rid of it so that the write loop down below
				# works.
	} else {
	    if (! &k_object_info($machine)) {
		warn "Couldn't find configuration info for machine $machine, skipping.\n";
		next;
	    }
	}

	if (! &k_connector_info($O_connector)) {
	    warn "Couldn't find configuration info for terminal server $O_connector $_, skipping\n";
	    next;
	}

	&k_users($O_connector, $C_type);
    }
} else {

    # If classes are given...
    #
    if ($opt_c) {		
	@classes = split(/:/, $opt_c);
    } else {
	@classes = split(/:/, $ENV{KTOOLS_CLASS});
    }

    print STDERR "Classes are @classes\n" if $opt_debug;

    if ($#classes > -1  ) {
	foreach $class (sort @classes) {
	    &k_class_info($class);
	    
	    print STDERR "Connectors for class $class are " . join (" ", @Class_connectors) . "\n"
		if $opt_debug;

	    foreach $connector (@Class_connectors) {
		if (! &k_connector_info($connector)) {
		    warn "Couldn't find configuration info for terminal server $connector $_, skipping\n";
		    next;
		}
		
		# XXX Hmm, could query the same connector multiple times if it's in multiple classes...

		&k_users($connector, $C_type);
	    }
	}
    } else {
	# no classes specified, found, or whatnot, do all the connectors.
	#
	# handle netgroup-style things here.
	#
	if ( ($temp = &k_info_lookup('annexes')) =~ /\(/) {
	    foreach (sort split(/\s/, $temp)) {
		$temp2 .= &k_info_lookup($_) . '`';
	    }
	    chop $temp2;
	    $temp = $temp2;
	}
	@connectors = split('`', $temp);

	
	print "Connectors are @connectors\n" if $opt_debug;

	foreach $connector (@connectors) {
	    if (! &k_connector_info($connector)) {
		print STDERR "Couldn't find configuration info for terminal server $connector $_, \n\tadding .sgi.com and trying again.\n"
		    if $opt_debug;
		# XXX Hack around the hack of PQDNs for the annexes key.
		#
		$connector .= '.sgi.com' if ( ($connector =~ tr/\./\./) == 1);

		if (! &k_connector_info($connector)) {
		    print STDERR "Couldn't find configuration info for terminal server $connector $_, \n\tadding .sgi.com and trying again.\n"
			if $opt_debug;
		    next;
		}
	    }
	    
	    # XXX Hmm, could query the same connector multiple times if it's in multiple classes...
	    
	    &k_users($connector, $C_type);
	}
    }
}

	
# print out the activity.
#
if ($opt_l) {
    $^ = 'LONG_TOP';
    $~ = 'LONG';
} elsif ($opt_a) {
    $^ = 'ALL_TOP';
    $~ = 'ALL';
} else {
    $^ = 'NORMAL_TOP';
    $~ = 'NORMAL';
}

foreach $machine ( (@ARGV ? @ARGV : sort keys %k_who)) {
    $machine .= '.con.0' if !$k_who{$machine};

    next if !$k_who{$machine};
# if printing out all the information, get the machine specific information.
#
    if ($opt_a || $opt_l) {
	&k_object_info( $machine =~ /^(\w+)/ );
    }

    ($user, $when, $idle, $from) = split('`', $k_who{$machine});
    $from =~ s/\.sgi\.com//;
    write;
}

	#                           'asset' => '',
	#                           'serial' => '',
	#                           'name' => 'gotham',
	#                           'reset_method' => '',
	#                           'gfx' => '',
#?                           'consoles' => '16',
#?                           'email' => 'aje',
#                           'notes' => '',
	#                           'cputype' => '',
	#                           'connector' => 'ibmcomms-annex.engr.sgi.com',
	#                           'owner' => 'aje',
	#                           'chassis' => '',
#?                           'mktgtype' => '',
#?                           'ports' => '1',
	#                           'memory' => '',
#?                           'debugs' => '',
	#                           'loc' => '9L Comms lab',
#                           'hw' => '',
	#                           'desc' => 'Indigo'


format ALL_TOP=
Mach Name          Connected to        User        Connect Time  Idle Time      CPU  Memory Gfx                              Description                     
 Owner            Location           Connector           Asset#    Serial#      Reset Method     Chassis       Misc Hardware
 Notes
--------------------------------------------------------------------------      --------------------------------------------------------------------------
.

format ALL=
@<<<<<<<<<<<<<<<<<<@<<<<<<<<<<<<<<<<<@<<<<<<<<<<<<<@<<<<<<<<<<<<<@<<<<<<<<       @<<< @<<<<< @<<<<<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 
$machine, $from, $user, $when, $idle,$O_mach_type, $O_mem, $O_gfx,$O_desc, 
  @<<<<<<<<<<<<< @<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<< @<<<<<<< @<<<<<<<       @<<<<<<<<<<<<<< @<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$O_owner, $O_loc, $O_connector, $O_asset, $O_serial,$O_reset, $O_chassis, $O_special
~~^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$O_notes
.

format LONG_TOP=
Mach Name          Connected to        User        Connect Time  Idle Time
   CPU  Memory Gfx                              Description                         
--------------------------------------------------------------------------
.

format LONG=
@<<<<<<<<<<<<<<<<<<@<<<<<<<<<<<<<<<<<@<<<<<<<<<<<<<@<<<<<<<<<<<<<@<<<<<<<<
$machine, $from, $user, $when, $idle
  @<<< @<<<<< @<<<<<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 
$O_mach_type, $O_mem, $O_gfx,$O_desc, 
.

format NORMAL_TOP=
Machine Name         Connected to            User          Connect Time   Idle
------------         ----------------------  ----------    ------------  ------
.

format NORMAL=
@<<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<<  @<<<<<<<<<<<<<@<<<<<<<<<<<<@>>>>>>
$machine, $from, $user, $when, $idle
.

#------------------------------------------------------------
# checkversion.  If there's a major (>1) change, the data format
# has changed and we can't run at all.
#
sub checkversion {
    $nis_version = &k_info_lookup("kwho_version");
    if (!$nis_version) {
	warn "\n$0: unable to find kwho_version in ktools NIS map.\n";
    } else {
	  if ($nis_version > $REVISION) {
	      if (($nis_version - $REVISION) > 1) {
		  printf(stderr "\nNOTICE: A newer version of kwho is currently being used.  The data format used\n");
		  printf(stderr "        for the new version of kwho has made this version incompatible.  Please\n");
		  printf(stderr "        update the ktools software to restore functionality.\n");
		  printf(stderr "        Images can be found at dist.engr:/sgi/hacks/ktools.\n");
		  exit (1);
	      } else {
		  printf(stderr "\n NOTICE: A newer version of kwho is available.  Please upgrade at your\n");
		  printf(stderr    "        earliest convenience for added functionality and bug fixes.\n");
		  printf(stderr "        Images can be found at dist.engr:/sgi/hacks/ktools.\n");
	      }
	  }
      }
}

#----------------------------------------------------------------------
# checkclasses:  verify that there's a classes map to check against, and that
# the KTOOLS_CLASS envariable has valid values.
#
sub checkclasses {
    chop ($x = &k_info_lookup('classes'));
    if (!length($x)) {
	die "No classes found in ktools NIS map.\n";
    }
    
    foreach $class (split ('`', $x)) {
	$Class{$class} = &k_info_lookup($class . '_config');
    }

    if ($ENV{KTOOLS_CLASS}) {
	foreach (split(':', $ENV{KTOOLS_CLASS})) {
	    if (!$Class{$_}) {
		print "ERROR: Unknown class: $ENV{KTOOLS_CLASS}.   Valid classes are:\n";
		&PrintClasses;	 
		exit 1;		
	    }
	}
    }
}

#----------------------------------------------------------------------
# k_info_lookup:
# retrieve information about the desired thing, either from the nis map
# or the $PORTSFILE.
#
sub k_info_lookup {

    local($info) = $opt_noyp 
	? `grep "^$_[0]" $PORTSFILE` 
	: `ypmatch -k "$_[0]" $NIS_MAP 2>&1`;

    chomp $info;

    if ($opt_noyp) {
	return undef if ($info !~ /\Q$_[0]\E/);
	$info =~ s/([^ ]*)\Q$_[0]\E\s*(.*)/$2/;
    } else {
	return undef if ($info !~ /\Q$_[0]\E:/);
	$info =~ s/(.*)\Q$_[0]\E: (.*)/$2/;
    }

    $info;
}

#----------------------------------------------------------------------
# ypcat:
# do the ypcat thing
#
sub ypcat {
    local(@foo) = $opt_noyp ? `cat $PORTSFILE` : `ypcat -k $NIS_MAP 2>&1`;

    @foo;
}

#----------------------------------------------------------------------
# Fills in the connector info.
#
sub k_connector_info {

    $k_connector{$_[0]} = &k_info_lookup($_[0] .  "_config") 
	if !defined($k_connector{$_[0]});
    
    ($C_ports, $C_boot, $C_loc, $C_owner, $C_email, $C_type)
	= split('`', $k_connector{$connector}); 

    $k_connector{$_[0]};
}

#----------------------------------------------------------------------
# fills in the class info.
#
sub k_class_info {
    $k_class{$_[0]} = &k_info_lookup($_[0] . '_config')
	if !defined($k_class{$_[0]});

    print STDERR "class $_[0] retrieve: $k_class{$_[0]}\n" if $opt_debug;

    ($Class_title, $Class_owner, $Class_email, $Class_extension, 
     $x) = split('`', $k_class{$_[0]});
    @Class_connectors = split(',', $x);

    print STDERR "\tClass title: $Class_title\n\tOwner $Class_owner\n\t@Class_connectors\n" 
	if $opt_debug;
}
#----------------------------------------------------------------------
# fills in the object info.
#
sub k_object_info {
    $k_object{$_[0]} = &k_info_lookup($_[0])
	if !defined($k_object{$_[0]});
    
    ($O_desc, $O_mach_type, $O_mktg_mach_type, $O_chassis, $O_gfx, $O_mem,
     $O_special, $O_loc, $O_asset, $O_serial, $O_notes, $O_owner,
     $O_owner_uname, $O_owner_ext, $O_ports, $O_connector, $O_reset,
     $O_consoles, $O_dbg_consoles, $O_access) 
	= split('`', $k_object{$_[0]});

    @O_consoles = split(',', $O_consoles);
    @O_dbg_consoles = split(',', $O_dbg_consoles);
    grep(s/\s//g, @O_consoles);
    grep(s/\s//g, @O_dbg_consoles);

    $O_notes =~ s/^\s+//;
    $O_full_owner = '';
    $O_full_owner = "$O_owner ($O_owner_uname), $O_owner_ext" if 
	( length($O_owner) && length($O_owner_uname) && length($O_owner_ext));

    $k_object{$_[0]};
}

#----------------------------------------------------------------------
# k_users: query a terminal server.  
# in:  terminal server name, type
# out: array of something.
#
sub k_users {

    local($connector, $type) = @_;

    # XXX special case connector types here.  
    if ($type =~ /cisco/i) {
	# do cisco connector type stuff here
    } else {

    # default is an annex connector.  Annex boxes will report the activity 
    # by a 'finger' in the following format:

#  Port What User             Location          When         Idle  Address
#   1   PSVR mdf              agony.con.0      Fri  6:41pm   4:15  192.132.191.43
#   2   PSVR troot            agony.con.1      Mon 11:01am   4:22  192.132.191.43
#   7   PSVR celeste          blitzen.con.0    Jul  9 16:47   :01  192.132.191.9
#
	# we build up the finger connection manually so that we can
	# timeout around things gracefully.
	#
	@stuff = &do_finger_connection($connector);

	# now parse the output
	#
	foreach (@stuff) {
	    next unless (($blah, $port, $user, $object, $rest) = 
# annex 13.3 o/s changes the finger output.
			 /(\s+|asy)(\d+)\s+PSVR\s+(\S+)\s+(\S+)\s+(.*)$/);
#			 /^\s+(\d+)\s+PSVR\s+(\w+)\s+(\S+)\s+(.*)$/);
	    $object .= $unknown_count++ if $object =~ /unknown/;
	    #
	    # Connect time can be over 24 hours, in which case the day of week is
	    # prepended, or over 7 days, in which case the month and day are also prepended.
	    #
	    if ($rest =~ s/^(\d+:\d+..)//) {
		($when) = $1;
	    } elsif ( $rest =~ s/^((Mon|Tue|Wed|Thu|Fri|Sat|Sun)\s+\d+:\d+..)// ) {
		($when) = $1;
	    } elsif ( $rest =~ s/^(\w+\s+\d+\s+\d+:\d+)// ) {
		($when) = $1;
	    } elsif ( $rest =~ s/^---// ) {
		$when = 'unknown';
	    }
	    # 
	    # idle time is simply HHH:MM
	    #
	    ($idle) = $1 if $rest =~ s/(\d*:\d+)//;
	    
	    ($from) = $1 if $rest =~ s/(\d+\.\d+\.\d+\.\d+)//;
	    
	    $rest =~ s/\s//g;


	    # in case multiple same things on multiple ports.
	    $object .= '.0' if $k_who{$object};
	    $object++ until !$k_who{$object};
	    
	    if (!$opt_n) {
		$from = ((gethostbyaddr( pack('C4', split(/\./, $from)), AF_INET))[0])[0];
	    }

	    print "$object\t$user\t$when\t$idle\t$from\n" if $opt_debug;
	    print "LEFTOVER: '$rest'\n" if $opt_debug && length($rest);

	    $k_who{$object} = join('`', $user, $when, $idle, $from);

	}	    
    }
}
	
#----------------------------------------------------------------------
# do_finger_connection.
# This sets up the finger socket with timeouts so that down connectors
# don't waste lots of time before they timeout.
#
sub do_finger_connection {
    local($connector) = $_[0];
    local(@data);

    # add a '.' at the end of the connector hostname so that we 
    # don't meander down a 'search x.y.z.com' in resolv.conf.  All
    # the connectors should be FQDNs.
    #
    $connector .= '.' if (split(/\./, $connector) >=4);

    warn "gethostbyname($connector)\n" if $opt_debug > 2;

    if (! ($connector_ipaddr = ((gethostbyname($connector))[4])[0])) {
	warn("Couldn't get IP address of $connector, $?")
	    if $opt_debug;
	return undef;
    };

    
    $sockaddr = 'S n a4 x8';

    $proto = (getprotobyname('tcp'))[2];

    $this = pack($sockaddr, AF_INET, 0, (gethostbyname($hostname))[4]);
    $that = pack($sockaddr, AF_INET, $FINGER_PORT, $connector_ipaddr);

    print STDERR "socket.." if $opt_debug > 2;

    socket(S, PF_INET, SOCK_STREAM, $proto) 
 	|| do {
 	    warn("couldn't do a socket, $!");
 	    return undef;
 	};

    print STDERR "okay.\nbind... " if $opt_debug > 2;

    bind(S, $this) 
 	|| do {
 	    warn("Couldn't bind, $!");
 	    return undef;
 	};
    
    alarm($DELAY);
 
    warn "okay.\nconnect..." if $opt_debug > 2;

    connect(S, $that)
 	|| do {
 	    print STDERR "Couldn't connect to $connector, $!" if $opt_debug;
 	    return undef;
 	};

    warn "okay.\nread..." if $opt_debug > 2;

    select(S); $| = 1; select(STDOUT);

    # the 'who' from the annex shouldn't take more than 6 seconds */
    alarm($FINGER_DELAY);

    print S "\n";

    while (<S>) {
	push (@data, $_); 
    }

    warn "data:\n@data\n" if $opt_debug > 2;

    alarm(0);

    @data;
}


