#!./perl

# This test harness will (eventually) test the "tie" functionality
# without the need for a *DBM* implementation.

# Currently it only tests the untie warning 

chdir 't' if -d 't';
@INC = "../lib";
$ENV{PERL5LIB} = "../lib";

$|=1;

# catch warnings into fatal errors
$SIG{__WARN__} = sub { die "WARNING: @_" } ;

undef $/;
@prgs = split "\n########\n", <DATA>;
print "1..", scalar @prgs, "\n";

for (@prgs){
    my($prog,$expected) = split(/\nEXPECT\n/, $_);
    eval "$prog" ;
    $status = $?;
    $results = $@ ;
    $results =~ s/\n+$//;
    $expected =~ s/\n+$//;
    if ( $status or $results and $results !~ /^WARNING: $expected/){
	print STDERR "STATUS: $status\n";
	print STDERR "PROG: $prog\n";
	print STDERR "EXPECTED:\n$expected\n";
	print STDERR "GOT:\n$results\n";
	print "not ";
    }
    print "ok ", ++$i, "\n";
}

__END__

# standard behaviour, without any extra references
use Tie::Hash ;
tie %h, Tie::StdHash;
untie %h;
EXPECT
########

# standard behaviour, with 1 extra reference
use Tie::Hash ;
$a = tie %h, Tie::StdHash;
untie %h;
EXPECT
########

# standard behaviour, with 1 extra reference via tied
use Tie::Hash ;
tie %h, Tie::StdHash;
$a = tied %h;
untie %h;
EXPECT
########

# standard behaviour, with 1 extra reference which is destroyed
use Tie::Hash ;
$a = tie %h, Tie::StdHash;
$a = 0 ;
untie %h;
EXPECT
########

# standard behaviour, with 1 extra reference via tied which is destroyed
use Tie::Hash ;
tie %h, Tie::StdHash;
$a = tied %h;
$a = 0 ;
untie %h;
EXPECT
########

# strict behaviour, without any extra references
#use warning 'untie';
local $^W = 1 ;
use Tie::Hash ;
tie %h, Tie::StdHash;
untie %h;
EXPECT
########

# strict behaviour, with 1 extra references generating an error
#use warning 'untie';
local $^W = 1 ;
use Tie::Hash ;
$a = tie %h, Tie::StdHash;
untie %h;
EXPECT
untie attempted while 1 inner references still exist
########

# strict behaviour, with 1 extra references via tied generating an error
#use warning 'untie';
local $^W = 1 ;
use Tie::Hash ;
tie %h, Tie::StdHash;
$a = tied %h;
untie %h;
EXPECT
untie attempted while 1 inner references still exist
########

# strict behaviour, with 1 extra references which are destroyed
#use warning 'untie';
local $^W = 1 ;
use Tie::Hash ;
$a = tie %h, Tie::StdHash;
$a = 0 ;
untie %h;
EXPECT
########

# strict behaviour, with extra 1 references via tied which are destroyed
#use warning 'untie';
local $^W = 1 ;
use Tie::Hash ;
tie %h, Tie::StdHash;
$a = tied %h;
$a = 0 ;
untie %h;
EXPECT
########

# strict error behaviour, with 2 extra references 
#use warning 'untie';
local $^W = 1 ;
use Tie::Hash ;
$a = tie %h, Tie::StdHash;
$b = tied %h ;
untie %h;
EXPECT
untie attempted while 2 inner references still exist
########

# strict behaviour, check scope of strictness.
#no warning 'untie';
local $^W = 0 ;
use Tie::Hash ;
$A = tie %H, Tie::StdHash;
$C = $B = tied %H ;
{
    #use warning 'untie';
    local $^W = 1 ;
    use Tie::Hash ;
    tie %h, Tie::StdHash;
    untie %h;
}
untie %H;
EXPECT
