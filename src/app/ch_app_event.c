#include "ch_internal.h"
#include "ch_app.h"
#include "game/ch_game.h"
#include <rabbit/rb_inmgr.h>
#include <rabbit/rb_video.h>

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
  rb_video_suppress_screensaver(app->video);
  if (event->value&&!event->plrid) switch (event->btnid) {
    case RB_BTNID_LEFT: return app->cb_event(CH_EVENTID_LEFT,app->event_userdata);
    case RB_BTNID_RIGHT: return app->cb_event(CH_EVENTID_RIGHT,app->event_userdata);
    case RB_BTNID_A:
    case RB_BTNID_B:
    /*case RB_BTNID_R:*/ return app->cb_event(CH_EVENTID_CLOCK,app->event_userdata);
    case RB_BTNID_C:
    case RB_BTNID_D:
    /*case RB_BTNID_L:*/ return app->cb_event(CH_EVENTID_CCLOCK,app->event_userdata);
    case RB_BTNID_DOWN: return app->cb_event(CH_EVENTID_DROP,app->event_userdata);
    case RB_BTNID_START:
    case RB_BTNID_SELECT: return app->cb_event(CH_EVENTID_PAUSE,app->event_userdata);
    
    //XXX TEMP
    case RB_BTNID_L: app->suspend=app->suspend?0:1; break;
    case RB_BTNID_R: if (app->suspend) app->suspend=2; break;
  }
  return 0;
}

/* Note On.
 */
 
static int ch_app_midi_note_on(struct ch_app *app,uint8_t noteid,uint8_t velocity) {
  if (!velocity) return 0; // actually note off
  rb_video_suppress_screensaver(app->video);
  switch (noteid) {
    case 0x23: return app->cb_event(CH_EVENTID_DROP,app->event_userdata); // Acoustic Bass Drum
    case 0x24: return app->cb_event(CH_EVENTID_DROP,app->event_userdata); // Bass Drum 1
    case 0x25: return app->cb_event(CH_EVENTID_LEFT,app->event_userdata); // Side Stick
    case 0x26: return app->cb_event(CH_EVENTID_LEFT,app->event_userdata); // Acoustic Snare
    case 0x27: return app->cb_event(CH_EVENTID_LEFT,app->event_userdata); // Hand Clap
    case 0x28: return app->cb_event(CH_EVENTID_LEFT,app->event_userdata); // Electric Snare
    case 0x29: return app->cb_event(CH_EVENTID_RIGHT,app->event_userdata); // Low Floor Tom
    case 0x2a: break; // Closed Hi Hat
    case 0x2b: return app->cb_event(CH_EVENTID_RIGHT,app->event_userdata); // High Floor Tom
    case 0x2c: return app->cb_event(CH_EVENTID_DROP,app->event_userdata); // Pedal Hi Hat
    case 0x2d: return app->cb_event(CH_EVENTID_CLOCK,app->event_userdata); // Low Tom
    case 0x2e: break; // Open Hi Hat
    case 0x2f: return app->cb_event(CH_EVENTID_CLOCK,app->event_userdata); // Low Mid Tom
    case 0x30: return app->cb_event(CH_EVENTID_CCLOCK,app->event_userdata); // Hi Mid Tom
    case 0x31: return app->cb_event(CH_EVENTID_PAUSE,app->event_userdata); // Crash Cymbal 1
    case 0x32: return app->cb_event(CH_EVENTID_CCLOCK,app->event_userdata); // Hi Tom
    case 0x33: return app->cb_event(CH_EVENTID_PAUSE,app->event_userdata); // Ride Cymbal 1
    case 0x34: return app->cb_event(CH_EVENTID_PAUSE,app->event_userdata); // Chinese Cymbal
    case 0x35: return app->cb_event(CH_EVENTID_PAUSE,app->event_userdata); // Ride Bell
    case 0x36: return app->cb_event(CH_EVENTID_PAUSE,app->event_userdata); // Tambourine
    case 0x37: return app->cb_event(CH_EVENTID_PAUSE,app->event_userdata); // Splash Cymbal
    case 0x39: return app->cb_event(CH_EVENTID_PAUSE,app->event_userdata); // Crash Cymbal 2
    case 0x3b: return app->cb_event(CH_EVENTID_PAUSE,app->event_userdata); // Ride Cymbal 2
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
 
int ch_app_cb_midi(void *userdata,int devid,const void *src,int srcc) {
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

/* Glue.
 */
 
void ch_app_set_video_callbacks(struct rb_video_delegate *delegate) {
  delegate->cb_close=ch_app_cb_close;
  delegate->cb_resize=ch_app_cb_resize;
  delegate->cb_focus=ch_app_cb_focus;
  delegate->cb_key=ch_app_cb_key;
  delegate->cb_text=ch_app_cb_text;
  delegate->cb_mmotion=ch_app_cb_mmotion;
  delegate->cb_mbutton=ch_app_cb_mbutton;
  delegate->cb_mwheel=ch_app_cb_mwheel;
}

void ch_app_set_inmgr_callbacks(struct rb_inmgr_delegate *delegate) {
  delegate->cb_event=ch_app_cb_input;
}

int ch_app_cb_event_null(int eventid,void *userdata) {
  return 0;
}
