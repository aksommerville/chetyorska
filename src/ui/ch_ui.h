/* ch_ui.h
 */
 
#ifndef CH_UI_H
#define CH_UI_H

struct rb_image;

#define CH_UI_MODE_INTRO     1 /* Lobby, on startup only. */
#define CH_UI_MODE_LOBBY     2 /* Lobby, with a completed game (report scores etc) */
#define CH_UI_MODE_PLAY      3 /* Game in progress. */

struct ch_ui {
  struct ch_app *app; // WEAK
  struct ch_game *game; // STRONG,OPTIONAL
  int mode;
  
  struct rb_image *font;
  int fontcontent;
  int fontflags;
  struct rb_image *label_chetyorska;
  struct rb_image *label_recap;
  struct rb_image *label_play;
  struct rb_image *label_quit;
  struct rb_image *label_directions;
  struct rb_image *label_select;
  struct rb_image *label_copyright;
  int optionp;
  int quit_requested;
};

struct ch_ui *ch_ui_new(struct ch_app *app);
void ch_ui_del(struct ch_ui *ui);

/* 0 to quit, >0 to proceed, <0 for errors.
 */
int ch_ui_update(struct ch_ui *ui);

/* Mode changes.
 */
int ch_ui_begin_game(struct ch_ui *ui);
int ch_ui_end_game(struct ch_ui *ui);

int ch_ui_intro_draw(struct rb_image *fb,struct ch_ui *ui);
int ch_ui_lobby_draw(struct rb_image *fb,struct ch_ui *ui);
int ch_ui_cb_postrender(struct rb_image *fb,void *userdata);

int ch_ui_intro_event(int eventid,void *userdata);
int ch_ui_lobby_event(int eventid,void *userdata);
// In PLAY mode, game receives events directly; we don't see them.

#endif
