#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	@INC = '../lib' if -d '../lib';
    }
}

use Config;

BEGIN {
    if(-d "lib" && -f "TEST") {
        if ( ($Config{'extensions'} !~ /\bSocket\b/ ||
              $Config{'extensions'} !~ /\bIO\b/	||
	      $^O eq 'os2')    &&
              !(($^O eq 'VMS') && $Config{d_socket})) {
	    print "1..0\n";
	    exit 0;
        }
    }
}

$| = 1;
print "1..3\n";

use Socket;
use IO::Socket qw(AF_INET SOCK_DGRAM INADDR_ANY);

    # This can fail if localhost is undefined or the
    # special 'loopback' address 127.0.0.1 is not configured
    # on your system. (/etc/rc.config.d/netconfig on HP-UX.)

$udpa = IO::Socket::INET->new(Proto => 'udp', LocalAddr => 'localhost');
$udpb = IO::Socket::INET->new(Proto => 'udp', LocalAddr => 'localhost');

print "ok 1\n";

$udpa->send("ok 2\n",0,$udpb->sockname);
$udpb->recv($buf="",5);
print $buf;
$udpb->send("ok 3\n");
$udpa->recv($buf="",5);
print $buf;
