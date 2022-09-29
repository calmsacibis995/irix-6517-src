/*
 * tiny binary to display result of gethostid(2) in hex.
 */

extern char* itox (unsigned long);
extern long gethostid(void);

main(){
    char *s = itox (gethostid());
    write (1, s, strlen(s));
    exit (0);
}

char *itox(unsigned long idval)
{
    static char buf[32];
    char *p = buf + sizeof(buf) - 1;

    *p-- = 0;
    *p-- = '\n';
    if (idval == 0)
	*p-- = '0';
    else while (idval) {
	int digit = idval % 16;
	idval /= 16;
	*p-- = digit < 10 ? digit + '0' : digit - 10 + 'a';
    }
    *p-- = 'x';
    *p = '0';
    return p;
}
