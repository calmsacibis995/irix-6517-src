#! perl -w
use strict ;
use ExtUtils::MakeMaker 5.16 ;
use Config ;

my $VER_INFO ;
my $LIB_DIR ;
my $INC_DIR ;
my $DB_NAME ;
my $LIBS ;

ParseCONFIG() ;

if (defined $DB_NAME)
  { $LIBS = $DB_NAME }
else {
    if ($^O eq 'MSWin32')
      { $LIBS = '-llibdb' }
    else
      { $LIBS = '-ldb' }
}

# Solaris is special.
$LIBS .= " -lthread" if $^O eq 'solaris' ;

# OS2 is a special case, so check for it now.
my $OS2 = "" ;
$OS2 = "-DOS2" if $Config{'osname'} eq 'os2' ;

WriteMakefile(
	NAME 		=> 'DB_File',
	LIBS 		=> ["-L${LIB_DIR} $LIBS"],
        MAN3PODS        => ' ',         # Pods will be built by installman.
	INC		=> "-I$INC_DIR",
	VERSION_FROM	=> 'DB_File.pm',
	XSPROTOARG	=> '-noprototypes',
	DEFINE		=> "$OS2 $VER_INFO",
	'macro'		=> { INSTALLDIRS => 'perl' },
        'dist'          => {COMPRESS=>'gzip', SUFFIX=>'gz'},    
	);


sub MY::postamble {
	'
$(NAME).xs:	typemap
	@$(TOUCH) $(NAME).xs

Makefile:	config.in

' ;
}


sub ParseCONFIG
{
    my ($k, $v) ;
    my @badkey = () ;
    my %Info = () ;
    my @Options = qw( INCLUDE LIB PREFIX HASH DBNAME ) ;
    my %ValidOption = map {$_, 1} @Options ;
    my %Parsed = %ValidOption ;
    my $CONFIG = 'config.in' ;

    print "Parsing $CONFIG...\n" ;

    # DBNAME is optional, so pretend it has been parsed.
    delete $Parsed{'DBNAME'} ;


    open(F, "$CONFIG") or die "Cannot open file $CONFIG: $!\n" ;
    while (<F>) {
	s/^\s*|\s*$//g ;
	next if /^\s*$/ or /^\s*#/ ;
	s/\s*#\s*$// ;

	($k, $v) = split(/\s+=\s+/, $_, 2) ;
	$k = uc $k ;
	if ($ValidOption{$k}) {
	    delete $Parsed{$k} ;
	    $Info{$k} = $v ;
	}
	else {
	    push(@badkey, $k) ;
	}
    }
    close F ;

    print "Unknown keys in $CONFIG ignored [@badkey]\n"
	if @badkey ;

    # check parsed values
    my @missing = () ;
    die "The following keys are missing from $CONFIG file: [@missing]\n" 
        if @missing = keys %Parsed ;

    $INC_DIR = $Info{'INCLUDE'} ;
    $LIB_DIR = $Info{'LIB'} ;
    $DB_NAME = $Info{'DBNAME'} if defined $Info{'DBNAME'} ;
    my $PREFIX  = $Info{'PREFIX'} ;
    my $HASH    = $Info{'HASH'} ;

    $VER_INFO = "-DmDB_Prefix_t=${PREFIX} -DmDB_Hash_t=${HASH}" ;

    print <<EOM if 0 ;
    INCLUDE	[$INC_DIR]
    LIB		[$LIB_DIR]
    HASH	[$HASH]
    PREFIX	[$PREFIX]
    DBNAME      [$DB_NAME]

EOM

    print "Looks Good.\n" ;

}

# end of file Makefile.PL
