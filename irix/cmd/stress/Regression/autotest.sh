#!/bin/sh
#
# Perform automatic testing, generate reports, and mail them off

# NO_CYCLES it the number of times it will execute the test
NO_CYCLES=5

# Location of regression tests
LOCATION=/usr/stress

# Send reports to who???
# should look like "foo@machine foo2@othermachine"
#
REPORT_MAIL=

#
# For list of tests being run - check out the runall.auto or runall
#

#
# Initialize variables
#
TERM=vt100
export TERM
PATH=/usr/sbin:/usr/local/bin:/usr/bsd:/sbin:/usr/bin:/bin:/usr/bin/X11:.
export PATH
cd $LOCATION

# Run the tests
begin=`date | tr ' ' '_'`
./runall.auto $NO_CYCLES
end=`date | tr ' ' '_'`

# Generate the reports & mail them
./genreport -b $begin -e $end | \
Mail -s "Daily Regression Test Report" $REPORT_MAIL

# Generate list of core files & mail them
find . -name "core*" -print | \
Mail -s "Daily Regression Test CORE file Report" $REPORT_MAIL

exit 0
