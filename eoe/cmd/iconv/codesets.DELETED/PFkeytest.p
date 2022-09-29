#ident	"@(#)iconv:codesets/PFkeytest.p	1.1"
#
# A sample test for timed input of ANSI-style arrow keys.
#
map (funkey) {
	timed
	define(fun "\033[")
	fun(A "funkeyA")
	fun(B "funkeyB")
	fun(C "funkeyC")
	fun(D "funkeyD")
}
