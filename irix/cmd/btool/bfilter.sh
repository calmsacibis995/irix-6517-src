#!/bin/sh

if [ "x$1" = "x-v" ]
then
	grep -v "TRUE [1-9][0-9]*, FALSE [1-9][0-9]*" | \
		grep -v "was taken [1-9][0-9]* times" | \
		grep -v "was called [1-9][0-9]* times"
else
	grep -v "TRUE 0, FALSE 0" | grep -v "was taken 0 times"  | \
		grep -v "was called 0 times"
fi
