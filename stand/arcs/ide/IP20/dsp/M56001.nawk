# generate a include file, dsp_diag.h, from a Motorola 56001 loadable file.
# the loadable file must have at most 3 sections, a DATA P section and/or a
# DATA X section, and/or a DATA Y section. Each section must be contiguous

($1 == "_DATA") && ($2 == "P") {
	printf "#define DSP_TEXT_ORG 0x%s\n",$3
	printf "static u_int dsp_text[] = {\n"
}
($1 == "_DATA") && ($2 == "Y") {
	printf "};\n\n"
	printf "#define DSP_YDATA_ORG 0x%s\n",$3
	printf "static u_int dsp_ydata[] = {\n"
}
($1 == "_DATA") && ($2 == "X") {
	printf "};\n\n"
	printf "#define DSP_XDATA_ORG 0x%s\n",$3
	printf "static u_int dsp_xdata[] = {\n"
}
($1 == "_END") {
	printf "};\n\n"
}
($1 != "_DATA") && ($1 != "_START") && ($1 != "_END") && ($0 != "") {
	printf "    "
	for (i = 1; i <= NF; i++) {
		printf "0x%s, ", $i
		if (i == 5)
			printf "\n    "
	}
	printf "\n"
}
