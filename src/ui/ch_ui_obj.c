#include "ch_internal.h"
#include "ch_ui.h"
#include "app/ch_app.h"
#include "game/ch_game.h"
#include <rabbit/rb_image.h>

/* Delete.
 */
 
void ch_ui_del(struct ch_ui *ui) {
  if (!ui) return;
  
  ch_game_del(ui->game);
  
  rb_image_del(ui->font);
  rb_image_del(ui->label_chetyorska);
  rb_image_del(ui->label_recap);
  rb_image_del(ui->label_play);
  rb_image_del(ui->label_quit);
  rb_image_del(ui->label_directions);
  rb_image_del(ui->label_select);
  rb_image_del(ui->label_copyright);
  
  free(ui);
}

/* Dispatch render.
 */

int ch_ui_cb_postrender(struct rb_image *fb,void *userdata) {
  struct ch_ui *ui=userdata;
  switch (ui->mode) {
    case CH_UI_MODE_INTRO: return ch_ui_intro_draw(fb,ui);
    case CH_UI_MODE_LOBBY: return ch_ui_lobby_draw(fb,ui);
  }
  return 0;
}

/* New.
 */
 
struct ch_ui *ch_ui_new(struct ch_app *app) {
  struct ch_ui *ui=calloc(1,sizeof(struct ch_ui));
  if (!ui) return 0;
  
  ui->app=app;
  ui->mode=CH_UI_MODE_INTRO;
  ui->optionp=-1;
  
  app->cb_event=ch_ui_intro_event;
  app->event_userdata=ui;
  app->cb_postrender=ch_ui_cb_postrender;
  app->postrender_userdata=ui;
  
  return ui;
}

/* Update.
 */
 
int ch_ui_update(struct ch_ui *ui) {
  if (ui->quit_requested) return 0;
  switch (ui->mode) {
    case CH_UI_MODE_INTRO: {
      } break;
    case CH_UI_MODE_LOBBY: {
      } break;
    case CH_UI_MODE_PLAY: {
        if (ch_game_update(ui->game,ui->app->beatp,ui->app->beatc)<0) return -1;
        if (ui->game->finished) {
          if (ch_ui_end_game(ui)<0) return -1;
        }
      } break;
  }
  return 1;
}

/* Begin game.
 */
 
int ch_ui_begin_game(struct ch_ui *ui) {

  if (ch_app_play_song(ui->app,CH_SONGID_SEQUENTIAL)<0) return -1;
  
  rb_image_del(ui->label_recap);
  ui->label_recap=0;

  ch_game_del(ui->game);
  if (!(ui->game=ch_game_new())) return -1;
  if (ch_app_set_game(ui->app,ui->game)<0) return -1;
  
  ui->mode=CH_UI_MODE_PLAY;
  ui->optionp=-1;
  
  ui->app->cb_postrender=0;
  ui->app->postrender_userdata=0;
  
  return 0;
}

/* End game.
 */
 
int ch_ui_end_game(struct ch_ui *ui) {

  if (ui->game) {
    //ch_game_report_quality(ui->game);
    ui->game->tempo=1.0;
  }
  if (ch_app_play_song(ui->app,CH_SONGID_LOBBY)<0) return -1;

  ui->mode=CH_UI_MODE_LOBBY;
 
  ui->app->cb_event=ch_ui_intro_event;
  ui->app->event_userdata=ui; 
  ui->app->cb_postrender=ch_ui_cb_postrender;
  ui->app->postrender_userdata=ui;
  
  return 0;
}
