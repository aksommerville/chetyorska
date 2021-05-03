#include "ch_internal.h"
#include "ch_game.h"
#include <rabbit/rb_grid.h>
#include <rabbit/rb_image.h>

/* New.
 */
 
struct ch_game *ch_game_new() {
  struct ch_game *game=calloc(1,sizeof(struct ch_game));
  if (!game) return 0;
  
  game->refc=1;
  
  game->towerw=10;
  game->towerh=20;
  game->framesperfall=60;
  game->framesperfall_drop=1;
  game->fallskip=1;
  game->fallcounter=game->framesperfall;
  game->linescorev[0]=100;//TODO scoring
  game->linescorev[1]=300;
  game->linescorev[2]=500;
  game->linescorev[3]=800;
  
  return game;
}

/* Delete.
 */
 
void ch_game_del(struct ch_game *game) {
  if (!game) return;
  if (game->refc-->1) return;
  
  free(game);
}

/* Retain.
 */
 
int ch_game_ref(struct ch_game *game) {
  if (!game) return -1;
  if (game->refc<1) return -1;
  if (game->refc==INT_MAX) return -1;
  game->refc++;
  return 0;
}

/* Generate grid.
 * This happens when a game gets attached to the app.
 */
 
struct rb_grid *ch_game_generate_grid(struct ch_game *game) {
  const int tilesize=6;
  const int colc=RB_FB_W/tilesize;
  const int rowc=RB_FB_H/tilesize;
  fprintf(stderr,"grid %dx%d, tilesize=%d, fb=%d,%d\n",colc,rowc,tilesize,RB_FB_W,RB_FB_H);
  struct rb_grid *grid=rb_grid_new(colc,rowc);
  if (!grid) return 0;
  grid->imageid=1;
  
  if (rb_grid_ref(grid)<0) {
    rb_grid_del(grid);
    return 0;
  }
  game->grid=grid;
  
  memset(grid->v,0x31,grid->w*grid->h);
  
  // "tower" is the inner part, where the game actually happens.
  // 10x20 matches NES Tetris.
  int towerw=10,towerh=20;
  int towerx=(grid->w>>1)-(towerw>>1);
  int towery=(grid->h>>1)-(towerh>>1);
  if ((towerx<1)||(towery<1)||(towerx+towerw>=grid->w)||(towery+towerh>=grid->h)) {
    rb_grid_del(grid);
    return 0;
  }
  // top and bottom edges of tower...
  memset(grid->v+(towery-1)*grid->w+towerx,0x21,towerw);
  memset(grid->v+(towery+towerh)*grid->w+towerx,0x41,towerw);
  grid->v[(towery-1)*grid->w+towerx-1]=0x20;
  grid->v[(towery-1)*grid->w+towerx+towerw]=0x22;
  grid->v[(towery+towerh)*grid->w+towerx-1]=0x40;
  grid->v[(towery+towerh)*grid->w+towerx+towerw]=0x42;
  // vertical middle of tower...
  uint8_t *v=grid->v+towery*grid->w+towerx;
  int y=towerh;
  for (;y-->0;v+=grid->w) {
    v[-1]=0x30;
    memset(v,0x00,towerw);
    v[towerw]=0x32;
  }
  game->towerx=towerx;
  game->towery=towery;
  game->towerw=towerw;
  game->towerh=towerh;
  
  //XXX TEMP -- oh this whole thing is TEMP, but the score boxes especially
  grid->v[(towery-1)*grid->w+towerx+towerw+1]=0x20;
  grid->v[(towery-1)*grid->w+towerx+towerw+2]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+3]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+4]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+5]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+6]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+7]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+8]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+9]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+10]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+11]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+12]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+13]=0x21;
  grid->v[(towery-1)*grid->w+towerx+towerw+14]=0x22;
  
  grid->v[(towery)*grid->w+towerx+towerw+1]=0x30;
  grid->v[(towery)*grid->w+towerx+towerw+2]=0x50;
  grid->v[(towery)*grid->w+towerx+towerw+3]=0x51;
  grid->v[(towery)*grid->w+towerx+towerw+4]=0x52;
  grid->v[(towery)*grid->w+towerx+towerw+5]=0x53;
  grid->v[(towery)*grid->w+towerx+towerw+6]=0x54;
  grid->v[(towery)*grid->w+towerx+towerw+7]=0x55;
  grid->v[(towery)*grid->w+towerx+towerw+8]=0x00;
  game->linesuip=(towery)*grid->w+towerx+towerw+9;
  game->linesuic=5;
  grid->v[(towery)*grid->w+towerx+towerw+9]=0x00;//begin lines here (top row)
  grid->v[(towery)*grid->w+towerx+towerw+10]=0x00;
  grid->v[(towery)*grid->w+towerx+towerw+11]=0x00;
  grid->v[(towery)*grid->w+towerx+towerw+12]=0x00;
  grid->v[(towery)*grid->w+towerx+towerw+13]=0x00;
  grid->v[(towery)*grid->w+towerx+towerw+14]=0x32;
  
  grid->v[(towery+1)*grid->w+towerx+towerw+1]=0x30;
  grid->v[(towery+1)*grid->w+towerx+towerw+2]=0x60;
  grid->v[(towery+1)*grid->w+towerx+towerw+3]=0x61;
  grid->v[(towery+1)*grid->w+towerx+towerw+4]=0x62;
  grid->v[(towery+1)*grid->w+towerx+towerw+5]=0x63;
  grid->v[(towery+1)*grid->w+towerx+towerw+6]=0x64;
  grid->v[(towery+1)*grid->w+towerx+towerw+7]=0x65;
  grid->v[(towery+1)*grid->w+towerx+towerw+8]=0x00;
  grid->v[(towery+1)*grid->w+towerx+towerw+9]=0x00;//begin lines here (bottom row)
  grid->v[(towery+1)*grid->w+towerx+towerw+10]=0x00;
  grid->v[(towery+1)*grid->w+towerx+towerw+11]=0x00;
  grid->v[(towery+1)*grid->w+towerx+towerw+12]=0x00;
  grid->v[(towery+1)*grid->w+towerx+towerw+13]=0x00;
  grid->v[(towery+1)*grid->w+towerx+towerw+14]=0x32;
  
  grid->v[(towery+2)*grid->w+towerx+towerw+1]=0x40;
  grid->v[(towery+2)*grid->w+towerx+towerw+2]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+3]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+4]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+5]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+6]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+7]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+8]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+9]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+10]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+11]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+12]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+13]=0x41;
  grid->v[(towery+2)*grid->w+towerx+towerw+14]=0x42;
  
  //...and "score"...
  grid->v[(towery+3)*grid->w+towerx+towerw+1]=0x20;
  grid->v[(towery+3)*grid->w+towerx+towerw+2]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+3]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+4]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+5]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+6]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+7]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+8]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+9]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+10]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+11]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+12]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+13]=0x21;
  grid->v[(towery+3)*grid->w+towerx+towerw+14]=0x22;
  
  grid->v[(towery+4)*grid->w+towerx+towerw+1]=0x30;
  grid->v[(towery+4)*grid->w+towerx+towerw+2]=0x70;
  grid->v[(towery+4)*grid->w+towerx+towerw+3]=0x71;
  grid->v[(towery+4)*grid->w+towerx+towerw+4]=0x72;
  grid->v[(towery+4)*grid->w+towerx+towerw+5]=0x73;
  grid->v[(towery+4)*grid->w+towerx+towerw+6]=0x74;
  grid->v[(towery+4)*grid->w+towerx+towerw+7]=0x75;
  grid->v[(towery+4)*grid->w+towerx+towerw+8]=0x00;
  game->scoreuip=(towery+4)*grid->w+towerx+towerw+9;
  game->scoreuic=5;//TODO too low
  grid->v[(towery+4)*grid->w+towerx+towerw+9]=0x00;//begin score here (top row)
  grid->v[(towery+4)*grid->w+towerx+towerw+10]=0x00;
  grid->v[(towery+4)*grid->w+towerx+towerw+11]=0x00;
  grid->v[(towery+4)*grid->w+towerx+towerw+12]=0x00;
  grid->v[(towery+4)*grid->w+towerx+towerw+13]=0x00;
  grid->v[(towery+4)*grid->w+towerx+towerw+14]=0x32;
  
  grid->v[(towery+5)*grid->w+towerx+towerw+1]=0x30;
  grid->v[(towery+5)*grid->w+towerx+towerw+2]=0x80;
  grid->v[(towery+5)*grid->w+towerx+towerw+3]=0x81;
  grid->v[(towery+5)*grid->w+towerx+towerw+4]=0x82;
  grid->v[(towery+5)*grid->w+towerx+towerw+5]=0x83;
  grid->v[(towery+5)*grid->w+towerx+towerw+6]=0x84;
  grid->v[(towery+5)*grid->w+towerx+towerw+7]=0x85;
  grid->v[(towery+5)*grid->w+towerx+towerw+8]=0x00;
  grid->v[(towery+5)*grid->w+towerx+towerw+9]=0x00;//begin score here (bottom row)
  grid->v[(towery+5)*grid->w+towerx+towerw+10]=0x00;
  grid->v[(towery+5)*grid->w+towerx+towerw+11]=0x00;
  grid->v[(towery+5)*grid->w+towerx+towerw+12]=0x00;
  grid->v[(towery+5)*grid->w+towerx+towerw+13]=0x00;
  grid->v[(towery+5)*grid->w+towerx+towerw+14]=0x32;
  
  grid->v[(towery+6)*grid->w+towerx+towerw+1]=0x40;
  grid->v[(towery+6)*grid->w+towerx+towerw+2]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+3]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+4]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+5]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+6]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+7]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+8]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+9]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+10]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+11]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+12]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+13]=0x41;
  grid->v[(towery+6)*grid->w+towerx+towerw+14]=0x42;
  
  ch_game_print_number(game,game->linesuip,game->linesuic,game->lines,0);
  ch_game_print_number(game,game->scoreuip,game->scoreuic,game->score,0);
  
  game->rhuip=(towery+7)*grid->w+towerx+towerw+2;
  game->rhuic=5;
  grid->v[game->rhuip-1]=0x23;
  grid->v[game->rhuip+game->rhuic]=0x25;
  memset(grid->v+game->rhuip,0x24,game->rhuic);
  
  game->nextuip=(towery)*grid->w+towerx-6;
  grid->v[game->nextuip-grid->w-1]=0x20;
  grid->v[game->nextuip-grid->w  ]=0x21;
  grid->v[game->nextuip-grid->w+1]=0x21;
  grid->v[game->nextuip-grid->w+2]=0x21;
  grid->v[game->nextuip-grid->w+3]=0x21;
  grid->v[game->nextuip-grid->w+4]=0x22;
  grid->v[game->nextuip-1]=0x30;
  grid->v[game->nextuip  ]=0x00;
  grid->v[game->nextuip+1]=0x00;
  grid->v[game->nextuip+2]=0x00;
  grid->v[game->nextuip+3]=0x00;
  grid->v[game->nextuip+4]=0x32;
  grid->v[game->nextuip+grid->w-1]=0x30;
  grid->v[game->nextuip+grid->w  ]=0x00;
  grid->v[game->nextuip+grid->w+1]=0x00;
  grid->v[game->nextuip+grid->w+2]=0x00;
  grid->v[game->nextuip+grid->w+3]=0x00;
  grid->v[game->nextuip+grid->w+4]=0x32;
  grid->v[game->nextuip+grid->w*2-1]=0x30;
  grid->v[game->nextuip+grid->w*2  ]=0x00;
  grid->v[game->nextuip+grid->w*2+1]=0x00;
  grid->v[game->nextuip+grid->w*2+2]=0x00;
  grid->v[game->nextuip+grid->w*2+3]=0x00;
  grid->v[game->nextuip+grid->w*2+4]=0x32;
  grid->v[game->nextuip+grid->w*3-1]=0x30;
  grid->v[game->nextuip+grid->w*3  ]=0x00;
  grid->v[game->nextuip+grid->w*3+1]=0x00;
  grid->v[game->nextuip+grid->w*3+2]=0x00;
  grid->v[game->nextuip+grid->w*3+3]=0x00;
  grid->v[game->nextuip+grid->w*3+4]=0x32;
  grid->v[game->nextuip+grid->w*4-1]=0x40;
  grid->v[game->nextuip+grid->w*4  ]=0x41;
  grid->v[game->nextuip+grid->w*4+1]=0x41;
  grid->v[game->nextuip+grid->w*4+2]=0x41;
  grid->v[game->nextuip+grid->w*4+3]=0x41;
  grid->v[game->nextuip+grid->w*4+4]=0x42;
  
  // the brick
  ch_game_new_brick(game);
  ch_game_print_brick_cells(game,&game->brick);
  
  return grid;
}

/* Select a random brick shape.
 * There are 19 valid shapes, of which 7 are canonical forms.
 * We will return only the canonical forms.
 */

uint16_t ch_game_random_brick_shape(struct ch_game *game) {
  switch (rand()%7) {
    case 0: return 0x0660;
    case 1: return 0x00f0;
    case 2: return 0x0446;
    case 3: return 0x0226;
    case 4: return 0x0720;
    case 5: return 0x0630;
    case 6: return 0x0360;
  }
  return 0x0660;
}

/* Rotate a shape by 90 degrees.
 */
 
uint16_t ch_game_rotate_shape(uint16_t shape,int d) {
  if (d<0) {
    switch (shape) {
      case 0x0660: return 0x0660;
      case 0x00f0: return 0x2222;
      case 0x2222: return 0x00f0;
      case 0x0446: return 0x0170;
      case 0x0740: return 0x0446;
      case 0x0622: return 0x0740;
      case 0x0170: return 0x0622;
      case 0x0226: return 0x0710;
      case 0x0470: return 0x0226;
      case 0x0644: return 0x0470;
      case 0x0710: return 0x0644;
      case 0x0720: return 0x0464;
      case 0x0262: return 0x0720;
      case 0x0270: return 0x0262;
      case 0x0464: return 0x0270;
      case 0x0630: return 0x0264;
      case 0x0264: return 0x0630;
      case 0x0360: return 0x0462;
      case 0x0462: return 0x0360;
    }
  } else if (d>0) {
    switch (shape) {
      case 0x0660: return 0x0660;
      case 0x00f0: return 0x2222;
      case 0x2222: return 0x00f0;
      case 0x0446: return 0x0740;
      case 0x0740: return 0x0622;
      case 0x0622: return 0x0170;
      case 0x0170: return 0x0446;
      case 0x0226: return 0x0470;
      case 0x0470: return 0x0644;
      case 0x0644: return 0x0710;
      case 0x0710: return 0x0226;
      case 0x0720: return 0x0262;
      case 0x0262: return 0x0270;
      case 0x0270: return 0x0464;
      case 0x0464: return 0x0720;
      case 0x0630: return 0x0264;
      case 0x0264: return 0x0630;
      case 0x0360: return 0x0462;
      case 0x0462: return 0x0360;
    }
  }
  return shape;
}

/* Replace cells occupied by a brick.
 */
 
static void ch_game_replace_brick_cells(
  struct ch_game *game,
  int x,int y, // in tower space
  uint16_t shape,
  uint8_t tileid
) {
  // TODO Since there's only 19 valid shapes, might make sense to enumerate them instead of doing it generically?
  uint16_t rowmask=0xf000;
  uint8_t *dstrow=game->grid->v+(game->towery+y)*game->grid->w+game->towerx+x;
  int yi=4; for (;yi-->0;y++,rowmask>>=4,dstrow+=game->grid->w) {
    if (y<0) continue;
    if (y>=game->towerh) break;
    if (!(shape&rowmask)) continue;
    int xnext=x;
    uint8_t *dst=dstrow;
    uint16_t mask=rowmask&0x8888;
    int xi=4; for (;xi-->0;x++,mask>>=1,dst++) {
      if (x<0) continue;
      if (x>=game->towerw) break;
      if (!(shape&mask)) continue;
      *dst=tileid;
    }
    x=xnext;
  }
}

void ch_game_clear_brick_cells(struct ch_game *game,const struct ch_brick *brick) {
  ch_game_replace_brick_cells(game,brick->x,brick->y,brick->shape,0x00);
}

void ch_game_print_brick_cells(struct ch_game *game,const struct ch_brick *brick) {
  ch_game_replace_brick_cells(game,brick->x,brick->y,brick->shape,brick->tileid);
}

/* Next brick UI.
 */
 
void ch_game_redraw_next_brick(struct ch_game *game) {
  uint8_t *dst=game->grid->v+game->nextuip;
  uint16_t src=game->nextbrick.shape;
  int i=4; for (;i-->0;dst+=game->grid->w,src<<=4) {
    uint16_t mask=0x1000;
    int x=4; for (;x-->0;mask<<=1) {
      if (src&mask) dst[x]=game->nextbrick.tileid;
      else dst[x]=0x00;
    }
  }
}

/* Generate a new brick at the top of the tower, fossilize the old one.
 */
 
void ch_game_new_brick(struct ch_game *game) {

  if (game->nextbrick.shape) {
    game->brick=game->nextbrick;
  } else {
    game->brick.shape=ch_game_random_brick_shape(game);
    game->brick.tileid=0x01+rand()%3;//TODO per shape instead?
  }

  if (game->nextbrickdelay) {
    game->nextbrick.shape=ch_game_random_brick_shape(game);
    game->nextbrick.tileid=0x01+rand()%3;//TODO per shape instead?
    ch_game_redraw_next_brick(game);
  } else {
    game->nextbrickdelay=1;
  }

  game->brick.x=(game->towerw>>1)-2;
  // Position vertically so the brick's bottom edge is just above the tower.
       if (game->brick.shape&0x000f) game->brick.y=-4;
  else if (game->brick.shape&0x00f0) game->brick.y=-3;
  else if (game->brick.shape&0x0f00) game->brick.y=-2;
  else game->brick.y=-1;
}

void ch_game_generate_next_brick(struct ch_game *game) {
  game->nextbrick.shape=ch_game_random_brick_shape(game);
  game->nextbrick.tileid=0x01+rand()%3;//TODO per shape instead?
  ch_game_redraw_next_brick(game);
}

/* Iterate the lower neighbors of a brick.
 */
 
int ch_game_for_brick_lower_neighbors(
  struct ch_game *game,
  const struct ch_brick *brick,
  int (*cb)(int x,int y,void *userdata),
  void *userdata
) {
  //TODO More efficient to enumerate the 19 valid shapes.
  int err;
  uint16_t mask=0x8000;
  int suby=0; for (;suby<4;suby++) {
    int subx=0; for (;subx<4;subx++,mask>>=1) {
      if (!(brick->shape&mask)) continue;
      if (brick->shape&(mask>>4)) continue; // brick is its own neighbor here; skip it
      if (err=cb(brick->x+subx,brick->y+suby+1,userdata)) return err;
    }
  }
  return 0;
}

/* Iterate all the tiles of a shape.
 * These are not necessarily valid shapes.
 */
 
int ch_game_for_shape(
  int x,int y,uint16_t shape,
  int (*cb)(int x,int y,void *userdata),
  void *userdata
) {
  int err;
  uint16_t rowmask=0xf000;
  int yi=4; for (;yi-->0;y++,rowmask>>=4) {
    if (!(shape&rowmask)) continue;
    uint16_t mask=0x8888&rowmask;
    int xi=4,xp=x; for (;xi-->0;xp++,mask>>=1) {
      if (!(shape&mask)) continue;
      if (err=cb(xp,y,userdata)) return err;
    }
  }
  return 0;
}

/* Return the row occupied by the lowest tile of brick.
 */
 
int ch_game_brick_bottom_row(const struct ch_brick *brick) {
  if (brick->shape&0x000f) return brick->y+3;
  if (brick->shape&0x00f0) return brick->y+2;
  if (brick->shape&0x0f00) return brick->y+1;
  return brick->y;
}

/* Write a positive decimal integer onto the grid.
 */
 
void ch_game_print_number(
  struct ch_game *game,
  int dstp,int dstc, // offset in grid->v, and column count
  int src,
  int leading_zeroes // nonzero for "00123", zero for "  123"
) {

  // Confirm output coordinates fit in the grid.
  // If they cross the right screen edge, things will get weird, we only guarantee memory safety.
  if (dstp<0) return;
  if (dstc<1) return;
  int dstlimit=game->grid->w*game->grid->h;
  int outputend=dstp+game->grid->w+dstc;
  if (outputend>dstlimit) return;
  
  // We don't do negative. Clamp at zero.
  if (src<0) src=0;
  
  // Determine the largest printable integer and clamp to nines if overflown.
  // The largest 31-bit integer is ten digits decimal; we'll limit to 9 to be able to completely fill the valid range.
  // From a UI point of view, I feel 6 or 7 digits should be the limit.
  int srclimit;
  if (dstc>=9) {
    srclimit=999999999;
  } else {
    srclimit=10;
    int i=dstc;
    while (i-->1) srclimit*=10;
    srclimit--;
  }
  if (src>srclimit) src=srclimit;
  
  // Emit digits right to left.
  int x=dstc,first=1;
  for (;x-->0;src/=10,first=0) {
    int digit=src%10;
    if (!src&&!first&&!leading_zeroes) {
      game->grid->v[dstp+x]=0x00;
      game->grid->v[dstp+game->grid->w+x]=0x00;
    } else {
      game->grid->v[dstp+x]=0x56+digit;
      game->grid->v[dstp+game->grid->w+x]=0x66+digit;
    }
  }
}

/* Print rhythm bar.
 */
 
void ch_game_print_rhythm_bar(struct ch_game *game) {
  // 6 columns per tile, scale rhlopass to the exact width in pixels.
  // It saturates at 900 instead of the actual 999, so the player has a sticky "perfect zone".
  if (game->rhuic<1) return;
  int barw=6*game->rhuic;
  int w;
  if (game->rhlopass<=0) w=0;
  else if (game->rhlopass>=900) w=barw;
  else {
    w=(game->rhlopass*barw)/900;
    if (w<1) w=1;
    else if (w>=barw) w=barw;
  }
  if (w>=barw) {
    memset(game->grid->v+game->rhuip,0x2c,game->rhuic);
  } else {
    int p=game->rhuip;
    int c=game->rhuic;
    for (;c-->0;p++) {
      if (w>=6) game->grid->v[p]=0x2b;
      else if (w<=0) game->grid->v[p]=0x24;
      else game->grid->v[p]=0x25+w;
      w-=6;
    }
  }
}

/* Finish the game.
 */

int ch_game_end(struct ch_game *game) {
  //TODO
  fprintf(stderr,"*********** game over, lines=%d, score=%d **************\n",game->lines,game->score);
  ch_game_sound(game,CH_SFX_GAMEOVER);
  rb_grid_del(game->grid); // prevent update from doing anything more
  game->grid=0;
  return 0;
}

/* Play sound effect.
 */
 
int ch_game_sound(struct ch_game *game,int sfx) {
  if (!game->cb_sound) return 0;
  return game->cb_sound(sfx,game->sound_userdata);
}

/* Advance to the next level (every 10 lines).
 */
 
int ch_game_advance_level(struct ch_game *game) {
  if (game->framesperfall>1) {
    game->framesperfall>>=1;
  } else {
    // Seriously?
    game->fallskip++;
  }
  //TODO should scoring change with the level?
  //TODO bells, whistles
  return 0;
}
