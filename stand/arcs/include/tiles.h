#ifndef _tile_h_
#define _tile_h_

/*
 * Tiles are 64KB and are aligned on 64KB boundries
 */

void  initTiles(void);
void* getTile(void);
void  freeTile(void* tile);

#endif
