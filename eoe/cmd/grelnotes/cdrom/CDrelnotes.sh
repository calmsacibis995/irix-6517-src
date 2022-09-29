#! /bin/sh
#Tag 0x00000600

# 'relnotes' - View on-line release notes

SRCDIR=`dirname $0`/relnotes/

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

if [ "$1" = "-h" ] # give help and exit
then
	echo $Usage
	exit
fi

list_relnotes=/tmp/relnoteslist$$
product=/tmp/relnotesproduct$$
cleanup="rm -f $list_relnotes $product"
trap "$cleanup" 1 2 3 15

# Create a list of products which have release notes installed.
HERE=`pwd`
cd $SRCDIR
find . -follow -type f -name "ch*.z" -print |  \
    sed -e 's%./%%' -e 's%/ch.*\.z%%' | sort -u > $list_relnotes
cd $HERE
if [ ! -s $list_relnotes ]
then
	echo "Sorry, but no products have release notes installed\n"
	rm -f $list_relnotes
	exit
fi

if [ $# -eq 0 ]	# no args - show installed relnotes
then
	echo "The following products have release notes installed:\n"
	cat $list_relnotes
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
	if [ `expr "$1" : '\(.\).*'` = "-" ]
	then
	  echo "$1 is an invalid option."
	  echo $Usage
	  $cleanup
	  exit 1
	fi

	if [ "$validproduct" = "no" ]
	then
	  echo $1 > $product
	  match=`comm -12 $list_relnotes $product  | wc -l`
	  if [ $match -eq 1 ];
	  then
	     validproduct=$1	
	     shift
	     if [ $# -gt 0 ];
	     then
			continue
	     fi
	     if [ -f $SRCDIR$validproduct/TC ];
	     then
	    	echo "The chapters for the \"$validproduct\" product's release notes are:\n"
	    	cat $SRCDIR$validproduct/TC 
	     else
	    	echo "The \"$validproduct\" product's release notes are installed, "
	    	echo "but its table of contents file is missing.\n"
	    	echo "The chapters that are installed are:\n"
	    	cd $SRCDIR${validproduct}; /bin/ls ch*.z | sed -e 's/ch//' -e 's/.z//'
	     fi
	  else  # Not an installed product
	     echo "Sorry, but there are no installed release notes for the \"$1\" product.\n"
	     $cleanup ; exit 1
	  fi
	else # Have a valid product.
	  cd $SRCDIR$validproduct
          # Check for the existence of ch#.z. If not found check
          # for ch0#.z. If still not found report an error
          if [ -r ch${1}.z ]; then
                man $tflag -d ch${1}.z
          elif [ -r ch0${1}.z ]; then
                man $tflag -d ch0${1}.z
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
