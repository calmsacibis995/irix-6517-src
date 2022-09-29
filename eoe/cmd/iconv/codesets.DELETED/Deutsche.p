#ident	"@(#)iconv:codesets/Deutsche.p	1.1"
#
# Sample simple mapping for u-umlaut, y->z, z->y, ss and "".
# This is a sample only.
#
map (Deutsche) {
	define(umlaut \042)
	umlaut(u '\374')
	umlaut(\042 \042)
	string("#s" '\315')
	keylist("yzYZ" "zyZY")
}
