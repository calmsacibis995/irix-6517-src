#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    unless ($Config{'d_fork'}) {
	print "1..0\n";
	exit 0;
    }
    # make warnings fatal
    $SIG{__WARN__} = sub { die @_ };
}

use strict;
use IO::Handle;
use IPC::Open3;
#require 'open3.pl'; use subs 'open3';

my $perl = './perl';

sub ok {
    my ($n, $result, $info) = @_;
    if ($result) {
	print "ok $n\n";
    }
    else {
    	print "not ok $n\n";
	print "# $info\n" if $info;
    }
}

my ($pid, $reaped_pid);
STDOUT->autoflush;
STDERR->autoflush;

print "1..21\n";

# basic
ok 1, $pid = open3 'WRITE', 'READ', 'ERROR', $perl, '-e', <<'EOF';
    $| = 1;
    print scalar <STDIN>;
    print STDERR "hi error\n";
EOF
ok 2, print WRITE "hi kid\n";
ok 3, <READ> eq "hi kid\n";
ok 4, <ERROR> eq "hi error\n";
ok 5, close(WRITE), $!;
ok 6, close(READ), $!;
ok 7, close(ERROR), $!;
$reaped_pid = waitpid $pid, 0;
ok 8, $reaped_pid == $pid, $reaped_pid;
ok 9, $? == 0, $?;

# read and error together, both named
$pid = open3 'WRITE', 'READ', 'READ', $perl, '-e', <<'EOF';
    $| = 1;
    print scalar <STDIN>;
    print STDERR scalar <STDIN>;
EOF
print WRITE "ok 10\n";
print scalar <READ>;
print WRITE "ok 11\n";
print scalar <READ>;
waitpid $pid, 0;

# read and error together, error empty
$pid = open3 'WRITE', 'READ', '', $perl, '-e', <<'EOF';
    $| = 1;
    print scalar <STDIN>;
    print STDERR scalar <STDIN>;
EOF
print WRITE "ok 12\n";
print scalar <READ>;
print WRITE "ok 13\n";
print scalar <READ>;
waitpid $pid, 0;

# dup writer
ok 14, pipe PIPE_READ, PIPE_WRITE;
$pid = open3 '<&PIPE_READ', 'READ', '',
		    $perl, '-e', 'print scalar <STDIN>';
close PIPE_READ;
print PIPE_WRITE "ok 15\n";
close PIPE_WRITE;
print scalar <READ>;
waitpid $pid, 0;

# dup reader
$pid = open3 'WRITE', '>&STDOUT', 'ERROR',
		    $perl, '-e', 'print scalar <STDIN>';
print WRITE "ok 16\n";
waitpid $pid, 0;

# dup error:  This particular case, duping stderr onto the existing
# stdout but putting stdout somewhere else, is a good case because it
# used not to work.
$pid = open3 'WRITE', 'READ', '>&STDOUT',
		    $perl, '-e', 'print STDERR scalar <STDIN>';
print WRITE "ok 17\n";
waitpid $pid, 0;

# dup reader and error together, both named
$pid = open3 'WRITE', '>&STDOUT', '>&STDOUT', $perl, '-e', <<'EOF';
    $| = 1;
    print STDOUT scalar <STDIN>;
    print STDERR scalar <STDIN>;
EOF
print WRITE "ok 18\n";
print WRITE "ok 19\n";
waitpid $pid, 0;

# dup reader and error together, error empty
$pid = open3 'WRITE', '>&STDOUT', '', $perl, '-e', <<'EOF';
    $| = 1;
    print STDOUT scalar <STDIN>;
    print STDERR scalar <STDIN>;
EOF
print WRITE "ok 20\n";
print WRITE "ok 21\n";
waitpid $pid, 0;
