package MDBM_File;

use strict;
use vars qw($VERSION @ISA); 

require Tie::Hash;
require DynaLoader;

@ISA = qw(Tie::Hash DynaLoader);

$VERSION = "1.00";

bootstrap MDBM_File $VERSION;

1;

__END__

=head1 NAME

MDBM_File - Tied access to mdbm files

=head1 SYNOPSIS

 use MDBM_File;

 $db=tie(%h,MDBM_File,'Op.dbmx', O_RDWR|O_CREAT, 0640);
 
 MDBM_FILE::sethash($mdbm);

 untie %h;

=head1 DESCRIPTION

See L<perlfunc/tie>

The MDBM_File is a perl binding for the mdbm which is included partly in
the C library libc and in libmdbm.  

The following functions are suported on MDBM_Files::

=over 4

=item chain($db)

mark the database bucked chained to use large values.  All tie based
accesses will automaticly use the mdbm_chain interface.

=item invalidate($db)

mark the database invalid, and make safe to unlink.

=item setthash($db,number)

set the hash function for this database.   The hashes and their properties
are defined in L<mdbm_open(3)>.

=item sync($db)

sync the database to disk.

=back 4

=head1 SEE ALSO

L<perl(1)>, L<mdbm_open(3)>

=cut
