#!/bin/sh --
eval 'exec /usr/bin/perl -S -x $0 ${1+"$@"}'
if 0;

#!/usr/bin/perl
#
# script used to modify the crontab file for the diag user.  It only
# understands how to deal with "cached" and "onlinediag" commands.
#

$DIAG_CRONTAB = "/var/spool/cron/crontabs/diag";
#$DIAG_CRONTAB = "diag"; #used to make original diag file
#$DIAG_CRONTAB = "diag.off"; #used to make original diag.off file

$USAGE = "USAGE: $0 -x \"onlinediag|cached <options>\" \\ \n\t\t\t[-h |-d hour|-w weekday|-m monthday|-n] \\ \n\t\t\t[-t hour]\n\t-x onlinediag|cached followed by options to pass to them\n\t-h run hourly on the hour (default for cached)\n\t-d run daily on given hour (default for onlinediag)\n\t-w run weekly on given weekday\n\t-m run monthly on given monthday\n\t-n never run -- turn off the diag\n\t-t hour of day to run (for the -w or -m options;\n\t\tdefaults to midnight)\n";


# initialization
$min = "0";
$hour = "0";
$daymo = "*";
$month = "*";
$daywk = "*";

# parse command line
$ret = &getopts("x:hd:w:m:nt:");

if(defined $opt_x) {
    $executable = $opt_x;
    if ($executable =~ "cached") {
	$hour = "*";
    }
} else {
    $err = "-x option must be specified";
}
if (defined $opt_h) {
    if (defined $opt_d || defined $opt_w || 
	defined $opt_m || defined $opt_n) {
	$err = "-h mixed with incompatable option";
    }
    $hour = "*";
}
if (defined $opt_d) {
    if (defined $opt_w || defined $opt_m || defined $opt_n) {
	$err = "-d mixed with incompatable option";
    }
    $hour = $opt_d;
}
if (defined $opt_w) {
    if (defined $opt_m || defined $opt_n) {
	$err = "-w mixed with incompatable option";
    } elsif ($opt_w < 0 || $opt_w > 6) {
	$err = "-w value out of range";
    }
    $daywk = $opt_w;
}
if (defined $opt_m) {
    if (defined $opt_n) {
	$err = "-m mixed with incompatable option";
    } elsif ($opt_m > 31 || $opt_m < 1) {
	$err = "-m value out of range";
    }
    $daymo = $opt_m;
}
if (defined $opt_t) {
    if(defined $opt_h || defined $opt_d) {
	$err = "-t options not compatable with -h or -d options";
    } elsif ($opt_t < 0 || $opt_t > 23) {
	$err = "-t value out of range";
    }
    $hour = $opt_t;
}
if (defined $err || !($ret)) {
    if (defined $err) {
	print "ERROR: $err\n";
    }
    print "$USAGE";
    exit 1;
}



$HEADER = "#\n# The diag crontab is used to run diagnostic tests during run time\n#\n# Format of lines:\n#min\thour\tdaymo\tmonth\tdaywk\tcmd\n#\n# DO NOT MODIFY THIS FILE!  Please use the 'diagsetup' script to control\n#\tthe contents of this file.  All modifications to this file will\n#\tbe wiped out whenever 'diagsetup' is run.\n#\n";


# build up command replacement and fetch other command from file
$format_str = "${min}\t${hour}\t${daymo}\t${month}\t${daywk}\t";
open(CRONFILE, "$DIAG_CRONTAB");

# cached is changing 
if ($executable =~ "cached") {
    if (defined $opt_n) {
	$cached_command = "# turned off\n";
   } else {
       $cached_command = "${format_str}if test -x /usr/diags/bin/cached; then /sbin/su root -C CAP_SCHED_MGT,CAP_DEVICE_MGT+ip -c \"/usr/diags/bin/${executable}\" 2>& /dev/null; fi\n";
   }
    while (<CRONFILE>) {
	if ($_ =~ "# ONLINEDIAG") {
	    $onlinediag_command = <CRONFILE>;
	    last;
	}
    }

# onlinediag is changing
} elsif ($executable =~ "onlinediag") {
    if (defined $opt_n) {
	$onlinediag_command = "# turned off\n";
    } else {
	$onlinediag_command = "${format_str}if test -x /usr/diags/bin/onlinediag; then /sbin/su root -C CAP_SCHED_MGT,CAP_NVRAM_MGT+ip -c \"/usr/diags/bin/${executable}\" 2>& /dev/null; fi\n";
    }
    while (<CRONFILE>) {
	if ($_ =~ "# CACHED") {
	    $cached_command = <CRONFILE>;
	    last;
	}
    }
} else {
    print "ERROR: -x must be followed by either \"onlinediag <options>\" or \"cached <options>\"\n";
    print "$USAGE";
    close(CRONFILE);
    exit 1;
}
close(CRONFILE);


# write out commands to diag crontab
open(CRONFILE, ">$DIAG_CRONTAB");
print CRONFILE "$HEADER";
print CRONFILE "# CACHED\n";
print CRONFILE "$cached_command";
print CRONFILE "# ONLINEDIAG\n";
print CRONFILE "$onlinediag_command";
close(CRONFILE);


# set a SIGHUP to cron so it will get this update
system("/sbin/killall -HUP cron");

exit 0;

;# stolen from the SPEC95/bin/lib/getopts.pl
;# Usage:
;#      do getopts('a:bc');  # -a takes arg. -b & -c not. Sets opt_* as a
;#                           #  side effect.

sub getopts {
    local($argumentative) = @_;
    local(@args,$_,$first,$rest);
    local($errs) = 0;
    local($[) = 0;

    @args = split( / */, $argumentative );
    while(@ARGV && ($_ = $ARGV[0]) =~ /^-(.)(.*)/) {
	($first,$rest) = ($1,$2);
	$pos = index($argumentative,$first);
	if($pos >= $[) {
	    if($args[$pos+1] eq ':') {
		shift(@ARGV);
		if($rest eq '') {
		    ++$errs unless @ARGV;
		    $rest = shift(@ARGV);
		}
		eval "\$opt_$first = \$rest;";
	    }
	    else {
		eval "\$opt_$first = 1";
		if($rest eq '') {
		    shift(@ARGV);
		}
		else {
		    $ARGV[0] = "-$rest";
		}
	    }
	}
	else {
	    print STDERR "Unknown option: $first\n";
	    ++$errs;
	    if($rest ne '') {
		$ARGV[0] = "-$rest";
	    }
	    else {
		shift(@ARGV);
	    }
	}
    }
    $errs == 0;
}

1;
