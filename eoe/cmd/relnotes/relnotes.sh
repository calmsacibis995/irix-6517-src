#!/sbin/ksh

# 'relnotes' - View on-line release notes

Usage="\n
	'relnotes -h' -- print this message\n
	'relnotes' -- list products that have on-line release notes \n
\t\t\tcurrently installed\n
	'relnotes <product>' -- show table of contents for a product's \n
\t\t\ton-line release notes\n
	'relnotes <product> <chapter> ... ' -- display specific chapters of \n
\t\t\t\ta product's on-line release notes\n
	'relnotes -t <product> <chapter> ... ' -- print specific chapters of \n
\t\t\t\ta product's on-line release notes\n"

if [ "${1:-}" = "-h" ] # give help and exit
then
	echo $Usage
	exit
fi

# Determine the root relnotes directory
if [ "${RELNOTESPATH:-}" = "" ]
then
	relpath=/usr/relnotes/
else
	relpath=`dirname $RELNOTESPATH`/`basename $RELNOTESPATH`/
fi

list_relnotes=/tmp/relnoteslist$$
product=/tmp/relnotesproduct$$
cleanup="rm -f $list_relnotes $product"
trap "$cleanup" 1 2 3 15

# Create a list of products which have release notes installed.
find $relpath -follow -type f -name "TC" -print |  \
    sed  -e "s%$relpath%%" -e 's%/TC%%' | sort -u > $list_relnotes
if [ ! -s $list_relnotes ]
then
	echo "Sorry, but no products have release notes installed\n"
	rm -f $list_relnotes
	exit
fi

max=0
length=0
while read prodname
do
        length=`echo -n "$prodname" | wc -c`
        if test $length -gt $max; then
                max=`expr $length`
        fi
done < $list_relnotes

if test $max -gt 35; then
        columns=1
elif test $max -gt 23; then
        columns=2
elif test $max -gt 17; then
        columns=3
else
        columns=4
fi

if [ $# -eq 0 ]	# no args - show installed relnotes
then
	echo "The following products have release notes installed:\n"
	pr -t -$columns $list_relnotes
	$cleanup
	exit
fi

validproduct=no
tflag=
while [ $# -gt 0 ] # As long as we have arguments ....
do
	# Recognize old-style args, but don't support them.
	if [ "$1" = "-p" ]
	then
	  echo "The -p and -c options are no longer needed."
	  echo $Usage
	  $cleanup 
	  exit 1
	fi

	# Support for printing chapters.
	if [ "$1" = "-t" ]
	then
	  tflag=$1
	  shift 
	  continue
	fi
	
	# Invalid option?
	case "$1" {
	-*)
	  echo "$1 is an invalid option."
	  echo $Usage
	  $cleanup
	  exit 1
	  ;;
	}

	if [ "$validproduct" = "no" ]
	then
	  echo $1 > $product
	  match="`grep -i \"^$1$\" $list_relnotes`"
	  if [ -n "$match" ]
	  then
	     validproduct="$match"
	     shift
	     if [ $# -gt 0 ];
	     then
			continue
	     fi
	     if [ -f ${relpath}${validproduct}/TC ];
	     then
	    	echo "The chapters for the \"$validproduct\" product's release notes are:\n"
	    	cat ${relpath}${validproduct}/TC 
	     else
	    	echo "The \"$validproduct\" product's release notes are installed, "
	    	echo "but its table of contents file is missing.\n"
	    	echo "The chapters that are installed are:\n"
	    	cd ${relpath}${validproduct}; /bin/ls ch*.z | sed -e 's/ch//' -e 's/.z//'
	     fi
		 echo "\nUse \"$0 productname chapter\" to view a chapter"
	  else  # Not an installed product
	     echo "Sorry, but there are no installed release notes for the \"$1\" product.\n"
	     $cleanup ; exit 1
	  fi
	else # Have a valid product.
	  cd ${relpath}${validproduct}
	  # Check for the existence of ch#.z. If not found check
	  # for ch0#.z. If still not found report an error
	  if [ -r ch${1}.z ]; then
	  	man $tflag -d ch${1}.z
	  elif [ -r ch0${1}.z ]; then
	  	man $tflag -d ch0${1}.z
	  elif [ -r ${validproduct}.gz ]; then
		# Find the name of the ${1}th chapter to pass as an arg to html2term
		chapter=`egrep "^${1}" TC | cut -f2`;
		gzcat -c ${validproduct}.gz | html2term -c "$chapter" | ul -b | more -s -f
	  else
		echo "There is no chapter $1 in the \"$validproduct\" release notes."
	  fi
	  shift
	  if [ $# -gt 0 ];
	  then
		echo "Next chapter ('q' to quit):\c"
		read ans
		if [ "$ans" = "q" ];
		then
			$cleanup
			exit
		fi
	  fi
	fi
done
$cleanup
