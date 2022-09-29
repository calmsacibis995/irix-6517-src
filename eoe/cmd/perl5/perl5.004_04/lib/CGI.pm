package CGI;
require 5.001;

# See the bottom of this file for the POD documentation.  Search for the
# string '=head'.

# You can run this file through either pod2man or pod2html to produce pretty
# documentation in manual or html file format (these utilities are part of the
# Perl 5 distribution).

# Copyright 1995-1997 Lincoln D. Stein.  All rights reserved.
# It may be used and modified freely, but I do request that this copyright
# notice remain attached to the file.  You may modify this module as you 
# wish, but if you redistribute a modified version, please attach a note
# listing the modifications you have made.

# The most recent version and complete docs are available at:
#   http://www.genome.wi.mit.edu/ftp/pub/software/WWW/cgi_docs.html
#   ftp://ftp-genome.wi.mit.edu/pub/software/WWW/

# Set this to 1 to enable copious autoloader debugging messages
$AUTOLOAD_DEBUG=0;

# Set this to 1 to enable NPH scripts
# or: 
#    1) use CGI qw(:nph)
#    2) $CGI::nph(1)
#    3) print header(-nph=>1)
$NPH=0;

# Set this to 1 to make the temporary files created
# during file uploads safe from prying eyes
# or do...
#    1) use CGI qw(:private_tempfiles)
#    2) $CGI::private_tempfiles(1);
$PRIVATE_TEMPFILES=0;

$CGI::revision = '$Id: CGI.pm,v 2.36 1997/5/10 8:22 lstein Exp $';
$CGI::VERSION='2.36';

# OVERRIDE THE OS HERE IF CGI.pm GUESSES WRONG
# $OS = 'UNIX';
# $OS = 'MACINTOSH';
# $OS = 'WINDOWS';
# $OS = 'VMS';
# $OS = 'OS2';

# HARD-CODED LOCATION FOR FILE UPLOAD TEMPORARY FILES.
# UNCOMMENT THIS ONLY IF YOU KNOW WHAT YOU'RE DOING.
# $TempFile::TMPDIRECTORY = '/usr/tmp';

# ------------------ START OF THE LIBRARY ------------

# FIGURE OUT THE OS WE'RE RUNNING UNDER
# Some systems support the $^O variable.  If not
# available then require() the Config library
unless ($OS) {
    unless ($OS = $^O) {
	require Config;
	$OS = $Config::Config{'osname'};
    }
}
if ($OS=~/Win/i) {
    $OS = 'WINDOWS';
} elsif ($OS=~/vms/i) {
    $OS = 'VMS';
} elsif ($OS=~/Mac/i) {
    $OS = 'MACINTOSH';
} elsif ($OS=~/os2/i) {
    $OS = 'OS2';
} else {
    $OS = 'UNIX';
}

# Some OS logic.  Binary mode enabled on DOS, NT and VMS
$needs_binmode = $OS=~/^(WINDOWS|VMS|OS2)/;

# This is the default class for the CGI object to use when all else fails.
$DefaultClass = 'CGI' unless defined $CGI::DefaultClass;
# This is where to look for autoloaded routines.
$AutoloadClass = $DefaultClass unless defined $CGI::AutoloadClass;

# The path separator is a slash, backslash or semicolon, depending
# on the paltform.
$SL = {
    UNIX=>'/',
    OS2=>'\\',
    WINDOWS=>'\\',
    MACINTOSH=>':',
    VMS=>'\\'
    }->{$OS};

# Turn on NPH scripts by default when running under IIS server!
$NPH++ if defined($ENV{'SERVER_SOFTWARE'}) && $ENV{'SERVER_SOFTWARE'}=~/IIS/;

# Turn on special checking for Doug MacEachern's modperl
if (defined($ENV{'GATEWAY_INTERFACE'}) && ($MOD_PERL = $ENV{'GATEWAY_INTERFACE'} =~ /^CGI-Perl/)) {
    $NPH++;
    $| = 1;
    $SEQNO = 1;
}

# This is really "\r\n", but the meaning of \n is different
# in MacPerl, so we resort to octal here.
$CRLF = "\015\012";

if ($needs_binmode) {
    $CGI::DefaultClass->binmode(main::STDOUT);
    $CGI::DefaultClass->binmode(main::STDIN);
    $CGI::DefaultClass->binmode(main::STDERR);
}

# Cute feature, but it broke when the overload mechanism changed...
# %OVERLOAD = ('""'=>'as_string');

%EXPORT_TAGS = (
	      ':html2'=>[h1..h6,qw/p br hr ol ul li dl dt dd menu code var strong em
			 tt i b blockquote pre img a address cite samp dfn html head
			 base body link nextid title meta kbd start_html end_html
			 input Select option/],
	      ':html3'=>[qw/div table caption th td TR Tr super sub strike applet PARAM embed basefont style span/],
	      ':netscape'=>[qw/blink frameset frame script font fontsize center/],
	      ':form'=>[qw/textfield textarea filefield password_field hidden checkbox checkbox_group 
		       submit reset defaults radio_group popup_menu button autoEscape
		       scrolling_list image_button start_form end_form startform endform
		       start_multipart_form isindex tmpFileName uploadInfo URL_ENCODED MULTIPART/],
	      ':cgi'=>[qw/param path_info path_translated url self_url script_name cookie dump
		       raw_cookie request_method query_string accept user_agent remote_host 
		       remote_addr referer server_name server_software server_port server_protocol
		       virtual_host remote_ident auth_type http use_named_parameters
		       remote_user user_name header redirect import_names put/],
	      ':ssl' => [qw/https/],
	      ':cgi-lib' => [qw/ReadParse PrintHeader HtmlTop HtmlBot SplitParam/],
	      ':html' => [qw/:html2 :html3 :netscape/],
	      ':standard' => [qw/:html2 :form :cgi/],
	      ':all' => [qw/:html2 :html3 :netscape :form :cgi/]
	 );

# to import symbols into caller
sub import {
    my $self = shift;
    my ($callpack, $callfile, $callline) = caller;
    foreach (@_) {
	$NPH++, next if $_ eq ':nph';
	$PRIVATE_TEMPFILES++, next if $_ eq ':private_tempfiles';
	foreach (&expand_tags($_)) {
	    tr/a-zA-Z0-9_//cd;  # don't allow weird function names
	    $EXPORT{$_}++;
	}
    }
    # To allow overriding, search through the packages
    # Till we find one in which the correct subroutine is defined.
    my @packages = ($self,@{"$self\:\:ISA"});
    foreach $sym (keys %EXPORT) {
	my $pck;
	my $def = ${"$self\:\:AutoloadClass"} || $DefaultClass;
	foreach $pck (@packages) {
	    if (defined(&{"$pck\:\:$sym"})) {
		$def = $pck;
		last;
	    }
	}
	*{"${callpack}::$sym"} = \&{"$def\:\:$sym"};
    }
}

sub expand_tags {
    my($tag) = @_;
    my(@r);
    return ($tag) unless $EXPORT_TAGS{$tag};
    foreach (@{$EXPORT_TAGS{$tag}}) {
	push(@r,&expand_tags($_));
    }
    return @r;
}

#### Method: new
# The new routine.  This will check the current environment
# for an existing query string, and initialize itself, if so.
####
sub new {
    my($class,$initializer) = @_;
    my $self = {};
    bless $self,ref $class || $class || $DefaultClass;
    $CGI::DefaultClass->_reset_globals() if $MOD_PERL;
    $initializer = to_filehandle($initializer) if $initializer;
    $self->init($initializer);
    return $self;
}

# We provide a DESTROY method so that the autoloader
# doesn't bother trying to find it.
sub DESTROY { }

#### Method: param
# Returns the value(s)of a named parameter.
# If invoked in a list context, returns the
# entire list.  Otherwise returns the first
# member of the list.
# If name is not provided, return a list of all
# the known parameters names available.
# If more than one argument is provided, the
# second and subsequent arguments are used to
# set the value of the parameter.
####
sub param {
    my($self,@p) = self_or_default(@_);
    return $self->all_parameters unless @p;
    my($name,$value,@other);

    # For compatibility between old calling style and use_named_parameters() style, 
    # we have to special case for a single parameter present.
    if (@p > 1) {
	($name,$value,@other) = $self->rearrange([NAME,[DEFAULT,VALUE,VALUES]],@p);
	my(@values);

	if (substr($p[0],0,1) eq '-' || $self->use_named_parameters) {
	    @values = defined($value) ? (ref($value) && ref($value) eq 'ARRAY' ? @{$value} : $value) : ();
	} else {
	    foreach ($value,@other) {
		push(@values,$_) if defined($_);
	    }
	}
	# If values is provided, then we set it.
	if (@values) {
	    $self->add_parameter($name);
	    $self->{$name}=[@values];
	}
    } else {
	$name = $p[0];
    }

    return () unless defined($name) && $self->{$name};
    return wantarray ? @{$self->{$name}} : $self->{$name}->[0];
}

#### Method: delete
# Deletes the named parameter entirely.
####
sub delete {
    my($self,$name) = self_or_default(@_);
    delete $self->{$name};
    delete $self->{'.fieldnames'}->{$name};
    @{$self->{'.parameters'}}=grep($_ ne $name,$self->param());
    return wantarray ? () : undef;
}

sub self_or_default {
    return @_ if defined($_[0]) && !ref($_[0]) && ($_[0] eq 'CGI');
    unless (defined($_[0]) && 
	    ref($_[0]) &&
	    (ref($_[0]) eq 'CGI' ||
	     eval "\$_[0]->isaCGI()")) { # optimize for the common case
	$CGI::DefaultClass->_reset_globals() 
	    if defined($Q) && $MOD_PERL && $CGI::DefaultClass->_new_request();
	$Q = $CGI::DefaultClass->new unless defined($Q);
	unshift(@_,$Q);
    }
    return @_;
}

sub _new_request {
    return undef unless (defined(Apache->seqno()) or eval { require Apache });
    if (Apache->seqno() != $SEQNO) {
	$SEQNO = Apache->seqno();
	return 1;
    } else {
	return undef;
    }
}

sub _reset_globals {
    undef $Q;
    undef @QUERY_PARAM;
}

sub self_or_CGI {
    local $^W=0;                # prevent a warning
    if (defined($_[0]) &&
	(substr(ref($_[0]),0,3) eq 'CGI' 
	 || eval "\$_[0]->isaCGI()")) {
	return @_;
    } else {
	return ($DefaultClass,@_);
    }
}

sub isaCGI {
    return 1;
}

#### Method: import_names
# Import all parameters into the given namespace.
# Assumes namespace 'Q' if not specified
####
sub import_names {
    my($self,$namespace) = self_or_default(@_);
    $namespace = 'Q' unless defined($namespace);
    die "Can't import names into 'main'\n"
	if $namespace eq 'main';
    my($param,@value,$var);
    foreach $param ($self->param) {
	# protect against silly names
	($var = $param)=~tr/a-zA-Z0-9_/_/c;
	$var = "${namespace}::$var";
	@value = $self->param($param);
	@{$var} = @value;
	${$var} = $value[0];
    }
}

#### Method: use_named_parameters
# Force CGI.pm to use named parameter-style method calls
# rather than positional parameters.  The same effect
# will happen automatically if the first parameter
# begins with a -.
sub use_named_parameters {
    my($self,$use_named) = self_or_default(@_);
    return $self->{'.named'} unless defined ($use_named);

    # stupidity to avoid annoying warnings
    return $self->{'.named'}=$use_named;
}

########################################
# THESE METHODS ARE MORE OR LESS PRIVATE
# GO TO THE __DATA__ SECTION TO SEE MORE
# PUBLIC METHODS
########################################

# Initialize the query object from the environment.
# If a parameter list is found, this object will be set
# to an associative array in which parameter names are keys
# and the values are stored as lists
# If a keyword list is found, this method creates a bogus
# parameter list with the single parameter 'keywords'.

sub init {
    my($self,$initializer) = @_;
    my($query_string,@lines);
    my($meth) = '';

    # if we get called more than once, we want to initialize
    # ourselves from the original query (which may be gone
    # if it was read from STDIN originally.)
    if (defined(@QUERY_PARAM) && !defined($initializer)) {

	foreach (@QUERY_PARAM) {
	    $self->param('-name'=>$_,'-value'=>$QUERY_PARAM{$_});
	}
	return;
    }

    $meth=$ENV{'REQUEST_METHOD'} if defined($ENV{'REQUEST_METHOD'});

    # If initializer is defined, then read parameters
    # from it.
  METHOD: {
      if (defined($initializer)) {

	  if (ref($initializer) && ref($initializer) eq 'HASH') {
	      foreach (keys %$initializer) {
		  $self->param('-name'=>$_,'-value'=>$initializer->{$_});
	      }
	      last METHOD;
	  }
	  
	  $initializer = $$initializer if ref($initializer);
	  if (defined(fileno($initializer))) {
	      while (<$initializer>) {
		  chomp;
		  last if /^=/;
		  push(@lines,$_);
	      }
	      # massage back into standard format
	      if ("@lines" =~ /=/) {
		  $query_string=join("&",@lines);
	      } else {
		  $query_string=join("+",@lines);
	      }
	      last METHOD;
	  }
	  $query_string = $initializer;
	  last METHOD;
      }
	  # If method is GET or HEAD, fetch the query from
	  # the environment.
      if ($meth=~/^(GET|HEAD)$/) {
	$query_string = $ENV{'QUERY_STRING'};
	last METHOD;
    }
	
      # If the method is POST, fetch the query from standard
      # input.
      if ($meth eq 'POST') {

	  if (defined($ENV{'CONTENT_TYPE'}) 
	      && 
	      $ENV{'CONTENT_TYPE'}=~m|^multipart/form-data|) {
	      my($boundary) = $ENV{'CONTENT_TYPE'}=~/boundary=(\S+)/;
	      $self->read_multipart($boundary,$ENV{'CONTENT_LENGTH'});

	  } else {

	      $self->read_from_client(\*STDIN,\$query_string,$ENV{'CONTENT_LENGTH'},0)
		  if $ENV{'CONTENT_LENGTH'} > 0;

	  }
	  # Some people want to have their cake and eat it too!
	  # Uncomment this line to have the contents of the query string
	  # APPENDED to the POST data.
	  # $query_string .= ($query_string ? '&' : '') . $ENV{'QUERY_STRING'} if $ENV{'QUERY_STRING'};
	  last METHOD;
      }
	  
      # If neither is set, assume we're being debugged offline.
      # Check the command line and then the standard input for data.
      # We use the shellwords package in order to behave the way that
      # UN*X programmers expect.
      $query_string = &read_from_cmdline;
  }
    
    # We now have the query string in hand.  We do slightly
    # different things for keyword lists and parameter lists.
    if ($query_string) {
	if ($query_string =~ /=/) {
	    $self->parse_params($query_string);
	} else {
	    $self->add_parameter('keywords');
	    $self->{'keywords'} = [$self->parse_keywordlist($query_string)];
	}
    }

    # Special case.  Erase everything if there is a field named
    # .defaults.
    if ($self->param('.defaults')) {
	undef %{$self};
    }

    # Associative array containing our defined fieldnames
    $self->{'.fieldnames'} = {};
    foreach ($self->param('.cgifields')) {
	$self->{'.fieldnames'}->{$_}++;
    }
    
    # Clear out our default submission button flag if present
    $self->delete('.submit');
    $self->delete('.cgifields');
    $self->save_request unless $initializer;

}


# FUNCTIONS TO OVERRIDE:

# Turn a string into a filehandle
sub to_filehandle {
    my $string = shift;
    if ($string && !ref($string)) {
	my($package) = caller(1);
	my($tmp) = $string=~/[':]/ ? $string : "$package\:\:$string"; 
	return $tmp if defined(fileno($tmp));
    }
    return $string;
}

# Create a new multipart buffer
sub new_MultipartBuffer {
    my($self,$boundary,$length,$filehandle) = @_;
    return MultipartBuffer->new($self,$boundary,$length,$filehandle);
}

# Read data from a file handle
sub read_from_client {
    my($self, $fh, $buff, $len, $offset) = @_;
    local $^W=0;                # prevent a warning
    return read($fh, $$buff, $len, $offset);
}

# put a filehandle into binary mode (DOS)
sub binmode {
    binmode($_[1]);
}

# send output to the browser
sub put {
    my($self,@p) = self_or_default(@_);
    $self->print(@p);
}

# print to standard output (for overriding in mod_perl)
sub print {
    shift;
    CORE::print(@_);
}

# unescape URL-encoded data
sub unescape {
    my($todecode) = @_;
    $todecode =~ tr/+/ /;       # pluses become spaces
    $todecode =~ s/%([0-9a-fA-F]{2})/pack("c",hex($1))/ge;
    return $todecode;
}

# URL-encode data
sub escape {
    my($toencode) = @_;
    $toencode=~s/([^a-zA-Z0-9_\-.])/uc sprintf("%%%02x",ord($1))/eg;
    return $toencode;
}

sub save_request {
    my($self) = @_;
    # We're going to play with the package globals now so that if we get called
    # again, we initialize ourselves in exactly the same way.  This allows
    # us to have several of these objects.
    @QUERY_PARAM = $self->param; # save list of parameters
    foreach (@QUERY_PARAM) {
	$QUERY_PARAM{$_}=$self->{$_};
    }
}

sub parse_keywordlist {
    my($self,$tosplit) = @_;
    $tosplit = &unescape($tosplit); # unescape the keywords
    $tosplit=~tr/+/ /;          # pluses to spaces
    my(@keywords) = split(/\s+/,$tosplit);
    return @keywords;
}

sub parse_params {
    my($self,$tosplit) = @_;
    my(@pairs) = split('&',$tosplit);
    my($param,$value);
    foreach (@pairs) {
	($param,$value) = split('=');
	$param = &unescape($param);
	$value = &unescape($value);
	$self->add_parameter($param);
	push (@{$self->{$param}},$value);
    }
}

sub add_parameter {
    my($self,$param)=@_;
    push (@{$self->{'.parameters'}},$param) 
	unless defined($self->{$param});
}

sub all_parameters {
    my $self = shift;
    return () unless defined($self) && $self->{'.parameters'};
    return () unless @{$self->{'.parameters'}};
    return @{$self->{'.parameters'}};
}

#### Method as_string
#
# synonym for "dump"
####
sub as_string {
    &dump(@_);
}

sub AUTOLOAD {
    print STDERR "CGI::AUTOLOAD for $AUTOLOAD\n" if $CGI::AUTOLOAD_DEBUG;
    my($func) = $AUTOLOAD;
    my($pack,$func_name) = $func=~/(.+)::([^:]+)$/;
    $pack = ${"$pack\:\:AutoloadClass"} || $CGI::DefaultClass
                unless defined(${"$pack\:\:AUTOLOADED_ROUTINES"});

    my($sub) = \%{"$pack\:\:SUBS"};
    unless (%$sub) {
	my($auto) = \${"$pack\:\:AUTOLOADED_ROUTINES"};
	eval "package $pack; $$auto";
	die $@ if $@;
    }
    my($code) = $sub->{$func_name};

    $code = "sub $AUTOLOAD { }" if (!$code and $func_name eq 'DESTROY');
    if (!$code) {
	if ($EXPORT{':any'} || 
	    $EXPORT{$func_name} || 
	    (%EXPORT_OK || grep(++$EXPORT_OK{$_},&expand_tags(':html')))
	    && $EXPORT_OK{$func_name}) {
	    $code = $sub->{'HTML_FUNC'};
	    $code=~s/func_name/$func_name/mg;
	}
    }
    die "Undefined subroutine $AUTOLOAD\n" unless $code;
    eval "package $pack; $code";
    if ($@) {
	$@ =~ s/ at .*\n//;
	die $@;
    }
    goto &{"$pack\:\:$func_name"};
}

# PRIVATE SUBROUTINE
# Smart rearrangement of parameters to allow named parameter
# calling.  We do the rearangement if:
# 1. The first parameter begins with a -
# 2. The use_named_parameters() method returns true
sub rearrange {
    my($self,$order,@param) = @_;
    return () unless @param;
    
    return @param unless (defined($param[0]) && substr($param[0],0,1) eq '-')
	|| $self->use_named_parameters;

    my $i;
    for ($i=0;$i<@param;$i+=2) {
	$param[$i]=~s/^\-//;     # get rid of initial - if present
	$param[$i]=~tr/a-z/A-Z/; # parameters are upper case
    }
    
    my(%param) = @param;                # convert into associative array
    my(@return_array);
    
    my($key)='';
    foreach $key (@$order) {
	my($value);
	# this is an awful hack to fix spurious warnings when the
	# -w switch is set.
	if (ref($key) && ref($key) eq 'ARRAY') {
	    foreach (@$key) {
		last if defined($value);
		$value = $param{$_};
		delete $param{$_};
	    }
	} else {
	    $value = $param{$key};
	    delete $param{$key};
	}
	push(@return_array,$value);
    }
    push (@return_array,$self->make_attributes(\%param)) if %param;
    return (@return_array);
}

###############################################################################
################# THESE FUNCTIONS ARE AUTOLOADED ON DEMAND ####################
###############################################################################
$AUTOLOADED_ROUTINES = '';      # get rid of -w warning
$AUTOLOADED_ROUTINES=<<'END_OF_AUTOLOAD';

%SUBS = (

'URL_ENCODED'=> <<'END_OF_FUNC',
sub URL_ENCODED { 'application/x-www-form-urlencoded'; }
END_OF_FUNC

'MULTIPART' => <<'END_OF_FUNC',
sub MULTIPART {  'multipart/form-data'; }
END_OF_FUNC

'HTML_FUNC' => <<'END_OF_FUNC',
sub func_name { 

    # handle various cases in which we're called
    # most of this bizarre stuff is to avoid -w errors
    shift if $_[0] && 
	(!ref($_[0]) && $_[0] eq $CGI::DefaultClass) ||
	    (ref($_[0]) &&
	     (substr(ref($_[0]),0,3) eq 'CGI' ||
	      eval "\$_[0]->isaCGI()"));

    my($attr) = '';
    if (ref($_[0]) && ref($_[0]) eq 'HASH') {
	my(@attr) = CGI::make_attributes('',shift);
	$attr = " @attr" if @attr;
    }
    my($tag,$untag) = ("\U<func_name\E$attr>","\U</func_name>\E");
    return $tag unless @_;
    if (ref($_[0]) eq 'ARRAY') {
	my(@r);
	foreach (@{$_[0]}) {
	    push(@r,"$tag$_$untag");
	}
	return "@r";
    } else {
	return "$tag@_$untag";
    }
}
END_OF_FUNC

#### Method: keywords
# Keywords acts a bit differently.  Calling it in a list context
# returns the list of keywords.  
# Calling it in a scalar context gives you the size of the list.
####
'keywords' => <<'END_OF_FUNC',
sub keywords {
    my($self,@values) = self_or_default(@_);
    # If values is provided, then we set it.
    $self->{'keywords'}=[@values] if @values;
    my(@result) = @{$self->{'keywords'}};
    @result;
}
END_OF_FUNC

# These are some tie() interfaces for compatibility
# with Steve Brenner's cgi-lib.pl routines
'ReadParse' => <<'END_OF_FUNC',
sub ReadParse {
    local(*in);
    if (@_) {
	*in = $_[0];
    } else {
	my $pkg = caller();
	*in=*{"${pkg}::in"};
    }
    tie(%in,CGI);
}
END_OF_FUNC

'PrintHeader' => <<'END_OF_FUNC',
sub PrintHeader {
    my($self) = self_or_default(@_);
    return $self->header();
}
END_OF_FUNC

'HtmlTop' => <<'END_OF_FUNC',
sub HtmlTop {
    my($self,@p) = self_or_default(@_);
    return $self->start_html(@p);
}
END_OF_FUNC

'HtmlBot' => <<'END_OF_FUNC',
sub HtmlBot {
    my($self,@p) = self_or_default(@_);
    return $self->end_html(@p);
}
END_OF_FUNC

'SplitParam' => <<'END_OF_FUNC',
sub SplitParam {
    my ($param) = @_;
    my (@params) = split ("\0", $param);
    return (wantarray ? @params : $params[0]);
}
END_OF_FUNC

'MethGet' => <<'END_OF_FUNC',
sub MethGet {
    return request_method() eq 'GET';
}
END_OF_FUNC

'MethPost' => <<'END_OF_FUNC',
sub MethPost {
    return request_method() eq 'POST';
}
END_OF_FUNC

'TIEHASH' => <<'END_OF_FUNC',
sub TIEHASH { 
    return new CGI;
}
END_OF_FUNC

'STORE' => <<'END_OF_FUNC',
sub STORE {
    $_[0]->param($_[1],split("\0",$_[2]));
}
END_OF_FUNC

'FETCH' => <<'END_OF_FUNC',
sub FETCH {
    return $_[0] if $_[1] eq 'CGI';
    return undef unless defined $_[0]->param($_[1]);
    return join("\0",$_[0]->param($_[1]));
}
END_OF_FUNC

'FIRSTKEY' => <<'END_OF_FUNC',
sub FIRSTKEY {
    $_[0]->{'.iterator'}=0;
    $_[0]->{'.parameters'}->[$_[0]->{'.iterator'}++];
}
END_OF_FUNC

'NEXTKEY' => <<'END_OF_FUNC',
sub NEXTKEY {
    $_[0]->{'.parameters'}->[$_[0]->{'.iterator'}++];
}
END_OF_FUNC

'EXISTS' => <<'END_OF_FUNC',
sub EXISTS {
    exists $_[0]->{$_[1]};
}
END_OF_FUNC

'DELETE' => <<'END_OF_FUNC',
sub DELETE {
    $_[0]->delete($_[1]);
}
END_OF_FUNC

'CLEAR' => <<'END_OF_FUNC',
sub CLEAR {
    %{$_[0]}=();
}
####
END_OF_FUNC

####
# Append a new value to an existing query
####
'append' => <<'EOF',
sub append {
    my($self,@p) = @_;
    my($name,$value) = $self->rearrange([NAME,[VALUE,VALUES]],@p);
    my(@values) = defined($value) ? (ref($value) ? @{$value} : $value) : ();
    if (@values) {
	$self->add_parameter($name);
	push(@{$self->{$name}},@values);
    }
    return $self->param($name);
}
EOF

#### Method: delete_all
# Delete all parameters
####
'delete_all' => <<'EOF',
sub delete_all {
    my($self) = self_or_default(@_);
    undef %{$self};
}
EOF

#### Method: autoescape
# If you want to turn off the autoescaping features,
# call this method with undef as the argument
'autoEscape' => <<'END_OF_FUNC',
sub autoEscape {
    my($self,$escape) = self_or_default(@_);
    $self->{'dontescape'}=!$escape;
}
END_OF_FUNC


#### Method: version
# Return the current version
####
'version' => <<'END_OF_FUNC',
sub version {
    return $VERSION;
}
END_OF_FUNC

'make_attributes' => <<'END_OF_FUNC',
sub make_attributes {
    my($self,$attr) = @_;
    return () unless $attr && ref($attr) && ref($attr) eq 'HASH';
    my(@att);
    foreach (keys %{$attr}) {
	my($key) = $_;
	$key=~s/^\-//;     # get rid of initial - if present
	$key=~tr/a-z/A-Z/; # parameters are upper case
	push(@att,$attr->{$_} ne '' ? qq/$key="$attr->{$_}"/ : qq/$key/);
    }
    return @att;
}
END_OF_FUNC

#### Method: dump
# Returns a string in which all the known parameter/value 
# pairs are represented as nested lists, mainly for the purposes 
# of debugging.
####
'dump' => <<'END_OF_FUNC',
sub dump {
    my($self) = self_or_default(@_);
    my($param,$value,@result);
    return '<UL></UL>' unless $self->param;
    push(@result,"<UL>");
    foreach $param ($self->param) {
	my($name)=$self->escapeHTML($param);
	push(@result,"<LI><STRONG>$param</STRONG>");
	push(@result,"<UL>");
	foreach $value ($self->param($param)) {
	    $value = $self->escapeHTML($value);
	    push(@result,"<LI>$value");
	}
	push(@result,"</UL>");
    }
    push(@result,"</UL>\n");
    return join("\n",@result);
}
END_OF_FUNC


#### Method: save
# Write values out to a filehandle in such a way that they can
# be reinitialized by the filehandle form of the new() method
####
'save' => <<'END_OF_FUNC',
sub save {
    my($self,$filehandle) = self_or_default(@_);
    my($param);
    my($package) = caller;
# Check that this still works!
#    $filehandle = $filehandle=~/[':]/ ? $filehandle : "$package\:\:$filehandle";
    $filehandle = to_filehandle($filehandle);
    foreach $param ($self->param) {
	my($escaped_param) = &escape($param);
	my($value);
	foreach $value ($self->param($param)) {
	    print $filehandle "$escaped_param=",escape($value),"\n";
	}
    }
    print $filehandle "=\n";    # end of record
}
END_OF_FUNC


#### Method: header
# Return a Content-Type: style header
#
####
'header' => <<'END_OF_FUNC',
sub header {
    my($self,@p) = self_or_default(@_);
    my(@header);

    my($type,$status,$cookie,$target,$expires,$nph,@other) = 
	$self->rearrange([TYPE,STATUS,[COOKIE,COOKIES],TARGET,EXPIRES,NPH],@p);

    # rearrange() was designed for the HTML portion, so we
    # need to fix it up a little.
    foreach (@other) {
	next unless my($header,$value) = /([^\s=]+)=(.+)/;
	substr($header,1,1000)=~tr/A-Z/a-z/;
	($value)=$value=~/^"(.*)"$/;
	$_ = "$header: $value";
    }

    $type = $type || 'text/html';

    push(@header,'HTTP/1.0 ' . ($status || '200 OK')) if $nph || $NPH;
    push(@header,"Status: $status") if $status;
    push(@header,"Window-target: $target") if $target;
    # push all the cookies -- there may be several
    if ($cookie) {
	my(@cookie) = ref($cookie) ? @{$cookie} : $cookie;
	foreach (@cookie) {
	    push(@header,"Set-cookie: $_");
	}
    }
    # if the user indicates an expiration time, then we need
    # both an Expires and a Date header (so that the browser is
    # uses OUR clock)
    push(@header,"Expires: " . &date(&expire_calc($expires),'http'))
	if $expires;
    push(@header,"Date: " . &date(&expire_calc(0),'http')) if $expires || $cookie;
    push(@header,"Pragma: no-cache") if $self->cache();
    push(@header,@other);
    push(@header,"Content-type: $type");

    my $header = join($CRLF,@header);
    return $header . "${CRLF}${CRLF}";
}
END_OF_FUNC


#### Method: cache
# Control whether header() will produce the no-cache
# Pragma directive.
####
'cache' => <<'END_OF_FUNC',
sub cache {
    my($self,$new_value) = self_or_default(@_);
    $new_value = '' unless $new_value;
    if ($new_value ne '') {
	$self->{'cache'} = $new_value;
    }
    return $self->{'cache'};
}
END_OF_FUNC


#### Method: redirect
# Return a Location: style header
#
####
'redirect' => <<'END_OF_FUNC',
sub redirect {
    my($self,@p) = self_or_default(@_);
    my($url,$target,$cookie,$nph,@other) = $self->rearrange([[URI,URL],TARGET,COOKIE,NPH],@p);
    $url = $url || $self->self_url;
    my(@o);
    foreach (@other) { push(@o,split("=")); }
    if($MOD_PERL or exists $self->{'.req'}) {
	my $r = $self->{'.req'} || Apache->request;
	$r->header_out(Location => $url);
	$r->err_header_out(Location => $url);
	$r->status(302);
	return;
    }
    push(@o,
	 '-Status'=>'302 Found',
	 '-Location'=>$url,
	 '-URI'=>$url,
	 '-nph'=>($nph||$NPH));
    push(@o,'-Target'=>$target) if $target;
    push(@o,'-Cookie'=>$cookie) if $cookie;
    return $self->header(@o);
}
END_OF_FUNC


#### Method: start_html
# Canned HTML header
#
# Parameters:
# $title -> (optional) The title for this HTML document (-title)
# $author -> (optional) e-mail address of the author (-author)
# $base -> (optional) if set to true, will enter the BASE address of this document
#          for resolving relative references (-base) 
# $xbase -> (optional) alternative base at some remote location (-xbase)
# $target -> (optional) target window to load all links into (-target)
# $script -> (option) Javascript code (-script)
# $no_script -> (option) Javascript <noscript> tag (-noscript)
# $meta -> (optional) Meta information tags
# $head -> (optional) any other elements you'd like to incorporate into the <HEAD> tag
#           (a scalar or array ref)
# $style -> (optional) reference to an external style sheet
# @other -> (optional) any other named parameters you'd like to incorporate into
#           the <BODY> tag.
####
'start_html' => <<'END_OF_FUNC',
sub start_html {
    my($self,@p) = &self_or_default(@_);
    my($title,$author,$base,$xbase,$script,$noscript,$target,$meta,$head,$style,@other) = 
	$self->rearrange([TITLE,AUTHOR,BASE,XBASE,SCRIPT,NOSCRIPT,TARGET,META,HEAD,STYLE],@p);

    # strangely enough, the title needs to be escaped as HTML
    # while the author needs to be escaped as a URL
    $title = $self->escapeHTML($title || 'Untitled Document');
    $author = $self->escapeHTML($author);
    my(@result);
    push(@result,'<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">');
    push(@result,"<HTML><HEAD><TITLE>$title</TITLE>");
    push(@result,"<LINK REV=MADE HREF=\"mailto:$author\">") if $author;

    if ($base || $xbase || $target) {
	my $href = $xbase || $self->url();
	my $t = $target ? qq/ TARGET="$target"/ : '';
	push(@result,qq/<BASE HREF="$href"$t>/);
    }

    if ($meta && ref($meta) && (ref($meta) eq 'HASH')) {
	foreach (keys %$meta) { push(@result,qq(<META NAME="$_" CONTENT="$meta->{$_}">)); }
    }

    push(@result,ref($head) ? @$head : $head) if $head;

    # handle various types of -style parameters
    if ($style) {
	if (ref($style)) {
	    my($src,$code,@other) =
		$self->rearrange([SRC,CODE],
				 '-foo'=>'bar',	# a trick to allow the '-' to be omitted
				 ref($style) eq 'ARRAY' ? @$style : %$style);
	    push(@result,qq/<LINK REL="stylesheet" HREF="$src">/) if $src;
	    push(@result,style($code)) if $code;
	} else {
	    push(@result,style($style))
	}
    }

    # handle -script parameter
    if ($script) {
	my($src,$code,$language);
	if (ref($script)) { # script is a hash
	    ($src,$code,$language) =
		$self->rearrange([SRC,CODE,LANGUAGE],
				 '-foo'=>'bar',	# a trick to allow the '-' to be omitted
				 ref($style) eq 'ARRAY' ? @$script : %$script);
	
	} else {
	    ($src,$code,$language) = ('',$script,'JavaScript');
	}
	my(@satts);
	push(@satts,'src'=>$src) if $src;
	push(@satts,'language'=>$language || 'JavaScript');
	$code = "<!-- Hide script\n$code\n// End script hiding -->"
	    if $code && $language=~/javascript/i;
	$code = "<!-- Hide script\n$code\n\# End script hiding -->"
	    if $code && $language=~/perl/i;
	push(@result,script({@satts},$code));
    }

    # handle -noscript parameter
    push(@result,<<END) if $noscript;
<NOSCRIPT>
$noscript
</NOSCRIPT>
END
    ;
    my($other) = @other ? " @other" : '';
    push(@result,"</HEAD><BODY$other>");
    return join("\n",@result);
}
END_OF_FUNC


#### Method: end_html
# End an HTML document.
# Trivial method for completeness.  Just returns "</BODY>"
####
'end_html' => <<'END_OF_FUNC',
sub end_html {
    return "</BODY></HTML>";
}
END_OF_FUNC


################################
# METHODS USED IN BUILDING FORMS
################################

#### Method: isindex
# Just prints out the isindex tag.
# Parameters:
#  $action -> optional URL of script to run
# Returns:
#   A string containing a <ISINDEX> tag
'isindex' => <<'END_OF_FUNC',
sub isindex {
    my($self,@p) = self_or_default(@_);
    my($action,@other) = $self->rearrange([ACTION],@p);
    $action = qq/ACTION="$action"/ if $action;
    my($other) = @other ? " @other" : '';
    return "<ISINDEX $action$other>";
}
END_OF_FUNC


#### Method: startform
# Start a form
# Parameters:
#   $method -> optional submission method to use (GET or POST)
#   $action -> optional URL of script to run
#   $enctype ->encoding to use (URL_ENCODED or MULTIPART)
'startform' => <<'END_OF_FUNC',
sub startform {
    my($self,@p) = self_or_default(@_);

    my($method,$action,$enctype,@other) = 
	$self->rearrange([METHOD,ACTION,ENCTYPE],@p);

    $method = $method || 'POST';
    $enctype = $enctype || &URL_ENCODED;
    $action = $action ? qq/ACTION="$action"/ : $method eq 'GET' ?
	'ACTION="'.$self->script_name.'"' : '';
    my($other) = @other ? " @other" : '';
    $self->{'.parametersToAdd'}={};
    return qq/<FORM METHOD="$method" $action ENCTYPE="$enctype"$other>\n/;
}
END_OF_FUNC


#### Method: start_form
# synonym for startform
'start_form' => <<'END_OF_FUNC',
sub start_form {
    &startform;
}
END_OF_FUNC


#### Method: start_multipart_form
# synonym for startform
'start_multipart_form' => <<'END_OF_FUNC',
sub start_multipart_form {
    my($self,@p) = self_or_default(@_);
    if ($self->use_named_parameters || 
	(defined($param[0]) && substr($param[0],0,1) eq '-')) {
	my(%p) = @p;
	$p{'-enctype'}=&MULTIPART;
	return $self->startform(%p);
    } else {
	my($method,$action,@other) = 
	    $self->rearrange([METHOD,ACTION],@p);
	return $self->startform($method,$action,&MULTIPART,@other);
    }
}
END_OF_FUNC


#### Method: endform
# End a form
'endform' => <<'END_OF_FUNC',
sub endform {
    my($self,@p) = self_or_default(@_);    
    return ($self->get_fields,"</FORM>");
}
END_OF_FUNC


#### Method: end_form
# synonym for endform
'end_form' => <<'END_OF_FUNC',
sub end_form {
    &endform;
}
END_OF_FUNC


#### Method: textfield
# Parameters:
#   $name -> Name of the text field
#   $default -> Optional default value of the field if not
#                already defined.
#   $size ->  Optional width of field in characaters.
#   $maxlength -> Optional maximum number of characters.
# Returns:
#   A string containing a <INPUT TYPE="text"> field
#
'textfield' => <<'END_OF_FUNC',
sub textfield {
    my($self,@p) = self_or_default(@_);
    my($name,$default,$size,$maxlength,$override,@other) = 
	$self->rearrange([NAME,[DEFAULT,VALUE],SIZE,MAXLENGTH,[OVERRIDE,FORCE]],@p);

    my $current = $override ? $default : 
	(defined($self->param($name)) ? $self->param($name) : $default);

    $current = defined($current) ? $self->escapeHTML($current) : '';
    $name = defined($name) ? $self->escapeHTML($name) : '';
    my($s) = defined($size) ? qq/ SIZE=$size/ : '';
    my($m) = defined($maxlength) ? qq/ MAXLENGTH=$maxlength/ : '';
    my($other) = @other ? " @other" : '';    
    return qq/<INPUT TYPE="text" NAME="$name" VALUE="$current"$s$m$other>/;
}
END_OF_FUNC


#### Method: filefield
# Parameters:
#   $name -> Name of the file upload field
#   $size ->  Optional width of field in characaters.
#   $maxlength -> Optional maximum number of characters.
# Returns:
#   A string containing a <INPUT TYPE="text"> field
#
'filefield' => <<'END_OF_FUNC',
sub filefield {
    my($self,@p) = self_or_default(@_);

    my($name,$default,$size,$maxlength,$override,@other) = 
	$self->rearrange([NAME,[DEFAULT,VALUE],SIZE,MAXLENGTH,[OVERRIDE,FORCE]],@p);

    $current = $override ? $default :
	(defined($self->param($name)) ? $self->param($name) : $default);

    $name = defined($name) ? $self->escapeHTML($name) : '';
    my($s) = defined($size) ? qq/ SIZE=$size/ : '';
    my($m) = defined($maxlength) ? qq/ MAXLENGTH=$maxlength/ : '';
    $current = defined($current) ? $self->escapeHTML($current) : '';
    $other = ' ' . join(" ",@other);
    return qq/<INPUT TYPE="file" NAME="$name" VALUE="$current"$s$m$other>/;
}
END_OF_FUNC


#### Method: password
# Create a "secret password" entry field
# Parameters:
#   $name -> Name of the field
#   $default -> Optional default value of the field if not
#                already defined.
#   $size ->  Optional width of field in characters.
#   $maxlength -> Optional maximum characters that can be entered.
# Returns:
#   A string containing a <INPUT TYPE="password"> field
#
'password_field' => <<'END_OF_FUNC',
sub password_field {
    my ($self,@p) = self_or_default(@_);

    my($name,$default,$size,$maxlength,$override,@other) = 
	$self->rearrange([NAME,[DEFAULT,VALUE],SIZE,MAXLENGTH,[OVERRIDE,FORCE]],@p);

    my($current) =  $override ? $default :
	(defined($self->param($name)) ? $self->param($name) : $default);

    $name = defined($name) ? $self->escapeHTML($name) : '';
    $current = defined($current) ? $self->escapeHTML($current) : '';
    my($s) = defined($size) ? qq/ SIZE=$size/ : '';
    my($m) = defined($maxlength) ? qq/ MAXLENGTH=$maxlength/ : '';
    my($other) = @other ? " @other" : '';
    return qq/<INPUT TYPE="password" NAME="$name" VALUE="$current"$s$m$other>/;
}
END_OF_FUNC


#### Method: textarea
# Parameters:
#   $name -> Name of the text field
#   $default -> Optional default value of the field if not
#                already defined.
#   $rows ->  Optional number of rows in text area
#   $columns -> Optional number of columns in text area
# Returns:
#   A string containing a <TEXTAREA></TEXTAREA> tag
#
'textarea' => <<'END_OF_FUNC',
sub textarea {
    my($self,@p) = self_or_default(@_);
    
    my($name,$default,$rows,$cols,$override,@other) =
	$self->rearrange([NAME,[DEFAULT,VALUE],ROWS,[COLS,COLUMNS],[OVERRIDE,FORCE]],@p);

    my($current)= $override ? $default :
	(defined($self->param($name)) ? $self->param($name) : $default);

    $name = defined($name) ? $self->escapeHTML($name) : '';
    $current = defined($current) ? $self->escapeHTML($current) : '';
    my($r) = $rows ? " ROWS=$rows" : '';
    my($c) = $cols ? " COLS=$cols" : '';
    my($other) = @other ? " @other" : '';
    return qq{<TEXTAREA NAME="$name"$r$c$other>$current</TEXTAREA>};
}
END_OF_FUNC


#### Method: button
# Create a javascript button.
# Parameters:
#   $name ->  (optional) Name for the button. (-name)
#   $value -> (optional) Value of the button when selected (and visible name) (-value)
#   $onclick -> (optional) Text of the JavaScript to run when the button is
#                clicked.
# Returns:
#   A string containing a <INPUT TYPE="button"> tag
####
'button' => <<'END_OF_FUNC',
sub button {
    my($self,@p) = self_or_default(@_);

    my($label,$value,$script,@other) = $self->rearrange([NAME,[VALUE,LABEL],
							 [ONCLICK,SCRIPT]],@p);

    $label=$self->escapeHTML($label);
    $value=$self->escapeHTML($value);
    $script=$self->escapeHTML($script);

    my($name) = '';
    $name = qq/ NAME="$label"/ if $label;
    $value = $value || $label;
    my($val) = '';
    $val = qq/ VALUE="$value"/ if $value;
    $script = qq/ ONCLICK="$script"/ if $script;
    my($other) = @other ? " @other" : '';
    return qq/<INPUT TYPE="button"$name$val$script$other>/;
}
END_OF_FUNC


#### Method: submit
# Create a "submit query" button.
# Parameters:
#   $name ->  (optional) Name for the button.
#   $value -> (optional) Value of the button when selected (also doubles as label).
#   $label -> (optional) Label printed on the button(also doubles as the value).
# Returns:
#   A string containing a <INPUT TYPE="submit"> tag
####
'submit' => <<'END_OF_FUNC',
sub submit {
    my($self,@p) = self_or_default(@_);

    my($label,$value,@other) = $self->rearrange([NAME,[VALUE,LABEL]],@p);

    $label=$self->escapeHTML($label);
    $value=$self->escapeHTML($value);

    my($name) = ' NAME=".submit"';
    $name = qq/ NAME="$label"/ if $label;
    $value = $value || $label;
    my($val) = '';
    $val = qq/ VALUE="$value"/ if defined($value);
    my($other) = @other ? " @other" : '';
    return qq/<INPUT TYPE="submit"$name$val$other>/;
}
END_OF_FUNC


#### Method: reset
# Create a "reset" button.
# Parameters:
#   $name -> (optional) Name for the button.
# Returns:
#   A string containing a <INPUT TYPE="reset"> tag
####
'reset' => <<'END_OF_FUNC',
sub reset {
    my($self,@p) = self_or_default(@_);
    my($label,@other) = $self->rearrange([NAME],@p);
    $label=$self->escapeHTML($label);
    my($value) = defined($label) ? qq/ VALUE="$label"/ : '';
    my($other) = @other ? " @other" : '';
    return qq/<INPUT TYPE="reset"$value$other>/;
}
END_OF_FUNC


#### Method: defaults
# Create a "defaults" button.
# Parameters:
#   $name -> (optional) Name for the button.
# Returns:
#   A string containing a <INPUT TYPE="submit" NAME=".defaults"> tag
#
# Note: this button has a special meaning to the initialization script,
# and tells it to ERASE the current query string so that your defaults
# are used again!
####
'defaults' => <<'END_OF_FUNC',
sub defaults {
    my($self,@p) = self_or_default(@_);

    my($label,@other) = $self->rearrange([[NAME,VALUE]],@p);

    $label=$self->escapeHTML($label);
    $label = $label || "Defaults";
    my($value) = qq/ VALUE="$label"/;
    my($other) = @other ? " @other" : '';
    return qq/<INPUT TYPE="submit" NAME=".defaults"$value$other>/;
}
END_OF_FUNC


#### Method: checkbox
# Create a checkbox that is not logically linked to any others.
# The field value is "on" when the button is checked.
# Parameters:
#   $name -> Name of the checkbox
#   $checked -> (optional) turned on by default if true
#   $value -> (optional) value of the checkbox, 'on' by default
#   $label -> (optional) a user-readable label printed next to the box.
#             Otherwise the checkbox name is used.
# Returns:
#   A string containing a <INPUT TYPE="checkbox"> field
####
'checkbox' => <<'END_OF_FUNC',
sub checkbox {
    my($self,@p) = self_or_default(@_);

    my($name,$checked,$value,$label,$override,@other) = 
	$self->rearrange([NAME,[CHECKED,SELECTED,ON],VALUE,LABEL,[OVERRIDE,FORCE]],@p);
    
    if (!$override && defined($self->param($name))) {
	$value = $self->param($name) unless defined $value;
	$checked = $self->param($name) eq $value ? ' CHECKED' : '';
    } else {
	$checked = $checked ? ' CHECKED' : '';
	$value = defined $value ? $value : 'on';
    }
    my($the_label) = defined $label ? $label : $name;
    $name = $self->escapeHTML($name);
    $value = $self->escapeHTML($value);
    $the_label = $self->escapeHTML($the_label);
    my($other) = @other ? " @other" : '';
    $self->register_parameter($name);
    return <<END;
<INPUT TYPE="checkbox" NAME="$name" VALUE="$value"$checked$other>$the_label
END
}
END_OF_FUNC


#### Method: checkbox_group
# Create a list of logically-linked checkboxes.
# Parameters:
#   $name -> Common name for all the check boxes
#   $values -> A pointer to a regular array containing the
#             values for each checkbox in the group.
#   $defaults -> (optional)
#             1. If a pointer to a regular array of checkbox values,
#             then this will be used to decide which
#             checkboxes to turn on by default.
#             2. If a scalar, will be assumed to hold the
#             value of a single checkbox in the group to turn on. 
#   $linebreak -> (optional) Set to true to place linebreaks
#             between the buttons.
#   $labels -> (optional)
#             A pointer to an associative array of labels to print next to each checkbox
#             in the form $label{'value'}="Long explanatory label".
#             Otherwise the provided values are used as the labels.
# Returns:
#   An ARRAY containing a series of <INPUT TYPE="checkbox"> fields
####
'checkbox_group' => <<'END_OF_FUNC',
sub checkbox_group {
    my($self,@p) = self_or_default(@_);

    my($name,$values,$defaults,$linebreak,$labels,$rows,$columns,
       $rowheaders,$colheaders,$override,$nolabels,@other) =
	$self->rearrange([NAME,[VALUES,VALUE],[DEFAULTS,DEFAULT],
			  LINEBREAK,LABELS,ROWS,[COLUMNS,COLS],
			  ROWHEADERS,COLHEADERS,
			  [OVERRIDE,FORCE],NOLABELS],@p);

    my($checked,$break,$result,$label);

    my(%checked) = $self->previous_or_default($name,$defaults,$override);

    $break = $linebreak ? "<BR>" : '';
    $name=$self->escapeHTML($name);

    # Create the elements
    my(@elements);
    my(@values) = $values ? @$values : $self->param($name);
    my($other) = @other ? " @other" : '';
    foreach (@values) {
	$checked = $checked{$_} ? ' CHECKED' : '';
	$label = '';
	unless (defined($nolabels) && $nolabels) {
	    $label = $_;
	    $label = $labels->{$_} if defined($labels) && $labels->{$_};
	    $label = $self->escapeHTML($label);
	}
	$_ = $self->escapeHTML($_);
	push(@elements,qq/<INPUT TYPE="checkbox" NAME="$name" VALUE="$_"$checked$other>${label} ${break}/);
    }
    $self->register_parameter($name);
    return wantarray ? @elements : join('',@elements) unless $columns;
    return _tableize($rows,$columns,$rowheaders,$colheaders,@elements);
}
END_OF_FUNC


# Escape HTML -- used internally
'escapeHTML' => <<'END_OF_FUNC',
sub escapeHTML {
    my($self,$toencode) = @_;
    return undef unless defined($toencode);
    return $toencode if $self->{'dontescape'};
    $toencode=~s/&/&amp;/g;
    $toencode=~s/\"/&quot;/g;
    $toencode=~s/>/&gt;/g;
    $toencode=~s/</&lt;/g;
    return $toencode;
}
END_OF_FUNC


# Internal procedure - don't use
'_tableize' => <<'END_OF_FUNC',
sub _tableize {
    my($rows,$columns,$rowheaders,$colheaders,@elements) = @_;
    my($result);

    $rows = int(0.99 + @elements/$columns) unless $rows;
    # rearrange into a pretty table
    $result = "<TABLE>";
    my($row,$column);
    unshift(@$colheaders,'') if @$colheaders && @$rowheaders;
    $result .= "<TR>" if @{$colheaders};
    foreach (@{$colheaders}) {
	$result .= "<TH>$_</TH>";
    }
    for ($row=0;$row<$rows;$row++) {
	$result .= "<TR>";
	$result .= "<TH>$rowheaders->[$row]</TH>" if @$rowheaders;
	for ($column=0;$column<$columns;$column++) {
	    $result .= "<TD>" . $elements[$column*$rows + $row] . "</TD>";
	}
	$result .= "</TR>";
    }
    $result .= "</TABLE>";
    return $result;
}
END_OF_FUNC


#### Method: radio_group
# Create a list of logically-linked radio buttons.
# Parameters:
#   $name -> Common name for all the buttons.
#   $values -> A pointer to a regular array containing the
#             values for each button in the group.
#   $default -> (optional) Value of the button to turn on by default.  Pass '-'
#               to turn _nothing_ on.
#   $linebreak -> (optional) Set to true to place linebreaks
#             between the buttons.
#   $labels -> (optional)
#             A pointer to an associative array of labels to print next to each checkbox
#             in the form $label{'value'}="Long explanatory label".
#             Otherwise the provided values are used as the labels.
# Returns:
#   An ARRAY containing a series of <INPUT TYPE="radio"> fields
####
'radio_group' => <<'END_OF_FUNC',
sub radio_group {
    my($self,@p) = self_or_default(@_);

    my($name,$values,$default,$linebreak,$labels,
       $rows,$columns,$rowheaders,$colheaders,$override,$nolabels,@other) =
	$self->rearrange([NAME,[VALUES,VALUE],DEFAULT,LINEBREAK,LABELS,
			  ROWS,[COLUMNS,COLS],
			  ROWHEADERS,COLHEADERS,
			  [OVERRIDE,FORCE],NOLABELS],@p);
    my($result,$checked);

    if (!$override && defined($self->param($name))) {
	$checked = $self->param($name);
    } else {
	$checked = $default;
    }
    # If no check array is specified, check the first by default
    $checked = $values->[0] unless $checked;
    $name=$self->escapeHTML($name);

    my(@elements);
    my(@values) = $values ? @$values : $self->param($name);
    my($other) = @other ? " @other" : '';
    foreach (@values) {
	my($checkit) = $checked eq $_ ? ' CHECKED' : '';
	my($break) = $linebreak ? '<BR>' : '';
	my($label)='';
	unless (defined($nolabels) && $nolabels) {
	    $label = $_;
	    $label = $labels->{$_} if defined($labels) && $labels->{$_};
	    $label = $self->escapeHTML($label);
	}
	$_=$self->escapeHTML($_);
	push(@elements,qq/<INPUT TYPE="radio" NAME="$name" VALUE="$_"$checkit$other>${label} ${break}/);
    }
    $self->register_parameter($name);
    return wantarray ? @elements : join('',@elements) unless $columns;
    return _tableize($rows,$columns,$rowheaders,$colheaders,@elements);
}
END_OF_FUNC


#### Method: popup_menu
# Create a popup menu.
# Parameters:
#   $name -> Name for all the menu
#   $values -> A pointer to a regular array containing the
#             text of each menu item.
#   $default -> (optional) Default item to display
#   $labels -> (optional)
#             A pointer to an associative array of labels to print next to each checkbox
#             in the form $label{'value'}="Long explanatory label".
#             Otherwise the provided values are used as the labels.
# Returns:
#   A string containing the definition of a popup menu.
####
'popup_menu' => <<'END_OF_FUNC',
sub popup_menu {
    my($self,@p) = self_or_default(@_);

    my($name,$values,$default,$labels,$override,@other) =
	$self->rearrange([NAME,[VALUES,VALUE],[DEFAULT,DEFAULTS],LABELS,[OVERRIDE,FORCE]],@p);
    my($result,$selected);

    if (!$override && defined($self->param($name))) {
	$selected = $self->param($name);
    } else {
	$selected = $default;
    }
    $name=$self->escapeHTML($name);
    my($other) = @other ? " @other" : '';

    my(@values) = $values ? @$values : $self->param($name);
    $result = qq/<SELECT NAME="$name"$other>\n/;
    foreach (@values) {
	my($selectit) = defined($selected) ? ($selected eq $_ ? 'SELECTED' : '' ) : '';
	my($label) = $_;
	$label = $labels->{$_} if defined($labels) && $labels->{$_};
	my($value) = $self->escapeHTML($_);
	$label=$self->escapeHTML($label);
	$result .= "<OPTION $selectit VALUE=\"$value\">$label\n";
    }

    $result .= "</SELECT>\n";
    return $result;
}
END_OF_FUNC


#### Method: scrolling_list
# Create a scrolling list.
# Parameters:
#   $name -> name for the list
#   $values -> A pointer to a regular array containing the
#             values for each option line in the list.
#   $defaults -> (optional)
#             1. If a pointer to a regular array of options,
#             then this will be used to decide which
#             lines to turn on by default.
#             2. Otherwise holds the value of the single line to turn on.
#   $size -> (optional) Size of the list.
#   $multiple -> (optional) If set, allow multiple selections.
#   $labels -> (optional)
#             A pointer to an associative array of labels to print next to each checkbox
#             in the form $label{'value'}="Long explanatory label".
#             Otherwise the provided values are used as the labels.
# Returns:
#   A string containing the definition of a scrolling list.
####
'scrolling_list' => <<'END_OF_FUNC',
sub scrolling_list {
    my($self,@p) = self_or_default(@_);
    my($name,$values,$defaults,$size,$multiple,$labels,$override,@other)
	= $self->rearrange([NAME,[VALUES,VALUE],[DEFAULTS,DEFAULT],
			    SIZE,MULTIPLE,LABELS,[OVERRIDE,FORCE]],@p);

    my($result);
    my(@values) = $values ? @$values : $self->param($name);
    $size = $size || scalar(@values);

    my(%selected) = $self->previous_or_default($name,$defaults,$override);
    my($is_multiple) = $multiple ? ' MULTIPLE' : '';
    my($has_size) = $size ? " SIZE=$size" : '';
    my($other) = @other ? " @other" : '';

    $name=$self->escapeHTML($name);
    $result = qq/<SELECT NAME="$name"$has_size$is_multiple$other>\n/;
    foreach (@values) {
	my($selectit) = $selected{$_} ? 'SELECTED' : '';
	my($label) = $_;
	$label = $labels->{$_} if defined($labels) && $labels->{$_};
	$label=$self->escapeHTML($label);
	my($value)=$self->escapeHTML($_);
	$result .= "<OPTION $selectit VALUE=\"$value\">$label\n";
    }
    $result .= "</SELECT>\n";
    $self->register_parameter($name);
    return $result;
}
END_OF_FUNC


#### Method: hidden
# Parameters:
#   $name -> Name of the hidden field
#   @default -> (optional) Initial values of field (may be an array)
#      or
#   $default->[initial values of field]
# Returns:
#   A string containing a <INPUT TYPE="hidden" NAME="name" VALUE="value">
####
'hidden' => <<'END_OF_FUNC',
sub hidden {
    my($self,@p) = self_or_default(@_);

    # this is the one place where we departed from our standard
    # calling scheme, so we have to special-case (darn)
    my(@result,@value);
    my($name,$default,$override,@other) = 
	$self->rearrange([NAME,[DEFAULT,VALUE,VALUES],[OVERRIDE,FORCE]],@p);

    my $do_override = 0;
    if ( substr($p[0],0,1) eq '-' || $self->use_named_parameters ) {
	@value = ref($default) ? @{$default} : $default;
	$do_override = $override;
    } else {
	foreach ($default,$override,@other) {
	    push(@value,$_) if defined($_);
	}
    }

    # use previous values if override is not set
    my @prev = $self->param($name);
    @value = @prev if !$do_override && @prev;

    $name=$self->escapeHTML($name);
    foreach (@value) {
	$_=$self->escapeHTML($_);
	push(@result,qq/<INPUT TYPE="hidden" NAME="$name" VALUE="$_">/);
    }
    return wantarray ? @result : join('',@result);
}
END_OF_FUNC


#### Method: image_button
# Parameters:
#   $name -> Name of the button
#   $src ->  URL of the image source
#   $align -> Alignment style (TOP, BOTTOM or MIDDLE)
# Returns:
#   A string containing a <INPUT TYPE="image" NAME="name" SRC="url" ALIGN="alignment">
####
'image_button' => <<'END_OF_FUNC',
sub image_button {
    my($self,@p) = self_or_default(@_);

    my($name,$src,$alignment,@other) =
	$self->rearrange([NAME,SRC,ALIGN],@p);

    my($align) = $alignment ? " ALIGN=\U$alignment" : '';
    my($other) = @other ? " @other" : '';
    $name=$self->escapeHTML($name);
    return qq/<INPUT TYPE="image" NAME="$name" SRC="$src"$align$other>/;
}
END_OF_FUNC


#### Method: self_url
# Returns a URL containing the current script and all its
# param/value pairs arranged as a query.  You can use this
# to create a link that, when selected, will reinvoke the
# script with all its state information preserved.
####
'self_url' => <<'END_OF_FUNC',
sub self_url {
    my($self) = self_or_default(@_);
    my($query_string) = $self->query_string;
    my $protocol = $self->protocol();
    my $name = "$protocol://" . $self->server_name;
    $name .= ":" . $self->server_port
	unless $self->server_port == 80;
    $name .= $self->script_name;
    $name .= $self->path_info if $self->path_info;
    return $name unless $query_string;
    return "$name?$query_string";
}
END_OF_FUNC


# This is provided as a synonym to self_url() for people unfortunate
# enough to have incorporated it into their programs already!
'state' => <<'END_OF_FUNC',
sub state {
    &self_url;
}
END_OF_FUNC


#### Method: url
# Like self_url, but doesn't return the query string part of
# the URL.
####
'url' => <<'END_OF_FUNC',
sub url {
    my($self) = self_or_default(@_);
    my $protocol = $self->protocol();
    my $name = "$protocol://" . $self->server_name;
    $name .= ":" . $self->server_port
	unless $self->server_port == 80;
    $name .= $self->script_name;
    return $name;
}

END_OF_FUNC

#### Method: cookie
# Set or read a cookie from the specified name.
# Cookie can then be passed to header().
# Usual rules apply to the stickiness of -value.
#  Parameters:
#   -name -> name for this cookie (optional)
#   -value -> value of this cookie (scalar, array or hash) 
#   -path -> paths for which this cookie is valid (optional)
#   -domain -> internet domain in which this cookie is valid (optional)
#   -secure -> if true, cookie only passed through secure channel (optional)
#   -expires -> expiry date in format Wdy, DD-Mon-YYYY HH:MM:SS GMT (optional)
####
'cookie' => <<'END_OF_FUNC',
# temporary, for debugging.
sub cookie {
    my($self,@p) = self_or_default(@_);
    my($name,$value,$path,$domain,$secure,$expires) =
	$self->rearrange([NAME,[VALUE,VALUES],PATH,DOMAIN,SECURE,EXPIRES],@p);


    # if no value is supplied, then we retrieve the
    # value of the cookie, if any.  For efficiency, we cache the parsed
    # cookie in our state variables.
    unless (defined($value)) {
	unless ($self->{'.cookies'}) {
	    my(@pairs) = split("; ",$self->raw_cookie);
	    foreach (@pairs) {
		my($key,$value) = split("=");
		my(@values) = map unescape($_),split('&',$value);
		$self->{'.cookies'}->{unescape($key)} = [@values];
	    }
	}

	# If no name is supplied, then retrieve the names of all our cookies.
	return () unless $self->{'.cookies'};
	return wantarray ? @{$self->{'.cookies'}->{$name}} : $self->{'.cookies'}->{$name}->[0]
	    if defined($name) && $name ne '';
	return keys %{$self->{'.cookies'}};
    }
    my(@values);

    # Pull out our parameters.
    if (ref($value)) {
	if (ref($value) eq 'ARRAY') {
	    @values = @$value;
	} elsif (ref($value) eq 'HASH') {
	    @values = %$value;
	}
    } else {
	@values = ($value);
    }
    @values = map escape($_),@values;

    # I.E. requires the path to be present.
    ($path = $ENV{'SCRIPT_NAME'})=~s![^/]+$!! unless $path;

    my(@constant_values);
    push(@constant_values,"domain=$domain") if $domain;
    push(@constant_values,"path=$path") if $path;
    push(@constant_values,"expires=".&date(&expire_calc($expires),'cookie'))
	if $expires;
    push(@constant_values,'secure') if $secure;

    my($key) = &escape($name);
    my($cookie) = join("=",$key,join("&",@values));
    return join("; ",$cookie,@constant_values);
}
END_OF_FUNC


# This internal routine creates an expires time exactly some number of
# hours from the current time.  It incorporates modifications from 
# Fisher Mark.
'expire_calc' => <<'END_OF_FUNC',
sub expire_calc {
    my($time) = @_;
    my(%mult) = ('s'=>1,
                 'm'=>60,
                 'h'=>60*60,
                 'd'=>60*60*24,
                 'M'=>60*60*24*30,
                 'y'=>60*60*24*365);
    # format for time can be in any of the forms...
    # "now" -- expire immediately
    # "+180s" -- in 180 seconds
    # "+2m" -- in 2 minutes
    # "+12h" -- in 12 hours
    # "+1d"  -- in 1 day
    # "+3M"  -- in 3 months
    # "+2y"  -- in 2 years
    # "-3m"  -- 3 minutes ago(!)
    # If you don't supply one of these forms, we assume you are
    # specifying the date yourself
    my($offset);
    if (!$time || ($time eq 'now')) {
        $offset = 0;
    } elsif ($time=~/^([+-]?\d+)([mhdMy]?)/) {
        $offset = ($mult{$2} || 1)*$1;
    } else {
        return $time;
    }
    return (time+$offset);
}
END_OF_FUNC

# This internal routine creates date strings suitable for use in
# cookies and HTTP headers.  (They differ, unfortunately.)
# Thanks to Fisher Mark for this.
'date' => <<'END_OF_FUNC',
sub date {
    my($time,$format) = @_;
    my(@MON)=qw/Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec/;
    my(@WDAY) = qw/Sun Mon Tue Wed Thu Fri Sat/;

    # pass through preformatted dates for the sake of expire_calc()
    if ("$time" =~ m/^[^0-9]/o) {
        return $time;
    }

    # make HTTP/cookie date string from GMT'ed time
    # (cookies use '-' as date separator, HTTP uses ' ')
    my($sc) = ' ';
    $sc = '-' if $format eq "cookie";
    my($sec,$min,$hour,$mday,$mon,$year,$wday) = gmtime($time);
    $year += 1900;
    return sprintf("%s, %02d$sc%s$sc%04d %02d:%02d:%02d GMT",
                   $WDAY[$wday],$mday,$MON[$mon],$year,$hour,$min,$sec);
}
END_OF_FUNC

###############################################
# OTHER INFORMATION PROVIDED BY THE ENVIRONMENT
###############################################

#### Method: path_info
# Return the extra virtual path information provided
# after the URL (if any)
####
'path_info' => <<'END_OF_FUNC',
sub path_info {
    return $ENV{'PATH_INFO'};
}
END_OF_FUNC


#### Method: request_method
# Returns 'POST', 'GET', 'PUT' or 'HEAD'
####
'request_method' => <<'END_OF_FUNC',
sub request_method {
    return $ENV{'REQUEST_METHOD'};
}
END_OF_FUNC

#### Method: path_translated
# Return the physical path information provided
# by the URL (if any)
####
'path_translated' => <<'END_OF_FUNC',
sub path_translated {
    return $ENV{'PATH_TRANSLATED'};
}
END_OF_FUNC


#### Method: query_string
# Synthesize a query string from our current
# parameters
####
'query_string' => <<'END_OF_FUNC',
sub query_string {
    my($self) = self_or_default(@_);
    my($param,$value,@pairs);
    foreach $param ($self->param) {
	my($eparam) = &escape($param);
	foreach $value ($self->param($param)) {
	    $value = &escape($value);
	    push(@pairs,"$eparam=$value");
	}
    }
    return join("&",@pairs);
}
END_OF_FUNC


#### Method: accept
# Without parameters, returns an array of the
# MIME types the browser accepts.
# With a single parameter equal to a MIME
# type, will return undef if the browser won't
# accept it, 1 if the browser accepts it but
# doesn't give a preference, or a floating point
# value between 0.0 and 1.0 if the browser
# declares a quantitative score for it.
# This handles MIME type globs correctly.
####
'accept' => <<'END_OF_FUNC',
sub accept {
    my($self,$search) = self_or_CGI(@_);
    my(%prefs,$type,$pref,$pat);
    
    my(@accept) = split(',',$self->http('accept'));

    foreach (@accept) {
	($pref) = /q=(\d\.\d+|\d+)/;
	($type) = m#(\S+/[^;]+)#;
	next unless $type;
	$prefs{$type}=$pref || 1;
    }

    return keys %prefs unless $search;
    
    # if a search type is provided, we may need to
    # perform a pattern matching operation.
    # The MIME types use a glob mechanism, which
    # is easily translated into a perl pattern match

    # First return the preference for directly supported
    # types:
    return $prefs{$search} if $prefs{$search};

    # Didn't get it, so try pattern matching.
    foreach (keys %prefs) {
	next unless /\*/;       # not a pattern match
	($pat = $_) =~ s/([^\w*])/\\$1/g; # escape meta characters
	$pat =~ s/\*/.*/g; # turn it into a pattern
	return $prefs{$_} if $search=~/$pat/;
    }
}
END_OF_FUNC


#### Method: user_agent
# If called with no parameters, returns the user agent.
# If called with one parameter, does a pattern match (case
# insensitive) on the user agent.
####
'user_agent' => <<'END_OF_FUNC',
sub user_agent {
    my($self,$match)=self_or_CGI(@_);
    return $self->http('user_agent') unless $match;
    return $self->http('user_agent') =~ /$match/i;
}
END_OF_FUNC


#### Method: cookie
# Returns the magic cookie for the session.
# To set the magic cookie for new transations,
# try print $q->header('-Set-cookie'=>'my cookie')
####
'raw_cookie' => <<'END_OF_FUNC',
sub raw_cookie {
    my($self) = self_or_CGI(@_);
    return $self->http('cookie') || $ENV{'COOKIE'} || '';
}
END_OF_FUNC

#### Method: virtual_host
# Return the name of the virtual_host, which
# is not always the same as the server
######
'virtual_host' => <<'END_OF_FUNC',
sub virtual_host {
    return http('host') || server_name();
}
END_OF_FUNC

#### Method: remote_host
# Return the name of the remote host, or its IP
# address if unavailable.  If this variable isn't
# defined, it returns "localhost" for debugging
# purposes.
####
'remote_host' => <<'END_OF_FUNC',
sub remote_host {
    return $ENV{'REMOTE_HOST'} || $ENV{'REMOTE_ADDR'} 
    || 'localhost';
}
END_OF_FUNC


#### Method: remote_addr
# Return the IP addr of the remote host.
####
'remote_addr' => <<'END_OF_FUNC',
sub remote_addr {
    return $ENV{'REMOTE_ADDR'} || '127.0.0.1';
}
END_OF_FUNC


#### Method: script_name
# Return the partial URL to this script for
# self-referencing scripts.  Also see
# self_url(), which returns a URL with all state information
# preserved.
####
'script_name' => <<'END_OF_FUNC',
sub script_name {
    return $ENV{'SCRIPT_NAME'} if $ENV{'SCRIPT_NAME'};
    # These are for debugging
    return "/$0" unless $0=~/^\//;
    return $0;
}
END_OF_FUNC


#### Method: referer
# Return the HTTP_REFERER: useful for generating
# a GO BACK button.
####
'referer' => <<'END_OF_FUNC',
sub referer {
    my($self) = self_or_CGI(@_);
    return $self->http('referer');
}
END_OF_FUNC


#### Method: server_name
# Return the name of the server
####
'server_name' => <<'END_OF_FUNC',
sub server_name {
    return $ENV{'SERVER_NAME'} || 'localhost';
}
END_OF_FUNC

#### Method: server_software
# Return the name of the server software
####
'server_software' => <<'END_OF_FUNC',
sub server_software {
    return $ENV{'SERVER_SOFTWARE'} || 'cmdline';
}
END_OF_FUNC

#### Method: server_port
# Return the tcp/ip port the server is running on
####
'server_port' => <<'END_OF_FUNC',
sub server_port {
    return $ENV{'SERVER_PORT'} || 80; # for debugging
}
END_OF_FUNC

#### Method: server_protocol
# Return the protocol (usually HTTP/1.0)
####
'server_protocol' => <<'END_OF_FUNC',
sub server_protocol {
    return $ENV{'SERVER_PROTOCOL'} || 'HTTP/1.0'; # for debugging
}
END_OF_FUNC

#### Method: http
# Return the value of an HTTP variable, or
# the list of variables if none provided
####
'http' => <<'END_OF_FUNC',
sub http {
    my ($self,$parameter) = self_or_CGI(@_);
    return $ENV{$parameter} if $parameter=~/^HTTP/;
    return $ENV{"HTTP_\U$parameter\E"} if $parameter;
    my(@p);
    foreach (keys %ENV) {
	push(@p,$_) if /^HTTP/;
    }
    return @p;
}
END_OF_FUNC

#### Method: https
# Return the value of HTTPS
####
'https' => <<'END_OF_FUNC',
sub https {
    local($^W)=0;
    my ($self,$parameter) = self_or_CGI(@_);
    return $ENV{HTTPS} unless $parameter;
    return $ENV{$parameter} if $parameter=~/^HTTPS/;
    return $ENV{"HTTPS_\U$parameter\E"} if $parameter;
    my(@p);
    foreach (keys %ENV) {
	push(@p,$_) if /^HTTPS/;
    }
    return @p;
}
END_OF_FUNC

#### Method: protocol
# Return the protocol (http or https currently)
####
'protocol' => <<'END_OF_FUNC',
sub protocol {
    local($^W)=0;
    my $self = shift;
    return 'https' if $self->https() eq 'ON'; 
    return 'https' if $self->server_port == 443;
    my $prot = $self->server_protocol;
    my($protocol,$version) = split('/',$prot);
    return "\L$protocol\E";
}
END_OF_FUNC

#### Method: remote_ident
# Return the identity of the remote user
# (but only if his host is running identd)
####
'remote_ident' => <<'END_OF_FUNC',
sub remote_ident {
    return $ENV{'REMOTE_IDENT'};
}
END_OF_FUNC


#### Method: auth_type
# Return the type of use verification/authorization in use, if any.
####
'auth_type' => <<'END_OF_FUNC',
sub auth_type {
    return $ENV{'AUTH_TYPE'};
}
END_OF_FUNC


#### Method: remote_user
# Return the authorization name used for user
# verification.
####
'remote_user' => <<'END_OF_FUNC',
sub remote_user {
    return $ENV{'REMOTE_USER'};
}
END_OF_FUNC


#### Method: user_name
# Try to return the remote user's name by hook or by
# crook
####
'user_name' => <<'END_OF_FUNC',
sub user_name {
    my ($self) = self_or_CGI(@_);
    return $self->http('from') || $ENV{'REMOTE_IDENT'} || $ENV{'REMOTE_USER'};
}
END_OF_FUNC

#### Method: nph
# Set or return the NPH global flag
####
'nph' => <<'END_OF_FUNC',
sub nph {
    my ($self,$param) = self_or_CGI(@_);
    $CGI::NPH = $param if defined($param);
    return $CGI::NPH;
}
END_OF_FUNC

#### Method: private_tempfiles
# Set or return the private_tempfiles global flag
####
'private_tempfiles' => <<'END_OF_FUNC',
sub private_tempfiles {
    my ($self,$param) = self_or_CGI(@_);
    $CGI::$PRIVATE_TEMPFILES = $param if defined($param);
    return $CGI::PRIVATE_TEMPFILES;
}
END_OF_FUNC

# -------------- really private subroutines -----------------
'previous_or_default' => <<'END_OF_FUNC',
sub previous_or_default {
    my($self,$name,$defaults,$override) = @_;
    my(%selected);

    if (!$override && ($self->{'.fieldnames'}->{$name} || 
		       defined($self->param($name)) ) ) {
	grep($selected{$_}++,$self->param($name));
    } elsif (defined($defaults) && ref($defaults) && 
	     (ref($defaults) eq 'ARRAY')) {
	grep($selected{$_}++,@{$defaults});
    } else {
	$selected{$defaults}++ if defined($defaults);
    }

    return %selected;
}
END_OF_FUNC

'register_parameter' => <<'END_OF_FUNC',
sub register_parameter {
    my($self,$param) = @_;
    $self->{'.parametersToAdd'}->{$param}++;
}
END_OF_FUNC

'get_fields' => <<'END_OF_FUNC',
sub get_fields {
    my($self) = @_;
    return $self->hidden('-name'=>'.cgifields',
			 '-values'=>[keys %{$self->{'.parametersToAdd'}}],
			 '-override'=>1);
}
END_OF_FUNC

'read_from_cmdline' => <<'END_OF_FUNC',
sub read_from_cmdline {
    require "shellwords.pl";
    my($input,@words);
    my($query_string);
    if (@ARGV) {
	$input = join(" ",@ARGV);
    } else {
	print STDERR "(offline mode: enter name=value pairs on standard input)\n";
	chomp(@lines = <>); # remove newlines
	$input = join(" ",@lines);
    }

    # minimal handling of escape characters
    $input=~s/\\=/%3D/g;
    $input=~s/\\&/%26/g;
    
    @words = &shellwords($input);
    if ("@words"=~/=/) {
	$query_string = join('&',@words);
    } else {
	$query_string = join('+',@words);
    }
    return $query_string;
}
END_OF_FUNC

#####
# subroutine: read_multipart
#
# Read multipart data and store it into our parameters.
# An interesting feature is that if any of the parts is a file, we
# create a temporary file and open up a filehandle on it so that the
# caller can read from it if necessary.
#####
'read_multipart' => <<'END_OF_FUNC',
sub read_multipart {
    my($self,$boundary,$length) = @_;
    my($buffer) = $self->new_MultipartBuffer($boundary,$length);
    return unless $buffer;
    my(%header,$body);
    while (!$buffer->eof) {
	%header = $buffer->readHeader;
	die "Malformed multipart POST\n" unless %header;

	# In beta1 it was "Content-disposition".  In beta2 it's "Content-Disposition"
	# Sheesh.
	my($key) = $header{'Content-disposition'} ? 'Content-disposition' : 'Content-Disposition';
	my($param)= $header{$key}=~/ name="([^\"]*)"/;

	# possible bug: our regular expression expects the filename= part to fall
	# at the end of the line.  Netscape doesn't escape quotation marks in file names!!!
	my($filename) = $header{$key}=~/ filename="(.*)"$/;

	# add this parameter to our list
	$self->add_parameter($param);

	# If no filename specified, then just read the data and assign it
	# to our parameter list.
	unless ($filename) {
	    my($value) = $buffer->readBody;
	    push(@{$self->{$param}},$value);
	    next;
	}

	# If we get here, then we are dealing with a potentially large
	# uploaded form.  Save the data to a temporary file, then open
	# the file for reading.
	my($tmpfile) = new TempFile;
	my $tmp = $tmpfile->as_string;
	
	# Now create a new filehandle in the caller's namespace.
	# The name of this filehandle just happens to be identical
	# to the original filename (NOT the name of the temporary
	# file, which is hidden!)
	my($filehandle);
	if ($filename=~/^[a-zA-Z_]/) {
	    my($frame,$cp)=(1);
	    do { $cp = caller($frame++); } until !eval("'$cp'->isaCGI()");
	    $filehandle = "$cp\:\:$filename";
	} else {
	    $filehandle = "\:\:$filename";
	}

        # potential security problem -- this type of line can clobber 
	# tempfile, and can be abused by malicious users.
	# open ($filehandle,">$tmp") || die "CGI open of $tmpfile: $!\n";

	# This technique causes open to fail if file already exists.
	unless (defined(&O_RDWR)) {
	    require Fcntl;
	    import Fcntl qw/O_RDWR O_CREAT O_EXCL/;
	}
	sysopen($filehandle,$tmp,&O_RDWR|&O_CREAT|&O_EXCL) || die "CGI open of $tmp: $!\n";
	unlink($tmp) if $PRIVATE_TEMPFILES;

	$CGI::DefaultClass->binmode($filehandle) if $CGI::needs_binmode;
	chmod 0600,$tmp;    # only the owner can tamper with it
	my $data;
	while (defined($data = $buffer->read)) {
	    print $filehandle $data;
	}

	seek($filehandle,0,0); #rewind file
	push(@{$self->{$param}},$filename);

	# Under Unix, it would be safe to let the temporary file
	# be deleted immediately.  However, I fear that other operating
	# systems are not so forgiving.  Therefore we save a reference
	# to the temporary file in the CGI object so that the file
	# isn't unlinked until the CGI object itself goes out of
	# scope.  This is a bit hacky, but it has the interesting side
	# effect that one can access the name of the tmpfile by
	# asking for $query->{$query->param('foo')}, where 'foo'
	# is the name of the file upload field.
	$self->{'.tmpfiles'}->{$filename}= {
	    name=>($PRIVATE_TEMPFILES ? '' : $tmpfile),
	    info=>{%header}
	}
    }
}
END_OF_FUNC

'tmpFileName' => <<'END_OF_FUNC',
sub tmpFileName {
    my($self,$filename) = self_or_default(@_);
    return $self->{'.tmpfiles'}->{$filename}->{name} ?
	$self->{'.tmpfiles'}->{$filename}->{name}->as_string
	    : '';
}
END_OF_FUNC

'uploadInfo' => <<'END_OF_FUNC'
sub uploadInfo {
    my($self,$filename) = self_or_default(@_);
    return $self->{'.tmpfiles'}->{$filename}->{info};
}
END_OF_FUNC

);
END_OF_AUTOLOAD
;

# Globals and stubs for other packages that we use
package MultipartBuffer;

# how many bytes to read at a time.  We use
# a 5K buffer by default.
$FILLUNIT = 1024 * 5;
$TIMEOUT = 10*60;       # 10 minute timeout
$SPIN_LOOP_MAX = 1000;  # bug fix for some Netscape servers
$CRLF=$CGI::CRLF;

#reuse the autoload function
*MultipartBuffer::AUTOLOAD = \&CGI::AUTOLOAD;

###############################################################################
################# THESE FUNCTIONS ARE AUTOLOADED ON DEMAND ####################
###############################################################################
$AUTOLOADED_ROUTINES = '';      # prevent -w error
$AUTOLOADED_ROUTINES=<<'END_OF_AUTOLOAD';
%SUBS =  (

'new' => <<'END_OF_FUNC',
sub new {
    my($package,$interface,$boundary,$length,$filehandle) = @_;
    my $IN;
    if ($filehandle) {
	my($package) = caller;
	# force into caller's package if necessary
	$IN = $filehandle=~/[':]/ ? $filehandle : "$package\:\:$filehandle"; 
    }
    $IN = "main::STDIN" unless $IN;

    $CGI::DefaultClass->binmode($IN) if $CGI::needs_binmode;
    
    # If the user types garbage into the file upload field,
    # then Netscape passes NOTHING to the server (not good).
    # We may hang on this read in that case. So we implement
    # a read timeout.  If nothing is ready to read
    # by then, we return.

    # Netscape seems to be a little bit unreliable
    # about providing boundary strings.
    if ($boundary) {

	# Under the MIME spec, the boundary consists of the 
	# characters "--" PLUS the Boundary string
	$boundary = "--$boundary";
	# Read the topmost (boundary) line plus the CRLF
	my($null) = '';
	$length -= $interface->read_from_client($IN,\$null,length($boundary)+2,0);
    } else { # otherwise we find it ourselves
	my($old);
	($old,$/) = ($/,$CRLF); # read a CRLF-delimited line
	$boundary = <$IN>;      # BUG: This won't work correctly under mod_perl
	$length -= length($boundary);
	chomp($boundary);               # remove the CRLF
	$/ = $old;                      # restore old line separator
    }

    my $self = {LENGTH=>$length,
		BOUNDARY=>$boundary,
		IN=>$IN,
		INTERFACE=>$interface,
		BUFFER=>'',
	    };

    $FILLUNIT = length($boundary)
	if length($boundary) > $FILLUNIT;

    return bless $self,ref $package || $package;
}
END_OF_FUNC

'readHeader' => <<'END_OF_FUNC',
sub readHeader {
    my($self) = @_;
    my($end);
    my($ok) = 0;
    my($bad) = 0;
    do {
	$self->fillBuffer($FILLUNIT);
	$ok++ if ($end = index($self->{BUFFER},"${CRLF}${CRLF}")) >= 0;
	$ok++ if $self->{BUFFER} eq '';
	$bad++ if !$ok && $self->{LENGTH} <= 0;
	$FILLUNIT *= 2 if length($self->{BUFFER}) >= $FILLUNIT; 
    } until $ok || $bad;
    return () if $bad;

    my($header) = substr($self->{BUFFER},0,$end+2);
    substr($self->{BUFFER},0,$end+4) = '';
    my %return;
    while ($header=~/^([\w-]+): (.*)$CRLF/mog) {
	$return{$1}=$2;
    }
    return %return;
}
END_OF_FUNC

# This reads and returns the body as a single scalar value.
'readBody' => <<'END_OF_FUNC',
sub readBody {
    my($self) = @_;
    my($data);
    my($returnval)='';
    while (defined($data = $self->read)) {
	$returnval .= $data;
    }
    return $returnval;
}
END_OF_FUNC

# This will read $bytes or until the boundary is hit, whichever happens
# first.  After the boundary is hit, we return undef.  The next read will
# skip over the boundary and begin reading again;
'read' => <<'END_OF_FUNC',
sub read {
    my($self,$bytes) = @_;

    # default number of bytes to read
    $bytes = $bytes || $FILLUNIT;       

    # Fill up our internal buffer in such a way that the boundary
    # is never split between reads.
    $self->fillBuffer($bytes);

    # Find the boundary in the buffer (it may not be there).
    my $start = index($self->{BUFFER},$self->{BOUNDARY});
    # protect against malformed multipart POST operations
    die "Malformed multipart POST\n" unless ($start >= 0) || ($self->{LENGTH} > 0);

    # If the boundary begins the data, then skip past it
    # and return undef.  The +2 here is a fiendish plot to
    # remove the CR/LF pair at the end of the boundary.
    if ($start == 0) {

	# clear us out completely if we've hit the last boundary.
	if (index($self->{BUFFER},"$self->{BOUNDARY}--")==0) {
	    $self->{BUFFER}='';
	    $self->{LENGTH}=0;
	    return undef;
	}

	# just remove the boundary.
	substr($self->{BUFFER},0,length($self->{BOUNDARY})+2)='';
	return undef;
    }

    my $bytesToReturn;    
    if ($start > 0) {           # read up to the boundary
	$bytesToReturn = $start > $bytes ? $bytes : $start;
    } else {    # read the requested number of bytes
	# leave enough bytes in the buffer to allow us to read
	# the boundary.  Thanks to Kevin Hendrick for finding
	# this one.
	$bytesToReturn = $bytes - (length($self->{BOUNDARY})+1);
    }

    my $returnval=substr($self->{BUFFER},0,$bytesToReturn);
    substr($self->{BUFFER},0,$bytesToReturn)='';
    
    # If we hit the boundary, remove the CRLF from the end.
    return ($start > 0) ? substr($returnval,0,-2) : $returnval;
}
END_OF_FUNC


# This fills up our internal buffer in such a way that the
# boundary is never split between reads
'fillBuffer' => <<'END_OF_FUNC',
sub fillBuffer {
    my($self,$bytes) = @_;
    return unless $self->{LENGTH};

    my($boundaryLength) = length($self->{BOUNDARY});
    my($bufferLength) = length($self->{BUFFER});
    my($bytesToRead) = $bytes - $bufferLength + $boundaryLength + 2;
    $bytesToRead = $self->{LENGTH} if $self->{LENGTH} < $bytesToRead;

    # Try to read some data.  We may hang here if the browser is screwed up.  
    my $bytesRead = $self->{INTERFACE}->read_from_client($self->{IN},
							 \$self->{BUFFER},
							 $bytesToRead,
							 $bufferLength);

    # An apparent bug in the Apache server causes the read()
    # to return zero bytes repeatedly without blocking if the
    # remote user aborts during a file transfer.  I don't know how
    # they manage this, but the workaround is to abort if we get
    # more than SPIN_LOOP_MAX consecutive zero reads.
    if ($bytesRead == 0) {
	die  "CGI.pm: Server closed socket during multipart read (client aborted?).\n"
	    if ($self->{ZERO_LOOP_COUNTER}++ >= $SPIN_LOOP_MAX);
    } else {
	$self->{ZERO_LOOP_COUNTER}=0;
    }

    $self->{LENGTH} -= $bytesRead;
}
END_OF_FUNC


# Return true when we've finished reading
'eof' => <<'END_OF_FUNC'
sub eof {
    my($self) = @_;
    return 1 if (length($self->{BUFFER}) == 0)
		 && ($self->{LENGTH} <= 0);
    undef;
}
END_OF_FUNC

);
END_OF_AUTOLOAD

####################################################################################
################################## TEMPORARY FILES #################################
####################################################################################
package TempFile;

$SL = $CGI::SL;
unless ($TMPDIRECTORY) {
    @TEMP=("${SL}usr${SL}tmp","${SL}var${SL}tmp","${SL}tmp","${SL}temp","${SL}Temporary Items");
    foreach (@TEMP) {
	do {$TMPDIRECTORY = $_; last} if -d $_ && -w _;
    }
}

$TMPDIRECTORY  = "." unless $TMPDIRECTORY;
$SEQUENCE="CGItemp${$}0000";

# cute feature, but overload implementation broke it
# %OVERLOAD = ('""'=>'as_string');
*TempFile::AUTOLOAD = \&CGI::AUTOLOAD;

###############################################################################
################# THESE FUNCTIONS ARE AUTOLOADED ON DEMAND ####################
###############################################################################
$AUTOLOADED_ROUTINES = '';      # prevent -w error
$AUTOLOADED_ROUTINES=<<'END_OF_AUTOLOAD';
%SUBS = (

'new' => <<'END_OF_FUNC',
sub new {
    my($package) = @_;
    $SEQUENCE++;
    my $directory = "${TMPDIRECTORY}${SL}${SEQUENCE}";
    return bless \$directory;
}
END_OF_FUNC

'DESTROY' => <<'END_OF_FUNC',
sub DESTROY {
    my($self) = @_;
    unlink $$self;              # get rid of the file
}
END_OF_FUNC

'as_string' => <<'END_OF_FUNC'
sub as_string {
    my($self) = @_;
    return $$self;
}
END_OF_FUNC

);
END_OF_AUTOLOAD

package CGI;

# We get a whole bunch of warnings about "possibly uninitialized variables"
# when running with the -w switch.  Touch them all once to get rid of the
# warnings.  This is ugly and I hate it.
if ($^W) {
    $CGI::CGI = '';
    $CGI::CGI=<<EOF;
    $CGI::VERSION;
    $MultipartBuffer::SPIN_LOOP_MAX;
    $MultipartBuffer::CRLF;
    $MultipartBuffer::TIMEOUT;
    $MultipartBuffer::FILLUNIT;
    $TempFile::SEQUENCE;
EOF
    ;
}

$revision;

__END__

=head1 NAME

CGI - Simple Common Gateway Interface Class

=head1 SYNOPSIS

  use CGI;
  # the rest is too complicated for a synopsis; keep reading

=head1 ABSTRACT

This perl library uses perl5 objects to make it easy to create
Web fill-out forms and parse their contents.  This package
defines CGI objects, entities that contain the values of the
current query string and other state variables.
Using a CGI object's methods, you can examine keywords and parameters
passed to your script, and create forms whose initial values
are taken from the current query (thereby preserving state
information).

The current version of CGI.pm is available at

  http://www.genome.wi.mit.edu/ftp/pub/software/WWW/cgi_docs.html
  ftp://ftp-genome.wi.mit.edu/pub/software/WWW/

=head1 INSTALLATION

CGI is a part of the base Perl installation.  However, you may need
to install a newer version someday.  Therefore:

To install this package, just change to the directory in which this
file is found and type the following:

	perl Makefile.PL
	make
	make install

This will copy CGI.pm to your perl library directory for use by all
perl scripts.  You probably must be root to do this.   Now you can
load the CGI routines in your Perl scripts with the line:

	use CGI;

If you don't have sufficient privileges to install CGI.pm in the Perl
library directory, you can put CGI.pm into some convenient spot, such
as your home directory, or in cgi-bin itself and prefix all Perl
scripts that call it with something along the lines of the following
preamble:

	use lib '/home/davis/lib';
	use CGI;

If you are using a version of perl earlier than 5.002 (such as NT perl), use
this instead:

	BEGIN {
		unshift(@INC,'/home/davis/lib');
	}
	use CGI;

The CGI distribution also comes with a cute module called L<CGI::Carp>.
It redefines the die(), warn(), confess() and croak() error routines
so that they write nicely formatted error messages into the server's
error log (or to the output stream of your choice).  This avoids long
hours of groping through the error and access logs, trying to figure
out which CGI script is generating  error messages.  If you choose,
you can even have fatal error messages echoed to the browser to avoid
the annoying and uninformative "Server Error" message.

=head1 DESCRIPTION

=head2 CREATING A NEW QUERY OBJECT:

     $query = new CGI;

This will parse the input (from both POST and GET methods) and store
it into a perl5 object called $query.  

=head2 CREATING A NEW QUERY OBJECT FROM AN INPUT FILE

     $query = new CGI(INPUTFILE);

If you provide a file handle to the new() method, it
will read parameters from the file (or STDIN, or whatever).  The
file can be in any of the forms describing below under debugging
(i.e. a series of newline delimited TAG=VALUE pairs will work).
Conveniently, this type of file is created by the save() method
(see below).  Multiple records can be saved and restored.

Perl purists will be pleased to know that this syntax accepts
references to file handles, or even references to filehandle globs,
which is the "official" way to pass a filehandle:

    $query = new CGI(\*STDIN);

You can also initialize the query object from an associative array
reference:

    $query = new CGI( {'dinosaur'=>'barney',
		       'song'=>'I love you',
		       'friends'=>[qw/Jessica George Nancy/]}
		    );

or from a properly formatted, URL-escaped query string:

    $query = new CGI('dinosaur=barney&color=purple');

To create an empty query, initialize it from an empty string or hash:

	$empty_query = new CGI("");
	     -or-
	$empty_query = new CGI({});

=head2 FETCHING A LIST OF KEYWORDS FROM THE QUERY:

     @keywords = $query->keywords

If the script was invoked as the result of an <ISINDEX> search, the
parsed keywords can be obtained as an array using the keywords() method.

=head2 FETCHING THE NAMES OF ALL THE PARAMETERS PASSED TO YOUR SCRIPT:

     @names = $query->param

If the script was invoked with a parameter list
(e.g. "name1=value1&name2=value2&name3=value3"), the param()
method will return the parameter names as a list.  If the
script was invoked as an <ISINDEX> script, there will be a
single parameter named 'keywords'.

NOTE: As of version 1.5, the array of parameter names returned will
be in the same order as they were submitted by the browser.
Usually this order is the same as the order in which the 
parameters are defined in the form (however, this isn't part
of the spec, and so isn't guaranteed).

=head2 FETCHING THE VALUE OR VALUES OF A SINGLE NAMED PARAMETER:

    @values = $query->param('foo');

	      -or-

    $value = $query->param('foo');

Pass the param() method a single argument to fetch the value of the
named parameter. If the parameter is multivalued (e.g. from multiple
selections in a scrolling list), you can ask to receive an array.  Otherwise
the method will return a single value.

=head2 SETTING THE VALUE(S) OF A NAMED PARAMETER:

    $query->param('foo','an','array','of','values');

This sets the value for the named parameter 'foo' to an array of
values.  This is one way to change the value of a field AFTER
the script has been invoked once before.  (Another way is with
the -override parameter accepted by all methods that generate
form elements.)

param() also recognizes a named parameter style of calling described
in more detail later:

    $query->param(-name=>'foo',-values=>['an','array','of','values']);

			      -or-

    $query->param(-name=>'foo',-value=>'the value');

=head2 APPENDING ADDITIONAL VALUES TO A NAMED PARAMETER:

   $query->append(-name=>;'foo',-values=>['yet','more','values']);

This adds a value or list of values to the named parameter.  The
values are appended to the end of the parameter if it already exists.
Otherwise the parameter is created.  Note that this method only
recognizes the named argument calling syntax.

=head2 IMPORTING ALL PARAMETERS INTO A NAMESPACE:

   $query->import_names('R');

This creates a series of variables in the 'R' namespace.  For example,
$R::foo, @R:foo.  For keyword lists, a variable @R::keywords will appear.
If no namespace is given, this method will assume 'Q'.
WARNING:  don't import anything into 'main'; this is a major security
risk!!!!

In older versions, this method was called B<import()>.  As of version 2.20, 
this name has been removed completely to avoid conflict with the built-in
Perl module B<import> operator.

=head2 DELETING A PARAMETER COMPLETELY:

    $query->delete('foo');

This completely clears a parameter.  It sometimes useful for
resetting parameters that you don't want passed down between
script invocations.

=head2 DELETING ALL PARAMETERS:

$query->delete_all();

This clears the CGI object completely.  It might be useful to ensure
that all the defaults are taken when you create a fill-out form.

=head2 SAVING THE STATE OF THE FORM TO A FILE:

    $query->save(FILEHANDLE)

This will write the current state of the form to the provided
filehandle.  You can read it back in by providing a filehandle
to the new() method.  Note that the filehandle can be a file, a pipe,
or whatever!

The format of the saved file is:

	NAME1=VALUE1
	NAME1=VALUE1'
	NAME2=VALUE2
	NAME3=VALUE3
	=

Both name and value are URL escaped.  Multi-valued CGI parameters are
represented as repeated names.  A session record is delimited by a
single = symbol.  You can write out multiple records and read them
back in with several calls to B<new>.  You can do this across several
sessions by opening the file in append mode, allowing you to create
primitive guest books, or to keep a history of users' queries.  Here's
a short example of creating multiple session records:

   use CGI;

   open (OUT,">>test.out") || die;
   $records = 5;
   foreach (0..$records) {
       my $q = new CGI;
       $q->param(-name=>'counter',-value=>$_);
       $q->save(OUT);
   }
   close OUT;

   # reopen for reading
   open (IN,"test.out") || die;
   while (!eof(IN)) {
       my $q = new CGI(IN);
       print $q->param('counter'),"\n";
   }

The file format used for save/restore is identical to that used by the
Whitehead Genome Center's data exchange format "Boulderio", and can be
manipulated and even databased using Boulderio utilities.  See
	
  http://www.genome.wi.mit.edu/genome_software/other/boulder.html

for further details.

=head2 CREATING A SELF-REFERENCING URL THAT PRESERVES STATE INFORMATION:

    $myself = $query->self_url;
    print "<A HREF=$myself>I'm talking to myself.</A>";

self_url() will return a URL, that, when selected, will reinvoke
this script with all its state information intact.  This is most
useful when you want to jump around within the document using
internal anchors but you don't want to disrupt the current contents
of the form(s).  Something like this will do the trick.

     $myself = $query->self_url;
     print "<A HREF=$myself#table1>See table 1</A>";
     print "<A HREF=$myself#table2>See table 2</A>";
     print "<A HREF=$myself#yourself>See for yourself</A>";

If you don't want to get the whole query string, call
the method url() to return just the URL for the script:

    $myself = $query->url;
    print "<A HREF=$myself>No query string in this baby!</A>\n";

You can also retrieve the unprocessed query string with query_string():

    $the_string = $query->query_string;

=head2 COMPATIBILITY WITH CGI-LIB.PL

To make it easier to port existing programs that use cgi-lib.pl
the compatibility routine "ReadParse" is provided.  Porting is
simple:

OLD VERSION
    require "cgi-lib.pl";
    &ReadParse;
    print "The value of the antique is $in{antique}.\n";

NEW VERSION
    use CGI;
    CGI::ReadParse
    print "The value of the antique is $in{antique}.\n";

CGI.pm's ReadParse() routine creates a tied variable named %in,
which can be accessed to obtain the query variables.  Like
ReadParse, you can also provide your own variable.  Infrequently
used features of ReadParse, such as the creation of @in and $in 
variables, are not supported.

Once you use ReadParse, you can retrieve the query object itself
this way:

    $q = $in{CGI};
    print $q->textfield(-name=>'wow',
			-value=>'does this really work?');

This allows you to start using the more interesting features
of CGI.pm without rewriting your old scripts from scratch.

=head2 CALLING CGI FUNCTIONS THAT TAKE MULTIPLE ARGUMENTS

In versions of CGI.pm prior to 2.0, it could get difficult to remember
the proper order of arguments in CGI function calls that accepted five
or six different arguments.  As of 2.0, there's a better way to pass
arguments to the various CGI functions.  In this style, you pass a
series of name=>argument pairs, like this:

   $field = $query->radio_group(-name=>'OS',
				-values=>[Unix,Windows,Macintosh],
				-default=>'Unix');

The advantages of this style are that you don't have to remember the
exact order of the arguments, and if you leave out a parameter, in
most cases it will default to some reasonable value.  If you provide
a parameter that the method doesn't recognize, it will usually do
something useful with it, such as incorporating it into the HTML form
tag.  For example if Netscape decides next week to add a new
JUSTIFICATION parameter to the text field tags, you can start using
the feature without waiting for a new version of CGI.pm:

   $field = $query->textfield(-name=>'State',
			      -default=>'gaseous',
			      -justification=>'RIGHT');

This will result in an HTML tag that looks like this:

	<INPUT TYPE="textfield" NAME="State" VALUE="gaseous"
	       JUSTIFICATION="RIGHT">

Parameter names are case insensitive: you can use -name, or -Name or
-NAME.  You don't have to use the hyphen if you don't want to.  After
creating a CGI object, call the B<use_named_parameters()> method with
a nonzero value.  This will tell CGI.pm that you intend to use named
parameters exclusively:

   $query = new CGI;
   $query->use_named_parameters(1);
   $field = $query->radio_group('name'=>'OS',
				'values'=>['Unix','Windows','Macintosh'],
				'default'=>'Unix');

Actually, CGI.pm only looks for a hyphen in the first parameter.  So
you can leave it off subsequent parameters if you like.  Something to
be wary of is the potential that a string constant like "values" will
collide with a keyword (and in fact it does!) While Perl usually
figures out when you're referring to a function and when you're
referring to a string, you probably should put quotation marks around
all string constants just to play it safe.

=head2 CREATING THE HTTP HEADER:

	print $query->header;

	     -or-

	print $query->header('image/gif');

	     -or-

	print $query->header('text/html','204 No response');

	     -or-

	print $query->header(-type=>'image/gif',
			     -nph=>1,
			     -status=>'402 Payment required',
			     -expires=>'+3d',
			     -cookie=>$cookie,
			     -Cost=>'$2.00');

header() returns the Content-type: header.  You can provide your own
MIME type if you choose, otherwise it defaults to text/html.  An
optional second parameter specifies the status code and a human-readable
message.  For example, you can specify 204, "No response" to create a
script that tells the browser to do nothing at all.  If you want to
add additional fields to the header, just tack them on to the end:

    print $query->header('text/html','200 OK','Content-Length: 3002');

The last example shows the named argument style for passing arguments
to the CGI methods using named parameters.  Recognized parameters are
B<-type>, B<-status>, B<-expires>, and B<-cookie>.  Any other 
parameters will be stripped of their initial hyphens and turned into
header fields, allowing you to specify any HTTP header you desire.

Most browsers will not cache the output from CGI scripts.  Every time
the browser reloads the page, the script is invoked anew.  You can
change this behavior with the B<-expires> parameter.  When you specify
an absolute or relative expiration interval with this parameter, some
browsers and proxy servers will cache the script's output until the
indicated expiration date.  The following forms are all valid for the
-expires field:

	+30s                              30 seconds from now
	+10m                              ten minutes from now
	+1h                               one hour from now
	-1d                               yesterday (i.e. "ASAP!")
	now                               immediately
	+3M                               in three months
	+10y                              in ten years time
	Thursday, 25-Apr-96 00:40:33 GMT  at the indicated time & date

(CGI::expires() is the static function call used internally that turns
relative time intervals into HTTP dates.  You can call it directly if
you wish.)

The B<-cookie> parameter generates a header that tells the browser to provide
a "magic cookie" during all subsequent transactions with your script.
Netscape cookies have a special format that includes interesting attributes
such as expiration time.  Use the cookie() method to create and retrieve
session cookies.

The B<-nph> parameter, if set to a true value, will issue the correct
headers to work with a NPH (no-parse-header) script.  This is important
to use with certain servers, such as Microsoft Internet Explorer, which
expect all their scripts to be NPH.

=head2 GENERATING A REDIRECTION INSTRUCTION

   print $query->redirect('http://somewhere.else/in/movie/land');

redirects the browser elsewhere.  If you use redirection like this,
you should B<not> print out a header as well.  As of version 2.0, we
produce both the unofficial Location: header and the official URI:
header.  This should satisfy most servers and browsers.

One hint I can offer is that relative links may not work correctly
when you generate a redirection to another document on your site.
This is due to a well-intentioned optimization that some servers use.
The solution to this is to use the full URL (including the http: part)
of the document you are redirecting to.

You can use named parameters:

    print $query->redirect(-uri=>'http://somewhere.else/in/movie/land',
			   -nph=>1);

The B<-nph> parameter, if set to a true value, will issue the correct
headers to work with a NPH (no-parse-header) script.  This is important
to use with certain servers, such as Microsoft Internet Explorer, which
expect all their scripts to be NPH.


=head2 CREATING THE HTML HEADER:

   print $query->start_html(-title=>'Secrets of the Pyramids',
			    -author=>'fred@capricorn.org',
			    -base=>'true',
			    -target=>'_blank',
			    -meta=>{'keywords'=>'pharaoh secret mummy',
				    'copyright'=>'copyright 1996 King Tut'},
			    -style=>{'src'=>'/styles/style1.css'},
			    -BGCOLOR=>'blue');

   -or-

   print $query->start_html('Secrets of the Pyramids',
			    'fred@capricorn.org','true',
			    'BGCOLOR="blue"');

This will return a canned HTML header and the opening <BODY> tag.  
All parameters are optional.   In the named parameter form, recognized
parameters are -title, -author, -base, -xbase and -target (see below for the
explanation).  Any additional parameters you provide, such as the
Netscape unofficial BGCOLOR attribute, are added to the <BODY> tag.

The argument B<-xbase> allows you to provide an HREF for the <BASE> tag
different from the current location, as in

    -xbase=>"http://home.mcom.com/"

All relative links will be interpreted relative to this tag.

The argument B<-target> allows you to provide a default target frame
for all the links and fill-out forms on the page.  See the Netscape
documentation on frames for details of how to manipulate this.

    -target=>"answer_window"

All relative links will be interpreted relative to this tag.
You add arbitrary meta information to the header with the B<-meta>
argument.  This argument expects a reference to an associative array
containing name/value pairs of meta information.  These will be turned
into a series of header <META> tags that look something like this:

    <META NAME="keywords" CONTENT="pharaoh secret mummy">
    <META NAME="description" CONTENT="copyright 1996 King Tut">

There is no support for the HTTP-EQUIV type of <META> tag.  This is
because you can modify the HTTP header directly with the B<header()>
method.  For example, if you want to send the Refresh: header, do it
in the header() method:

    print $q->header(-Refresh=>'10; URL=http://www.capricorn.com');

The B<-style> tag is used to incorporate cascading stylesheets into
your code.  See the section on CASCADING STYLESHEETS for more information.

You can place other arbitrary HTML elements to the <HEAD> section with the
B<-head> tag.  For example, to place the rarely-used <LINK> element in the
head section, use this:

    print $q->header(-head=>link({-rel=>'next',
				  -href=>'http://www.capricorn.com/s2.html'}));

To incorporate multiple HTML elements into the <HEAD> section, just pass an
array reference:

    print $q->header(-head=>[ link({-rel=>'next',
				    -href=>'http://www.capricorn.com/s2.html'}),
			      link({-rel=>'previous',
				    -href=>'http://www.capricorn.com/s1.html'})
			     ]
		     );


JAVASCRIPTING: The B<-script>, B<-noScript>, B<-onLoad> and B<-onUnload> parameters
are used to add Netscape JavaScript calls to your pages.  B<-script>
should point to a block of text containing JavaScript function
definitions.  This block will be placed within a <SCRIPT> block inside
the HTML (not HTTP) header.  The block is placed in the header in
order to give your page a fighting chance of having all its JavaScript
functions in place even if the user presses the stop button before the
page has loaded completely.  CGI.pm attempts to format the script in
such a way that JavaScript-naive browsers will not choke on the code:
unfortunately there are some browsers, such as Chimera for Unix, that
get confused by it nevertheless.

The B<-onLoad> and B<-onUnload> parameters point to fragments of JavaScript
code to execute when the page is respectively opened and closed by the
browser.  Usually these parameters are calls to functions defined in the
B<-script> field:

      $query = new CGI;
      print $query->header;
      $JSCRIPT=<<END;
      // Ask a silly question
      function riddle_me_this() {
	 var r = prompt("What walks on four legs in the morning, " +
		       "two legs in the afternoon, " +
		       "and three legs in the evening?");
	 response(r);
      }
      // Get a silly answer
      function response(answer) {
	 if (answer == "man")
	    alert("Right you are!");
	 else
	    alert("Wrong!  Guess again.");
      }
      END
      print $query->start_html(-title=>'The Riddle of the Sphinx',
			       -script=>$JSCRIPT);

Use the B<-noScript> parameter to pass some HTML text that will be displayed on 
browsers that do not have JavaScript (or browsers where JavaScript is turned
off).

Netscape 3.0 recognizes several attributes of the <SCRIPT> tag,
including LANGUAGE and SRC.  The latter is particularly interesting,
as it allows you to keep the JavaScript code in a file or CGI script
rather than cluttering up each page with the source.  To use these
attributes pass a HASH reference in the B<-script> parameter containing
one or more of -language, -src, or -code:

    print $q->start_html(-title=>'The Riddle of the Sphinx',
			 -script=>{-language=>'JAVASCRIPT',
                                   -src=>'/javascript/sphinx.js'}
			 );

    print $q->(-title=>'The Riddle of the Sphinx',
	       -script=>{-language=>'PERLSCRIPT'},
			 -code=>'print "hello world!\n;"'
	       );


See

   http://home.netscape.com/eng/mozilla/2.0/handbook/javascript/

for more information about JavaScript.

The old-style positional parameters are as follows:

=over 4

=item B<Parameters:>

=item 1.

The title

=item 2.

The author's e-mail address (will create a <LINK REV="MADE"> tag if present

=item 3.

A 'true' flag if you want to include a <BASE> tag in the header.  This
helps resolve relative addresses to absolute ones when the document is moved, 
but makes the document hierarchy non-portable.  Use with care!

=item 4, 5, 6...

Any other parameters you want to include in the <BODY> tag.  This is a good
place to put Netscape extensions, such as colors and wallpaper patterns.

=back

=head2 ENDING THE HTML DOCUMENT:

	print $query->end_html

This ends an HTML document by printing the </BODY></HTML> tags.

=head1 CREATING FORMS

I<General note>  The various form-creating methods all return strings
to the caller, containing the tag or tags that will create the requested
form element.  You are responsible for actually printing out these strings.
It's set up this way so that you can place formatting tags
around the form elements.

I<Another note> The default values that you specify for the forms are only
used the B<first> time the script is invoked (when there is no query
string).  On subsequent invocations of the script (when there is a query
string), the former values are used even if they are blank.  

If you want to change the value of a field from its previous value, you have two
choices:

(1) call the param() method to set it.

(2) use the -override (alias -force) parameter (a new feature in version 2.15).
This forces the default value to be used, regardless of the previous value:

   print $query->textfield(-name=>'field_name',
			   -default=>'starting value',
			   -override=>1,
			   -size=>50,
			   -maxlength=>80);

I<Yet another note> By default, the text and labels of form elements are
escaped according to HTML rules.  This means that you can safely use
"<CLICK ME>" as the label for a button.  However, it also interferes with
your ability to incorporate special HTML character sequences, such as &Aacute;,
into your fields.  If you wish to turn off automatic escaping, call the
autoEscape() method with a false value immediately after creating the CGI object:

   $query = new CGI;
   $query->autoEscape(undef);
			     

=head2 CREATING AN ISINDEX TAG

   print $query->isindex(-action=>$action);

	 -or-

   print $query->isindex($action);

Prints out an <ISINDEX> tag.  Not very exciting.  The parameter
-action specifies the URL of the script to process the query.  The
default is to process the query with the current script.

=head2 STARTING AND ENDING A FORM

    print $query->startform(-method=>$method,
			    -action=>$action,
			    -encoding=>$encoding);
      <... various form stuff ...>
    print $query->endform;

	-or-

    print $query->startform($method,$action,$encoding);
      <... various form stuff ...>
    print $query->endform;

startform() will return a <FORM> tag with the optional method,
action and form encoding that you specify.  The defaults are:
	
    method: POST
    action: this script
    encoding: application/x-www-form-urlencoded

endform() returns the closing </FORM> tag.  

Startform()'s encoding method tells the browser how to package the various
fields of the form before sending the form to the server.  Two
values are possible:

=over 4

=item B<application/x-www-form-urlencoded>

This is the older type of encoding used by all browsers prior to
Netscape 2.0.  It is compatible with many CGI scripts and is
suitable for short fields containing text data.  For your
convenience, CGI.pm stores the name of this encoding
type in B<$CGI::URL_ENCODED>.

=item B<multipart/form-data>

This is the newer type of encoding introduced by Netscape 2.0.
It is suitable for forms that contain very large fields or that
are intended for transferring binary data.  Most importantly,
it enables the "file upload" feature of Netscape 2.0 forms.  For
your convenience, CGI.pm stores the name of this encoding type
in B<$CGI::MULTIPART>

Forms that use this type of encoding are not easily interpreted
by CGI scripts unless they use CGI.pm or another library designed
to handle them.

=back

For compatibility, the startform() method uses the older form of
encoding by default.  If you want to use the newer form of encoding
by default, you can call B<start_multipart_form()> instead of
B<startform()>.

JAVASCRIPTING: The B<-name> and B<-onSubmit> parameters are provided
for use with JavaScript.  The -name parameter gives the
form a name so that it can be identified and manipulated by
JavaScript functions.  -onSubmit should point to a JavaScript
function that will be executed just before the form is submitted to your
server.  You can use this opportunity to check the contents of the form 
for consistency and completeness.  If you find something wrong, you
can put up an alert box or maybe fix things up yourself.  You can 
abort the submission by returning false from this function.  

Usually the bulk of JavaScript functions are defined in a <SCRIPT>
block in the HTML header and -onSubmit points to one of these function
call.  See start_html() for details.

=head2 CREATING A TEXT FIELD

    print $query->textfield(-name=>'field_name',
			    -default=>'starting value',
			    -size=>50,
			    -maxlength=>80);
	-or-

    print $query->textfield('field_name','starting value',50,80);

textfield() will return a text input field.  

=over 4

=item B<Parameters>

=item 1.

The first parameter is the required name for the field (-name).  

=item 2.

The optional second parameter is the default starting value for the field
contents (-default).  

=item 3.

The optional third parameter is the size of the field in
      characters (-size).

=item 4.

The optional fourth parameter is the maximum number of characters the
      field will accept (-maxlength).

=back

As with all these methods, the field will be initialized with its 
previous contents from earlier invocations of the script.
When the form is processed, the value of the text field can be
retrieved with:

       $value = $query->param('foo');

If you want to reset it from its initial value after the script has been
called once, you can do so like this:

       $query->param('foo',"I'm taking over this value!");

NEW AS OF VERSION 2.15: If you don't want the field to take on its previous
value, you can force its current value by using the -override (alias -force)
parameter:

    print $query->textfield(-name=>'field_name',
			    -default=>'starting value',
			    -override=>1,
			    -size=>50,
			    -maxlength=>80);

JAVASCRIPTING: You can also provide B<-onChange>, B<-onFocus>, B<-onBlur>
and B<-onSelect> parameters to register JavaScript event handlers.
The onChange handler will be called whenever the user changes the
contents of the text field.  You can do text validation if you like.
onFocus and onBlur are called respectively when the insertion point
moves into and out of the text field.  onSelect is called when the
user changes the portion of the text that is selected.

=head2 CREATING A BIG TEXT FIELD

   print $query->textarea(-name=>'foo',
			  -default=>'starting value',
			  -rows=>10,
			  -columns=>50);

	-or

   print $query->textarea('foo','starting value',10,50);

textarea() is just like textfield, but it allows you to specify
rows and columns for a multiline text entry box.  You can provide
a starting value for the field, which can be long and contain
multiple lines.

JAVASCRIPTING: The B<-onChange>, B<-onFocus>, B<-onBlur>
and B<-onSelect> parameters are recognized.  See textfield().

=head2 CREATING A PASSWORD FIELD

   print $query->password_field(-name=>'secret',
				-value=>'starting value',
				-size=>50,
				-maxlength=>80);
	-or-

   print $query->password_field('secret','starting value',50,80);

password_field() is identical to textfield(), except that its contents 
will be starred out on the web page.

JAVASCRIPTING: The B<-onChange>, B<-onFocus>, B<-onBlur>
and B<-onSelect> parameters are recognized.  See textfield().

=head2 CREATING A FILE UPLOAD FIELD

    print $query->filefield(-name=>'uploaded_file',
			    -default=>'starting value',
			    -size=>50,
			    -maxlength=>80);
	-or-

    print $query->filefield('uploaded_file','starting value',50,80);

filefield() will return a file upload field for Netscape 2.0 browsers.
In order to take full advantage of this I<you must use the new 
multipart encoding scheme> for the form.  You can do this either
by calling B<startform()> with an encoding type of B<$CGI::MULTIPART>,
or by calling the new method B<start_multipart_form()> instead of
vanilla B<startform()>.

=over 4

=item B<Parameters>

=item 1.

The first parameter is the required name for the field (-name).  

=item 2.

The optional second parameter is the starting value for the field contents
to be used as the default file name (-default).

The beta2 version of Netscape 2.0 currently doesn't pay any attention
to this field, and so the starting value will always be blank.  Worse,
the field loses its "sticky" behavior and forgets its previous
contents.  The starting value field is called for in the HTML
specification, however, and possibly later versions of Netscape will
honor it.

=item 3.

The optional third parameter is the size of the field in
characters (-size).

=item 4.

The optional fourth parameter is the maximum number of characters the
field will accept (-maxlength).

=back

When the form is processed, you can retrieve the entered filename
by calling param().

       $filename = $query->param('uploaded_file');

In Netscape Gold, the filename that gets returned is the full local filename
on the B<remote user's> machine.  If the remote user is on a Unix
machine, the filename will follow Unix conventions:

	/path/to/the/file

On an MS-DOS/Windows and OS/2 machines, the filename will follow DOS conventions:

	C:\PATH\TO\THE\FILE.MSW

On a Macintosh machine, the filename will follow Mac conventions:

	HD 40:Desktop Folder:Sort Through:Reminders

The filename returned is also a file handle.  You can read the contents
of the file using standard Perl file reading calls:

	# Read a text file and print it out
	while (<$filename>) {
	   print;
	}

	# Copy a binary file to somewhere safe
	open (OUTFILE,">>/usr/local/web/users/feedback");
	while ($bytesread=read($filename,$buffer,1024)) {
	   print OUTFILE $buffer;
	}

When a file is uploaded the browser usually sends along some
information along with it in the format of headers.  The information
usually includes the MIME content type.  Future browsers may send
other information as well (such as modification date and size). To
retrieve this information, call uploadInfo().  It returns a reference to
an associative array containing all the document headers.

       $filename = $query->param('uploaded_file');
       $type = $query->uploadInfo($filename)->{'Content-Type'};
       unless ($type eq 'text/html') {
	  die "HTML FILES ONLY!";
       }

If you are using a machine that recognizes "text" and "binary" data
modes, be sure to understand when and how to use them (see the Camel book).  
Otherwise you may find that binary files are corrupted during file uploads.

JAVASCRIPTING: The B<-onChange>, B<-onFocus>, B<-onBlur>
and B<-onSelect> parameters are recognized.  See textfield()
for details. 

=head2 CREATING A POPUP MENU

   print $query->popup_menu('menu_name',
			    ['eenie','meenie','minie'],
			    'meenie');

      -or-

   %labels = ('eenie'=>'your first choice',
	      'meenie'=>'your second choice',
	      'minie'=>'your third choice');
   print $query->popup_menu('menu_name',
			    ['eenie','meenie','minie'],
			    'meenie',\%labels);

	-or (named parameter style)-

   print $query->popup_menu(-name=>'menu_name',
			    -values=>['eenie','meenie','minie'],
			    -default=>'meenie',
			    -labels=>\%labels);

popup_menu() creates a menu.

=over 4

=item 1.

The required first argument is the menu's name (-name).

=item 2.

The required second argument (-values) is an array B<reference>
containing the list of menu items in the menu.  You can pass the
method an anonymous array, as shown in the example, or a reference to
a named array, such as "\@foo".

=item 3.

The optional third parameter (-default) is the name of the default
menu choice.  If not specified, the first item will be the default.
The values of the previous choice will be maintained across queries.

=item 4.

The optional fourth parameter (-labels) is provided for people who
want to use different values for the user-visible label inside the
popup menu nd the value returned to your script.  It's a pointer to an
associative array relating menu values to user-visible labels.  If you
leave this parameter blank, the menu values will be displayed by
default.  (You can also leave a label undefined if you want to).

=back

When the form is processed, the selected value of the popup menu can
be retrieved using:

      $popup_menu_value = $query->param('menu_name');

JAVASCRIPTING: popup_menu() recognizes the following event handlers:
B<-onChange>, B<-onFocus>, and B<-onBlur>.  See the textfield()
section for details on when these handlers are called.

=head2 CREATING A SCROLLING LIST

   print $query->scrolling_list('list_name',
				['eenie','meenie','minie','moe'],
				['eenie','moe'],5,'true');
      -or-

   print $query->scrolling_list('list_name',
				['eenie','meenie','minie','moe'],
				['eenie','moe'],5,'true',
				\%labels);

	-or-

   print $query->scrolling_list(-name=>'list_name',
				-values=>['eenie','meenie','minie','moe'],
				-default=>['eenie','moe'],
				-size=>5,
				-multiple=>'true',
				-labels=>\%labels);

scrolling_list() creates a scrolling list.  

=over 4

=item B<Parameters:>

=item 1.

The first and second arguments are the list name (-name) and values
(-values).  As in the popup menu, the second argument should be an
array reference.

=item 2.

The optional third argument (-default) can be either a reference to a
list containing the values to be selected by default, or can be a
single value to select.  If this argument is missing or undefined,
then nothing is selected when the list first appears.  In the named
parameter version, you can use the synonym "-defaults" for this
parameter.

=item 3.

The optional fourth argument is the size of the list (-size).

=item 4.

The optional fifth argument can be set to true to allow multiple
simultaneous selections (-multiple).  Otherwise only one selection
will be allowed at a time.

=item 5.

The optional sixth argument is a pointer to an associative array
containing long user-visible labels for the list items (-labels).
If not provided, the values will be displayed.

When this form is processed, all selected list items will be returned as
a list under the parameter name 'list_name'.  The values of the
selected items can be retrieved with:

      @selected = $query->param('list_name');

=back

JAVASCRIPTING: scrolling_list() recognizes the following event handlers:
B<-onChange>, B<-onFocus>, and B<-onBlur>.  See textfield() for
the description of when these handlers are called.

=head2 CREATING A GROUP OF RELATED CHECKBOXES

   print $query->checkbox_group(-name=>'group_name',
				-values=>['eenie','meenie','minie','moe'],
				-default=>['eenie','moe'],
				-linebreak=>'true',
				-labels=>\%labels);

   print $query->checkbox_group('group_name',
				['eenie','meenie','minie','moe'],
				['eenie','moe'],'true',\%labels);

   HTML3-COMPATIBLE BROWSERS ONLY:

   print $query->checkbox_group(-name=>'group_name',
				-values=>['eenie','meenie','minie','moe'],
				-rows=2,-columns=>2);
    

checkbox_group() creates a list of checkboxes that are related
by the same name.

=over 4

=item B<Parameters:>

=item 1.

The first and second arguments are the checkbox name and values,
respectively (-name and -values).  As in the popup menu, the second
argument should be an array reference.  These values are used for the
user-readable labels printed next to the checkboxes as well as for the
values passed to your script in the query string.

=item 2.

The optional third argument (-default) can be either a reference to a
list containing the values to be checked by default, or can be a
single value to checked.  If this argument is missing or undefined,
then nothing is selected when the list first appears.

=item 3.

The optional fourth argument (-linebreak) can be set to true to place
line breaks between the checkboxes so that they appear as a vertical
list.  Otherwise, they will be strung together on a horizontal line.

=item 4.

The optional fifth argument is a pointer to an associative array
relating the checkbox values to the user-visible labels that will
be printed next to them (-labels).  If not provided, the values will
be used as the default.

=item 5.

B<HTML3-compatible browsers> (such as Netscape) can take advantage 
of the optional 
parameters B<-rows>, and B<-columns>.  These parameters cause
checkbox_group() to return an HTML3 compatible table containing
the checkbox group formatted with the specified number of rows
and columns.  You can provide just the -columns parameter if you
wish; checkbox_group will calculate the correct number of rows
for you.

To include row and column headings in the returned table, you
can use the B<-rowheader> and B<-colheader> parameters.  Both
of these accept a pointer to an array of headings to use.
The headings are just decorative.  They don't reorganize the
interpretation of the checkboxes -- they're still a single named
unit.

=back

When the form is processed, all checked boxes will be returned as
a list under the parameter name 'group_name'.  The values of the
"on" checkboxes can be retrieved with:

      @turned_on = $query->param('group_name');

The value returned by checkbox_group() is actually an array of button
elements.  You can capture them and use them within tables, lists,
or in other creative ways:

    @h = $query->checkbox_group(-name=>'group_name',-values=>\@values);
    &use_in_creative_way(@h);

JAVASCRIPTING: checkbox_group() recognizes the B<-onClick>
parameter.  This specifies a JavaScript code fragment or
function call to be executed every time the user clicks on
any of the buttons in the group.  You can retrieve the identity
of the particular button clicked on using the "this" variable.

=head2 CREATING A STANDALONE CHECKBOX

    print $query->checkbox(-name=>'checkbox_name',
			   -checked=>'checked',
			   -value=>'ON',
			   -label=>'CLICK ME');

	-or-

    print $query->checkbox('checkbox_name','checked','ON','CLICK ME');

checkbox() is used to create an isolated checkbox that isn't logically
related to any others.

=over 4

=item B<Parameters:>

=item 1.

The first parameter is the required name for the checkbox (-name).  It
will also be used for the user-readable label printed next to the
checkbox.

=item 2.

The optional second parameter (-checked) specifies that the checkbox
is turned on by default.  Synonyms are -selected and -on.

=item 3.

The optional third parameter (-value) specifies the value of the
checkbox when it is checked.  If not provided, the word "on" is
assumed.

=item 4.

The optional fourth parameter (-label) is the user-readable label to
be attached to the checkbox.  If not provided, the checkbox name is
used.

=back

The value of the checkbox can be retrieved using:

    $turned_on = $query->param('checkbox_name');

JAVASCRIPTING: checkbox() recognizes the B<-onClick>
parameter.  See checkbox_group() for further details.

=head2 CREATING A RADIO BUTTON GROUP

   print $query->radio_group(-name=>'group_name',
			     -values=>['eenie','meenie','minie'],
			     -default=>'meenie',
			     -linebreak=>'true',
			     -labels=>\%labels);

	-or-

   print $query->radio_group('group_name',['eenie','meenie','minie'],
					  'meenie','true',\%labels);


   HTML3-COMPATIBLE BROWSERS ONLY:

   print $query->radio_group(-name=>'group_name',
			     -values=>['eenie','meenie','minie','moe'],
			     -rows=2,-columns=>2);

radio_group() creates a set of logically-related radio buttons
(turning one member of the group on turns the others off)

=over 4

=item B<Parameters:>

=item 1.

The first argument is the name of the group and is required (-name).

=item 2.

The second argument (-values) is the list of values for the radio
buttons.  The values and the labels that appear on the page are
identical.  Pass an array I<reference> in the second argument, either
using an anonymous array, as shown, or by referencing a named array as
in "\@foo".

=item 3.

The optional third parameter (-default) is the name of the default
button to turn on. If not specified, the first item will be the
default.  You can provide a nonexistent button name, such as "-" to
start up with no buttons selected.

=item 4.

The optional fourth parameter (-linebreak) can be set to 'true' to put
line breaks between the buttons, creating a vertical list.

=item 5.

The optional fifth parameter (-labels) is a pointer to an associative
array relating the radio button values to user-visible labels to be
used in the display.  If not provided, the values themselves are
displayed.

=item 6.

B<HTML3-compatible browsers> (such as Netscape) can take advantage 
of the optional 
parameters B<-rows>, and B<-columns>.  These parameters cause
radio_group() to return an HTML3 compatible table containing
the radio group formatted with the specified number of rows
and columns.  You can provide just the -columns parameter if you
wish; radio_group will calculate the correct number of rows
for you.

To include row and column headings in the returned table, you
can use the B<-rowheader> and B<-colheader> parameters.  Both
of these accept a pointer to an array of headings to use.
The headings are just decorative.  They don't reorganize the
interpetation of the radio buttons -- they're still a single named
unit.

=back

When the form is processed, the selected radio button can
be retrieved using:

      $which_radio_button = $query->param('group_name');

The value returned by radio_group() is actually an array of button
elements.  You can capture them and use them within tables, lists,
or in other creative ways:

    @h = $query->radio_group(-name=>'group_name',-values=>\@values);
    &use_in_creative_way(@h);

=head2 CREATING A SUBMIT BUTTON 

   print $query->submit(-name=>'button_name',
			-value=>'value');

	-or-

   print $query->submit('button_name','value');

submit() will create the query submission button.  Every form
should have one of these.

=over 4

=item B<Parameters:>

=item 1.

The first argument (-name) is optional.  You can give the button a
name if you have several submission buttons in your form and you want
to distinguish between them.  The name will also be used as the
user-visible label.  Be aware that a few older browsers don't deal with this correctly and
B<never> send back a value from a button.

=item 2.

The second argument (-value) is also optional.  This gives the button
a value that will be passed to your script in the query string.

=back

You can figure out which button was pressed by using different
values for each one:

     $which_one = $query->param('button_name');

JAVASCRIPTING: radio_group() recognizes the B<-onClick>
parameter.  See checkbox_group() for further details.

=head2 CREATING A RESET BUTTON

   print $query->reset

reset() creates the "reset" button.  Note that it restores the
form to its value from the last time the script was called, 
NOT necessarily to the defaults.

=head2 CREATING A DEFAULT BUTTON

   print $query->defaults('button_label')

defaults() creates a button that, when invoked, will cause the
form to be completely reset to its defaults, wiping out all the
changes the user ever made.

=head2 CREATING A HIDDEN FIELD

	print $query->hidden(-name=>'hidden_name',
			     -default=>['value1','value2'...]);

		-or-

	print $query->hidden('hidden_name','value1','value2'...);

hidden() produces a text field that can't be seen by the user.  It
is useful for passing state variable information from one invocation
of the script to the next.

=over 4

=item B<Parameters:>

=item 1.

The first argument is required and specifies the name of this
field (-name).

=item 2.  

The second argument is also required and specifies its value
(-default).  In the named parameter style of calling, you can provide
a single value here or a reference to a whole list

=back

Fetch the value of a hidden field this way:

     $hidden_value = $query->param('hidden_name');

Note, that just like all the other form elements, the value of a
hidden field is "sticky".  If you want to replace a hidden field with
some other values after the script has been called once you'll have to
do it manually:

     $query->param('hidden_name','new','values','here');

=head2 CREATING A CLICKABLE IMAGE BUTTON

     print $query->image_button(-name=>'button_name',
				-src=>'/source/URL',
				-align=>'MIDDLE');      

	-or-

     print $query->image_button('button_name','/source/URL','MIDDLE');

image_button() produces a clickable image.  When it's clicked on the
position of the click is returned to your script as "button_name.x"
and "button_name.y", where "button_name" is the name you've assigned
to it.

JAVASCRIPTING: image_button() recognizes the B<-onClick>
parameter.  See checkbox_group() for further details.

=over 4

=item B<Parameters:>

=item 1.

The first argument (-name) is required and specifies the name of this
field.

=item 2.

The second argument (-src) is also required and specifies the URL

=item 3.
The third option (-align, optional) is an alignment type, and may be
TOP, BOTTOM or MIDDLE

=back

Fetch the value of the button this way:
     $x = $query->param('button_name.x');
     $y = $query->param('button_name.y');

=head2 CREATING A JAVASCRIPT ACTION BUTTON

     print $query->button(-name=>'button_name',
			  -value=>'user visible label',
			  -onClick=>"do_something()");

	-or-

     print $query->button('button_name',"do_something()");

button() produces a button that is compatible with Netscape 2.0's
JavaScript.  When it's pressed the fragment of JavaScript code
pointed to by the B<-onClick> parameter will be executed.  On
non-Netscape browsers this form element will probably not even
display.

=head1 NETSCAPE COOKIES

Netscape browsers versions 1.1 and higher support a so-called
"cookie" designed to help maintain state within a browser session.
CGI.pm has several methods that support cookies.

A cookie is a name=value pair much like the named parameters in a CGI
query string.  CGI scripts create one or more cookies and send
them to the browser in the HTTP header.  The browser maintains a list
of cookies that belong to a particular Web server, and returns them
to the CGI script during subsequent interactions.

In addition to the required name=value pair, each cookie has several
optional attributes:

=over 4

=item 1. an expiration time

This is a time/date string (in a special GMT format) that indicates
when a cookie expires.  The cookie will be saved and returned to your
script until this expiration date is reached if the user exits
Netscape and restarts it.  If an expiration date isn't specified, the cookie
will remain active until the user quits Netscape.

=item 2. a domain

This is a partial or complete domain name for which the cookie is 
valid.  The browser will return the cookie to any host that matches
the partial domain name.  For example, if you specify a domain name
of ".capricorn.com", then Netscape will return the cookie to
Web servers running on any of the machines "www.capricorn.com", 
"www2.capricorn.com", "feckless.capricorn.com", etc.  Domain names
must contain at least two periods to prevent attempts to match
on top level domains like ".edu".  If no domain is specified, then
the browser will only return the cookie to servers on the host the
cookie originated from.

=item 3. a path

If you provide a cookie path attribute, the browser will check it
against your script's URL before returning the cookie.  For example,
if you specify the path "/cgi-bin", then the cookie will be returned
to each of the scripts "/cgi-bin/tally.pl", "/cgi-bin/order.pl",
and "/cgi-bin/customer_service/complain.pl", but not to the script
"/cgi-private/site_admin.pl".  By default, path is set to "/", which
causes the cookie to be sent to any CGI script on your site.

=item 4. a "secure" flag

If the "secure" attribute is set, the cookie will only be sent to your
script if the CGI request is occurring on a secure channel, such as SSL.

=back

The interface to Netscape cookies is the B<cookie()> method:

    $cookie = $query->cookie(-name=>'sessionID',
			     -value=>'xyzzy',
			     -expires=>'+1h',
			     -path=>'/cgi-bin/database',
			     -domain=>'.capricorn.org',
			     -secure=>1);
    print $query->header(-cookie=>$cookie);

B<cookie()> creates a new cookie.  Its parameters include:

=over 4

=item B<-name>

The name of the cookie (required).  This can be any string at all.
Although Netscape limits its cookie names to non-whitespace
alphanumeric characters, CGI.pm removes this restriction by escaping
and unescaping cookies behind the scenes.

=item B<-value>

The value of the cookie.  This can be any scalar value,
array reference, or even associative array reference.  For example,
you can store an entire associative array into a cookie this way:

	$cookie=$query->cookie(-name=>'family information',
			       -value=>\%childrens_ages);

=item B<-path>

The optional partial path for which this cookie will be valid, as described
above.

=item B<-domain>

The optional partial domain for which this cookie will be valid, as described
above.

=item B<-expires>

The optional expiration date for this cookie.  The format is as described 
in the section on the B<header()> method:

	"+1h"  one hour from now

=item B<-secure>

If set to true, this cookie will only be used within a secure
SSL session.

=back

The cookie created by cookie() must be incorporated into the HTTP
header within the string returned by the header() method:

	print $query->header(-cookie=>$my_cookie);

To create multiple cookies, give header() an array reference:

	$cookie1 = $query->cookie(-name=>'riddle_name',
				  -value=>"The Sphynx's Question");
	$cookie2 = $query->cookie(-name=>'answers',
				  -value=>\%answers);
	print $query->header(-cookie=>[$cookie1,$cookie2]);

To retrieve a cookie, request it by name by calling cookie()
method without the B<-value> parameter:

	use CGI;
	$query = new CGI;
	%answers = $query->cookie(-name=>'answers');
	# $query->cookie('answers') will work too!

The cookie and CGI namespaces are separate.  If you have a parameter
named 'answers' and a cookie named 'answers', the values retrieved by
param() and cookie() are independent of each other.  However, it's
simple to turn a CGI parameter into a cookie, and vice-versa:

   # turn a CGI parameter into a cookie
   $c=$q->cookie(-name=>'answers',-value=>[$q->param('answers')]);
   # vice-versa
   $q->param(-name=>'answers',-value=>[$q->cookie('answers')]);

See the B<cookie.cgi> example script for some ideas on how to use
cookies effectively.

B<NOTE:> There appear to be some (undocumented) restrictions on
Netscape cookies.  In Netscape 2.01, at least, I haven't been able to
set more than three cookies at a time.  There may also be limits on
the length of cookies.  If you need to store a lot of information,
it's probably better to create a unique session ID, store it in a
cookie, and use the session ID to locate an external file/database
saved on the server's side of the connection.

=head1 WORKING WITH NETSCAPE FRAMES

It's possible for CGI.pm scripts to write into several browser
panels and windows using Netscape's frame mechanism.  
There are three techniques for defining new frames programmatically:

=over 4

=item 1. Create a <Frameset> document

After writing out the HTTP header, instead of creating a standard
HTML document using the start_html() call, create a <FRAMESET> 
document that defines the frames on the page.  Specify your script(s)
(with appropriate parameters) as the SRC for each of the frames.

There is no specific support for creating <FRAMESET> sections 
in CGI.pm, but the HTML is very simple to write.  See the frame
documentation in Netscape's home pages for details 

  http://home.netscape.com/assist/net_sites/frames.html

=item 2. Specify the destination for the document in the HTTP header

You may provide a B<-target> parameter to the header() method:
   
    print $q->header(-target=>'ResultsWindow');

This will tell Netscape to load the output of your script into the
frame named "ResultsWindow".  If a frame of that name doesn't
already exist, Netscape will pop up a new window and load your
script's document into that.  There are a number of magic names
that you can use for targets.  See the frame documents on Netscape's
home pages for details.

=item 3. Specify the destination for the document in the <FORM> tag

You can specify the frame to load in the FORM tag itself.  With
CGI.pm it looks like this:

    print $q->startform(-target=>'ResultsWindow');

When your script is reinvoked by the form, its output will be loaded
into the frame named "ResultsWindow".  If one doesn't already exist
a new window will be created.

=back

The script "frameset.cgi" in the examples directory shows one way to
create pages in which the fill-out form and the response live in
side-by-side frames.

=head1 LIMITED SUPPORT FOR CASCADING STYLE SHEETS

CGI.pm has limited support for HTML3's cascading style sheets (css).
To incorporate a stylesheet into your document, pass the
start_html() method a B<-style> parameter.  The value of this
parameter may be a scalar, in which case it is incorporated directly
into a <STYLE> section, or it may be a hash reference.  In the latter
case you should provide the hash with one or more of B<-src> or
B<-code>.  B<-src> points to a URL where an externally-defined
stylesheet can be found.  B<-code> points to a scalar value to be
incorporated into a <STYLE> section.  Style definitions in B<-code>
override similarly-named ones in B<-src>, hence the name "cascading."

To refer to a style within the body of your document, add the
B<-class> parameter to any HTML element:

    print h1({-class=>'Fancy'},'Welcome to the Party');

Or define styles on the fly with the B<-style> parameter:

    print h1({-style=>'Color: red;'},'Welcome to Hell');

You may also use the new B<span()> element to apply a style to a
section of text:

    print span({-style=>'Color: red;'},
	       h1('Welcome to Hell'),
	       "Where did that handbasket get to?"
	       );

Note that you must import the ":html3" definitions to have the
B<span()> method available.  Here's a quick and dirty example of using
CSS's.  See the CSS specification at
http://www.w3.org/pub/WWW/TR/Wd-css-1.html for more information.

    use CGI qw/:standard :html3/;

    #here's a stylesheet incorporated directly into the page
    $newStyle=<<END;
    <!-- 
    P.Tip {
	margin-right: 50pt;
	margin-left: 50pt;
        color: red;
    }
    P.Alert {
	font-size: 30pt;
        font-family: sans-serif;
      color: red;
    }
    -->
    END
    print header();
    print start_html( -title=>'CGI with Style',
		      -style=>{-src=>'http://www.capricorn.com/style/st1.css',
		               -code=>$newStyle}
	             );
    print h1('CGI with Style'),
          p({-class=>'Tip'},
	    "Better read the cascading style sheet spec before playing with this!"),
          span({-style=>'color: magenta'},
	       "Look Mom, no hands!",
	       p(),
	       "Whooo wee!"
	       );
    print end_html;

=head1 DEBUGGING

If you are running the script
from the command line or in the perl debugger, you can pass the script
a list of keywords or parameter=value pairs on the command line or 
from standard input (you don't have to worry about tricking your
script into reading from environment variables).
You can pass keywords like this:

    your_script.pl keyword1 keyword2 keyword3

or this:

   your_script.pl keyword1+keyword2+keyword3

or this:

    your_script.pl name1=value1 name2=value2

or this:

    your_script.pl name1=value1&name2=value2

or even as newline-delimited parameters on standard input.

When debugging, you can use quotes and backslashes to escape 
characters in the familiar shell manner, letting you place
spaces and other funny characters in your parameter=value
pairs:

   your_script.pl "name1='I am a long value'" "name2=two\ words"

=head2 DUMPING OUT ALL THE NAME/VALUE PAIRS

The dump() method produces a string consisting of all the query's
name/value pairs formatted nicely as a nested list.  This is useful
for debugging purposes:

    print $query->dump
    

Produces something that looks like:

    <UL>
    <LI>name1
	<UL>
	<LI>value1
	<LI>value2
	</UL>
    <LI>name2
	<UL>
	<LI>value1
	</UL>
    </UL>

You can pass a value of 'true' to dump() in order to get it to
print the results out as plain text, suitable for incorporating
into a <PRE> section.

As a shortcut, as of version 1.56 you can interpolate the entire CGI
object into a string and it will be replaced with the a nice HTML dump
shown above:

    $query=new CGI;
    print "<H2>Current Values</H2> $query\n";

=head1 FETCHING ENVIRONMENT VARIABLES

Some of the more useful environment variables can be fetched
through this interface.  The methods are as follows:

=over 4

=item B<accept()>

Return a list of MIME types that the remote browser
accepts. If you give this method a single argument
corresponding to a MIME type, as in
$query->accept('text/html'), it will return a
floating point value corresponding to the browser's
preference for this type from 0.0 (don't want) to 1.0.
Glob types (e.g. text/*) in the browser's accept list
are handled correctly.

=item B<raw_cookie()>

Returns the HTTP_COOKIE variable, an HTTP extension
implemented by Netscape browsers version 1.1
and higher.  Cookies have a special format, and this 
method call just returns the raw form (?cookie dough).
See cookie() for ways of setting and retrieving
cooked cookies.

=item B<user_agent()>

Returns the HTTP_USER_AGENT variable.  If you give
this method a single argument, it will attempt to
pattern match on it, allowing you to do something
like $query->user_agent(netscape);

=item B<path_info()>

Returns additional path information from the script URL.
E.G. fetching /cgi-bin/your_script/additional/stuff will
result in $query->path_info() returning
"additional/stuff".

NOTE: The Microsoft Internet Information Server
is broken with respect to additional path information.  If
you use the Perl DLL library, the IIS server will attempt to
execute the additional path information as a Perl script.
If you use the ordinary file associations mapping, the
path information will be present in the environment, 
but incorrect.  The best thing to do is to avoid using additional
path information in CGI scripts destined for use with IIS.

=item B<path_translated()>

As per path_info() but returns the additional
path information translated into a physical path, e.g.
"/usr/local/etc/httpd/htdocs/additional/stuff".

The Microsoft IIS is broken with respect to the translated
path as well.

=item B<remote_host()>

Returns either the remote host name or IP address.
if the former is unavailable.

=item B<script_name()>
Return the script name as a partial URL, for self-refering
scripts.

=item B<referer()>

Return the URL of the page the browser was viewing
prior to fetching your script.  Not available for all
browsers.

=item B<auth_type ()>

Return the authorization/verification method in use for this
script, if any.

=item B<server_name ()>

Returns the name of the server, usually the machine's host
name.

=item B<virtual_host ()>

When using virtual hosts, returns the name of the host that
the browser attempted to contact

=item B<server_software ()>

Returns the server software and version number.

=item B<remote_user ()>

Return the authorization/verification name used for user
verification, if this script is protected.

=item B<user_name ()>

Attempt to obtain the remote user's name, using a variety
of different techniques.  This only works with older browsers
such as Mosaic.  Netscape does not reliably report the user
name!

=item B<request_method()>

Returns the method used to access your script, usually
one of 'POST', 'GET' or 'HEAD'.

=back

=head1 CREATING HTML ELEMENTS

In addition to its shortcuts for creating form elements, CGI.pm
defines general HTML shortcut methods as well.  HTML shortcuts are
named after a single HTML element and return a fragment of HTML text
that you can then print or manipulate as you like.

This example shows how to use the HTML methods:

	$q = new CGI;
	print $q->blockquote(
			     "Many years ago on the island of",
			     $q->a({href=>"http://crete.org/"},"Crete"),
			     "there lived a minotaur named",
			     $q->strong("Fred."),
			    ),
	       $q->hr;

This results in the following HTML code (extra newlines have been
added for readability):

	<blockquote>
	Many years ago on the island of
	<a HREF="http://crete.org/">Crete</a> there lived
	a minotaur named <strong>Fred.</strong> 
	</blockquote>
	<hr>

If you find the syntax for calling the HTML shortcuts awkward, you can
import them into your namespace and dispense with the object syntax
completely (see the next section for more details):

	use CGI shortcuts;      # IMPORT HTML SHORTCUTS
	print blockquote(
		     "Many years ago on the island of",
		     a({href=>"http://crete.org/"},"Crete"),
		     "there lived a minotaur named",
		     strong("Fred."),
		     ),
	       hr;

=head2 PROVIDING ARGUMENTS TO HTML SHORTCUTS

The HTML methods will accept zero, one or multiple arguments.  If you
provide no arguments, you get a single tag:

	print hr;  
	#  gives "<hr>"

If you provide one or more string arguments, they are concatenated
together with spaces and placed between opening and closing tags:

	print h1("Chapter","1"); 
	# gives "<h1>Chapter 1</h1>"

If the first argument is an associative array reference, then the keys
and values of the associative array become the HTML tag's attributes:

	print a({href=>'fred.html',target=>'_new'},
		"Open a new frame");
	# gives <a href="fred.html",target="_new">Open a new frame</a>

You are free to use CGI.pm-style dashes in front of the attribute
names if you prefer:

	print img {-src=>'fred.gif',-align=>'LEFT'};
	# gives <img ALIGN="LEFT" SRC="fred.gif">

=head2 Generating new HTML tags

Since no mere mortal can keep up with Netscape and Microsoft as they
battle it out for control of HTML, the code that generates HTML tags
is general and extensible.  You can create new HTML tags freely just
by referring to them on the import line:

	use CGI shortcuts,winkin,blinkin,nod;

Now, in addition to the standard CGI shortcuts, you've created HTML
tags named "winkin", "blinkin" and "nod".  You can use them like this:

	print blinkin {color=>'blue',rate=>'fast'},"Yahoo!";
	# <blinkin COLOR="blue" RATE="fast">Yahoo!</blinkin>

=head1 IMPORTING CGI METHOD CALLS INTO YOUR NAME SPACE

As a convenience, you can import most of the CGI method calls directly
into your name space.  The syntax for doing this is:

	use CGI <list of methods>;

The listed methods will be imported into the current package; you can
call them directly without creating a CGI object first.  This example
shows how to import the B<param()> and B<header()>
methods, and then use them directly:

	use CGI param,header;
	print header('text/plain');
	$zipcode = param('zipcode');

You can import groups of methods by referring to a number of special
names:

=over 4

=item B<cgi>

Import all CGI-handling methods, such as B<param()>, B<path_info()>
and the like.

=item B<form>

Import all fill-out form generating methods, such as B<textfield()>.

=item B<html2>

Import all methods that generate HTML 2.0 standard elements.

=item B<html3>

Import all methods that generate HTML 3.0 proposed elements (such as
<table>, <super> and <sub>).

=item B<netscape>

Import all methods that generate Netscape-specific HTML extensions.

=item B<shortcuts>

Import all HTML-generating shortcuts (i.e. 'html2' + 'html3' +
'netscape')...

=item B<standard>

Import "standard" features, 'html2', 'form' and 'cgi'.

=item B<all>

Import all the available methods.  For the full list, see the CGI.pm
code, where the variable %TAGS is defined.

=back

Note that in the interests of execution speed CGI.pm does B<not> use
the standard L<Exporter> syntax for specifying load symbols.  This may
change in the future.

If you import any of the state-maintaining CGI or form-generating
methods, a default CGI object will be created and initialized
automatically the first time you use any of the methods that require
one to be present.  This includes B<param()>, B<textfield()>,
B<submit()> and the like.  (If you need direct access to the CGI
object, you can find it in the global variable B<$CGI::Q>).  By
importing CGI.pm methods, you can create visually elegant scripts:

   use CGI standard,html2;
   print 
       header,
       start_html('Simple Script'),
       h1('Simple Script'),
       start_form,
       "What's your name? ",textfield('name'),p,
       "What's the combination?",
       checkbox_group(-name=>'words',
		      -values=>['eenie','meenie','minie','moe'],
		      -defaults=>['eenie','moe']),p,
       "What's your favorite color?",
       popup_menu(-name=>'color',
		  -values=>['red','green','blue','chartreuse']),p,
       submit,
       end_form,
       hr,"\n";

    if (param) {
       print 
	   "Your name is ",em(param('name')),p,
	   "The keywords are: ",em(join(", ",param('words'))),p,
	   "Your favorite color is ",em(param('color')),".\n";
    }
    print end_html;

=head1 USING NPH SCRIPTS

NPH, or "no-parsed-header", scripts bypass the server completely by
sending the complete HTTP header directly to the browser.  This has
slight performance benefits, but is of most use for taking advantage
of HTTP extensions that are not directly supported by your server,
such as server push and PICS headers.

Servers use a variety of conventions for designating CGI scripts as
NPH.  Many Unix servers look at the beginning of the script's name for
the prefix "nph-".  The Macintosh WebSTAR server and Microsoft's
Internet Information Server, in contrast, try to decide whether a
program is an NPH script by examining the first line of script output.


CGI.pm supports NPH scripts with a special NPH mode.  When in this
mode, CGI.pm will output the necessary extra header information when
the header() and redirect() methods are
called.

The Microsoft Internet Information Server requires NPH mode.  As of version
2.30, CGI.pm will automatically detect when the script is running under IIS
and put itself into this mode.  You do not need to do this manually, although
it won't hurt anything if you do.

There are a number of ways to put CGI.pm into NPH mode:

=over 4

=item In the B<use> statement
Simply add ":nph" to the list of symbols to be imported into your script:

      use CGI qw(:standard :nph)

=item By calling the B<nph()> method:

Call B<nph()> with a non-zero parameter at any point after using CGI.pm in your program.

      CGI->nph(1)

=item By using B<-nph> parameters in the B<header()> and B<redirect()>  statements:

      print $q->header(-nph=&gt;1);

=back

=head1 AUTHOR INFORMATION

Copyright 1995,1996, Lincoln D. Stein.  All rights reserved.  It may
be used and modified freely, but I do request that this copyright
notice remain attached to the file.  You may modify this module as you
wish, but if you redistribute a modified version, please attach a note
listing the modifications you have made.

Address bug reports and comments to:
lstein@genome.wi.mit.edu

=head1 CREDITS

Thanks very much to:

=over 4

=item Matt Heffron (heffron@falstaff.css.beckman.com)

=item James Taylor (james.taylor@srs.gov)

=item Scott Anguish <sanguish@digifix.com>

=item Mike Jewell (mlj3u@virginia.edu)

=item Timothy Shimmin (tes@kbs.citri.edu.au)

=item Joergen Haegg (jh@axis.se)

=item Laurent Delfosse (delfosse@csgrad1.cs.wvu.edu)

=item Richard Resnick (applepi1@aol.com)

=item Craig Bishop (csb@barwonwater.vic.gov.au)

=item Tony Curtis (tc@vcpc.univie.ac.at)

=item Tim Bunce (Tim.Bunce@ig.co.uk)

=item Tom Christiansen (tchrist@convex.com)

=item Andreas Koenig (k@franz.ww.TU-Berlin.DE)

=item Tim MacKenzie (Tim.MacKenzie@fulcrum.com.au)

=item Kevin B. Hendricks (kbhend@dogwood.tyler.wm.edu)

=item Stephen Dahmen (joyfire@inxpress.net)

=item Ed Jordan (ed@fidalgo.net)

=item David Alan Pisoni (david@cnation.com)

=item ...and many many more...

for suggestions and bug fixes.

=back

=head1 A COMPLETE EXAMPLE OF A SIMPLE FORM-BASED SCRIPT


	#!/usr/local/bin/perl
     
	use CGI;
 
	$query = new CGI;

	print $query->header;
	print $query->start_html("Example CGI.pm Form");
	print "<H1> Example CGI.pm Form</H1>\n";
	&print_prompt($query);
	&do_work($query);
	&print_tail;
	print $query->end_html;
 
	sub print_prompt {
	   my($query) = @_;
 
	   print $query->startform;
	   print "<EM>What's your name?</EM><BR>";
	   print $query->textfield('name');
	   print $query->checkbox('Not my real name');
 
	   print "<P><EM>Where can you find English Sparrows?</EM><BR>";
	   print $query->checkbox_group(
				 -name=>'Sparrow locations',
				 -values=>[England,France,Spain,Asia,Hoboken],
				 -linebreak=>'yes',
				 -defaults=>[England,Asia]);
 
	   print "<P><EM>How far can they fly?</EM><BR>",
		$query->radio_group(
			-name=>'how far',
			-values=>['10 ft','1 mile','10 miles','real far'],
			-default=>'1 mile');
 
	   print "<P><EM>What's your favorite color?</EM>  ";
	   print $query->popup_menu(-name=>'Color',
				    -values=>['black','brown','red','yellow'],
				    -default=>'red');
 
	   print $query->hidden('Reference','Monty Python and the Holy Grail');
 
	   print "<P><EM>What have you got there?</EM><BR>";
	   print $query->scrolling_list(
			 -name=>'possessions',
			 -values=>['A Coconut','A Grail','An Icon',
				   'A Sword','A Ticket'],
			 -size=>5,
			 -multiple=>'true');
 
	   print "<P><EM>Any parting comments?</EM><BR>";
	   print $query->textarea(-name=>'Comments',
				  -rows=>10,
				  -columns=>50);
 
	   print "<P>",$query->reset;
	   print $query->submit('Action','Shout');
	   print $query->submit('Action','Scream');
	   print $query->endform;
	   print "<HR>\n";
	}
 
	sub do_work {
	   my($query) = @_;
	   my(@values,$key);

	   print "<H2>Here are the current settings in this form</H2>";

	   foreach $key ($query->param) {
	      print "<STRONG>$key</STRONG> -> ";
	      @values = $query->param($key);
	      print join(", ",@values),"<BR>\n";
	  }
	}
 
	sub print_tail {
	   print <<END;
	<HR>
	<ADDRESS>Lincoln D. Stein</ADDRESS><BR>
	<A HREF="/">Home Page</A>
	END
	}

=head1 BUGS

This module has grown large and monolithic.  Furthermore it's doing many
things, such as handling URLs, parsing CGI input, writing HTML, etc., that
are also done in the LWP modules. It should be discarded in favor of
the CGI::* modules, but somehow I continue to work on it.

Note that the code is truly contorted in order to avoid spurious
warnings when programs are run with the B<-w> switch.

=head1 SEE ALSO

L<CGI::Carp>, L<URI::URL>, L<CGI::Request>, L<CGI::MiniSvr>,
L<CGI::Base>, L<CGI::Form>, L<CGI::Apache>, L<CGI::Switch>,
L<CGI::Push>, L<CGI::Fast>

=cut

