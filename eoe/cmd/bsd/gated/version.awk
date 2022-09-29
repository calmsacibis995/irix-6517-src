#
#	$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/version.awk,v 1.1 1989/09/18 19:47:22 jleong Exp $
#
BEGIN {
	maxfields = 4;
	max = 0; strmax = ""; test =""; local="";
	for (i = 1; i <= maxfields; i++) {
		power[i] = exp(log(10)*(maxfields-i));
	}
}
{
	if (NF >= 3) {
		version = "";
		if ( $3 == "*rcsid" ) {
			version = $7;
                        locked = $11;
			newlock = $12;
		} 
		if ( $1 == "*" ) {
			version = $4;
			locked = $8;
			newlock = $9;
		}
		if ( version == "" ) {
			continue;
		}
                if ( locked == "Locked" ) {
			test = ".development";
		}
		sum = 0;
		num = split(version, string, ".")
		if (num > maxfields) {
			local = ".local";
			num = maxfields;
		}
		for (i = 1; i <= num; i++) {
			sum += string[i]*power[i];
		}
		if ( sum > max ) {
			max = sum;
			strmax = version;
		}
	}
}
END {
	print "char *version = \"" strmax local test "\";" > "version.c"
}

