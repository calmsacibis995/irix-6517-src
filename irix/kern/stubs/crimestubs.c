/* These stubs imply that if there is no gfx.a, CRIME is idle except
 * for calls from ml/MOOSEHEAD/mte_copy.c  -wsm12/18/95.
 */
int crmSavePP () { return 0; }
void crimeRestore() { }
int crimeLockOut() { return 0; }
