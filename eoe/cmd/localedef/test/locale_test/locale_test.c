/*	    		       langinfo Samples 			     */

#include <sys/types.h>
#include <stdio.h> 
#include <locale.h>
#include <nl_types.h>
#include <langinfo.h>                   /* Definitions for locale defs.      */
#include <time.h>
#include <unistd.h>

struct lconv *table;
time_t tim;

void get_LC_CTYPE(void)
{
	printf("\n LC_CTYPE locale: \t%s\n", setlocale(LC_CTYPE, NULL));

	printf("\tCodeset: \t%s\n", nl_langinfo ( CODESET ) ) ;

}

void get_LC_COLLATE(void)
{
	printf("\n LC_COLLATE locale: \t%s\n", setlocale(LC_COLLATE, NULL));
}

void get_LC_TIME(void)
{
	char	str[100];

	printf("\n LC_TIME locale: \t%s\n", setlocale(LC_TIME, NULL));

	/* Value for Wed Jul 23 18:28:19 1997 */
	tim = 869707699;
	/* tim = time(NULL); */

	printf("==============================\n");
	printf("\n\tFull named days of the week symbols\n");
	printf("\t------------------------------------\n");
	printf("\tnl_langinfo(DAY_1)\t%s\n", nl_langinfo(DAY_1));
	printf("\tnl_langinfo(DAY_2)\t%s\n", nl_langinfo(DAY_2));
	printf("\tnl_langinfo(DAY_3)\t%s\n", nl_langinfo(DAY_3));
	printf("\tnl_langinfo(DAY_4)\t%s\n", nl_langinfo(DAY_4));
	printf("\tnl_langinfo(DAY_5)\t%s\n", nl_langinfo(DAY_5));
	printf("\tnl_langinfo(DAY_6)\t%s\n", nl_langinfo(DAY_6));
	printf("\tnl_langinfo(DAY_7)\t%s\n", nl_langinfo(DAY_7));
	printf("\n");

	printf("\tAbbreviated days of the week symbols\n");
	printf("\t------------------------------------\n");
	printf("\tnl_langinfo(ABDAY_1)\t%s\n", nl_langinfo(ABDAY_1));
	printf("\tnl_langinfo(ABDAY_2)\t%s\n", nl_langinfo(ABDAY_2));
	printf("\tnl_langinfo(ABDAY_3)\t%s\n", nl_langinfo(ABDAY_3));
	printf("\tnl_langinfo(ABDAY_4)\t%s\n", nl_langinfo(ABDAY_4));
	printf("\tnl_langinfo(ABDAY_5)\t%s\n", nl_langinfo(ABDAY_5));
	printf("\tnl_langinfo(ABDAY_6)\t%s\n", nl_langinfo(ABDAY_6));
	printf("\tnl_langinfo(ABDAY_7)\t%s\n", nl_langinfo(ABDAY_7));

	printf("\n\tFull named month symbols\n");
	printf("\t------------------------------------\n");
	printf("\tnl_langinfo(MON_1)\t%s\n", nl_langinfo(MON_1));
	printf("\tnl_langinfo(MON_2)\t%s\n", nl_langinfo(MON_2));
	printf("\tnl_langinfo(MON_3)\t%s\n", nl_langinfo(MON_3));
	printf("\tnl_langinfo(MON_4)\t%s\n", nl_langinfo(MON_4));
	printf("\tnl_langinfo(MON_5)\t%s\n", nl_langinfo(MON_5));
	printf("\tnl_langinfo(MON_6)\t%s\n", nl_langinfo(MON_6));
	printf("\tnl_langinfo(MON_7)\t%s\n", nl_langinfo(MON_7));
	printf("\tnl_langinfo(MON_8)\t%s\n", nl_langinfo(MON_8));
	printf("\tnl_langinfo(MON_9)\t%s\n", nl_langinfo(MON_9));
	printf("\tnl_langinfo(MON_10)\t%s\n", nl_langinfo(MON_10));
	printf("\tnl_langinfo(MON_11)\t%s\n", nl_langinfo(MON_11));
	printf("\tnl_langinfo(MON_12)\t%s\n", nl_langinfo(MON_12));
	printf("\n");

	printf("\n\tAbbreviated month symbols\n");
	printf("\t------------------------------------\n");
	printf("\tnl_langinfo(ABMON_1)\t%s\n", nl_langinfo(ABMON_1));
	printf("\tnl_langinfo(ABMON_2)\t%s\n", nl_langinfo(ABMON_2));
	printf("\tnl_langinfo(ABMON_3)\t%s\n", nl_langinfo(ABMON_3));
	printf("\tnl_langinfo(ABMON_4)\t%s\n", nl_langinfo(ABMON_4));
	printf("\tnl_langinfo(ABMON_5)\t%s\n", nl_langinfo(ABMON_5));
	printf("\tnl_langinfo(ABMON_6)\t%s\n", nl_langinfo(ABMON_6));
	printf("\tnl_langinfo(ABMON_7)\t%s\n", nl_langinfo(ABMON_7));
	printf("\tnl_langinfo(ABMON_8)\t%s\n", nl_langinfo(ABMON_8));
	printf("\tnl_langinfo(ABMON_9)\t%s\n", nl_langinfo(ABMON_9));
	printf("\tnl_langinfo(ABMON_10)\t%s\n", nl_langinfo(ABMON_10));
	printf("\tnl_langinfo(ABMON_11)\t%s\n", nl_langinfo(ABMON_11));
	printf("\tnl_langinfo(ABMON_12)\t%s\n", nl_langinfo(ABMON_12));
	printf("\n");

	printf("\n\tDate and Time format string\n");
	printf("\t----------------------------\n");
	printf("\tDate/Time string based on \t'%s'\n", nl_langinfo(D_T_FMT));
	strftime(str, 100,  nl_langinfo(D_T_FMT), localtime(&tim));
	printf("\t\t '%s'\n", str);
	printf("\tDate string based on \t'%s'\n", nl_langinfo(D_FMT));
	strftime(str, 100,  nl_langinfo(D_FMT), localtime(&tim));
	printf("\t\t '%s'\n", str);
	printf("\tTime string based on \t'%s'\n", nl_langinfo(T_FMT));
	strftime(str, 100,  nl_langinfo(T_FMT), localtime(&tim));
	printf("\t\t '%s'\n", str);
	printf("\tAnte Meridian affix string\t%s\n", nl_langinfo(AM_STR));
	printf("\tPost Meridian affix string\t%s\n", nl_langinfo(PM_STR));

	printf("\n\tLocal Date and Time\n");
	printf("\t----------------------------\n");
	strftime(str, 100, NULL, localtime(&tim));
	printf("\tCurrent date/time in the default format\t'%s'", str);
	printf("\n");
}

void get_LC_NUMERIC(void)
{

	printf("\n LC_NUMERIC locale: \t%s\n", setlocale(LC_NUMERIC, NULL));

	printf("==============================\n");
        printf("\tDecimal point symble: '%s'\n", table->decimal_point);
        printf("\tThousands separator: '%s'\n",table-> thousands_sep);
	printf("\tRadix character is: '%s'\n\tThousands separator: '%s'\n",
	       nl_langinfo(RADIXCHAR), nl_langinfo(THOUSEP));

	printf("\n");
}

void get_LC_MONETARY(void)
{
	char out_str[200];
	double num=0.0;

	printf("\n LC_MONETARY locale: \t%s\n", setlocale(LC_MONETARY, NULL));

	printf("=======================================\n");
        printf("\tInternational monetary symbol: '%s'\n",
		table->int_curr_symbol);
        printf("\tCurrency symbol: '%s'\n", table->currency_symbol);
        printf("\tPositive sign: '%s'\n", table->positive_sign);
        printf("\tNegative sign: '%s'\n", table->negative_sign);
	printf("\t%s\n", table->p_cs_precedes ? 
	  "Currency symbol precedes a non-negative monetary quantity." :
	  "Currency symbol succeeds the non-negative monetary quantity.");
	printf("\t%s\n", table->p_sep_by_space ? 
	  "Space between the currency symbol and the value." :
	  "No space between the currency symbol and the value.");
	printf("\t%s\n", table->n_cs_precedes ? 
	  "Currency symbol precedes a negative monetary quantity." :
	  "Currency symbol succeeds the negative monetary quantity.");
	printf("\t%s\n", table->n_sep_by_space ? 
	  "Space between the currency symbol and the negative value." :
	  "No space between the currency symbol and the negative value.");
        printf("\tLocation of the positive sign: %x\n", table->p_sign_posn);
        printf("\tLocation of the negative sign: %x\n", table->n_sign_posn);
	printf("\n");

        printf("\tMonetary decimal point: '%s'\n", table->mon_decimal_point);
        printf("\tMonetary thousands separator: '%s'\n", 
		table->mon_thousands_sep);
        printf("\tMonetary grouping: '%x'\n", *table->mon_grouping);
        printf("\tNumber of international fractional digits : %x\n", 
		table->int_frac_digits);
        printf("\tNumber of fractional digits : %x\n", 
		table->frac_digits);
	printf("\n");

  	num = -12345.6789;
    	strfmon(out_str, sizeof(out_str), 
		"International format %i", num);
      	printf("\tValue is %f      %s\n", num, out_str);
    	strfmon(out_str, sizeof(out_str), 
		"National format %n", num);
      	printf("\t         %f      %s\n", num, out_str);

  	num = 12345.6789;
    	strfmon(out_str, sizeof(out_str), 
		"International format %i", num);
      	printf("\tValue is %f      %s\n", num, out_str);
    	strfmon(out_str, sizeof(out_str), 
		"National format %n", num);
      	printf("\t         %f      %s\n", num, out_str);


}

void get_LC_MESSAGES(void)
{
	printf("\n LC_MESSAGES locale: \t%s\n", setlocale(LC_MESSAGES, NULL));

	printf("==============================\n");
	printf("\tYes string is : '%s'\n\tNo string is : '%s'\n",
	       nl_langinfo(YESSTR),  nl_langinfo(NOSTR));

	if (! getenv("NLSPATH"))
		printf("\tUsing system default path\n");
	else printf("\tTranslated Message path: \t'%s'\n",
		getenv("NLSPATH"));
}

void main(void)
{
	printf ( "setlocale(LC_ALL, \"\") : %s\n", setlocale(LC_ALL, "") );
	printf("Locale settings is: %s\n", setlocale(LC_ALL, NULL));

	/* Get the table of locale info.     */
	table = localeconv();

	if (table == NULL)
	{
		printf("Error access localeconv data\n");
		exit(1);
	}

	get_LC_CTYPE();
	get_LC_COLLATE();
	get_LC_TIME();
	get_LC_NUMERIC();
	get_LC_MONETARY();
	get_LC_MESSAGES();
}
