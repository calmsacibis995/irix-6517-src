/*
 * Purpose: example test benchmark
 */

#include "hrn.h"

/* 
 * This bitmask specifies which thread libraries we can use 
 */
HRN_DECLARE_LIBCAP(HRN_LIBCAP_ALL);

/* 
 * The harness will start us executing here 
 */
int hrn_main(hrn_args_t* args)
{
	trc_info("your test here\n");
	return 0;
}

/* 
 * DEF_PARSER is a macro for defining trivial parser functions 
 */
DEF_PARSER(parsetest, trc_result("saw command-line\n"))

hrn_option_t hrn_options[]= {
	{ "Z", "Test command-line options",
		"Verbose description here", 
		parsetest },
	{ 0, 0, 0, 0 }
};

hrn_doc_t hrn_doc = {
	"Template",			/* name */

	"Example thread benchmark",	/* purpose */

	"nop",				/* operation */

	"No results reported.\n"	/* results */
};

