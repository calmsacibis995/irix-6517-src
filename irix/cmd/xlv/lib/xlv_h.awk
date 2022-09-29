# This is an awk script to generate a .h file defining all the error
# codes.
#
# The input file is assumed to be of the form:
# XLV_VOLUME_BAD	"volume is bad".
#

# Execute the following initialization code before trying to match
# any symbols.
# gencat message numbers must start with 1.

BEGIN { 
	print "/* ** THIS FILE WAS AUTOMATICALLY GENERATED ** */"
        msg_number = 1
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
# For each error message ID, output the message number, a space, and 
# the explanatory text.
#
/^[A-Za-z]/ {
        printf("#define %s %d\n", $1, msg_number)
        msg_number++
        next
}
