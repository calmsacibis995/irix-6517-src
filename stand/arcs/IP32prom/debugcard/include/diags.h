
struct testtable {
    char *name ;
    void (*diag)() ;
    int  testLoop, errorLoop ;
} ;

typedef struct testtable diags_t ;

