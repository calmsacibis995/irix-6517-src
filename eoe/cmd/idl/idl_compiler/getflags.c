/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: getflags.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:43  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:22  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:47:21  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:37  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:14  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      GETFLAGS.C
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Command Line Parser
**
**  VERSION: DCE 1.0
**
*/

#include <ctype.h>

#include <nidl.h>
#include <getflags.h>
#include <command.h>
#include <driver.h>
#include <message.h>

#define NFLAGS 128
static unsigned char option_count[NFLAGS]    = {0};
static int           other_count             = 0;
static char          *(other_flags[NFLAGS])  = {0};

char *last_string;           /* Last string parsed, for disambiguating */

/*
 *  flags_option_count: Returns #occurences of option on command line.
 */
int flags_option_count
#ifdef PROTO
(
    OPTIONS table[],
    char *option
)
#else
(table, option)
    OPTIONS table[];    /* Table of command options */
    char    *option;    /* Name of option */
#endif
{
    int o;

    if (*option == '-')
        option++;
    for (o = 0; table[o].option && o < NFLAGS; o++)
    {
        if (strcmp(option, table[o].option) == 0)
            return((int)option_count[o]);
    }
    return(-1);
}


/*
 *  flags_incr_count: Increments command option count by specified amount.
 */
void flags_incr_count
#ifdef PROTO
(
    OPTIONS table[],
    char *option,
    int delta
)
#else
(table, option, delta)
    OPTIONS table[];    /* Table of command options */
    char    *option;    /* Name of option */
    int     delta;      /* Amount to increment option count by */
#endif
{
    int o;

    if (*option == '-')
        option++;
    for (o = 0; table[o].option && o < NFLAGS; o++)
    {
        if (strlen(option) != strlen(table[o].option))
            continue;
        if (strcmp(option, table[o].option) == 0)
        {
            option_count[o] += delta;
            return;
        }
    }
}

/*
 *  flags_other_count: Returns count of command line parameters that are not
 *                     part of command line options.
 */
int flags_other_count
#ifdef PROTO
(
    void
)
#else
()
#endif
{
    return(other_count);
}


/*
 *  flags_other: Returns the Nth command line parameter that is not an option.
 */
char *flags_other
#ifdef PROTO
(
    int index
)
#else
(index)
    int index;      /* Index of parameter to get */
#endif
{
    if (0 <= index && index < other_count)
        return(other_flags[index]);
    else
        return(NULL);
}

/*
 *  is_number: Returns true if argument consists only of ASCII "0" thru "9"
 *             with optional leading "+" or "-".
 */

boolean is_number
#ifdef PROTO
(
    char *str
)
#else
(str)
    char *str;      /* String to test, assumed to be non-null */
#endif

{
    if (*str == '+' || *str == '-')
        str++;

    for ( ; *str != '\0' ; str++)
        if (!isdigit(*str))
            return false;

    return true;
}

/*
 *  getflags: Parses command parameters and options.
 */
void getflags
#ifdef PROTO
(
    int ac,
    char **av,
    OPTIONS table[]
)
#else
(ac, av, table)
    int     ac;         /* Argument count */
    char    **av;       /* Argument vector - one per space-delimited cmd arg */
    OPTIONS table[];    /* Table of command options */
#endif
{
    int             o;
    int             optlen;
    int             nflags, type;
    int             vflag;
    boolean         optval;
    register char   **pstring;
    register char   *pchar;
    register int    *pint;
    register char   *flag;
    register long   *plong;
    register double *pfloat;

    last_string = NULL;

    while (ac > 0)
    {
    thisf:
        for (o = 0;  table[o].option;  o++)
        {
            flag = *av;
            if (flag[0] == '-')
                flag++;

            if (strlen(flag) != strlen(table[o].option))
                continue;

            if (strcmp(flag, table[o].option) == 0)
            {
                optval = false;     /* This is not OptVal with no white space */
            matchf:
                nflags = (table[o].ftype >> 8) & 0xFF;
                vflag = nflags & VARARGFLAG;
                nflags &= MULTARGMASK;
                if (nflags <= 0)
                    nflags = 1;
                type = table[o].ftype & 0xFF;

                switch (type)
                {
                default:
                    INTERNAL_ERROR("Illegal option type");

                case INTARG:
                    pint = (int *)table[o].dest;
                    if (vflag)
                        pint += option_count[o];
                    /*
                     * Replacing "if (nflags" with "while (nflags--" allows
                     * lists such as -bug 1 2, but makes parameter determination
                     * ambiguous.  As it stands, -bug 1 -bug 2 must be used.
                     */
                    if (nflags && (ac > 1))
                    {
                        if (is_number(av[1]))
                        {
                            GETINT(*pint++);
                        }
                        else
                            goto nextf;
                        if (ac > 0 && vflag && **av == '-') goto thisf;
                        option_count[o]++;
                    }
                    goto nextf;

                case STRARG:
                    pstring = (char **)table[o].dest;

                    if (vflag)
                        pstring += option_count[o];
                    /*
                     * Replacing "if (nflags" with "while (nflags--" allows
                     * lists like -D foo bar, but makes parameter determination
                     * ambiguous.  As it stands, -D foo -D bar must be used.
                     */
                    if (nflags && (ac > 1))
                    {
                        GETSTR(*pstring);
                        if (ac > 0 && vflag && **av == '-')
                        {
                            *pstring = NULL;
                            goto thisf;
                        }
                        /** Add pstring++; for while loop version **/
                        option_count[o]++;
                    }
                    goto nextf;

                case OSTRARG:
                    /* Similar to STRARG, but allows for optional string arg. */
                    pstring = (char **)table[o].dest;

                    /*
                     * Allow the string argument to be optional.
                     */
                    if (!optval)
                    {
                        if (ac == 1 || (ac > 1 && *av[1] == '-'))
                        {
                            *pstring = "";
                            goto nextf;
                        }
                    }

                    if (vflag)
                        pstring += option_count[o];
                    /*
                     * Replacing "if (nflags" with "while (nflags--" allows
                     * lists like -D foo bar, but makes parameter determination
                     * ambiguous.  As it stands, -D foo -D bar must be used.
                     */
                    if (nflags && (ac > 1))
                    {
                        GETSTR(*pstring);
                        if (ac > 0 && vflag && **av == '-')
                        {
                            *pstring = NULL;
                            goto thisf;
                        }
                        /*
                         * Save pointer to this string, so caller can use to
                         * disambiguate ambiguous syntax.
                         */
                        last_string = *pstring;
                        /** Add pstring++; for while loop version **/
                        option_count[o]++;
                    }
                    goto nextf;

                case TOGGLEARG:
                    pchar = (char *)table[o].dest;
                    *pchar = ~*pchar;
                    goto nextf;

                case ASSERTARG:
                    pchar = (char *)table[o].dest;
                    *pchar = true;
                    goto nextf;

                case DENYARG:
                    pchar = (char *)table[o].dest;
                    *pchar = false;
                    goto nextf;

                case CHRARG:
                    pchar = (char *)table[o].dest;
                    if (vflag)
                        pchar += option_count[o];
                    /*
                     * Replacing "if (nflags" with "while (nflags--" allows
                     * lists such as -opt a b, but makes parameter determination
                     * ambiguous.  As it stands, -opt a -opt b must be used.
                     */
                    if (nflags && (ac > 1))
                    {
                        GETCH(*pchar++);
                        if (ac > 0 && vflag && **av == '-') goto thisf;
                        option_count[o]++;
                    }
                    goto nextf;

                case FLTARG:
                    pfloat = (double *)table[o].dest;
                    if (vflag)
                        pfloat += option_count[o];
                    /*
                     * Replacing "if (nflags" with "while (nflags--" allows
                     * lists like -f 1.1 2.2, but makes parameter determination
                     * ambiguous.  As it stands, -f 1.1 -f 2.2 must be used.
                     */
                    if (nflags && (ac > 1))
                    {
                        GETFLT(*pfloat++);
                        if (ac > 0 && vflag && **av == '-') goto thisf;
                        option_count[o]++;
                    }
                    goto nextf;

                case LONGARG:
                    plong = (long *)table[o].dest;
                    if (vflag)
                        plong += option_count[o];
                    /*
                     * Replacing "if (nflags" with "while (nflags--" allows
                     * lists such as -bug 1 2, but makes parameter determination
                     * ambiguous.  As it stands, -bug 1 -bug 2 must be used.
                     */
                    if (nflags && (ac > 1))
                    {
                        if (is_number(av[1]))
                        {
                            GETLONG(*plong++);
                        }
                        else
                            goto nextf;
                        if (ac > 0 && vflag && **av == '-') goto thisf;
                        option_count[o]++;
                    }
                    goto nextf;
                }
            }
        }

        if (**av == '-')
        {
            /*
             * Check for the case of -OptVal, i.e. where the option name and
             * its value are not separated by white space.  This code isn't
             * pretty.  So horrendous code promotes horrendous code!
             */
            for (o = 0;  table[o].option;  o++)
            {
                optlen = strlen(table[o].option);
                if (strncmp(flag, table[o].option, optlen) == 0)
                {
                    /*
                     * If an option that's not supposed to take a value, then
                     * issue error and exit.
                     */
                    type = table[o].ftype & 0xFF;
                    if (type==TOGGLEARG || type==ASSERTARG || type==DENYARG)
                    {
                        message_print(NIDL_OPTNOVAL, table[o].option);
                        CMD_explain_args();
                        exit(pgm_error);
                    }

                    /*
                     * Modify the argv entry to be just Val instead of -OptVal.
                     */
                    optval = true;      /* Parsed -OptVal with no white space */
                    *av += optlen+1;    /* Point argptr past -Opt part */
                    /*
                     * Fake out the code above as if -Opt and Val are two
                     * separate entries.  In reality, Val is now a separate
                     * entry and -Opt has been destroyed (no longer needed).
                     */
                    ac++;
                    av--;
                    goto matchf;
                }
            }

            /*
             * Unknown option.
             */
            message_print(NIDL_UNKFLAG, *av);
            CMD_explain_args();
            exit(pgm_error);
        }
        else
        {
            other_flags[other_count++] = *av;
        }

    nextf:
        ac--;
        av++;
    }
}

/*
 * printflags: Prints list of command options and values to stderr.
 */

#define yes_no(x) (x? "Yes" : "No")
#define no_yes(x) (x? "No" : "Yes")

void printflags
#ifdef PROTO
(
    OPTIONS table[]
)
#else
(table)
    OPTIONS table[];    /* Table of command options */
#endif
{
    register int    o;
    register int    nflags;
    register int    type;
    int             vflag;
    int             *pint;
    char            *pchar;
    char            **pstring;
    long            *plong;
    double          *pdouble;
    int             option_len;

    option_len = 0;

    for (o = 0; table[o].option; o++)
        if (strlen(table[o].option) > option_len)
            option_len = strlen(table[o].option);

    option_len += 3;

    message_print(NIDL_OPTIONSTABLE);
    for (o = 0;  table[o].option;  o++)
    {
        type = table[o].ftype;
        if (type & HIDARG) continue;
        nflags = (type >> 8) & 0xFF;
        vflag = nflags & VARARGFLAG;

        if (vflag)
            nflags = option_count[o];

        type &= 255;
        fprintf(stderr, "    %-*s", option_len, table[o].option);

        if (!vflag && nflags <= 0)
            nflags = 1;

        switch (type)
        {
        default:
            fprintf(stderr, "\tillegal option in printflags: %d\n",
                table[o].ftype);
            exit(pgm_error);

        case INTARG:
            pint = (int *)table[o].dest;
            while (nflags-- > 0)
                fprintf(stderr, "\t%d", *pint++);
            fprintf(stderr, "\n");
            break;

        case STRARG:
        case OSTRARG:
            pstring = (char **)table[o].dest;
            while (nflags-- > -0)
                fprintf(stderr, "\t%s", *pstring++);
            fprintf(stderr, "\n");
            break;

        case TOGGLEARG:
        case ASSERTARG:
            pchar = (char *)table[o].dest;
            while (nflags-- > 0)
                fprintf(stderr, "\t%s", yes_no(*pchar++));
            fprintf(stderr, "\n");
            break;

        case DENYARG:
            pchar = (char *)table[o].dest;
            while (nflags-- > 0)
                fprintf(stderr, "\t%s", no_yes(*pchar++));
            fprintf(stderr, "\n");
            break;

        case CHRARG:
            pchar = (char *)table[o].dest;
            while (nflags-- > 0)
                fprintf(stderr, "\t%c", *pchar++);
            fprintf(stderr, "\n");
            break;

        case FLTARG:
            pdouble = (double *)table[o].dest;
            while (nflags-- > 0)
                fprintf(stderr, "\t%.3f", *pdouble++);
            fprintf(stderr, "\n");
            break;

        case LONGARG:
            plong = (long *)table[o].dest;
            while (nflags-- > 0)
                fprintf(stderr, "\t%ld", *plong++);
            fprintf(stderr, "\n");
            break;
        }
    }

    fprintf(stderr, "\n");
}
