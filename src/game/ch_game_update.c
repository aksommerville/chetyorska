#include "ch_internal.h"
#include "ch_game.h"
#include <rabbit/rb_grid.h>
#include <rabbit/rb_sprite.h>
#include <math.h>

/* Fall the brick by one row.
 * Return >0 if fallen, <0 if stuck offscreen, or zero if it's at the ground and must bind.
 */
 
static int ch_game_fall_cb(int x,int y,void *userdata) {
  struct ch_game *game=userdata;
  if ((x<0)||(x>=CH_TOWER_W)) return 1; // how?
  if (y<0) return 0; // falling in from offscreen, let it roll
  if (y>=CH_TOWER_H) return 1; // struck bottom
  if (ch_gridder_read(&game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0),x,y)) return 1;
  return 0; // keep on falling
}
 
static int ch_game_fall(struct ch_game *game) {

  int i=game->fallskip;
  while (i-->0) {
    if (ch_game_for_brick_lower_neighbors(game,&game->brick,ch_game_fall_cb,game)) {
      if (ch_game_brick_bottom_row(&game->brick)<0) return -1;
      return 0;
    }
    ch_game_clear_brick_cells(game,&game->brick);
    game->brick.y++;
    ch_game_print_brick_cells(game,&game->brick);
  }
  
  return 1;
}

/* After fossilizing, check the grid for any completed lines (in rows y..y+3).
 * If found, this will enter "eliminate" state and update the score.
 */
 
static int ch_game_check_lines(struct ch_game *game,int y) {
  const struct ch_gridder_region *tower=ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0);
  if (!tower) return 0;
  
  const uint8_t *row=game->gridder.grid->v+(tower->y+y)*game->gridder.grid->w+tower->x;
  game->eliminatec=0;
  int i=4; for (;i-->0;y++,row+=game->gridder.grid->w) {
    if (y<0) continue;
    if (y>=tower->h) break;
    int filled=1,x=tower->w;
    while (x-->0) if (!row[x]) {
      filled=0;
      break;
    }
    if (filled) {
      game->eliminatev[game->eliminatec++]=y;
    }
  }
  if (!game->eliminatec) {
    ch_game_sound(game,CH_SFX_THUD);
    return 0;
  }
  
  if (game->eliminatec==4) {
    if (game->rhlopass>=900) {
      ch_game_sound(game,CH_SFX_CHETYORSKA);
    } else {
      ch_game_sound(game,CH_SFX_TETRIS);
    }
  } else {
    ch_game_sound(game,CH_SFX_LINES);
  }
  
  game->eliminatecounter=60;//TODO how long?
  
  int pvlevel=game->lines/10;
  game->lines+=game->eliminatec;
  int level=game->lines/10;
  int linescore;
  if (game->rhlopass>=900) {
    linescore=game->linescorev[game->eliminatec-1]*2;
  } else {
    linescore=(game->linescorev[game->eliminatec-1]*game->rhlopass)/900;
  }
  game->score+=linescore;
  
  if (level>pvlevel) {
    ch_game_advance_level(game);
  }
  
  ch_gridder_text_number(
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_LINES,0),
    6,0x56,game->lines
  );
  ch_gridder_text_number(
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_SCORE,0),
    6,0x56,game->score
  );
  
  int sprx=(tower->w*CH_TILESIZE)>>1;
  int spry=(y-2)*CH_TILESIZE;
  if (linescore>=1000) sprx-=2*CH_TILESIZE;
  else if (linescore>=100) sprx-=(3*CH_TILESIZE)>>1;
  else if (linescore>=10) sprx-=CH_TILESIZE;
  else sprx-=CH_TILESIZE>>1;
  ch_game_add_score_sprite(game,linescore,sprx,spry);
  
  return 0;
}

/* Finalize elimination, after animating.
 * Lines and score were advanced at the initial detection.
 */
 
static int ch_game_finalize_elimination(struct ch_game *game) {

  const struct ch_gridder_region *tower=ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0);
  if (!tower) return 0;
  uint8_t *cellv=game->gridder.grid->v+tower->y*game->gridder.grid->w+tower->x;
  
  ch_game_clear_brick_cells(game,&game->brick);

  const int *y=game->eliminatev;
  int i=game->eliminatec;
  for (;i-->0;y++) {
    int rowc=*y;
    uint8_t *p=cellv+(*y)*game->gridder.grid->w;
    for (;rowc-->0;p-=game->gridder.grid->w) {
      memmove(p,p-game->gridder.grid->w,tower->w);
    }
    memset(p,0,tower->w);
  }
  game->eliminatecounter=0;
  game->eliminatec=0;
  
  ch_game_print_brick_cells(game,&game->brick);
  
  return 0;
}

/* Flash the eliminating rows by toggling tiles between rows 0 and 1.
 */
 
static int ch_game_flash_eliminations(struct ch_game *game) {

  const struct ch_gridder_region *tower=ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0);
  if (!tower) return 0;
  uint8_t *cellv=game->gridder.grid->v+tower->y*game->gridder.grid->w+tower->x;
  
  const int *y=game->eliminatev;
  int i=game->eliminatec;
  for (;i-->0;y++) {
    uint8_t *v=cellv+(*y)*game->gridder.grid->w;
    int xi=tower->w;
    for (;xi-->0;v++) (*v)^=0x10;
  }
  return 0;
}

/* Update, main entry point.
 */
 
int ch_game_update(struct ch_game *game,int beatp,int beatc) {
  game->input_blackout=0;
  if (!game->gridder.grid) return 0;
  
  if (game->sprites) {
    int i=game->sprites->c;
    while (i-->0) {
      struct rb_sprite *sprite=game->sprites->v[i];
      if (sprite->type->update) {
        if (sprite->type->update(sprite)<0) return -1;
      }
    }
  }
  
  game->beatp=beatp;
  game->beatc=beatc;
  int metronome_color=7; // 0=bright .. 7=dim
  if (beatc>0) {
    metronome_color=(beatp*16)/beatc;
  }
  if (metronome_color<0) metronome_color=0;
  else if (metronome_color>7) metronome_color=7;
  ch_gridder_bulk_region(&game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_METRONOME,0),0x90+metronome_color*2);
  
  if (game->eliminatecounter>0) {
    game->eliminatecounter--;
    if (!game->eliminatecounter) {
      ch_game_finalize_elimination(game);
    } else if (game->eliminatecounter%5==4) {//TODO timing
      return ch_game_flash_eliminations(game);
    } else {
      return 0;
    }
  }
  
  if (game->rhlopass>0) {
    game->rhlopass--;
    ch_game_print_rhythm_bar(game);
  }

  game->fallcounter--;
  if (game->fallcounter<=0) {
    if (game->dropping) {
      game->fallcounter=game->framesperfall_drop;
    } else {
      game->fallcounter=game->framesperfall;
    }
    int err=ch_game_fall(game);
    if (err<0) {
      return ch_game_end(game);
    }
    if (!err) {
      // This both generates a new one and binds the old (bricks are kind of "always bound")
      int cktop=game->brick.y;
      ch_game_new_brick(game);
      if (game->newoverlapped) return ch_game_end(game);
      game->dropping=0;
      game->fallcounter=game->framesperfall;
      ch_game_check_lines(game,cktop);
    }
  }

  return 0;
}

/* Start of any event, consider timing against the background music.
 */
 
static int ch_game_rate_timing(struct ch_game *game) {
  if (game->beatc<1) return 0;
  
  double quality=0.0;
  
  // Perfect or very close. (beatc) is not necessarily the range of (beatp), there can be rounding errors.
  if (!game->beatp||(game->beatp>=game->beatc-1)) {
    quality=1.0;
    
  // Grade it like a triangle wave. 0 is the peak and 1/2 the floor.
  // Also a 0.5 downward bias -- The field of possible quality is biased low.
  } else {
    double phase=(double)game->beatp/(double)game->beatc;
    quality=fabs(0.5-phase)*4.0-1.5;
  }
  
  int dscore=(int)(quality*100);
  game->rhlopass+=dscore;
  if (game->rhlopass<0) game->rhlopass=0;
  else if (game->rhlopass>999) game->rhlopass=999;
  
  //fprintf(stderr,"%d/%d quality=%f dscore=%d lopass=%d\n",game->beatp,game->beatc,quality,dscore,game->rhlopass);
  
  ch_game_print_rhythm_bar(game);
  
  return 0;
}

/* Lateral movement.
 */
 
static int ch_game_move_cb(int x,int y,void *userdata) {
  struct ch_game *game=userdata;
  if ((y<0)&&(x>=0)&&(x<CH_TOWER_W)) return 0;
  if (ch_gridder_read(&game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0),x,y)) return 1;
  return 0;
}
 
static int ch_game_move_left(struct ch_game *game) {
  if (ch_game_rate_timing(game)<0) return -1;
  ch_game_sound(game,CH_SFX_MOVE);
  uint16_t edge=game->brick.shape&~((game->brick.shape&0xeeee)>>1);
  if (!ch_game_for_shape(game->brick.x-1,game->brick.y,edge,ch_game_move_cb,game)) {
    ch_game_clear_brick_cells(game,&game->brick);
    game->brick.x--;
    ch_game_print_brick_cells(game,&game->brick);
  }
  return 0;
}
 
static int ch_game_move_right(struct ch_game *game) {
  if (ch_game_rate_timing(game)<0) return -1;
  ch_game_sound(game,CH_SFX_MOVE);
  uint16_t edge=game->brick.shape&~((game->brick.shape&0x7777)<<1);
  if (!ch_game_for_shape(game->brick.x+1,game->brick.y,edge,ch_game_move_cb,game)) {
    ch_game_clear_brick_cells(game,&game->brick);
    game->brick.x++;
    ch_game_print_brick_cells(game,&game->brick);
  }
  return 0;
}

/* Rotation.
 */
 
static int ch_game_rotate(struct ch_game *game,int d) {
  if (ch_game_rate_timing(game)<0) return -1;
  ch_game_sound(game,CH_SFX_ROTATE);
  uint16_t nshape=ch_game_rotate_shape(game->brick.shape,d);
  if (nshape==game->brick.shape) return 0;
  ch_game_clear_brick_cells(game,&game->brick);
  if (!ch_game_for_shape(game->brick.x,game->brick.y,nshape,ch_game_move_cb,game)) {
    game->brick.shape=nshape;
  }
  ch_game_print_brick_cells(game,&game->brick);
  return 0;
}

/* Begin dropping.
 */
 
static int ch_game_drop(struct ch_game *game) {
  if (ch_game_rate_timing(game)<0) return -1;
  ch_game_sound(game,CH_SFX_DROP);
  game->fallcounter=0;
  game->dropping=1;
  return 0;
}

/* Receive input.
 */
 
int ch_game_input(struct ch_game *game,int eventid) {
  if (!game->gridder.grid) return 0;
  if (game->eliminatecounter) return 0;
  if (game->input_blackout&(1<<eventid)) return 0;
  game->input_blackout|=1<<eventid;
  switch (eventid) {
    case CH_EVENTID_LEFT: return ch_game_move_left(game);
    case CH_EVENTID_RIGHT: return ch_game_move_right(game);
    case CH_EVENTID_CLOCK: return ch_game_rotate(game,1);
    case CH_EVENTID_CCLOCK: return ch_game_rotate(game,-1);
    case CH_EVENTID_DROP: return ch_game_drop(game);
    case CH_EVENTID_PAUSE: break;//TODO
  }
  return 0;
}
