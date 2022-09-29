# This is an awk script to generate a C file containing an array
# of error strings indexed by the status code.
#
# The input file is assumed to be of the form:
# XLV_VOLUME_BAD	"volume is bad".
#

# Execute the following initialization code before trying to match
# any symbols.
# gencat message numbers must start with 1.

BEGIN { 
	print "/*"
	print " * THIS FILE WAS AUTOMATICALLY GENERATED *"
	printf (" */\n\n")

	print "char *xlv_error_text [] = {"
}

#
# Skip blank lines
#

/^[ \t]*$/ {
        next
}

#
# Skip any comments.  Comments start with '$'.
# 

/^[$]/ {
        next
}


#
# For each error message, output the explanatory text as a constant
# string in the array.
#
/^[A-Za-z]/ {
        printf ("\"")
	for (i=2; i <= NF; i++) {
		printf ("%s", $i)
		if (i < NF) {
			printf (" ")
		}
	}
	printf ("\",\n")
        next
}

END {
	print "\"\"};"
}
