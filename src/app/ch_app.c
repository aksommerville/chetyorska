#include "ch_internal.h"
#include "ch_app.h"
#include "game/ch_game.h"
#include "opt/ossmidi/ch_ossmidi.h"
#include <rabbit/rb_video.h>
#include <rabbit/rb_audio.h>
#include <rabbit/rb_inmgr.h>
#include <rabbit/rb_vmgr.h>
#include <rabbit/rb_synth.h>
#include <rabbit/rb_synth_event.h>
#include <rabbit/rb_grid.h>
#include <rabbit/rb_fs.h>//XXX

int ch_app_configure_synth(struct ch_app *app);

/* Audio PCM callback.
 */
 
static int ch_app_cb_pcm_out(int16_t *v,int c,struct rb_audio *audio) {
  struct ch_app *app=audio->delegate.userdata;
  if (app->synth) {
    if (rb_synth_update(v,c,app->synth)<0) return -1;
  } else {
    memset(v,0,c<<1);
  }
  return 0;
}

/* Close window.
 */
 
static int ch_app_cb_close(struct rb_video *video) {
  struct ch_app *app=video->delegate.userdata;
  app->quit=1;
  return 0;
}

/* Resize window.
 */
 
static int ch_app_cb_resize(struct rb_video *video) {
  struct ch_app *app=video->delegate.userdata;
  return 0;
}

/* Receive or lose WM focus.
 */
 
static int ch_app_cb_focus(struct rb_video *video,int focus) {
  struct ch_app *app=video->delegate.userdata;
  return 0;
}

/* Raw keyboard input from WM.
 */
 
static int ch_app_cb_key(struct rb_video *video,int keycode,int value) {
  struct ch_app *app=video->delegate.userdata;
  if (rb_inmgr_system_keyboard_event(app->inmgr,keycode,value)<0) return -1;
  return 0;
}

/* Digested keyboard input from WM.
 */
 
static int ch_app_cb_text(struct rb_video *video,int codepoint) {
  struct ch_app *app=video->delegate.userdata;
  return 0;
}

/* Mouse motion.
 */
 
static int ch_app_cb_mmotion(struct rb_video *video,int x,int y) {
  struct ch_app *app=video->delegate.userdata;
  return 0;
}

/* Mouse button.
 */
 
static int ch_app_cb_mbutton(struct rb_video *video,int btnid,int value) {
  struct ch_app *app=video->delegate.userdata;
  return 0;
}

/* Mouse wheel.
 */
 
static int ch_app_cb_mwheel(struct rb_video *video,int dx,int dy) {
  struct ch_app *app=video->delegate.userdata;
  return 0;
}

/* General input.
 */
 
static int ch_app_cb_input(struct rb_inmgr *inmgr,const struct rb_input_event *event) {
  struct ch_app *app=inmgr->delegate.userdata;
  if (app->game) {
    if (event->value&&!event->plrid) switch (event->btnid) {
      case RB_BTNID_LEFT: return ch_game_input(app->game,CH_EVENTID_LEFT);
      case RB_BTNID_RIGHT: return ch_game_input(app->game,CH_EVENTID_RIGHT);
      case RB_BTNID_A:
      case RB_BTNID_B:
      case RB_BTNID_R: return ch_game_input(app->game,CH_EVENTID_CLOCK);
      case RB_BTNID_C:
      case RB_BTNID_D:
      case RB_BTNID_L: return ch_game_input(app->game,CH_EVENTID_CCLOCK);
      case RB_BTNID_DOWN: return ch_game_input(app->game,CH_EVENTID_DROP);
      case RB_BTNID_START:
      case RB_BTNID_SELECT: return ch_game_input(app->game,CH_EVENTID_PAUSE);
    }
  }
  return 0;
}

/* Note On.
 */
 
static int ch_app_midi_note_on(struct ch_app *app,uint8_t noteid,uint8_t velocity) {
  if (!velocity) return 0; // actually note off
  if (app->game) {
    //fprintf(stderr,"Note On %02x\n",noteid);
    switch (noteid) {
      case 0x23: return ch_game_input(app->game,CH_EVENTID_DROP); // Acoustic Bass Drum
      case 0x24: return ch_game_input(app->game,CH_EVENTID_DROP); // Bass Drum 1
      case 0x25: return ch_game_input(app->game,CH_EVENTID_LEFT); // Side Stick
      case 0x26: return ch_game_input(app->game,CH_EVENTID_LEFT); // Acoustic Snare
      case 0x27: return ch_game_input(app->game,CH_EVENTID_LEFT); // Hand Clap
      case 0x28: return ch_game_input(app->game,CH_EVENTID_LEFT); // Electric Snare
      case 0x29: return ch_game_input(app->game,CH_EVENTID_RIGHT); // Low Floor Tom
      case 0x2a: break; // Closed Hi Hat
      case 0x2b: return ch_game_input(app->game,CH_EVENTID_RIGHT); // High Floor Tom
      case 0x2c: return ch_game_input(app->game,CH_EVENTID_DROP); // Pedal Hi Hat
      case 0x2d: return ch_game_input(app->game,CH_EVENTID_CLOCK); // Low Tom
      case 0x2e: break; // Open Hi Hat
      case 0x2f: return ch_game_input(app->game,CH_EVENTID_CLOCK); // Low Mid Tom
      case 0x30: return ch_game_input(app->game,CH_EVENTID_CCLOCK); // Hi Mid Tom
      case 0x31: return ch_game_input(app->game,CH_EVENTID_PAUSE); // Crash Cymbal 1
      case 0x32: return ch_game_input(app->game,CH_EVENTID_CCLOCK); // Hi Tom
      case 0x33: return ch_game_input(app->game,CH_EVENTID_PAUSE); // Ride Cymbal 1
      case 0x34: return ch_game_input(app->game,CH_EVENTID_PAUSE); // Chinese Cymbal
      case 0x35: return ch_game_input(app->game,CH_EVENTID_PAUSE); // Ride Bell
      case 0x36: return ch_game_input(app->game,CH_EVENTID_PAUSE); // Tambourine
      case 0x37: return ch_game_input(app->game,CH_EVENTID_PAUSE); // Splash Cymbal
      case 0x39: return ch_game_input(app->game,CH_EVENTID_PAUSE); // Crash Cymbal 2
      case 0x3b: return ch_game_input(app->game,CH_EVENTID_PAUSE); // Ride Cymbal 2
    }
  }
  return 0;
}

/* Consume one MIDI event.
 */
 
static int ch_app_midi_event(struct ch_app *app,const uint8_t *src,int srcc) {

  int srcp=0;
  uint8_t lead=src[srcp++];
  uint8_t opcode=lead&0xf0;
  
  switch (opcode) {
    #define REQUIRE(c) if (srcp>srcc-c) return srcc;
    #define SKIP(c) REQUIRE(c) srcp+=c;
    case 0x80: SKIP(2) break;
    case 0x90: REQUIRE(2) if (ch_app_midi_note_on(app,src[srcp],src[srcp+1])<0) return -1; srcp+=2; break;
    case 0xa0: SKIP(2) break;
    case 0xb0: SKIP(2) break;
    case 0xc0: SKIP(1) break;
    case 0xd0: SKIP(1) break;
    case 0xe0: SKIP(1) break;
    case 0xf0: switch (lead) {
        case 0xf0: {
            while ((srcp<srcc)&&(src[srcp++]!=0xf7)) ;
          } break;
      } break;
    #undef SKIP
    #undef REQUIRE
  }
  return srcp;
}

/* MIDI input.
 */
 
static int ch_app_cb_midi(void *userdata,int devid,const void *src,int srcc) {
  struct ch_app *app=userdata;
  
  if (0) {
    fprintf(stderr,"%s:%d:",__func__,devid);
    const uint8_t *v=src;
    int i=srcc;
    for (;i-->0;v++) fprintf(stderr," %02x",*v);
    fprintf(stderr,"\n");
  }
  
  /* For the time being, we treat all devices alike and no general mapping.
   * TODO We also don't support Running Status -- my hardware doesn't use it. But to be compliant, we have to.
   */
  const uint8_t *SRC=src;
  int srcp=0,err;
  while (srcp<srcc) {
    if ((err=ch_app_midi_event(app,SRC+srcp,srcc-srcp))<0) {
      return -1;
    } else if (!err) { // parsing fails, skip one byte and hope for the best
      srcp++;
    } else {
      srcp+=err;
    }
  }
   
  return 0;
}

/* Play song.
 */
 
static int ch_app_begin_song(struct ch_app *app,const char *path) {
  void *src=0;
  int srcc=rb_file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read MIDI file\n",path);
    return 0;
  }
  struct rb_song *song=rb_song_new(src,srcc,app->audio->delegate.rate);
  free(src);
  if (!song) {
    fprintf(stderr,"%s: Failed to decode MIDI file\n",path);
    return 0;
  }
  if (rb_audio_lock(app->audio)<0) {
    rb_song_del(song);
    return 0;
  }
  int err=rb_synth_play_song(app->synth,song,1);
  rb_audio_unlock(app->audio);
  rb_song_del(song);
  if (err<0) {
    fprintf(stderr,"%s: Failed to begin song\n",path);
    return 0;
  }
  return 0;
}

/* Init audio.
 */
 
static int ch_app_init_audio(struct ch_app *app) {
  //TODO configure drivertype,rate,chanc
  struct rb_audio_delegate delegate={
    .userdata=app,
    .rate=44100,
    .chanc=1,
    .cb_pcm_out=ch_app_cb_pcm_out,
  };
  if (!(app->audio=rb_audio_new(rb_audio_type_by_index(0),&delegate))) return -1;
  if (!(app->synth=rb_synth_new(app->audio->delegate.rate,app->audio->delegate.chanc))) return -1;
  if (ch_app_configure_synth(app)<0) return -1;
  
  /*
    anitrasdance.mid      dvorak-o96-2.mid  No03_Albumblatt.mid    rach-prelude23-09.mid   Troldtog.mid
    concerto-8-2.mid      dvorak-o96-3.mid  nobm.mid               scriabin_etude_2_1.mid  tuileries.mid
    dansenapolitaine.mid  Mvt3_Scherzo.mid  rach-prelude23-05.mid  turpin.mid
  */
  if (ch_app_begin_song(app,"mid/data/anitrasdance.mid")<0) return -1;//Awesome
  //if (ch_app_begin_song(app,"mid/data/concerto-8-2.mid")<0) return -1;//Beautiful
  //if (ch_app_begin_song(app,"mid/data/dansenapolitaine.mid")<0) return -1;//OK, not crazy about the tune but the rhythm is irresistible
  //if (ch_app_begin_song(app,"mid/data/dvorak-o96-2.mid")<0) return -1;
  //if (ch_app_begin_song(app,"mid/data/dvorak-o96-3.mid")<0) return -1;
  //if (ch_app_begin_song(app,"mid/data/Mvt3_Scherzo.mid")<0) return -1;//oy not with these instruments
  //if (ch_app_begin_song(app,"mid/data/No03_Albumblatt.mid")<0) return -1;//Nice and silly
  //if (ch_app_begin_song(app,"mid/data/nobm.mid")<0) return -1;//Great tune, kind of like In the Hall of the Mountain King
  //if (ch_app_begin_song(app,"mid/data/rach-prelude23-05.mid")<0) return -1;// Beautiful and distinctive
  //if (ch_app_begin_song(app,"mid/data/rach-prelude23-09.mid")<0) return -1;
  //if (ch_app_begin_song(app,"mid/data/scriabin-etude_2_1.mid")<0) return -1;//XXX Doesn't play with current instruments
  //if (ch_app_begin_song(app,"mid/data/turpin.mid")<0) return -1;
  //if (ch_app_begin_song(app,"mid/data/Troldtog.mid")<0) return -1;//Mmm nice n trolly
  //if (ch_app_begin_song(app,"mid/data/tuileries.mid")<0) return -1;

  return 0;
}

/* Init video.
 */
 
static int ch_app_init_video(struct ch_app *app) {
  //TODO configure (persist?) window size, fullscreen, drivertype
  struct rb_video_delegate delegate={
    .userdata=app,
    .winw=1200,
    .winh=675,
    .fullscreen=0,
    .title="Chetyorska",
    .cb_close=ch_app_cb_close,
    .cb_resize=ch_app_cb_resize,
    .cb_focus=ch_app_cb_focus,
    .cb_key=ch_app_cb_key,
    .cb_text=ch_app_cb_text,
    .cb_mmotion=ch_app_cb_mmotion,
    .cb_mbutton=ch_app_cb_mbutton,
    .cb_mwheel=ch_app_cb_mwheel,
  };
  if (!(app->video=rb_video_new(rb_video_type_by_index(0),&delegate))) return -1;
  if (!(app->vmgr=rb_vmgr_new())) return -1;
  
  //TODO get organized about loading data
  { void *serial=0;
    int serialc=rb_file_read(&serial,"mid/data/tiles");
    if (serialc<0) return -1;
    int err=rb_vmgr_set_image_serial(app->vmgr,1,serial,serialc);
    free(serial);
    if (err<0) return -1;
  }
  
  return 0;
}

/* Initialize input.
 */
 
static int ch_app_init_input(struct ch_app *app) {

  struct rb_inmgr_delegate delegate={
    .userdata=app,
    .cb_event=ch_app_cb_input,
  };
  if (!(app->inmgr=rb_inmgr_new(&delegate))) return -1;
  if (rb_inmgr_use_system_keyboard(app->inmgr)<0) return -1;
  if (rb_inmgr_connect_all(app->inmgr)<0) return -1;
  
  if (ch_ossmidi_init(ch_app_cb_midi,app)<0) return -1;
  
  return 0;
}

/* New.
 */
 
struct ch_app *ch_app_new(int argc,char **argv) {
  struct ch_app *app=calloc(1,sizeof(struct ch_app));
  if (!app) return 0;
  
  // Get sound effects running so we can judge this better
  //app->timing_bias=-2000;//TODO Figure out an appropriate value, accounting for latency in MIDI and PCM drivers.
  
  if (ch_app_init_audio(app)<0) {
    fprintf(stderr,"Failed to initialize audio.\n");
    ch_app_del(app);
    return 0;
  }
  
  if (ch_app_init_video(app)<0) {
    fprintf(stderr,"Failed to initialize video.\n");
    ch_app_del(app);
    return 0;
  }
  
  if (ch_app_init_input(app)<0) {
    fprintf(stderr,"Failed to initialize input.\n");
    ch_app_del(app);
    return 0;
  }
  
  return app;
}

/* Delete.
 */
 
void ch_app_del(struct ch_app *app) {
  if (!app) return;
  
  rb_audio_del(app->audio);
  rb_video_del(app->video);
  rb_inmgr_del(app->inmgr);
  rb_vmgr_del(app->vmgr);
  rb_synth_del(app->synth);
  ch_ossmidi_quit();
  ch_game_del(app->game);
  
  free(app);
}

/* Update.
 */

int ch_app_update(struct ch_app *app) {
  
  if (rb_audio_update(app->audio)<0) return -1;
  if (rb_inmgr_update(app->inmgr)<0) return -1;
  if (ch_ossmidi_update()<0) return -1;
  if (rb_video_update(app->video)<0) return -1;
  
  struct rb_image *fb=rb_vmgr_render(app->vmgr);
  if (!fb) return -1;
  if (rb_video_swap(app->video,fb)<0) return -1;
  
  if (app->quit) {
    app->quit=0;
    return 0;
  }
  return 1;
}

/* Get song phase.
 */
 
static int ch_app_get_phase(int *p,int *c,void *userdata) {
  struct ch_app *app=userdata;
  if (rb_synth_get_song_phase(p,c,app->synth)<0) return -1;
  (*p)+=app->timing_bias;
  if (*p<0) (*p)+=(*c);
  else if (*p>=*c) (*p)-=(*c);
  return 0;
}

/* Play sound effect.
 */
 
static int ch_app_play_sound(int sfx,void *userdata) {
  struct ch_app *app=userdata;
  if (rb_audio_lock(app->audio)<0) return -1;
  int err=rb_synth_play_note(app->synth,sfx>>8,sfx&0xff);
  rb_audio_unlock(app->audio);
  return err;
}

/* Set game.
 */
 
int ch_app_set_game(struct ch_app *app,struct ch_game *game) {
  if (app->game==game) return 0;
  
  if (game&&(ch_game_ref(game)<0)) return -1;
  ch_game_del(app->game);
  app->game=game;
  
  if (game) {
    struct rb_grid *grid=ch_game_generate_grid(game);
    if (!grid) return -1;
    if (rb_vmgr_set_grid(app->vmgr,grid)<0) return -1;
    app->vmgr->scrollx=-((RB_FB_W-CH_TILESIZE*grid->w)>>1);
    app->vmgr->scrolly=0;
    
    struct rb_sprite_group *group=ch_game_generate_sprites(game);
    if (!group) return -1;
    if (rb_vmgr_set_sprites(app->vmgr,group)<0) return -1;
    
    game->cb_get_phase=ch_app_get_phase;
    game->phase_userdata=app;
    game->cb_sound=ch_app_play_sound;
    game->sound_userdata=app;
 
  } else {
    if (rb_vmgr_set_grid(app->vmgr,0)<0) return -1;
  }
  return 0;
}
