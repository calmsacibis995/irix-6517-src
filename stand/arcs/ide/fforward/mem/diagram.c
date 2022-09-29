#ident "$Revision: 1.14 $"
#include <sys/cpu.h>
#include <libsc.h>

#if IP22 
static char *indigo2[] = {
	"w| w| w| w|  w| w| w| w|  w| w| w| w|",
	"o| o| o| o|  o| o| o| o|  o| o| o| o|",
	"r| r| r| r|  r| r| r| r|  r| r| r| r|",
	"d| d| d| d|  d| d| d| d|  d| d| d| d|",
	" |  |  |  |   |  |  |  |   |  |  |  |",
	"3| 2| 1| 0|  3| 2| 1| 0|  3| 2| 1| 0|",
	" |  |  |  |   |  |  |  |   |  |  |  |",
	" |  |  |  |   |  |  |  |   |  |  |  |",
	"S1  2  3  4   5  6  7  8   9  10 11 12",
	" Bank 2       Bank 1       Bank 0",
	"",
	"",
	"====================================================",
	"              EISA/GIO backplane",
	"",
	"",
	"",
	"",
	"",
	"",
	0
};
#endif

/* it turns out that the word order is slightly different for the */
/*  memory tests in IP22 and IP26/28 */
#if IP26 || IP28
static char *indigo2[] = {
	"w| w| w| w|  w| w| w| w|  w| w| w| w|",
	"o| o| o| o|  o| o| o| o|  o| o| o| o|",
	"r| r| r| r|  r| r| r| r|  r| r| r| r|",
	"d| d| d| d|  d| d| d| d|  d| d| d| d|",
	" |  |  |  |   |  |  |  |   |  |  |  |",
	"2| 3| 0| 1|  2| 3| 0| 1|  2| 3| 0| 1|",
	" |  |  |  |   |  |  |  |   |  |  |  |",
	" |  |  |  |   |  |  |  |   |  |  |  |",
	"S1  2  3  4   5  6  7  8   9  10 11 12",
	" Bank 2       Bank 1       Bank 0",
	"",
	"",
	"====================================================",
	"              EISA/GIO backplane",
	"",
	"",
	"",
	"",
	"",
	"",
	0
};
#endif

#if IP22
static char *indy[] = {
	"",
	"  -------------------- S8  word 3",
	"  -------------------- S7  word 2  Bank",
	"  -------------------- S6  word 1   1",
	"  -------------------- S5  word 0",
	"",
	"  -------------------- S4  word 3",
	"  -------------------- S3  word 2  Bank",
	"  -------------------- S2  word 1   0",
	"  -------------------- S1  word 0",
	"",
	"====================================================",
	"|                   Power Supply                   |",
	"",
	0
};
#endif

#ifdef IP20
void
print_memory_diagram(void)
{
    printf("+------------------------------------------------------------+\n");
    printf("| word 3 (s4c)   word 2 (s2c)   bank 0                       |\n");
    printf("| word 1 (s3c)   word 0 (s1c)                                |\n");
    printf("| ===========    ============   ======                       |\n");
    printf("| word 3 (s4b)   word 2 (s2b)   bank 1                       |\n");
    printf("| word 1 (s3b)   word 0 (s1b)                                |\n");
    printf("| ============   ============   ======                       |\n");
    printf("| word 3 (s4a)   word 2 (s2a)   bank 2                       |\n");
    printf("| word 1 (s3a)   word 0 (s1a)                                |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                                                            |\n");
    printf("|                              +-----------------------------+\n");
    printf("|                              |                              \n");
    printf("|                              |   Backplane connector        \n");
    printf("+------------------------------+                              \n");
    printf("                                                              \n");
    printf("    I/O panel                                                 \n");
    printf("                                                              \n");
}
#else
void
print_memory_diagram(void)
{
	char **p, *bd =
	   "+------------------------------------------------------------+\n";

#if IP22
	p = is_fullhouse() ? indigo2 : indy;
#else
	p = indigo2;
#endif

	printf(bd);

	while(*p)
		printf("|    %-52s    |\n", *p++);

	printf(bd);
}
#endif
