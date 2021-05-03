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
  
  game->framesperfall=60;
  game->framesperfall_drop=1;
  game->fallskip=1;
  game->fallcounter=game->framesperfall;
  game->linescorev[0]=100;
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
  
  ch_gridder_cleanup(&game->gridder);
  
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

  const int colc=RB_FB_W/CH_TILESIZE; // 42, and 4 columns leftover
  const int rowc=RB_FB_H/CH_TILESIZE; // 24
  struct rb_grid *grid=rb_grid_new(colc,rowc);
  if (!grid) return 0;
  grid->imageid=1;
  
  int err=ch_gridder_set_grid(&game->gridder,grid);
  rb_grid_del(grid);
  if (err<0) return 0;
  
  ch_gridder_fill(&game->gridder,0x31);
  
  struct ch_gridder_region *region;
  
  // Tower in the middle. Hold on to a copy of it; most of the rest of the UI builds around it.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_TOWER_FRAME))) return 0;
  region->w=CH_TOWER_W+2;
  region->h=CH_TOWER_H+2;
  region->x=(colc>>1)-(region->w>>1);
  region->y=(rowc>>1)-(region->h>>1);
  struct ch_gridder_region tframe=*region;
  
  // And a separate region for just the tower's innards.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_TOWER))) return 0;
  region->x=tframe.x+1;
  region->y=tframe.y+1;
  region->w=tframe.w-2;
  region->h=tframe.h-2;
  
  // Lines aligned with tower's top, flush against its right, extending to 1 column off the edge.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_LINES))) return 0;
  region->x=tframe.x+tframe.w;
  region->y=tframe.y;
  region->w=colc-region->x-1;
  region->h=4;
  
  // Score immediately below lines, same idea.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_SCORE))) return 0;
  region->x=tframe.x+tframe.w;
  region->y=tframe.y+4;
  region->w=colc-region->x-1;
  region->h=4;
  
  // Rhythm bar below score, with 1 row margin.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_RHYTHM))) return 0;
  region->x=tframe.x+tframe.w;
  region->y=tframe.y+9;
  region->w=colc-region->x-1;
  region->h=1;
  
  // Next brick aligned with tower's top, flush against its left. Fixed size 6x6.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_NEXT))) return 0;
  region->w=6;
  region->h=6;
  region->x=tframe.x-region->w;
  region->y=tframe.y;
  
  if (ch_gridder_validate_all(&game->gridder)<0) return 0;
  
  // Now draw everything...
  #define FF(tag,frame,fill) ch_gridder_framefill_region( \
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_##tag,0),frame,fill \
  );
  #define CONT(tag,tileid) ch_gridder_continuous_bar( \
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_##tag,0),tileid,0,0 \
  );
  #define LABEL(tag,tileid) ch_gridder_text_label( \
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_##tag,0),tileid,6 \
  );
  #define NUMBER(tag) ch_gridder_text_number( \
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_##tag,0),6,0x56,0 \
  );
  
  FF(TOWER_FRAME,0x20,0x00)
  FF(LINES,0x20,0x00)
  LABEL(LINES,0x50)
  NUMBER(LINES)
  FF(SCORE,0x20,0x00)
  LABEL(SCORE,0x70)
  NUMBER(SCORE)
  CONT(RHYTHM,0x23)
  FF(NEXT,0x20,0x00)
  
  #undef FF
  #undef CONT
  #undef LABEL
  #undef NUMBER
  
  ch_game_new_brick(game);
  
  return grid;
}

/* Select a random brick shape.
 * There are 19 valid shapes, of which 7 are canonical forms.
 * We will return only the canonical forms.
 */

uint16_t ch_game_random_brick_shape(uint8_t *tileid,struct ch_game *game) {
  switch (rand()%7) {
    case 0: *tileid=0x01; return 0x0660;
    case 1: *tileid=0x02; return 0x00f0;
    case 2: *tileid=0x03; return 0x0446;
    case 3: *tileid=0x04; return 0x0226;
    case 4: *tileid=0x05; return 0x0720;
    case 5: *tileid=0x06; return 0x0630;
    case 6: *tileid=0x07; return 0x0360;
  }
  *tileid=0x01;
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

void ch_game_clear_brick_cells(struct ch_game *game,const struct ch_brick *brick) {
  const struct ch_gridder_region *tower=ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0);
  if (!tower) return;
  ch_gridder_fill_shape(
    &game->gridder,
    tower->x+brick->x,
    tower->y+brick->y,
    brick->shape,
    0x00,
    tower
  );
}

void ch_game_print_brick_cells(struct ch_game *game,const struct ch_brick *brick) {
  const struct ch_gridder_region *tower=ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0);
  if (!tower) return;
  ch_gridder_fill_shape(
    &game->gridder,
    tower->x+brick->x,
    tower->y+brick->y,
    brick->shape,
    brick->tileid,
    tower
  );
}

/* Next brick UI.
 */
 
void ch_game_redraw_next_brick(struct ch_game *game) {
  //TODO use sprite
  const struct ch_gridder_region *dst=ch_gridder_get_region(&game->gridder,CH_RGN_NEXT,0);
  if (!dst) return;
  ch_gridder_framefill_region(&game->gridder,dst,0x20,0x00);
  ch_gridder_fill_shape(
    &game->gridder,
    dst->x+1,dst->y+1,
    game->nextbrick.shape,
    game->nextbrick.tileid,
    dst
  );
}

/* Generate a new brick at the top of the tower, fossilize the old one.
 */
 
void ch_game_new_brick(struct ch_game *game) {

  if (game->nextbrick.shape) {
    game->brick=game->nextbrick;
  } else {
    game->brick.shape=ch_game_random_brick_shape(&game->brick.tileid,game);
  }

  if (game->nextbrickdelay) {
    game->nextbrick.shape=ch_game_random_brick_shape(&game->nextbrick.tileid,game);
    ch_game_redraw_next_brick(game);
  } else {
    game->nextbrickdelay=1;
  }

  game->brick.x=(CH_TOWER_W>>1)-2;
  // Position vertically so the brick's bottom edge is just above the tower.
       if (game->brick.shape&0x000f) game->brick.y=-4;
  else if (game->brick.shape&0x00f0) game->brick.y=-3;
  else if (game->brick.shape&0x0f00) game->brick.y=-2;
  else game->brick.y=-1;
}

void ch_game_generate_next_brick(struct ch_game *game) {
  game->nextbrick.shape=ch_game_random_brick_shape(&game->nextbrick.tileid,game);
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

/* Print rhythm bar.
 */
 
void ch_game_print_rhythm_bar(struct ch_game *game) {
  const struct ch_gridder_region *rgn=ch_gridder_get_region(&game->gridder,CH_RGN_RHYTHM,0);
  if (!rgn) return;
  ch_gridder_continuous_bar(
    &game->gridder,rgn,
    0x23,game->rhlopass,900
  );
}

/* Finish the game.
 */

int ch_game_end(struct ch_game *game) {
  //TODO
  fprintf(stderr,"*********** game over, lines=%d, score=%d **************\n",game->lines,game->score);
  ch_game_sound(game,CH_SFX_GAMEOVER);
  ch_gridder_set_grid(&game->gridder,0);
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
  //TODO bells, whistles
  return 0;
}
