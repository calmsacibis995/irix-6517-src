#
# "$Revision: 1.12 $"
#
# Generate a C structure from a table of commands
#
BEGIN	{
	fname=TBL;
	print "#include \"ide.h\"";
	print "#include \"genpda.h\"";
	count = 0;
}
/^[^#]/ {
	name[count] = $1;
	type[count] = $2;
	addr[count] = $3;
	if ( NF > 3 )
		help[count] = $4;
	else
		help[count] = "";
	if ( NF > 4 )
		flag[count] = $5
	else
		flag[count] = ""
	count++;
}
END	{

	for ( i  = 0; i < count; i++ )
	    if ( (type[i] == "CMD" || type[i] == "SCMD" || type[i]== "SET_CMD"\
		|| type[i] == "DIAG_CMD" || type[i] == "DIAG_SCMD"  \
		|| type[i] == "DEBUG_CMD"|| type[i] == "DEBUG_SCMD" \
		|| type[i] == "SPEC_CMD" || type[i] == "SPEC_SCMD") \
		&& (flag[i] != "noproto") )
		printf("extern int %s();\n", addr[i]);

	printf("\nbuiltin_t %s[] = {\n", fname);

	for ( i = 0; i < count; i++ )
	if ( type[i] == "INT" )
		printf("\t\"%s\",\t(char *)%s,\tB_INT,\"%s\",\n",
			name[i],addr[i],help[i]);
	else
	if ( type[i] == "STR" )
		printf("\t\"%s\",\t%s,\tB_STR,\"%s\",\n",name[i],
			addr[i],help[i]);
	else
	if ( type[i] == "SETSTR" )
		printf("\t\"%s\",\t%s,\tB_SETSTR,\"%s\",\n",name[i],
			addr[i],help[i]);
	else
	if ( type[i] == "CMD" )
		printf("\t\"%s\",\t(char *)%s,\tB_CMD,\"%s\",\n",
			name[i],addr[i],help[i]);
	else
	if ( type[i] == "SCMD" )
		printf("\t\"%s\",\t(char *)%s,\tB_SCMD,\"%s\",\n",
			name[i],addr[i],help[i]);
	else
	if ( type[i] == "SET_CMD" )
		printf("#ifdef MULTIPROCESSOR\n\t\"%s\",\t(char *)%s,\tB_SET_CMD,\"%s\",\n#endif\n",
			name[i],addr[i],help[i]);
	else
	if ( type[i] == "DEBUG_CMD" )
		printf("\t\"%s\",\t(char *)%s,\tB_DEBUG_CMD,\"%s\",\n",
			name[i],addr[i],help[i]);
	else
	if ( type[i] == "DEBUG_SCMD" )
		printf("\t\"%s\",\t(char *)%s,\tB_DEBUG_SCMD,\"%s\",\n",
			name[i],addr[i],help[i]);
	else
	if ( type[i] == "SPEC_CMD" )
		printf("\t\"%s\",\t(char *)%s,\tB_CPUSPEC_CMD,\"%s\",\n",
			name[i],addr[i],help[i]);
	else {
		print "\tERROR, builtins.awk: bad type: ", type[i] | "cat 1>&2"	
		print "\n" | "cat 1>&2"
		exit 1;
	}

	printf("};\nint n%s = sizeof(%s) / sizeof(builtin_t);\n",fname,fname);
}
