/* ch_game.h
 * Represents the entire logical model of a running game.
 * There could be more than one per program run, but only one at a time.
 */
 
#ifndef CH_GAME_H
#define CH_GAME_H

// The one moving piece. Always composed of four tiles.
struct ch_brick {
  int x,y;
  uint16_t shape; // 16 positions LRTB big-endianly, top left at (x,y)
  uint8_t tileid;
};

struct ch_game {
  int refc;
  int input_blackout; // mask of (1<<CH_EVENTID_*), zeroed at update
  int towerx,towery,towerw,towerh;
  struct rb_grid *grid;
  struct ch_brick brick;
  struct ch_brick nextbrick;
  int nextuip;
  int nextbrickdelay;
  int framesperfall;
  int framesperfall_drop;
  int fallskip; // How many rows to drop on each fall -- only >1 at ludicrous speed
  int fallcounter;
  int dropping;
  int linescorev[4]; // how many points for each type of elimination
  // Eliminating rows stops the action temporarily:
  int eliminatecounter;
  int eliminatev[4]; // rows eliminating
  int eliminatec;
  // Scorekeeping:
  int lines;
  int score;
  int linesuip,scoreuip; // offset in (grid->v) of top left corner (of number only)
  int linesuic,scoreuic; // how many digits, 1 column and 2 rows for each, tileid from 0x56
  int rhlopass; // running state of rhythm quality, 0..999
  int rhuip,rhuic;
  
  /* Caller must provide this to enable rhythm rating.
   * On success, (p,c) are populated with the current phase relative to a qnote in the song.
   * (p) in (0..c-1): 0 is the top of a qnote, c/2 is an eighth note, etc.
   */
  int (*cb_get_phase)(int *p,int *c,void *phase_userdata);
  void *phase_userdata;
  
  int (*cb_sound)(int sfxid,void *sound_userdata);
  void *sound_userdata;
};

struct ch_game *ch_game_new();
void ch_game_del(struct ch_game *game);
int ch_game_ref(struct ch_game *game);

/* Advance time by one frame.
 */
int ch_game_update(struct ch_game *game);

/* Deliver input events.
 * Inputs are stateless impulses, not buttons as usual.
 */
int ch_game_input(struct ch_game *game,int eventid);
#define CH_EVENTID_LEFT    0x01 /* Move left */
#define CH_EVENTID_RIGHT   0x02 /* Move right */
#define CH_EVENTID_CCLOCK  0x03 /* Rotate counter-clockwise */
#define CH_EVENTID_CLOCK   0x04 /* Rotate clockwise */
#define CH_EVENTID_DROP    0x05 /* Drop the piece */
#define CH_EVENTID_PAUSE   0x06 /* Pause or resume */

struct rb_grid *ch_game_generate_grid(struct ch_game *game);

uint16_t ch_game_random_brick_shape(uint8_t *tileid,struct ch_game *game);
uint16_t ch_game_rotate_shape(uint16_t shape,int d);
void ch_game_clear_brick_cells(struct ch_game *game,const struct ch_brick *brick);
void ch_game_print_brick_cells(struct ch_game *game,const struct ch_brick *brick);
void ch_game_new_brick(struct ch_game *game);

int ch_game_for_brick_lower_neighbors(
  struct ch_game *game,
  const struct ch_brick *brick,
  int (*cb)(int x,int y,void *userdata),
  void *userdata
);
int ch_game_for_shape(
  int x,int y,uint16_t shape,
  int (*cb)(int x,int y,void *userdata),
  void *userdata
);

int ch_game_brick_bottom_row(const struct ch_brick *brick);

void ch_game_print_number(
  struct ch_game *game,
  int dstp,int dstc, // offset in grid->v, and column count
  int src,
  int leading_zeroes // nonzero for "00123", zero for "  123"
);

void ch_game_print_rhythm_bar(struct ch_game *game);

int ch_game_end(struct ch_game *game);

int ch_game_sound(struct ch_game *game,int sndid);
#define CH_SFX_THUD        0x7f30
#define CH_SFX_LINES       0x7f31
#define CH_SFX_TETRIS      0x7f32
#define CH_SFX_CHETYORSKA  0x7f33
#define CH_SFX_GAMEOVER    0x7f34
#define CH_SFX_MOVE        0x7f35
#define CH_SFX_ROTATE      0x7f36
#define CH_SFX_DROP        0x7f37

int ch_game_advance_level(struct ch_game *game);

void ch_game_redraw_next_brick(struct ch_game *game);
void ch_game_generate_next_brick(struct ch_game *game);

#endif
