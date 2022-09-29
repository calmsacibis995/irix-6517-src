#!/bin/ksh

# Run prodSume test in various configurations

prog=${1:-./prodSume}
tmp=${TMPDIR:-/usr/tmp}

outfile=${TMPDIR:-/usr/tmp}/prodSumeRes

# Set these as required
consList=${consList:-1 2 3 4 5 8 9 16 17 32 33}
prodList=${prodList:-1 2 3 4 5 8 9 16 17 32 33}

iterMax=${iterMax:-3}

# Repeat runs

i=1
while [ $i -le ${iterMax} ]; do

	# Change producers
	for prod in ${prodList}; do

		# Change consumers
		for cons in ${consList}; do

			echo "${prog}/$i: p ${prod} c ${cons}"
			cat <<EOF |
TESTING0.line.of.text
TESTING1.line.of.text
TESTING2.line.of.text
TESTING3.line.of.text
TESTING4.line.of.text
TESTING5.line.of.text
TESTING6.line.of.text
TESTING7.line.of.text
TESTING8.line.of.text
TESTING9.line.of.text
TESTINGa.line.of.text
TESTINGb.line.of.text
TESTINGc.line.of.text
TESTINGd.line.of.text
TESTINGe.line.of.text
TESTINGf.line.of.text
EOF
			if ! ${prog} -p ${prod} -c ${cons} > ${outfile}; then
				echo ":ERROR: ${prog} bad exit $?"
				exit 1;
			fi
		done
	done
	i=$((i + 1))
done

rm -f ${outfile}

exit 0
