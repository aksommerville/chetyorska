#include "ch_internal.h"
#include "ch_ui.h"
#include "game/ch_game.h"
#include "app/ch_app.h"
#include <rabbit/rb_image.h>
#include <rabbit/rb_font.h>
#include <rabbit/rb_vmgr.h>

/* Generate font and labels if we don't have them yet.
 */
 
static int ch_ui_init_font(struct ch_ui *ui) {
  struct rb_image *image=ui->app->vmgr->imagev[2];
  if (rb_image_ref(image)>=0) {
    ui->font=image;
    ui->fontcontent=RB_FONTCONTENT_G0;
    ui->fontflags=RB_FONT_FLAG_MARGINL|RB_FONT_FLAG_MARGINT;
  } else {
    if (!(ui->font=rb_font_generate_minimal())) return -1;
    ui->fontcontent=RB_FONTCONTENT_G0;
    ui->fontflags=RB_FONT_FLAG_MARGINL|RB_FONT_FLAG_MARGINT;
  }
  return 0;
}

static int ch_ui_init_labels(struct ch_ui *ui) {
  #define _(tag,text) if (!(ui->label_##tag=rb_font_print( \
    ui->font,ui->fontcontent,ui->fontflags,0, \
    text,sizeof(text)-1 \
  ))) return -1;
  
  _(chetyorska,"Chetyorska!")
  _(play,"Play")
  _(quit,"Quit")
  _(directions,"<-- Snare | Floor Tom -->")
  _(select,"Crash to select")
  _(copyright,"(C) 2021 AK Sommerville")
  
  #undef _
  return 0;
}
 
static int ch_ui_require_labels(struct ch_ui *ui) {
  if (ui->font) return 0;
  if (ch_ui_init_font(ui)<0) return -1;
  if (ch_ui_init_labels(ui)<0) {
    rb_image_del(ui->font);
    ui->font=0;
    return -1;
  }
  return 0;
}

/* Draw intro.
 */
 
static uint32_t ch_ui_blend_text(uint32_t dst,uint32_t src,void *userdata) {
  return 0xffffff00;
}
 
int ch_ui_intro_draw(struct rb_image *fb,struct ch_ui *ui) {
  if (ch_ui_require_labels(ui)<0) return -1;
  
  rb_image_blit_recolor(fb,RB_FB_W>>1,20,ui->label_chetyorska,RB_ALIGN_CENTER,0xffffff00);
  
  if (ui->label_recap) {
    rb_image_blit_recolor(fb,RB_FB_W>>1,50,ui->label_recap,RB_ALIGN_CENTER,0xffffffff);
  }
  
  int optioncplus1=3,p=0;
  int optiony=90;
  #define OPTION(tag) rb_image_blit_recolor( \
    fb,((p+1)*RB_FB_W)/optioncplus1,optiony,ui->label_##tag,RB_ALIGN_CENTER,(p==ui->optionp)?0xffffe0a0:0xff805030 \
  ); p++;
  OPTION(play)
  OPTION(quit)
  #undef OPTION
  
  rb_image_blit_recolor(fb,RB_FB_W>>1,105,ui->label_directions,RB_ALIGN_CENTER,0xff808080);
  rb_image_blit_recolor(fb,RB_FB_W>>1,115,ui->label_select,RB_ALIGN_CENTER,0xff808080);
  
  rb_image_blit_recolor(fb,1,RB_FB_H-1,ui->label_copyright,RB_ALIGN_SW,0xff808080);
  
  return 0;
}

/* Draw lobby.
 */
 
int ch_ui_lobby_draw(struct rb_image *fb,struct ch_ui *ui) {

  if (!ui->label_recap&&ui->game) {
    double average=0.0;
    if (ui->game->lines) {
      average=(double)(ui->game->score)/(double)(ui->game->lines);
    }
    ui->label_recap=rb_font_printf(
      ui->font,ui->fontcontent,ui->fontflags,0,
      "Score: %d, Lines: %d, Avg: %.1f",
      ui->game->score,ui->game->lines,average
    );
  }

  rb_image_darken(fb,0x40);
  return ch_ui_intro_draw(fb,ui);
}

/* Change selection.
 */
 
static int ch_ui_move_optionp(struct ch_ui *ui,int d) {
  ch_app_play_sound(ui->app,CH_SFX_MOVE);
  
  if (0) { // General case, if we add more options.
    const int optionc=2;
    ui->optionp+=d;
    if (ui->optionp<0) ui->optionp=optionc-1;
    else if (ui->optionp>=optionc) ui->optionp=0;
  } else { // Since there's only two, make it absolute, snare=Play and tom=Quit
    if (d<0) ui->optionp=0;
    else if (d>0) ui->optionp=1;
  }
  
  return 0;
}

/* Receive event.
 */
 
int ch_ui_intro_event(int eventid,void *userdata) {
  struct ch_ui *ui=userdata;
  switch (eventid) {
    case CH_EVENTID_PAUSE: switch (ui->optionp) {
        case 0: return ch_ui_begin_game(ui);
        case 1: ui->quit_requested=1; break;
      } break;
    case CH_EVENTID_LEFT: return ch_ui_move_optionp(ui,-1);
    case CH_EVENTID_RIGHT: return ch_ui_move_optionp(ui,1);
  }
  return 0;
}

int ch_ui_lobby_event(int eventid,void *userdata) {
  return ch_ui_intro_event(eventid,userdata);
}
