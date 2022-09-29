#! /bin/sh
# NAME
#	newvers.sh - generate a new standalone version control file
# USAGE
#	sh newvers.sh prefix release_maj release_min version opts
# NOTES
#	prefix		prepended to "_version" to yield the name of the
#			global version string variable (char *).
#	release_maj	is the current version number
#	release_min	is the current revision number
#	version		distinguishes the PROM saio from the downloadable
#			version, cpu type, etc., and optimized from
#			unoptimized.
#	opts		provides the compilation options (CFLAGS)
#
cat << EOF
/* Configuration Control File */
char *$1_version = "SGI Version $2.$3 $4 built `date +'%r %h %e, %Y'`";
char *getversion(void) {return $1_version;}
unsigned char prom_versnum = $2;
unsigned char prom_revnum = $3;
char *$1_opts = "$5";
EOF
