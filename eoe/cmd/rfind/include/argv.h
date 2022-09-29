typedef char *argstring;
bool_t xdr_argstring();


struct arguments {
	u_int argc;
	argstring *argv;
};
typedef struct arguments arguments;
bool_t xdr_arguments();
