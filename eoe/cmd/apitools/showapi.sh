#!/bin/sh

USAGE="$0: [-Dxx=yy][-l a,c,c++][-m o32,32,n32,64][-c] api files...\n\t-c - use cpp otherwise use unifdef\n\tapi is one of:\n\t\tirix4 - IRIX4.1\n\t\tirix5 - IRIX5.0\n\t\tposix - POSIX compliant\n\t\tansi - ANSI compliant\n\t\txopen - XOPEN4 compliant\n\t\txopenux - XPG4 with UX extensions\n\t\tposix93 - posix 1003.1b\n\t\tposix1c - posix 1003.1c(pthreads)\n\t\tcmplrs - use flags that cmplr ism uses (no SGI_SOURCE)\n\t\tabi - MIPS BB2.0"
if [ $# -eq 0 ]
then
	echo $USAGE
	exit 1
fi

# start with a clean slate!
# Note that if we use cpp - we can't -U then -D again, so we don't use these.
undefs="-U__mips -U_LANGUAGE_C -U_LANGUAGE_ASSEMBLY  \
	-U_LANGUAGE_C_PLUS_PLUS -U__cplusplus -U_POSIX_SOURCE -U_SGI_SOURCE \
	-U_SVR4_SOURCE -U_XOPEN_SOURCE -U__STDC__ -U_STYPES -U_STYPES_LATER \
	-U_LANGUAGE_FORTRAN -Uvax -Usparc -Umc68000 -Uaux \
	-U_BSD_COMPAT -U_KERNEL -U_BSD_SIGNALS \
	-U_SGI_MP_SOURCE -U_SGI_REENTRANT_FUNCTIONS -U_LINT -ULINT \
	-U_BSD_TYPES -U_KMEMUSER -U_STANDALONE -U_POSIX_4SOURCE \
	-U_XOPEN_SOURCE_EXTENDED -U_POSIX_C_SOURCE -U_ABI_SOURCE \
	-U_LARGEFILE64_SOURCE -U_MIPSABI_SOURCE"

# defines from standards.h
stdundefs="-U_POSIX90 -U_POSIX2 -U_POSIX93 -U_POSIX1C -U_NO_POSIX \
	-U_XOPEN4 -U_NO_XOPEN4 -U_SGIAPI -U_ABIAPI -U_NO_ABIAPI \
	-U_REENTRANT_FUNCTIONS -U_XOPEN4UX -U_ANSIMODE -U_NO_ANSIMODE \
	-U_LFAPI"

undefs="$undefs $stdundefs"

lang=-D_LANGUAGE_C
machconst="-D_ABIO32=1 -D_ABIN32=2 -D_ABI64=3 -D_MIPS_SZINT=32"
machabio32="-D__mips=1 -D_MIPS_ISA=1 -D_MIPS_FPSET=16 $machconst -D_MIPS_SIM=_ABIO32 -D_MIPS_SZLONG=32 -D_MIPS_SZPTR=32"

machabin32="-D__mips=3 -D_MIPS_ISA=3 -D_MIPS_FPSET=32 $machconst -D_MIPS_SIM=_ABIN32 -D_MIPS_SZLONG=32 -D_MIPS_SZPTR=32"

machabi64="-D__mips=3 -D_MIPS_ISA=3 -D_MIPS_FPSET=32 $machconst -D_MIPS_SIM=_ABI64 -D_MIPS_SZLONG=64 -D_MIPS_SZPTR=64"

# default to n32
mach=-n32
machabi=$machabin32

#
# Extra stuff that cpp pre-dfines - ANSI versions only!
#
extra="-D_MODERN_C -D__EXTENSIONS__ -D__host_mips -D__sgi -D__unix -D_MIPSEB"
usecpp=0

while getopts m:l:D:c opts
do
	case $opts in
	l)
		case $OPTARG in
		c) lang=-D_LANGUAGE_C ;;
		a) lang=-D_LANGUAGE_ASSEMBLY ;;
		f) lang=-D_LANGUAGE_FORTRAN ;;
		c++) lang="-D_LANGUAGE_C_PLUS_PLUS -D__cplusplus" ;;
		*) echo "Invalid language option (c, a, c++, f allowed)"; exit 1 ;;
		esac
		;;
	c)	usecpp=1;;
	D)
		user="$user -D$OPTARG"
		;;
	m)
		case $OPTARG in
		o32) mach=-o32 ; machabi=$machabio32 ;;
		n32) mach=-32 ; machabi=$machabin32 ;;
		32) mach=-32 ; machabi=$machabin32 ;;
		64) mach=-64 ; machabi=$machabi64 ;;
		*) echo "Invalid ABI (32, n32, o32, 64)"; exit 1 ;;
		esac
		;;
	\?)	echo $USAGE
		exit 1 ;;
	esac
done
shift `expr $OPTIND - 1`

case $1 in
	irix4) defs="" ;;
	irix5|sgi)
		defs="-D_SGI_SOURCE -D_SVR4_SOURCE"
		# add what standards.h would
		if [ $usecpp -eq 0 ]
		then
			defs="$defs -D_POSIX93 -D_POSIX90 -D_POSIX2 -D_POSIX1C \
				-D_XOPEN4 -D_SGIAPI -D_NO_XOPEN4 -D_NO_POSIX \
				-D_NO_ABIAPI -D_XOPEN4UX -D_NO_ANSIMODE \
				-D_LFAPI"
		fi
		;;
	# by turning on SGI and SVR4 make a harder test!
	posix)
		defs="-D_SGI_SOURCE -D_SVR4_SOURCE -D_POSIX_SOURCE"
		# add what standards.h would
		if [ $usecpp -eq 0 ]
		then
			defs="$defs -D_POSIX90 -D_NO_ANSIMODE"
		fi
		;;
	posix93)
		defs="-D_SGI_SOURCE -D_SVR4_SOURCE -D_POSIX_C_SOURCE=199309L"
		# add what standards.h would
		if [ $usecpp -eq 0 ]
		then
			defs="$defs -D_POSIX93 -D_POSIX90 -D_POSIX2 -D_NO_XOPEN4 -D_NO_ABIAPI -D_NO_ANSIMODE"
		fi
		;;
	posix1c)
		defs="-D_SGI_SOURCE -D_SVR4_SOURCE -D_POSIX_C_SOURCE=199506L"
		# if new standards.h header add _POSIX93
		if [ $usecpp -eq 0 ]
		then
			defs="$defs -D_POSIX93 -D_POSIX90 -D_POSIX2 -D_NO_XOPEN4 -D_NO_ABIAPI -D_POSIX1C -D_NO_ANSIMODE"
		fi
		;;
	ansi)
		# if we are using cpp - let it define things
		if [ $usecpp -eq 1 ]
		then
			extra=
		else
			defs="-D__STDC__"
			defs="$defs -D_POSIX90 -D_POSIX2 -D_POSIX93 -D_POSIX1C -D_NO_ABIAPI -D_XOPEN4 -D_XOPEN4UX -D_NO_POSIX -D_NO_XOPEN4 -D_ANSIMODE"
		fi
		;;
	xopenux)
		defs="-D_SGI_SOURCE -D_SVR4_SOURCE -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED=1"
		if [ $usecpp -eq 0 ]
		then
			defs="$defs -D_POSIX90 -D_POSIX2 -D_NO_POSIX -D_XOPEN4 \
				-D_NO_ABIAPI -D_XOPEN4UX -D_NO_ANSIMODE"
		fi
		;;
	xopen)
		defs="-D_SGI_SOURCE -D_SVR4_SOURCE -D_XOPEN_SOURCE"
		if [ $usecpp -eq 0 ]
		then
			defs="$defs -D_POSIX90 -D_POSIX2 -D_NO_POSIX -D_XOPEN4 -D_NO_ABIAPI -D_NO_ANSIMODE"
		fi
		;;
	abi)
		# BB2.0
		defs="-D_SGI_SOURCE -D_SVR4_SOURCE -D_XOPEN_SOURCE \
			-D_XOPEN_SOURCE_EXTENDED=1 -D_MIPSABI_SOURCE=2 \
			-D_LARGEFILE64_SOURCE"
		if [ $usecpp -eq 0 ]
		then
			defs="$defs -D_POSIX90 -D_POSIX2 -D_NO_POSIX -D_XOPEN4 \
				-D_ABIAPI -D_XOPEN4UX -D_NO_ANSIMODE -D_LFAPI"
		fi
		;;
	cmplrs)
		defs=
		if [ $usecpp -eq 0 ]
		then
			defs="$defs -D_POSIX93 -D_POSIX90 -D_POSIX2 -D_POSIX1C -D_XOPEN4 -D_SGIAPI -D_NO_XOPEN4 -D_NO_POSIX -D_NO_ABIAPI -D_XOPEN4UX -D_NO_ANSIMODE"
		fi
		;;
	*)	echo "Invalid api!"
		echo $USAGE;
		exit 1
		;;

esac
shift

defs="$defs $machabi"

#echo "Undefs $undefs"
#echo "Language $lang"
#echo "Defs $defs"
if [ $usecpp -eq 0 ]
then
	unifdef $undefs $lang $defs $extra $user $@
else
	# use -ansi - that predefines the fewest things..
	cc -v -ansi -E -I -I$ROOT/usr/include  $mach $lang $defs $extra $user $@ | \
		sed -e '/^#/d' -e '/^$/d' -e '/^[ 	]*$/d'
fi
