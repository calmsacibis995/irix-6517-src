/* only existed in mgras.a, but called by textport */
mgras_earlyinit(){return 0;}
MgrasProbe(){return 0;}

/* called from video driver */
/* ARGSUSED */
void MgrasVidTextureXferStart(void *ptr){}
/* ARGSUSED */
void MgrasVidTextureXferDone(void *ptr){}
/* ARGSUSED */
int MgrasGetVertLineCnt(void *ptr) { return(-1); }
