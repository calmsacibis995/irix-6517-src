use ExtUtils::MakeMaker;
WriteMakefile(
    LIBS => ["-L$ENV{'ROOT'}/usr/lib32 -lmdbm"],
    MAN3PODS 	=> ' ', 	# Pods will be built by installman.
    INC => "-I$ENV{'ROOT'}/usr/include",
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'MDBM_File.pm',
);
