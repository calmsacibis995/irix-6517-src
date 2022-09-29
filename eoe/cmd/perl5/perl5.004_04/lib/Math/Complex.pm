#
# Complex numbers and associated mathematical functions
# -- Raphael Manfredi	September 1996
# -- Jarkko Hietaniemi	March-October 1997
# -- Daniel S. Lewart	September-October 1997
#

require Exporter;
package Math::Complex;

$VERSION = 1.05;

# $Id: Complex.pm,v 1.2 1997/10/15 10:08:39 jhi Exp $

use strict;

use vars qw($VERSION @ISA
	    @EXPORT %EXPORT_TAGS
	    $package $display
	    $i $ip2 $logn %logn);

@ISA = qw(Exporter);

my @trig = qw(
	      pi
	      tan
	      csc cosec sec cot cotan
	      asin acos atan
	      acsc acosec asec acot acotan
	      sinh cosh tanh
	      csch cosech sech coth cotanh
	      asinh acosh atanh
	      acsch acosech asech acoth acotanh
	     );

@EXPORT = (qw(
	     i Re Im arg
	     sqrt log ln
	     log10 logn cbrt root
	     cplx cplxe
	     ),
	   @trig);

%EXPORT_TAGS = (
    'trig' => [@trig],
);

use overload
	'+'	=> \&plus,
	'-'	=> \&minus,
	'*'	=> \&multiply,
	'/'	=> \&divide,
	'**'	=> \&power,
	'<=>'	=> \&spaceship,
	'neg'	=> \&negate,
	'~'	=> \&conjugate,
	'abs'	=> \&abs,
	'sqrt'	=> \&sqrt,
	'exp'	=> \&exp,
	'log'	=> \&log,
	'sin'	=> \&sin,
	'cos'	=> \&cos,
	'tan'	=> \&tan,
	'atan2'	=> \&atan2,
	qw("" stringify);

#
# Package globals
#

$package = 'Math::Complex';		# Package name
$display = 'cartesian';			# Default display format

#
# Object attributes (internal):
#	cartesian	[real, imaginary] -- cartesian form
#	polar		[rho, theta] -- polar form
#	c_dirty		cartesian form not up-to-date
#	p_dirty		polar form not up-to-date
#	display		display format (package's global when not set)
#

#
# ->make
#
# Create a new complex number (cartesian form)
#
sub make {
	my $self = bless {}, shift;
	my ($re, $im) = @_;
	$self->{'cartesian'} = [$re, $im];
	$self->{c_dirty} = 0;
	$self->{p_dirty} = 1;
	return $self;
}

#
# ->emake
#
# Create a new complex number (exponential form)
#
sub emake {
	my $self = bless {}, shift;
	my ($rho, $theta) = @_;
	if ($rho < 0) {
	    $rho   = -$rho;
	    $theta = ($theta <= 0) ? $theta + pi() : $theta - pi();
	}
	$self->{'polar'} = [$rho, $theta];
	$self->{p_dirty} = 0;
	$self->{c_dirty} = 1;
	return $self;
}

sub new { &make }		# For backward compatibility only.

#
# cplx
#
# Creates a complex number from a (re, im) tuple.
# This avoids the burden of writing Math::Complex->make(re, im).
#
sub cplx {
	my ($re, $im) = @_;
	return $package->make($re, defined $im ? $im : 0);
}

#
# cplxe
#
# Creates a complex number from a (rho, theta) tuple.
# This avoids the burden of writing Math::Complex->emake(rho, theta).
#
sub cplxe {
	my ($rho, $theta) = @_;
	return $package->emake($rho, defined $theta ? $theta : 0);
}

#
# pi
#
# The number defined as pi = 180 degrees
#
use constant pi => 4 * atan2(1, 1);

#
# pit2
#
# The full circle
#
use constant pit2 => 2 * pi;

#
# pip2
#
# The quarter circle
#
use constant pip2 => pi / 2;

#
# uplog10
#
# Used in log10().
#
use constant uplog10 => 1 / log(10);

#
# i
#
# The number defined as i*i = -1;
#
sub i () {
        return $i if ($i);
	$i = bless {};
	$i->{'cartesian'} = [0, 1];
	$i->{'polar'}     = [1, pip2];
	$i->{c_dirty} = 0;
	$i->{p_dirty} = 0;
	return $i;
}

#
# Attribute access/set routines
#

sub cartesian {$_[0]->{c_dirty} ?
		   $_[0]->update_cartesian : $_[0]->{'cartesian'}}
sub polar     {$_[0]->{p_dirty} ?
		   $_[0]->update_polar : $_[0]->{'polar'}}

sub set_cartesian { $_[0]->{p_dirty}++; $_[0]->{'cartesian'} = $_[1] }
sub set_polar     { $_[0]->{c_dirty}++; $_[0]->{'polar'} = $_[1] }

#
# ->update_cartesian
#
# Recompute and return the cartesian form, given accurate polar form.
#
sub update_cartesian {
	my $self = shift;
	my ($r, $t) = @{$self->{'polar'}};
	$self->{c_dirty} = 0;
	return $self->{'cartesian'} = [$r * cos $t, $r * sin $t];
}

#
#
# ->update_polar
#
# Recompute and return the polar form, given accurate cartesian form.
#
sub update_polar {
	my $self = shift;
	my ($x, $y) = @{$self->{'cartesian'}};
	$self->{p_dirty} = 0;
	return $self->{'polar'} = [0, 0] if $x == 0 && $y == 0;
	return $self->{'polar'} = [sqrt($x*$x + $y*$y), atan2($y, $x)];
}

#
# (plus)
#
# Computes z1+z2.
#
sub plus {
	my ($z1, $z2, $regular) = @_;
	my ($re1, $im1) = @{$z1->cartesian};
	$z2 = cplx($z2) unless ref $z2;
	my ($re2, $im2) = ref $z2 ? @{$z2->cartesian} : ($z2, 0);
	unless (defined $regular) {
		$z1->set_cartesian([$re1 + $re2, $im1 + $im2]);
		return $z1;
	}
	return (ref $z1)->make($re1 + $re2, $im1 + $im2);
}

#
# (minus)
#
# Computes z1-z2.
#
sub minus {
	my ($z1, $z2, $inverted) = @_;
	my ($re1, $im1) = @{$z1->cartesian};
	$z2 = cplx($z2) unless ref $z2;
	my ($re2, $im2) = @{$z2->cartesian};
	unless (defined $inverted) {
		$z1->set_cartesian([$re1 - $re2, $im1 - $im2]);
		return $z1;
	}
	return $inverted ?
		(ref $z1)->make($re2 - $re1, $im2 - $im1) :
		(ref $z1)->make($re1 - $re2, $im1 - $im2);

}

#
# (multiply)
#
# Computes z1*z2.
#
sub multiply {
        my ($z1, $z2, $regular) = @_;
	if ($z1->{p_dirty} == 0 and ref $z2 and $z2->{p_dirty} == 0) {
	    # if both polar better use polar to avoid rounding errors
	    my ($r1, $t1) = @{$z1->polar};
	    my ($r2, $t2) = @{$z2->polar};
	    my $t = $t1 + $t2;
	    if    ($t >   pi()) { $t -= pit2 }
	    elsif ($t <= -pi()) { $t += pit2 }
	    unless (defined $regular) {
		$z1->set_polar([$r1 * $r2, $t]);
		return $z1;
	    }
	    return (ref $z1)->emake($r1 * $r2, $t);
	} else {
	    my ($x1, $y1) = @{$z1->cartesian};
	    if (ref $z2) {
		my ($x2, $y2) = @{$z2->cartesian};
		return (ref $z1)->make($x1*$x2-$y1*$y2, $x1*$y2+$y1*$x2);
	    } else {
		return (ref $z1)->make($x1*$z2, $y1*$z2);
	    }
	}
}

#
# _divbyzero
#
# Die on division by zero.
#
sub _divbyzero {
    my $mess = "$_[0]: Division by zero.\n";

    if (defined $_[1]) {
	$mess .= "(Because in the definition of $_[0], the divisor ";
	$mess .= "$_[1] " unless ($_[1] eq '0');
	$mess .= "is 0)\n";
    }

    my @up = caller(1);

    $mess .= "Died at $up[1] line $up[2].\n";

    die $mess;
}

#
# (divide)
#
# Computes z1/z2.
#
sub divide {
	my ($z1, $z2, $inverted) = @_;
	if ($z1->{p_dirty} == 0 and ref $z2 and $z2->{p_dirty} == 0) {
	    # if both polar better use polar to avoid rounding errors
	    my ($r1, $t1) = @{$z1->polar};
	    my ($r2, $t2) = @{$z2->polar};
	    my $t;
	    if ($inverted) {
		_divbyzero "$z2/0" if ($r1 == 0);
		$t = $t2 - $t1;
		if    ($t >   pi()) { $t -= pit2 }
		elsif ($t <= -pi()) { $t += pit2 }
		return (ref $z1)->emake($r2 / $r1, $t);
	    } else {
		_divbyzero "$z1/0" if ($r2 == 0);
		$t = $t1 - $t2;
		if    ($t >   pi()) { $t -= pit2 }
		elsif ($t <= -pi()) { $t += pit2 }
		return (ref $z1)->emake($r1 / $r2, $t);
	    }
	} else {
	    my ($d, $x2, $y2);
	    if ($inverted) {
		($x2, $y2) = @{$z1->cartesian};
		$d = $x2*$x2 + $y2*$y2;
		_divbyzero "$z2/0" if $d == 0;
		return (ref $z1)->make(($x2*$z2)/$d, -($y2*$z2)/$d);
	    } else {
		my ($x1, $y1) = @{$z1->cartesian};
		if (ref $z2) {
		    ($x2, $y2) = @{$z2->cartesian};
		    $d = $x2*$x2 + $y2*$y2;
		    _divbyzero "$z1/0" if $d == 0;
		    my $u = ($x1*$x2 + $y1*$y2)/$d;
		    my $v = ($y1*$x2 - $x1*$y2)/$d;
		    return (ref $z1)->make($u, $v);
		} else {
		    _divbyzero "$z1/0" if $z2 == 0;
		    return (ref $z1)->make($x1/$z2, $y1/$z2);
		}
	    }
	}
}

#
# _zerotozero
#
# Die on zero raised to the zeroth.
#
sub _zerotozero {
    my $mess = "The zero raised to the zeroth power is not defined.\n";

    my @up = caller(1);

    $mess .= "Died at $up[1] line $up[2].\n";

    die $mess;
}

#
# (power)
#
# Computes z1**z2 = exp(z2 * log z1)).
#
sub power {
	my ($z1, $z2, $inverted) = @_;
	my $z1z = $z1 == 0;
	my $z2z = $z2 == 0;
	_zerotozero if ($z1z and $z2z);
	if ($inverted) {
	    return 0 if ($z2z);
	    return 1 if ($z1z or $z2 == 1);
	} else {
	    return 0 if ($z1z);
	    return 1 if ($z2z or $z1 == 1);
	}
	return $inverted ? exp($z1 * log $z2) : exp($z2 * log $z1);
}

#
# (spaceship)
#
# Computes z1 <=> z2.
# Sorts on the real part first, then on the imaginary part. Thus 2-4i > 3+8i.
#
sub spaceship {
	my ($z1, $z2, $inverted) = @_;
	my ($re1, $im1) = ref $z1 ? @{$z1->cartesian} : ($z1, 0);
	my ($re2, $im2) = ref $z2 ? @{$z2->cartesian} : ($z2, 0);
	my $sgn = $inverted ? -1 : 1;
	return $sgn * ($re1 <=> $re2) if $re1 != $re2;
	return $sgn * ($im1 <=> $im2);
}

#
# (negate)
#
# Computes -z.
#
sub negate {
	my ($z) = @_;
	if ($z->{c_dirty}) {
		my ($r, $t) = @{$z->polar};
		$t = ($t <= 0) ? $t + pi : $t - pi;
		return (ref $z)->emake($r, $t);
	}
	my ($re, $im) = @{$z->cartesian};
	return (ref $z)->make(-$re, -$im);
}

#
# (conjugate)
#
# Compute complex's conjugate.
#
sub conjugate {
	my ($z) = @_;
	if ($z->{c_dirty}) {
		my ($r, $t) = @{$z->polar};
		return (ref $z)->emake($r, -$t);
	}
	my ($re, $im) = @{$z->cartesian};
	return (ref $z)->make($re, -$im);
}

#
# (abs)
#
# Compute complex's norm (rho).
#
sub abs {
	my ($z) = @_;
	my ($r, $t) = @{$z->polar};
	return $r;
}

#
# arg
#
# Compute complex's argument (theta).
#
sub arg {
	my ($z) = @_;
	return ($z < 0 ? pi : 0) unless ref $z;
	my ($r, $t) = @{$z->polar};
	if    ($t >   pi()) { $t -= pit2 }
	elsif ($t <= -pi()) { $t += pit2 }
	return $t;
}

#
# (sqrt)
#
# Compute sqrt(z).
#
sub sqrt {
	my ($z) = @_;
	return $z >= 0 ? sqrt($z) : cplx(0, sqrt(-$z)) unless ref $z;
	my ($re, $im) = @{$z->cartesian};
	return cplx($re < 0 ? (0, sqrt(-$re)) : (sqrt($re), 0)) if $im == 0;
	my ($r, $t) = @{$z->polar};
	return (ref $z)->emake(sqrt($r), $t/2);
}

#
# cbrt
#
# Compute cbrt(z) (cubic root).
#
sub cbrt {
	my ($z) = @_;
	return $z < 0 ? -exp(log(-$z)/3) : ($z > 0 ? exp(log($z)/3): 0)
	    unless ref $z;
	my ($r, $t) = @{$z->polar};
	return (ref $z)->emake(exp(log($r)/3), $t/3);
}

#
# _rootbad
#
# Die on bad root.
#
sub _rootbad {
    my $mess = "Root $_[0] not defined, root must be positive integer.\n";

    my @up = caller(1);

    $mess .= "Died at $up[1] line $up[2].\n";

    die $mess;
}

#
# root
#
# Computes all nth root for z, returning an array whose size is n.
# `n' must be a positive integer.
#
# The roots are given by (for k = 0..n-1):
#
# z^(1/n) = r^(1/n) (cos ((t+2 k pi)/n) + i sin ((t+2 k pi)/n))
#
sub root {
	my ($z, $n) = @_;
	_rootbad($n) if ($n < 1 or int($n) != $n);
	my ($r, $t) = ref $z ? @{$z->polar} : (abs($z), $z >= 0 ? 0 : pi);
	my @root;
	my $k;
	my $theta_inc = pit2 / $n;
	my $rho = $r ** (1/$n);
	my $theta;
	my $complex = ref($z) || $package;
	for ($k = 0, $theta = $t / $n; $k < $n; $k++, $theta += $theta_inc) {
		push(@root, $complex->emake($rho, $theta));
	}
	return @root;
}

#
# Re
#
# Return Re(z).
#
sub Re {
	my ($z) = @_;
	return $z unless ref $z;
	my ($re, $im) = @{$z->cartesian};
	return $re;
}

#
# Im
#
# Return Im(z).
#
sub Im {
	my ($z) = @_;
	return 0 unless ref $z;
	my ($re, $im) = @{$z->cartesian};
	return $im;
}

#
# (exp)
#
# Computes exp(z).
#
sub exp {
	my ($z) = @_;
	my ($x, $y) = @{$z->cartesian};
	return (ref $z)->emake(exp($x), $y);
}

#
# _logofzero
#
# Die on logarithm of zero.
#
sub _logofzero {
    my $mess = "$_[0]: Logarithm of zero.\n";

    if (defined $_[1]) {
	$mess .= "(Because in the definition of $_[0], the argument ";
	$mess .= "$_[1] " unless ($_[1] eq '0');
	$mess .= "is 0)\n";
    }

    my @up = caller(1);

    $mess .= "Died at $up[1] line $up[2].\n";

    die $mess;
}

#
# (log)
#
# Compute log(z).
#
sub log {
	my ($z) = @_;
	unless (ref $z) {
	    _logofzero("log") if $z == 0;
	    return $z > 0 ? log($z) : cplx(log(-$z), pi);
	}
	my ($r, $t) = @{$z->polar};
	_logofzero("log") if $r == 0;
	if    ($t >   pi()) { $t -= pit2 }
	elsif ($t <= -pi()) { $t += pit2 }
	return (ref $z)->make(log($r), $t);
}

#
# ln
#
# Alias for log().
#
sub ln { Math::Complex::log(@_) }

#
# log10
#
# Compute log10(z).
#

sub log10 {
	return Math::Complex::log($_[0]) * uplog10;
}

#
# logn
#
# Compute logn(z,n) = log(z) / log(n)
#
sub logn {
	my ($z, $n) = @_;
	$z = cplx($z, 0) unless ref $z;
	my $logn = $logn{$n};
	$logn = $logn{$n} = log($n) unless defined $logn;	# Cache log(n)
	return log($z) / $logn;
}

#
# (cos)
#
# Compute cos(z) = (exp(iz) + exp(-iz))/2.
#
sub cos {
	my ($z) = @_;
	my ($x, $y) = @{$z->cartesian};
	my $ey = exp($y);
	my $ey_1 = 1 / $ey;
	return (ref $z)->make(cos($x) * ($ey + $ey_1)/2,
			      sin($x) * ($ey_1 - $ey)/2);
}

#
# (sin)
#
# Compute sin(z) = (exp(iz) - exp(-iz))/2.
#
sub sin {
	my ($z) = @_;
	my ($x, $y) = @{$z->cartesian};
	my $ey = exp($y);
	my $ey_1 = 1 / $ey;
	return (ref $z)->make(sin($x) * ($ey + $ey_1)/2,
			      cos($x) * ($ey - $ey_1)/2);
}

#
# tan
#
# Compute tan(z) = sin(z) / cos(z).
#
sub tan {
	my ($z) = @_;
	my $cz = cos($z);
	_divbyzero "tan($z)", "cos($z)" if ($cz == 0);
	return sin($z) / $cz;
}

#
# sec
#
# Computes the secant sec(z) = 1 / cos(z).
#
sub sec {
	my ($z) = @_;
	my $cz = cos($z);
	_divbyzero "sec($z)", "cos($z)" if ($cz == 0);
	return 1 / $cz;
}

#
# csc
#
# Computes the cosecant csc(z) = 1 / sin(z).
#
sub csc {
	my ($z) = @_;
	my $sz = sin($z);
	_divbyzero "csc($z)", "sin($z)" if ($sz == 0);
	return 1 / $sz;
}

#
# cosec
#
# Alias for csc().
#
sub cosec { Math::Complex::csc(@_) }

#
# cot
#
# Computes cot(z) = cos(z) / sin(z).
#
sub cot {
	my ($z) = @_;
	my $sz = sin($z);
	_divbyzero "cot($z)", "sin($z)" if ($sz == 0);
	return cos($z) / $sz;
}

#
# cotan
#
# Alias for cot().
#
sub cotan { Math::Complex::cot(@_) }

#
# acos
#
# Computes the arc cosine acos(z) = -i log(z + sqrt(z*z-1)).
#
sub acos {
	my $z = $_[0];
	return atan2(sqrt(1-$z*$z), $z) if (! ref $z) && abs($z) <= 1;
	my ($x, $y) = ref $z ? @{$z->cartesian} : ($z, 0);
	my $t1 = sqrt(($x+1)*($x+1) + $y*$y);
	my $t2 = sqrt(($x-1)*($x-1) + $y*$y);
	my $alpha = ($t1 + $t2)/2;
	my $beta  = ($t1 - $t2)/2;
	$alpha = 1 if $alpha < 1;
	if    ($beta >  1) { $beta =  1 }
	elsif ($beta < -1) { $beta = -1 }
	my $u = atan2(sqrt(1-$beta*$beta), $beta);
	my $v = log($alpha + sqrt($alpha*$alpha-1));
	$v = -$v if $y > 0 || ($y == 0 && $x < -1);
	return $package->make($u, $v);
}

#
# asin
#
# Computes the arc sine asin(z) = -i log(iz + sqrt(1-z*z)).
#
sub asin {
	my $z = $_[0];
	return atan2($z, sqrt(1-$z*$z)) if (! ref $z) && abs($z) <= 1;
	my ($x, $y) = ref $z ? @{$z->cartesian} : ($z, 0);
	my $t1 = sqrt(($x+1)*($x+1) + $y*$y);
	my $t2 = sqrt(($x-1)*($x-1) + $y*$y);
	my $alpha = ($t1 + $t2)/2;
	my $beta  = ($t1 - $t2)/2;
	$alpha = 1 if $alpha < 1;
	if    ($beta >  1) { $beta =  1 }
	elsif ($beta < -1) { $beta = -1 }
	my $u =  atan2($beta, sqrt(1-$beta*$beta));
	my $v = -log($alpha + sqrt($alpha*$alpha-1));
	$v = -$v if $y > 0 || ($y == 0 && $x < -1);
	return $package->make($u, $v);
}

#
# atan
#
# Computes the arc tangent atan(z) = i/2 log((i+z) / (i-z)).
#
sub atan {
	my ($z) = @_;
	return atan2($z, 1) unless ref $z;
	_divbyzero "atan(i)"  if ( $z == i);
	_divbyzero "atan(-i)" if (-$z == i);
	my $log = log((i + $z) / (i - $z));
	$ip2 = 0.5 * i unless defined $ip2;
	return $ip2 * $log;
}

#
# asec
#
# Computes the arc secant asec(z) = acos(1 / z).
#
sub asec {
	my ($z) = @_;
	_divbyzero "asec($z)", $z if ($z == 0);
	return acos(1 / $z);
}

#
# acsc
#
# Computes the arc cosecant acsc(z) = asin(1 / z).
#
sub acsc {
	my ($z) = @_;
	_divbyzero "acsc($z)", $z if ($z == 0);
	return asin(1 / $z);
}

#
# acosec
#
# Alias for acsc().
#
sub acosec { Math::Complex::acsc(@_) }

#
# acot
#
# Computes the arc cotangent acot(z) = atan(1 / z)
#
sub acot {
	my ($z) = @_;
	return ($z >= 0) ? atan2(1, $z) : atan2(-1, -$z) unless ref $z;
	_divbyzero "acot(i)", if ( $z == i);
	_divbyzero "acot(-i)" if (-$z == i);
	return atan(1 / $z);
}

#
# acotan
#
# Alias for acot().
#
sub acotan { Math::Complex::acot(@_) }

#
# cosh
#
# Computes the hyperbolic cosine cosh(z) = (exp(z) + exp(-z))/2.
#
sub cosh {
	my ($z) = @_;
	my $ex;
	unless (ref $z) {
	    $ex = exp($z);
	    return ($ex + 1/$ex)/2;
	}
	my ($x, $y) = @{$z->cartesian};
	$ex = exp($x);
	my $ex_1 = 1 / $ex;
	return (ref $z)->make(cos($y) * ($ex + $ex_1)/2,
			      sin($y) * ($ex - $ex_1)/2);
}

#
# sinh
#
# Computes the hyperbolic sine sinh(z) = (exp(z) - exp(-z))/2.
#
sub sinh {
	my ($z) = @_;
	my $ex;
	unless (ref $z) {
	    $ex = exp($z);
	    return ($ex - 1/$ex)/2;
	}
	my ($x, $y) = @{$z->cartesian};
	$ex = exp($x);
	my $ex_1 = 1 / $ex;
	return (ref $z)->make(cos($y) * ($ex - $ex_1)/2,
			      sin($y) * ($ex + $ex_1)/2);
}

#
# tanh
#
# Computes the hyperbolic tangent tanh(z) = sinh(z) / cosh(z).
#
sub tanh {
	my ($z) = @_;
	my $cz = cosh($z);
	_divbyzero "tanh($z)", "cosh($z)" if ($cz == 0);
	return sinh($z) / $cz;
}

#
# sech
#
# Computes the hyperbolic secant sech(z) = 1 / cosh(z).
#
sub sech {
	my ($z) = @_;
	my $cz = cosh($z);
	_divbyzero "sech($z)", "cosh($z)" if ($cz == 0);
	return 1 / $cz;
}

#
# csch
#
# Computes the hyperbolic cosecant csch(z) = 1 / sinh(z).
#
sub csch {
	my ($z) = @_;
	my $sz = sinh($z);
	_divbyzero "csch($z)", "sinh($z)" if ($sz == 0);
	return 1 / $sz;
}

#
# cosech
#
# Alias for csch().
#
sub cosech { Math::Complex::csch(@_) }

#
# coth
#
# Computes the hyperbolic cotangent coth(z) = cosh(z) / sinh(z).
#
sub coth {
	my ($z) = @_;
	my $sz = sinh($z);
	_divbyzero "coth($z)", "sinh($z)" if ($sz == 0);
	return cosh($z) / $sz;
}

#
# cotanh
#
# Alias for coth().
#
sub cotanh { Math::Complex::coth(@_) }

#
# acosh
#
# Computes the arc hyperbolic cosine acosh(z) = log(z + sqrt(z*z-1)).
#
sub acosh {
	my ($z) = @_;
	unless (ref $z) {
	    return log($z + sqrt($z*$z-1)) if $z >= 1;
	    $z = cplx($z, 0);
	}
	my ($re, $im) = @{$z->cartesian};
	if ($im == 0) {
	    return cplx(log($re + sqrt($re*$re - 1)), 0) if $re >= 1;
	    return cplx(0, atan2(sqrt(1-$re*$re), $re)) if abs($re) <= 1;
	}
	return log($z + sqrt($z*$z - 1));
}

#
# asinh
#
# Computes the arc hyperbolic sine asinh(z) = log(z + sqrt(z*z-1))
#
sub asinh {
	my ($z) = @_;
	return log($z + sqrt($z*$z + 1));
}

#
# atanh
#
# Computes the arc hyperbolic tangent atanh(z) = 1/2 log((1+z) / (1-z)).
#
sub atanh {
	my ($z) = @_;
	unless (ref $z) {
	    return log((1 + $z)/(1 - $z))/2 if abs($z) < 1;
	    $z = cplx($z, 0);
	}
	_divbyzero 'atanh(1)',  "1 - $z" if ($z ==  1);
	_logofzero 'atanh(-1)'           if ($z == -1);
	return 0.5 * log((1 + $z) / (1 - $z));
}

#
# asech
#
# Computes the hyperbolic arc secant asech(z) = acosh(1 / z).
#
sub asech {
	my ($z) = @_;
	_divbyzero 'asech(0)', $z if ($z == 0);
	return acosh(1 / $z);
}

#
# acsch
#
# Computes the hyperbolic arc cosecant acsch(z) = asinh(1 / z).
#
sub acsch {
	my ($z) = @_;
	_divbyzero 'acsch(0)', $z if ($z == 0);
	return asinh(1 / $z);
}

#
# acosech
#
# Alias for acosh().
#
sub acosech { Math::Complex::acsch(@_) }

#
# acoth
#
# Computes the arc hyperbolic cotangent acoth(z) = 1/2 log((1+z) / (z-1)).
#
sub acoth {
	my ($z) = @_;
	unless (ref $z) {
	    return log(($z + 1)/($z - 1))/2 if abs($z) > 1;
	    $z = cplx($z, 0);
	}
	_divbyzero 'acoth(1)', "$z - 1" if ($z ==  1);
	_logofzero 'acoth(-1)'          if ($z == -1);
	return log((1 + $z) / ($z - 1)) / 2;
}

#
# acotanh
#
# Alias for acot().
#
sub acotanh { Math::Complex::acoth(@_) }

#
# (atan2)
#
# Compute atan(z1/z2).
#
sub atan2 {
	my ($z1, $z2, $inverted) = @_;
	my ($re1, $im1, $re2, $im2);
	if ($inverted) {
	    ($re1, $im1) = ref $z2 ? @{$z2->cartesian} : ($z2, 0);
	    ($re2, $im2) = @{$z1->cartesian};
	} else {
	    ($re1, $im1) = @{$z1->cartesian};
	    ($re2, $im2) = ref $z2 ? @{$z2->cartesian} : ($z2, 0);
	}
	if ($im2 == 0) {
	    return cplx(atan2($re1, $re2), 0) if $im1 == 0;
	    return cplx(($im1<=>0) * pip2, 0) if $re2 == 0;
	}
	my $w = atan($z1/$z2);
	my ($u, $v) = ref $w ? @{$w->cartesian} : ($w, 0);
	$u += pi   if $re2 < 0;
	$u -= pit2 if $u > pi;
	return cplx($u, $v);
}

#
# display_format
# ->display_format
#
# Set (fetch if no argument) display format for all complex numbers that
# don't happen to have overridden it via ->display_format
#
# When called as a method, this actually sets the display format for
# the current object.
#
# Valid object formats are 'c' and 'p' for cartesian and polar. The first
# letter is used actually, so the type can be fully spelled out for clarity.
#
sub display_format {
	my $self = shift;
	my $format = undef;

	if (ref $self) {			# Called as a method
		$format = shift;
	} else {				# Regular procedure call
		$format = $self;
		undef $self;
	}

	if (defined $self) {
		return defined $self->{display} ? $self->{display} : $display
			unless defined $format;
		return $self->{display} = $format;
	}

	return $display unless defined $format;
	return $display = $format;
}

#
# (stringify)
#
# Show nicely formatted complex number under its cartesian or polar form,
# depending on the current display format:
#
# . If a specific display format has been recorded for this object, use it.
# . Otherwise, use the generic current default for all complex numbers,
#   which is a package global variable.
#
sub stringify {
	my ($z) = shift;
	my $format;

	$format = $display;
	$format = $z->{display} if defined $z->{display};

	return $z->stringify_polar if $format =~ /^p/i;
	return $z->stringify_cartesian;
}

#
# ->stringify_cartesian
#
# Stringify as a cartesian representation 'a+bi'.
#
sub stringify_cartesian {
	my $z  = shift;
	my ($x, $y) = @{$z->cartesian};
	my ($re, $im);
	my $eps = 1e-14;

	$x = int($x + ($x < 0 ? -1 : 1) * $eps)
		if int(abs($x)) != int(abs($x) + $eps);
	$y = int($y + ($y < 0 ? -1 : 1) * $eps)
		if int(abs($y)) != int(abs($y) + $eps);

	$re = "$x" if abs($x) >= $eps;
        if ($y == 1)                           { $im = 'i' }
        elsif ($y == -1)                       { $im = '-i' }
        elsif (abs($y) >= $eps)                { $im = $y . "i" }

	my $str = '';
	$str = $re if defined $re;
	$str .= "+$im" if defined $im;
	$str =~ s/\+-/-/;
	$str =~ s/^\+//;
	$str = '0' unless $str;

	return $str;
}

#
# ->stringify_polar
#
# Stringify as a polar representation '[r,t]'.
#
sub stringify_polar {
	my $z  = shift;
	my ($r, $t) = @{$z->polar};
	my $theta;
	my $eps = 1e-14;

	return '[0,0]' if $r <= $eps;

	my $nt = $t / pit2;
	$nt = ($nt - int($nt)) * pit2;
	$nt += pit2 if $nt < 0;			# Range [0, 2pi]

	if (abs($nt) <= $eps)		{ $theta = 0 }
	elsif (abs(pi-$nt) <= $eps)	{ $theta = 'pi' }

	if (defined $theta) {
		$r = int($r + ($r < 0 ? -1 : 1) * $eps)
			if int(abs($r)) != int(abs($r) + $eps);
		$theta = int($theta + ($theta < 0 ? -1 : 1) * $eps)
			if ($theta ne 'pi' and
			    int(abs($theta)) != int(abs($theta) + $eps));
		return "\[$r,$theta\]";
	}

	#
	# Okay, number is not a real. Try to identify pi/n and friends...
	#

	$nt -= pit2 if $nt > pi;
	my ($n, $k, $kpi);

	for ($k = 1, $kpi = pi; $k < 10; $k++, $kpi += pi) {
		$n = int($kpi / $nt + ($nt > 0 ? 1 : -1) * 0.5);
		if (abs($kpi/$n - $nt) <= $eps) {
			$theta = ($nt < 0 ? '-':'').
				 ($k == 1 ? 'pi':"${k}pi").'/'.abs($n);
			last;
		}
	}

	$theta = $nt unless defined $theta;

	$r = int($r + ($r < 0 ? -1 : 1) * $eps)
		if int(abs($r)) != int(abs($r) + $eps);
	$theta = int($theta + ($theta < 0 ? -1 : 1) * $eps)
		if ($theta !~ m(^-?\d*pi/\d+$) and
		    int(abs($theta)) != int(abs($theta) + $eps));

	return "\[$r,$theta\]";
}

1;
__END__

=head1 NAME

Math::Complex - complex numbers and associated mathematical functions

=head1 SYNOPSIS

	use Math::Complex;

	$z = Math::Complex->make(5, 6);
	$t = 4 - 3*i + $z;
	$j = cplxe(1, 2*pi/3);

=head1 DESCRIPTION

This package lets you create and manipulate complex numbers. By default,
I<Perl> limits itself to real numbers, but an extra C<use> statement brings
full complex support, along with a full set of mathematical functions
typically associated with and/or extended to complex numbers.

If you wonder what complex numbers are, they were invented to be able to solve
the following equation:

	x*x = -1

and by definition, the solution is noted I<i> (engineers use I<j> instead since
I<i> usually denotes an intensity, but the name does not matter). The number
I<i> is a pure I<imaginary> number.

The arithmetics with pure imaginary numbers works just like you would expect
it with real numbers... you just have to remember that

	i*i = -1

so you have:

	5i + 7i = i * (5 + 7) = 12i
	4i - 3i = i * (4 - 3) = i
	4i * 2i = -8
	6i / 2i = 3
	1 / i = -i

Complex numbers are numbers that have both a real part and an imaginary
part, and are usually noted:

	a + bi

where C<a> is the I<real> part and C<b> is the I<imaginary> part. The
arithmetic with complex numbers is straightforward. You have to
keep track of the real and the imaginary parts, but otherwise the
rules used for real numbers just apply:

	(4 + 3i) + (5 - 2i) = (4 + 5) + i(3 - 2) = 9 + i
	(2 + i) * (4 - i) = 2*4 + 4i -2i -i*i = 8 + 2i + 1 = 9 + 2i

A graphical representation of complex numbers is possible in a plane
(also called the I<complex plane>, but it's really a 2D plane).
The number

	z = a + bi

is the point whose coordinates are (a, b). Actually, it would
be the vector originating from (0, 0) to (a, b). It follows that the addition
of two complex numbers is a vectorial addition.

Since there is a bijection between a point in the 2D plane and a complex
number (i.e. the mapping is unique and reciprocal), a complex number
can also be uniquely identified with polar coordinates:

	[rho, theta]

where C<rho> is the distance to the origin, and C<theta> the angle between
the vector and the I<x> axis. There is a notation for this using the
exponential form, which is:

	rho * exp(i * theta)

where I<i> is the famous imaginary number introduced above. Conversion
between this form and the cartesian form C<a + bi> is immediate:

	a = rho * cos(theta)
	b = rho * sin(theta)

which is also expressed by this formula:

	z = rho * exp(i * theta) = rho * (cos theta + i * sin theta)

In other words, it's the projection of the vector onto the I<x> and I<y>
axes. Mathematicians call I<rho> the I<norm> or I<modulus> and I<theta>
the I<argument> of the complex number. The I<norm> of C<z> will be
noted C<abs(z)>.

The polar notation (also known as the trigonometric
representation) is much more handy for performing multiplications and
divisions of complex numbers, whilst the cartesian notation is better
suited for additions and subtractions. Real numbers are on the I<x>
axis, and therefore I<theta> is zero or I<pi>.

All the common operations that can be performed on a real number have
been defined to work on complex numbers as well, and are merely
I<extensions> of the operations defined on real numbers. This means
they keep their natural meaning when there is no imaginary part, provided
the number is within their definition set.

For instance, the C<sqrt> routine which computes the square root of
its argument is only defined for non-negative real numbers and yields a
non-negative real number (it is an application from B<R+> to B<R+>).
If we allow it to return a complex number, then it can be extended to
negative real numbers to become an application from B<R> to B<C> (the
set of complex numbers):

	sqrt(x) = x >= 0 ? sqrt(x) : sqrt(-x)*i

It can also be extended to be an application from B<C> to B<C>,
whilst its restriction to B<R> behaves as defined above by using
the following definition:

	sqrt(z = [r,t]) = sqrt(r) * exp(i * t/2)

Indeed, a negative real number can be noted C<[x,pi]> (the modulus
I<x> is always non-negative, so C<[x,pi]> is really C<-x>, a negative
number) and the above definition states that

	sqrt([x,pi]) = sqrt(x) * exp(i*pi/2) = [sqrt(x),pi/2] = sqrt(x)*i

which is exactly what we had defined for negative real numbers above.

All the common mathematical functions defined on real numbers that
are extended to complex numbers share that same property of working
I<as usual> when the imaginary part is zero (otherwise, it would not
be called an extension, would it?).

A I<new> operation possible on a complex number that is
the identity for real numbers is called the I<conjugate>, and is noted
with an horizontal bar above the number, or C<~z> here.

	 z = a + bi
	~z = a - bi

Simple... Now look:

	z * ~z = (a + bi) * (a - bi) = a*a + b*b

We saw that the norm of C<z> was noted C<abs(z)> and was defined as the
distance to the origin, also known as:

	rho = abs(z) = sqrt(a*a + b*b)

so

	z * ~z = abs(z) ** 2

If z is a pure real number (i.e. C<b == 0>), then the above yields:

	a * a = abs(a) ** 2

which is true (C<abs> has the regular meaning for real number, i.e. stands
for the absolute value). This example explains why the norm of C<z> is
noted C<abs(z)>: it extends the C<abs> function to complex numbers, yet
is the regular C<abs> we know when the complex number actually has no
imaginary part... This justifies I<a posteriori> our use of the C<abs>
notation for the norm.

=head1 OPERATIONS

Given the following notations:

	z1 = a + bi = r1 * exp(i * t1)
	z2 = c + di = r2 * exp(i * t2)
	z = <any complex or real number>

the following (overloaded) operations are supported on complex numbers:

	z1 + z2 = (a + c) + i(b + d)
	z1 - z2 = (a - c) + i(b - d)
	z1 * z2 = (r1 * r2) * exp(i * (t1 + t2))
	z1 / z2 = (r1 / r2) * exp(i * (t1 - t2))
	z1 ** z2 = exp(z2 * log z1)
	~z1 = a - bi
	abs(z1) = r1 = sqrt(a*a + b*b)
	sqrt(z1) = sqrt(r1) * exp(i * t1/2)
	exp(z1) = exp(a) * exp(i * b)
	log(z1) = log(r1) + i*t1
	sin(z1) = 1/2i (exp(i * z1) - exp(-i * z1))
	cos(z1) = 1/2 (exp(i * z1) + exp(-i * z1))
	atan2(z1, z2) = atan(z1/z2)

The following extra operations are supported on both real and complex
numbers:

	Re(z) = a
	Im(z) = b
	arg(z) = t

	cbrt(z) = z ** (1/3)
	log10(z) = log(z) / log(10)
	logn(z, n) = log(z) / log(n)

	tan(z) = sin(z) / cos(z)

	csc(z) = 1 / sin(z)
	sec(z) = 1 / cos(z)
	cot(z) = 1 / tan(z)

	asin(z) = -i * log(i*z + sqrt(1-z*z))
	acos(z) = -i * log(z + i*sqrt(1-z*z))
	atan(z) = i/2 * log((i+z) / (i-z))

	acsc(z) = asin(1 / z)
	asec(z) = acos(1 / z)
	acot(z) = atan(1 / z) = -i/2 * log((i+z) / (z-i))

	sinh(z) = 1/2 (exp(z) - exp(-z))
	cosh(z) = 1/2 (exp(z) + exp(-z))
	tanh(z) = sinh(z) / cosh(z) = (exp(z) - exp(-z)) / (exp(z) + exp(-z))

	csch(z) = 1 / sinh(z)
	sech(z) = 1 / cosh(z)
	coth(z) = 1 / tanh(z)

	asinh(z) = log(z + sqrt(z*z+1))
	acosh(z) = log(z + sqrt(z*z-1))
	atanh(z) = 1/2 * log((1+z) / (1-z))

	acsch(z) = asinh(1 / z)
	asech(z) = acosh(1 / z)
	acoth(z) = atanh(1 / z) = 1/2 * log((1+z) / (z-1))

I<log>, I<csc>, I<cot>, I<acsc>, I<acot>, I<csch>, I<coth>,
I<acosech>, I<acotanh>, have aliases I<ln>, I<cosec>, I<cotan>,
I<acosec>, I<acotan>, I<cosech>, I<cotanh>, I<acosech>, I<acotanh>,
respectively.

The I<root> function is available to compute all the I<n>
roots of some complex, where I<n> is a strictly positive integer.
There are exactly I<n> such roots, returned as a list. Getting the
number mathematicians call C<j> such that:

	1 + j + j*j = 0;

is a simple matter of writing:

	$j = ((root(1, 3))[1];

The I<k>th root for C<z = [r,t]> is given by:

	(root(z, n))[k] = r**(1/n) * exp(i * (t + 2*k*pi)/n)

The I<spaceship> comparison operator, E<lt>=E<gt>, is also defined. In
order to ensure its restriction to real numbers is conform to what you
would expect, the comparison is run on the real part of the complex
number first, and imaginary parts are compared only when the real
parts match.

=head1 CREATION

To create a complex number, use either:

	$z = Math::Complex->make(3, 4);
	$z = cplx(3, 4);

if you know the cartesian form of the number, or

	$z = 3 + 4*i;

if you like. To create a number using the polar form, use either:

	$z = Math::Complex->emake(5, pi/3);
	$x = cplxe(5, pi/3);

instead. The first argument is the modulus, the second is the angle
(in radians, the full circle is 2*pi).  (Mnemonic: C<e> is used as a
notation for complex numbers in the polar form).

It is possible to write:

	$x = cplxe(-3, pi/4);

but that will be silently converted into C<[3,-3pi/4]>, since the modulus
must be non-negative (it represents the distance to the origin in the complex
plane).

=head1 STRINGIFICATION

When printed, a complex number is usually shown under its cartesian
form I<a+bi>, but there are legitimate cases where the polar format
I<[r,t]> is more appropriate.

By calling the routine C<Math::Complex::display_format> and supplying either
C<"polar"> or C<"cartesian">, you override the default display format,
which is C<"cartesian">. Not supplying any argument returns the current
setting.

This default can be overridden on a per-number basis by calling the
C<display_format> method instead. As before, not supplying any argument
returns the current display format for this number. Otherwise whatever you
specify will be the new display format for I<this> particular number.

For instance:

	use Math::Complex;

	Math::Complex::display_format('polar');
	$j = ((root(1, 3))[1];
	print "j = $j\n";		# Prints "j = [1,2pi/3]
	$j->display_format('cartesian');
	print "j = $j\n";		# Prints "j = -0.5+0.866025403784439i"

The polar format attempts to emphasize arguments like I<k*pi/n>
(where I<n> is a positive integer and I<k> an integer within [-9,+9]).

=head1 USAGE

Thanks to overloading, the handling of arithmetics with complex numbers
is simple and almost transparent.

Here are some examples:

	use Math::Complex;

	$j = cplxe(1, 2*pi/3);	# $j ** 3 == 1
	print "j = $j, j**3 = ", $j ** 3, "\n";
	print "1 + j + j**2 = ", 1 + $j + $j**2, "\n";

	$z = -16 + 0*i;			# Force it to be a complex
	print "sqrt($z) = ", sqrt($z), "\n";

	$k = exp(i * 2*pi/3);
	print "$j - $k = ", $j - $k, "\n";

=head1 ERRORS DUE TO DIVISION BY ZERO

The division (/) and the following functions

	tan
	sec
	csc
	cot
	asec
	acsc
	atan
	acot
	tanh
	sech
	csch
	coth
	atanh
	asech
	acsch
	acoth

cannot be computed for all arguments because that would mean dividing
by zero or taking logarithm of zero. These situations cause fatal
runtime errors looking like this

	cot(0): Division by zero.
	(Because in the definition of cot(0), the divisor sin(0) is 0)
	Died at ...

or

	atanh(-1): Logarithm of zero.
	Died at...

For the C<csc>, C<cot>, C<asec>, C<acsc>, C<acot>, C<csch>, C<coth>,
C<asech>, C<acsch>, the argument cannot be C<0> (zero).  For the
C<atanh>, C<acoth>, the argument cannot be C<1> (one).  For the
C<atanh>, C<acoth>, the argument cannot be C<-1> (minus one).  For the
C<atan>, C<acot>, the argument cannot be C<i> (the imaginary unit).
For the C<atan>, C<acoth>, the argument cannot be C<-i> (the negative
imaginary unit).  For the C<tan>, C<sec>, C<tanh>, C<sech>, the
argument cannot be I<pi/2 + k * pi>, where I<k> is any integer.

=head1 BUGS

Saying C<use Math::Complex;> exports many mathematical routines in the
caller environment and even overrides some (C<sqrt>, C<log>).
This is construed as a feature by the Authors, actually... ;-)

All routines expect to be given real or complex numbers. Don't attempt to
use BigFloat, since Perl has currently no rule to disambiguate a '+'
operation (for instance) between two overloaded entities.

=head1 AUTHORS

Raphael Manfredi <F<Raphael_Manfredi@grenoble.hp.com>> and
Jarkko Hietaniemi <F<jhi@iki.fi>>.

Extensive patches by Daniel S. Lewart <F<d-lewart@uiuc.edu>>.

=cut

# eof
