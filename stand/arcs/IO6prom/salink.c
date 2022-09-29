#ident "IO6prom/salink.c: $Revision: 1.1 $"

int _j_exceptnorm[1];
int _j_exceptutlb[1];
int _j_exceptxut[1];
int _j_exceptecc[1];

void exceptutlb(void) {}
void exceptnorm(void) {}
void exceptecc(void) {}
void _hook_exceptions() {}
void nested_exception_spin(void) {}
