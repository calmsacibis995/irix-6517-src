
#define isdigit(c)      ((c) >= '0' && (c) <= '9')
#define isupper(c)      ((c) >= 'A' && (c) <= 'Z')
#define islower(c)      ((c) >= 'a' && (c) <= 'z')

#define isxdigit(c)     ((c) >= '0' && (c) <= '9' || \
                         (c) >= 'a' && (c) <= 'f' || \
                         (c) >= 'A' && (c) <= 'F')

#define DIGIT(x)        (isdigit(x) ? (x) - '0' : \
                        islower(x) ? (x) + 10 - 'a' : (x) + 10 - 'A')

#define isalpha(c)      (isupper(c) || islower(c))
#define isprint(c)      ((c) >= ' ' && (c) <= '~')
#define isascii(c)      (!((c) & ~0177))

#define tolower(c)       (isupper(c) ? (c) - 'A' + 'a' : (c))


