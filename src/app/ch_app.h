#ifndef CH_APP_H
#define CH_APP_H

struct ch_app {
  struct rb_inmgr *inmgr;
  struct rb_audio *audio;
  struct rb_video *video;
  struct rb_vmgr *vmgr;
  struct rb_synth *synth;
  int quit; // nonzero if someone requested quit (eg close window)
  struct ch_game *game;
  int timing_bias;
  int beatp,beatc; // song's beat, mapped to game cycles
  int srcbeatp; // absolute beat
  struct ch_song {
    int id;
    struct rb_song *song;
  } *songv;
  int songc,songa;
  int seqsongp;
  
  //XXX TEMP suspension
  int suspend;

  // For this session only. TODO list and persistence
  int highscore;
  
  // This must not be null. ch_app_cb_event_null to discard events.
  int (*cb_event)(int eventid,void *userdata);
  void *event_userdata;
  
  // Null ok. Called after the standard vmgr render.
  int (*cb_postrender)(struct rb_image *fb,void *userdata);
  void *postrender_userdata;
};

struct ch_app *ch_app_new(int argc,char **argv);
void ch_app_del(struct ch_app *app);

// Blocks according to video timing.
int ch_app_update(struct ch_app *app);

/* Setting game replaces my event callback.
 */
int ch_app_set_game(struct ch_app *app,struct ch_game *game);

int ch_app_play_song(struct ch_app *app,int songid);
#define CH_SONGID_SILENCE    -1
#define CH_SONGID_RANDOM     -2
#define CH_SONGID_SEQUENTIAL -3
#define CH_SONGID_LOBBY      14

int ch_app_play_sound(struct ch_app *app,int sfx);

int ch_app_cb_event_null(int eventid,void *userdata);

#endif
