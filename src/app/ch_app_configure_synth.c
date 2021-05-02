#include "ch_internal.h"
#include "ch_app.h"
#include <rabbit/rb_archive.h>
#include <rabbit/rb_fs.h>
#include <rabbit/rb_synth.h>
#include <rabbit/rb_synth_node.h>
#include <rabbit/rb_audio.h>

//TODO synth config in a data archive

#define ENV(a,d,r) ( \
  RB_ENV_FLAG_PRESET| \
  RB_ENV_PRESET_ATTACK##a| \
  RB_ENV_PRESET_DECAY##d| \
  RB_ENV_PRESET_RELEASE##r| \
0)

static const uint8_t synth_config[]={

  0x00,66, // bright cartoon piano
    RB_SYNTH_NTID_instrument,
    0x02,RB_SYNTH_FIELD_TYPE_SERIAL2,0,61, // nodes
      RB_SYNTH_NTID_env,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_U8,0x01, // mode=set
        0x03,RB_SYNTH_FIELD_TYPE_SERIAL1,11, // content
          RB_ENV_FLAG_INIT_LEVEL|RB_ENV_FLAG_LEVEL_RANGE,
          0x00,0x01,0x00,
          0x20,
          0x08,0xff,
          0x40,0xc0,
          0xf0,0x80,
        0,
      RB_SYNTH_NTID_fm,
        0x01,0x00, // main
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ, // rate
        0x03,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x00,0x00,0x00, // mod0
        0x04,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x01,0x00,0x00, // mod1
        0x05,0x01, // range
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x00, // main
        0x03,RB_SYNTH_FIELD_TYPE_SERIAL1,13, // content
          RB_ENV_FLAG_CURVE|RB_ENV_FLAG_LEVEL_RANGE,
          0x00,0x00,0x10,
          0x04,0xff,0xc0,
          0x0c,0x50,0xc0,
          0x80,0x00,0x60,
        0x00,

  0x34,77, // ok electric guitar
    RB_SYNTH_NTID_instrument,
    0x01,0x00, // main
    0x02,RB_SYNTH_FIELD_TYPE_SERIAL2,0,70, // nodes
      RB_SYNTH_NTID_fm,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ, // rate
        0x03,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x00,0x00,0x00, // mod0
        0x04,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x00,0x80,0x00, // mod1
        0x05,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x01,0x00,0x00, // range
        0x00,
      RB_SYNTH_NTID_mlt,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_U0_8,0x80, // arg
        0x00,
      RB_SYNTH_NTID_add,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_U0_8,0x80, // arg
        0x00,
      RB_SYNTH_NTID_osc,
        0x01,0x00, // main
        0x03,RB_SYNTH_FIELD_TYPE_U8,RB_OSC_SHAPE_SINE,
        0x04,0x01, // phase
        0x05,RB_SYNTH_FIELD_TYPE_U0_8,0x40, // level
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x00, // main
        0x03,RB_SYNTH_FIELD_TYPE_U8,ENV(0,3,7),
        0x00,
      RB_SYNTH_NTID_gain,
        0x01,0x00, // main
        0x02,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x05,0x00,0x00,
        0x03,RB_SYNTH_FIELD_TYPE_U0_8,0x06,
        0x00,

  0x35,77, // wacky brass
    RB_SYNTH_NTID_instrument,
    0x01,0x00, // main=buffer[0]
    0x02,RB_SYNTH_FIELD_TYPE_SERIAL2,0,70, // nodes
      RB_SYNTH_NTID_osc,
        0x01,0x00, // main=buffer[0]
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ,
        0x03,RB_SYNTH_FIELD_TYPE_U8,RB_OSC_SHAPE_SINE,
        0x05,RB_SYNTH_FIELD_TYPE_U0_8,0xff, // level
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x00, // main=buffer[0]
        0x02,RB_SYNTH_FIELD_TYPE_U8,0x00, // mode=mlt
        0x03,RB_SYNTH_FIELD_TYPE_U8,
          RB_ENV_FLAG_PRESET
          |RB_ENV_PRESET_ATTACK1
          |RB_ENV_PRESET_DECAY1
          |RB_ENV_PRESET_RELEASE4,
        0x00,
        
      RB_SYNTH_NTID_osc,
        0x01,0x01, // main=buffer[1]
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ,
        0x03,RB_SYNTH_FIELD_TYPE_U8,RB_OSC_SHAPE_SINE,
        0x05,RB_SYNTH_FIELD_TYPE_U0_8,0xff, // level
        0x00,
      RB_SYNTH_NTID_mlt,
        0x01,0x01, // main=buffer[1]
        0x02,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x08,0x00,0x00,
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x01, // main=buffer[1]
        0x03,RB_SYNTH_FIELD_TYPE_U8,
          RB_ENV_FLAG_PRESET
          |RB_ENV_PRESET_ATTACK0
          |RB_ENV_PRESET_DECAY1
          |RB_ENV_PRESET_RELEASE5,
        0x00,
        
      RB_SYNTH_NTID_mlt,
        0x01,0x00, // main=buffer[0]
        0x02,0x01, // arg=buffer[1]
        0x00,
      RB_SYNTH_NTID_gain,
        0x01,0x00, // main=buffer[0]
        0x02,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x14,0x00,0x00, // gain
        0x03,RB_SYNTH_FIELD_TYPE_U0_8,0x04, // clip
        0x00,
        
  0x28,77,
    RB_SYNTH_NTID_instrument,
    0x01,0x00, // main
    0x02,RB_SYNTH_FIELD_TYPE_SERIAL2,0,70, // nodes
      RB_SYNTH_NTID_fm,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ, // rate
        0x03,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x00,0x00,0x00, // mod0
        0x04,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x02,0x00,0x00, // mod1
        0x05,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x02,0x00,0x00, // range
        0x00,
      RB_SYNTH_NTID_mlt,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_U0_8,0x80, // arg
        0x00,
      RB_SYNTH_NTID_add,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_U0_8,0x80, // arg
        0x00,
      RB_SYNTH_NTID_osc,
        0x01,0x00, // main
        0x03,RB_SYNTH_FIELD_TYPE_U8,RB_OSC_SHAPE_SINE,
        0x04,0x01, // phase
        0x05,RB_SYNTH_FIELD_TYPE_U0_8,0x40, // level
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x00, // main
        0x03,RB_SYNTH_FIELD_TYPE_U8,ENV(0,3,7),
        0x00,
      RB_SYNTH_NTID_gain,
        0x01,0x00, // main
        0x02,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x05,0x00,0x00,
        0x03,RB_SYNTH_FIELD_TYPE_U0_8,0x08,
        0x00,
    
  0x2d,77,
    RB_SYNTH_NTID_instrument,
    0x01,0x00, // main
    0x02,RB_SYNTH_FIELD_TYPE_SERIAL2,0,70, // nodes
      RB_SYNTH_NTID_fm,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ, // rate
        0x03,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x00,0x00,0x00, // mod0
        0x04,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x00,0x80,0x00, // mod1
        0x05,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x01,0x00,0x00, // range
        0x00,
      RB_SYNTH_NTID_mlt,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_U0_8,0x80, // arg
        0x00,
      RB_SYNTH_NTID_add,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_U0_8,0x80, // arg
        0x00,
      RB_SYNTH_NTID_osc,
        0x01,0x00, // main
        0x03,RB_SYNTH_FIELD_TYPE_U8,RB_OSC_SHAPE_SINE,
        0x04,0x01, // phase
        0x05,RB_SYNTH_FIELD_TYPE_U0_8,0x40, // level
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x00, // main
        0x03,RB_SYNTH_FIELD_TYPE_U8,ENV(0,3,7),
        0x00,
      RB_SYNTH_NTID_gain,
        0x01,0x00, // main
        0x02,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x05,0x00,0x00,
        0x03,RB_SYNTH_FIELD_TYPE_U0_8,0x05,
        0x00,
    
  0x70,1,
    RB_SYNTH_NTID_noop,
    
  0x7f,77,//sound effects (TODO)
    RB_SYNTH_NTID_instrument,
    0x01,0x00, // main=buffer[0]
    0x02,RB_SYNTH_FIELD_TYPE_SERIAL2,0,70, // nodes
      RB_SYNTH_NTID_osc,
        0x01,0x00, // main=buffer[0]
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ,
        0x03,RB_SYNTH_FIELD_TYPE_U8,RB_OSC_SHAPE_SINE,
        0x05,RB_SYNTH_FIELD_TYPE_U0_8,0xff, // level
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x00, // main=buffer[0]
        0x02,RB_SYNTH_FIELD_TYPE_U8,0x00, // mode=mlt
        0x03,RB_SYNTH_FIELD_TYPE_U8,
          RB_ENV_FLAG_PRESET
          |RB_ENV_PRESET_ATTACK1
          |RB_ENV_PRESET_DECAY1
          |RB_ENV_PRESET_RELEASE4,
        0x00,
        
      RB_SYNTH_NTID_osc,
        0x01,0x01, // main=buffer[1]
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ,
        0x03,RB_SYNTH_FIELD_TYPE_U8,RB_OSC_SHAPE_SINE,
        0x05,RB_SYNTH_FIELD_TYPE_U0_8,0xff, // level
        0x00,
      RB_SYNTH_NTID_mlt,
        0x01,0x01, // main=buffer[1]
        0x02,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x08,0x00,0x00,
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x01, // main=buffer[1]
        0x03,RB_SYNTH_FIELD_TYPE_U8,
          RB_ENV_FLAG_PRESET
          |RB_ENV_PRESET_ATTACK0
          |RB_ENV_PRESET_DECAY1
          |RB_ENV_PRESET_RELEASE5,
        0x00,
        
      RB_SYNTH_NTID_mlt,
        0x01,0x00, // main=buffer[0]
        0x02,0x01, // arg=buffer[1]
        0x00,
      RB_SYNTH_NTID_gain,
        0x01,0x00, // main=buffer[0]
        0x02,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x14,0x00,0x00, // gain
        0x03,RB_SYNTH_FIELD_TYPE_U0_8,0x50, // clip
        0x00,
};

/* Configure synth.
 */
 
static int ch_app_cb_archive(uint32_t type,int id,const void *src,int srcc,void *userdata) {
  struct ch_app *app=userdata;
  //fprintf(stderr,"%s %08x %d, %d bytes\n",__func__,type,id,srcc);
  switch (type) {
    case RB_RES_TYPE_snth: {
        if (rb_synth_load_program(app->synth,id,src,srcc)<0) return -1;
      } break;
  }
  return 0;
}
 
int ch_app_configure_synth(struct ch_app *app) {

  if (rb_audio_lock(app->audio)<0) return -1;
  if (rb_synth_configure(app->synth,synth_config,sizeof(synth_config))<0) {
    rb_audio_unlock(app->audio);
    return -1;
  }
  if (rb_archive_read("mid/data/sfxar",ch_app_cb_archive,app)<0) {//TODO path
    rb_audio_unlock(app->audio);
    return -1;
  }
  rb_audio_unlock(app->audio);
  
  return 0;
}
