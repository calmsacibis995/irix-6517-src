#ident	"@(#)iconv:codesets/8859-1.dk.p	1.1"
#
# A complete sample 8859-1 map, mostly using dead keys.
#
#	Compose key = #
#	The following keys are dead:
#		`	grave
#		'	acute
#		"	umlaut
#		^	cirumflex
#		~	tilde
#
# The compose and dead keys can be entered by doubling
# them, e.g. ## for #
#
map full (8859-1.dk) {
	define(comp "#")	# compose key
	comp('#' '#')		# compose x 2

	define(gr '`')		# accent grave
	gr('`' '`')		# backquote

	define(ac '\047')	# accent acute
	ac('\047' '\047')	# single quote

	define(um '"')		# umlaut
	um('"' '"')		# double quote

	define(cir '^')		# circumflex
	cir('^' '^')		# carat

	define(til '~')		# tilde
	til('~' '~')		# tilde alone

	comp('!' '\241')	# Spanish initial "!"
	comp("c/" '\242')	# US cents
	comp('L' '\243')	# British Pound
	comp("xo" '\244')	# currency
	comp('Y' '\245')	# Yen
	comp('|' '\246')	# broken bar
	comp('S' '\247')	# section sign
	comp('"' '\250')	# umlaut alone
	comp("co" '\251')	# copyright
	comp("^a" '\252')	# a sup (fem. ordinal)
	comp('<' '\253')	# <<
	comp('-' '\254')	# logical not
				# soft hyphen (255) omitted
	comp("rg" '\256')	# registered trademark
	comp("_" '\257')	# macron
	comp("dg" '\260')	# degrees
	comp("+-" '\261')	# plus or minus
	comp("^2" '\262')	# 2 sup
	comp("^3" '\263')	# 3 sup
	comp("'" '\264')	# accent acute
	comp("mu" '\265')	# micro sign
	comp('P' '\266')	# Pilcrow (paragraph)
	comp('.' '\267')	# middle dot
	comp(',' '\270')	# cedilla
	comp("^1" '\271')	# 1 sup
	comp("^o" '\272')	# o sup (masc. ordinal)
	comp('>' '\273')	# >>
	comp("/14" '\274')	# 1/4 frac.
	comp("/12" '\275')	# 1/2 frac.
	comp("/34" '\276')	# 3/4 frac.
	comp('?' '\277')	# Spanish initial "?"

	ac(A '\300')
	gr(A '\301')
	cir(A '\302')
	til(A '\303')
	um(A '\304')
	comp(Ao '\305')		# A with ring above
	comp(AE '\306')		# AE ligature
	comp("C," '\307')	# C cedilla
	gr(E '\310')
	ac(E '\311')
	cir(E '\312')
	um(E '\313')
	gr(I '\314')
	ac(I '\315')
	cir(I '\316')
	um(I '\317')

	comp("Eth" '\320')	# Capital Eth
	til(N '\321')
	gr(O '\322')
	ac(O '\323')
	cir(O '\324')
	til(O '\325')
	um(O '\326')
	comp("xx" '\327')	# multiplication
	comp("O/" '\330')	# O w/ oblique stroke
	gr(U '\331')
	ac(U '\332')
	cir(U '\333')
	um(U '\334')
	gr(Y '\335')
	comp(Th '\336')		# Capital Thorn
	comp(ss '\337')		# German sharp S
	gr(a '\340')
	ac(a '\341')
	cir(a '\342')
	til(a '\343')
	um(a '\344')
	comp(ao '\345')		# a with ring above
	comp(ae '\346')		# ae ligature
	comp("c," '\347')	# c cedilla
	gr(e '\350')
	ac(e '\351')
	cir(e '\352')
	um(e '\353')
	gr(i '\354')
	ac(i '\355')
	cir(i '\356')
	um(i '\357')
	comp("eth" '\360')	# small eth
	til(n '\361')
	gr(o '\362')
	ac(o '\363')
	cir(o '\364')
	til(o '\365')
	um(o '\366')
	comp(":-" '\367')	# division
	comp("/o" '\370')
	gr(u '\371')
	ac(u '\372')
	cir(u '\373')
	um(u '\374')
	ac(y '\375')
	comp("th" '\376')
	um(y '\377')
}
