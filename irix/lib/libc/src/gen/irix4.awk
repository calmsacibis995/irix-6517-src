#ident	"$Revision: 1.1 $"

# create a file from a list of input strings,
# irix4_lst.c contains an array of characters indexed into by perror
# and strerror,

BEGIN	{
		FS = "\t"
		hi = 0

		irixerr = "irix4_list.c"

		print "#ident\t\"$Revision: 1.1 $\"\n" >irixerr
		print "/*LINTLIBRARY*/" >irixerr
		print "#include \"synonyms.h\"\n" >irixerr

		print "#if defined(_STYPES) || defined(_STYPES_LATER)\n" > irixerr
	}

/^[0-9]+/ {
		if ($1 > hi)
			hi = $1
		astr[$1] = $2
	}

END	{
		print "const int _sys_index[] =\n{" >irixerr
		k = 0
		for (j = 0; j <= hi; ++j)
		{
			if (astr[j] == "")
				astr[j] = sprintf("Error %d", j)
			printf "\t%d,\n", k >irixerr
			k += length(astr[j]) + 1
		}
		print "};\n" >irixerr

		print "const char _sys_errs[] =\n{" >irixerr
		for (j = 0; j <= hi; ++j)
		{
			printf "\t" >irixerr
			n = length(astr[j])
			for (k = 1; k <= n; ++k)
				printf "'%s',", substr(astr[j],k,1) >irixerr
			print "'\\0'," >irixerr
		}
		print "};\n" >irixerr

		print "const int _sys_num_err = " hi + 1 ";" >irixerr

		print "#endif /* _STYPES  || _STYPES_LATER */\n" >irixerr
	}
