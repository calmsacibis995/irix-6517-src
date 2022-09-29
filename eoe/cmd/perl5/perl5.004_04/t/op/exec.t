#!./perl

# $RCSfile: exec.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:49 $

$| = 1;				# flush stdout

if ($^O eq 'MSWin32') {
    print "# exec is unsupported on Win32\n";
    print "1..0\n";
    exit(0);
}

print "1..8\n";

print "not ok 1\n" if system "echo ok \\1";	# shell interpreted
print "not ok 2\n" if system "echo ok 2";	# split and directly called
print "not ok 3\n" if system "echo", "ok", "3"; # directly called

if (system "true") {print "not ok 4\n";} else {print "ok 4\n";}

if ((system "/bin/sh -c 'exit 1'") != 256) { print "not "; }
print "ok 5\n";

if ((system "lskdfj") == 255 << 8) {print "ok 6\n";} else {print "not ok 6\n";}

unless (exec "lskdjfalksdjfdjfkls") {print "ok 7\n";} else {print "not ok 7\n";}

exec "echo","ok","8";
