#ident	"@(#)iconv:codesets/Case.p	1.1"
#
# Sample ASCII Upper-to-lower, Lower-to-upper, and "case-swap" maps.
#
map (uclc) {
	keylist("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz")
}

map (lcuc) {
	keylist("abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ")

}

map (uclc_lcuc) {
	keylist("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz")
	keylist("abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ")
}
