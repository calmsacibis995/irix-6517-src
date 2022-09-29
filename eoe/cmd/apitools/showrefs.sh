#!/bin/sh
#
# Show library references - those that are not prefixed by an '_'
# In general a library should enter into the users namespace only
# those symbols that are its external public interface. All others
# should not be visible. In addition, routines (other than ANSI)
# from other libraries should not be made visible
#
# In general undefines should not be satisfied by a weak symbol since
# these can (in general) be overridden by a users version
# (one could use non-preemptible symbols)
#
# Another set of options (-h, -p use elfdump -Dt to find hidden and non-hidden symbols)
#

USAGE="showrefs [-AadeghimnNPpruvw][-D<dir>][-M<manpath>] files\n
\t-A show ansi functions\n
\t-a show all instances (normally run through sort -u)\n
\t-D directory to use\n
\t-d show declarations only\n
\t-e show public interface\n
\t-g print a list of global variables\n
\t-h show hidden symbols\n
\t-i show implementation interface (compliant public symbols)\n
\t-M manpath to use\n
\t-m print all public symbols with man pages\n
\t-N show non-compliant public symbols without man pages\n
\t-n print all public symbols w/o man pagesn\n
\t-P show public symbols && demangled names\n
\t-p show public (non-hidden && non-compiler) symbols\n
\t-r show raw applications interface (no man page check)\n
\t-u show undefines only\n
\t-v verbose\n
\t-w show weak symbols\n"


if [ $# -eq 0 ]
then
	echo $USAGE
	exit 1
fi

trap "{ rm -f /usr/tmp/[a-z]showrefs$$ fshowrefs$$* ; exit 0; }"  0 1 2 3

#
# trans - do simple transitive closure
#
trans() {
# grab all undefines

$TOOLROOT/usr/bin/nm -Bou $lib | egrep -v ' _| N ' |  egrep ' U | V ' |
			sort +3 -4 >/usr/tmp/ashowrefs$$

# grab all defines - these always get output
$TOOLROOT/usr/bin/nm -Bog $lib | egrep -v ' _| N ' |  egrep -v ' U | V ' |
			stripbogus | sort +3 -4 | tee /usr/tmp/bshowrefs$$

#
# Create a file with just the defines in it
#
if [ "$noweak" = "true" ]
then
	grep -v '(weak)' /usr/tmp/bshowrefs$$ | tr '	' ' ' |
		cut -d ' ' -f4 | sed -e 's/^/ /' -e 's/$/\$/' >/usr/tmp/eshowrefs$$
else
	cat /usr/tmp/bshowrefs$$ | tr '	' ' ' |
		cut -d ' ' -f4 | sed -e 's/^/ /' -e 's/$/\$/' >/usr/tmp/eshowrefs$$
fi
dotrans

}

utrans() {

# grab all undefines

$TOOLROOT/usr/bin/nm -Bou $lib | egrep -v ' N ' |  egrep ' U | V ' |
			sort +3 -4 >/usr/tmp/ashowrefs$$

# grab all defines - these always get output

$TOOLROOT/usr/bin/nm -Bog $lib | egrep -v ' N ' |  egrep -v ' U | V ' |
			stripbogus | sort +3 -4 | tee /usr/tmp/bshowrefs$$

#
# Create a file with just the defines in it
#
if [ "$noweak" = "true" ]
then

	grep -v '(weak)' /usr/tmp/bshowrefs$$ | tr '	' ' ' |
		cut -d ' ' -f4 | sed -e 's/^/ /' -e 's/$/\$/' >/usr/tmp/eshowrefs$$
else

	cat /usr/tmp/bshowrefs$$ | tr '	' ' ' |
		cut -d ' ' -f4 | sed -e 's/^/ /' -e 's/$/\$/' >/usr/tmp/eshowrefs$$
fi
dotrans

}

dotrans() {

# fgrep has lots of limits, so we split the file
split -l400 /usr/tmp/eshowrefs$$ fshowrefs$$
cp /usr/tmp/ashowrefs$$ /usr/tmp/gshowrefs$$
for i in fshowrefs$$*
do
	cat /usr/tmp/gshowrefs$$ | fgrep -v -f $i >/usr/tmp/hshowrefs$$
	mv /usr/tmp/hshowrefs$$ /usr/tmp/gshowrefs$$
done
cat /usr/tmp/gshowrefs$$
}


stripansi() {
egrep -v ' isalnum$| errno$| isalpha$| iscntrl$| isdigit$| isgraph$| islower$| isprint$| ispunct$| isspace$| isupper$| isxdigit$| tolower$| toupper$| setlocale$| localeconv$| acos$| asin$| atan$| atan2$| cos$' |

egrep -v ' sin$| tan$| cosh$| sinh$| tanh$| exp$| frexp$| ldexp$| log$| log10$| modf$| pow$| sqrt$| ceil$| fabs$| floor$| fmod$| setjmp$| longjmp$| signal$| raise$| va_start$| va_end$| remove$| rename$| tmpfile$' |

egrep -v ' tmpnam$| fclose$| fflush$| fopen$| freopen$| setbuf$| setvbuf$| fprintf$| fscanf$| printf$| scanf$| sprintf$| sscanf$| vfprintf$| vprintf$| vsprintf$| fgetc$| fgets$| fputc$| fputs$| getc$| getchar$| gets$| putc$' |

egrep -v ' putchar$| puts$| ungetc$| fread$| fwrite$| fgetpos$| fseek$| fsetpos$| ftell$| rewind$| clearerr$| feof$| ferror$| perror$| atof$| atoi$| atol$| strtod$| strtol$| strtoul$| rand$| srand$| calloc$| free$' |

egrep -v ' malloc$| realloc$| abort$| atexit$| exit$| getenv$| system$| bsearch$| qsort$| abs$| div$| labs$| ldiv$| mblen$| mbtowc$| wctomb$| mbstowcs$| wcstombs$| memcpy$| memmove$| strcpy$| strncpy$| strcat$' |

egrep -v ' strncat$| memcmp$| strcmp$| strcoll$| strncmp$| strxfrm$| memchr$| strchr$| strcspn$| strpbrk$| strrchr$| strspn$| strstr$| strtok$| memset$| strerror$| strlen$| clock$| difftime$| mktime$| time$| asctime$| ctime$| gmtime$| localtime$| strftime$'
}

stripbogus() {
egrep -v ' \.text$| \.bss$| \.sbss$| \.data$| \.sdata$| \.lit4$| \.lit8$| \.rodata$| \.fini$| \.init$| \.rdata$| _gp_disp$'
}

printglobals() {
	elfdump -p -Dt $lib | awk '
		$1 ~ "\\[" &&
		$4 != "SECTION" &&
		$5 != "LOCAL" &&
		$6 != "PROTECT" &&
		($7 != "UNDEF" && $7 != "SUNDEF") \
		{ if (VERB == "true")
			print $8 " " $4 " " $5 " " $6 " " $7
		else
			print $8
		}' VERB="$verbose"
}

printpublic() {
	elfdump -p -Dt $lib | awk '
		$1 ~ "\\[" &&
		$4 != "SECTION" &&
		$5 != "LOCAL" &&
		($6 != "HIDDEN" && $6 != "PROTECT") &&
		($7 != "UNDEF" && $7 != "SUNDEF") \
		{ if (VERB == "true")
			print $8 " " $4 " " $5 " " $6 " " $7
		else
			print $8
		}' VERB="$verbose"
}

matchman() {
	while read i
	do
		res=`man -M $MPATH -w $i`
		rv=`expr "$res" : 'No manual.*'`
		#
		# If we didn't find a man page, and the name began with a '_'
		# look for man page for the name w/o the leading '_'
		#
		sq=`expr "$i" : '^_\(.*\)'`
		if [ $rv != 0 -a "$sq" != "" ]
		then
			res=`man -M $MPATH -w $sq`
			rv=`expr "$res" : 'No manual.*'` >/dev/null
		fi

		if [ $rv = 0 -a "$showman" = "positive" ]
		then
			# no match which means we found a man page
			echo $i $res
		elif [ $rv != 0 -a "$showman" = "negative" ]
		then
			echo $i
		else
			: nothing to do
		fi
	done
}
		

unique="| sort +3 -u"

nmargs=ug
nou="| trans"
showweak="| grep -v '(weak)'"
showansi="| stripansi"

# noweak true means that we don't let a weak symbol resolve
# an undefine.
noweak=true
filelist=
messy=false
verbose=false
MPATH=/usr/share/catman/p_man:/usr/share/man/p_man
showman=false

while getopts gvmNnhiPpreudawADC:KM: opts
do
	case $opts in
	u)
		#
		# if only want undeclared stuff then run trough utrans
		# and grep out declared stuff
		#
		if [ $nmargs = "g" ]
		then
			# they really want both
			nmargs=ug
			nou="| trans"
		else
			nmargs=ug
			nou="| utrans | egrep ' [UV] '"
		fi
		;;
	d)
		#
		# if only want declared stuff then don't
		# run through trans
		#
		if [ $nmargs = "u" ]
		then
			# they really want both
			nmargs=ug
			nou="| trans"
		else
			nmargs=g
			nou="| egrep -v ' [UV] '"
		fi
		;;
	w)
		showweak=
		;;
	a)
		unique=
		;;
	A)
		showansi=
		;;
	e)
		# public interface is really -dAw
		showweak=
		showansi=
		nmargs=g
		nou="| egrep -v ' [UV] '"
		;;
	i)
		# show all compliant public symbols (with leading
		# double underscore or underscore cap) 
		implementation=true
		;;
	N)
		# show non-compliant public symbols without man pages.
		# (Those that must be renamed or have man pages written).
		noncomp_noman=true
		showman=negative
		;;
	r)
		# raw applications interface (non-comp symb., no manpage check)
		raw_app=true
		;;

	D)
		# filter for all files in specified directory
		FL=`ls $OPTARG/*.[cs]`
		for i in $FL
		do
			basename $i | sed -e 's/\.[cs]/.o:/' >> /usr/tmp/dshowrefs$$
		done
		filelist="| fgrep -f /usr/tmp/dshowrefs$$"
		;;
	K)
		# messy/debug - don't remove temp files
		messy=true
		;;
	P)
		# show public symbols and demangled names
		showdemang=true
		;;
	p)
		# show public (non-hidden) symbols
		showpublic=true
		;;
	m)
		# try to match up man pages (print those with found man pages)
		showman=positive
		;;
	n)
		# try to match up man pages (print those w/o)
		showman=negative
		;;
	h)
		# show hidden symbols
		showhidden=true
		;;
	g)
		# show globals symbols
		showglobals=true
		;;
	M)
		MPATH=$OPTARG
		;;
	v)
		verbose=true
		;;
	\?)	echo $USAGE
		exit 1 ;;
	esac
done
shift `expr $OPTIND - 1`

if [ "$messy" = "true" ]
then
	trap "{ echo Temp files NOT removed: /usr/tmp/[a-z]showrefs$$ fshowrefs$$* ; exit 0; }"  0 1 2 3
fi

for i in $@
do
	if [ ! -r $i ]
	then
		echo "Cannot read $i"
		exit 1
	fi
	lib=$i
	if [ "$showpublic" = "true" ]
	then
		eval "printpublic | sort"
	elif [ "$showdemang" = "true" ]	
	then
		eval "printpublic | sort | $TOOLROOT/usr/sbin/exportsdem"
	elif [ "$composite" = "true" ]	
	then
		eval "printpublic | sort -u "
	elif [ "$noncomp_noman" = "true" ]	
	then
		eval "printpublic | awk '( ! /^__/) && (! /^_[A-Z]/)' \
		| sort -u | matchman"
	elif [ "$implementation" = "true" ]	
	then
		eval "printpublic | awk '/^__/ /^_[A-Z]/' | sort -u "
	elif [ "$raw_app" = "true" ]	
	then
		eval "printpublic | awk '( ! /^__/) && (! /^_[A-Z]/)' \
		| sort -u"
	elif [ "$showhidden" = "true" ]
	then
		elfdump -p -Dt $lib | awk '/HIDDEN/ { print $8 }' | sort
	elif [ "$showglobals" = "true" ]
	then
		eval "printglobals | sort"
	elif [ "$showman" != "false" ]
	then
		verbose="false"
		eval "printpublic | sort | matchman"
	else
		eval "$TOOLROOT/usr/bin/nm -Bo${nmargs} $i | egrep -v ' _| N ' | \
			stripbogus $nou $showweak $showansi $unique $filelist"

		# compute multiple defines
		if [ -s /usr/tmp/bshowrefs$$ ]
		then
			sort +3 -4 -u /usr/tmp/bshowrefs$$ |
				comm -13 - /usr/tmp/bshowrefs$$ >/usr/tmp/cshowrefs$$
			if [ -s /usr/tmp/cshowrefs$$ ]
			then
				echo "Multiple defines:"
				cat /usr/tmp/cshowrefs$$
			fi
		fi
	fi
done
