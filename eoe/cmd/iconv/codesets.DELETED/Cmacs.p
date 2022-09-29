#ident	"@(#)iconv:codesets/Cmacs.p	1.1"
#
# Sample mapping for C programming with EMACS.  The ^B characters
# move back to an appropriate spot in the insertion.
# This is only a sample.
#
map (Cmacs) {
	string("for" "for ( ; ; )")
	string("whi" "while ()")
	#
	# Comment is set up for auto-indent mode...
	#
	string("/*" "/*\n * \n*/")
	string("main" "main(argc, argv)\nint argc;\nchar **argv;\n{\n\n}\n	")
}

#
# Define Arrow keys for ANSI type terminals.
#
map (funkeys) {
	timed
	define(fun "\033[")
	fun(A "\020")		# up arrow
	fun(B "\016")		# down
	fun(D "\002")		# left
	fun(C "\006")		# right
}

link(Funmacs:funkeys,Cmacs)
