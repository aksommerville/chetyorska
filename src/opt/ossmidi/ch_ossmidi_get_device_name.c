#include "ch_internal.h"
#include "ch_ossmidi.h"
#include <rabbit/rb_fs.h>

/* Apparently MIDI devices under OSS emulation do not respond to any ioctls.
 * I think in the proper OSS days, you got a device name either from the device or /dev/sequencer?
 * Anyway, I've found that /proc/asound/oss/sndstat has the content we need.
 * That file was originally located at /dev/sndstat, so we'll check there too.
 * ...FWIW I'm not at all happy about this arrangement. Is there a better way?
 */
 
static int ch_ossmidi_device_name_from_sndstat_line(
  char *dst,int dsta,
  const char *src,int srcc,
  int devid
) {
  int thisid=0,thisidc=0;
  while ((thisidc<srcc)&&(src[thisidc]>='0')&&(src[thisidc]<='9')) {
    thisid*=10;
    thisid+=src[thisidc++]-'0';
  }
  if (!thisidc||(thisid!=devid)||(thisidc>=srcc)||(src[thisidc]!=':')) return -1;
  int srcp=thisidc+1;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int dstc=srcc-srcp;
  if (dstc<=dsta) {
    memcpy(dst,src+srcp,dstc);
    if (dstc<dsta) dst[dstc]=0;
  }
  return dstc;
}
 
static int ch_ossmidi_device_name_from_sndstat(
  char *dst,int dsta,
  const char *src,int srcc,
  int devid
) {
  int srcp=0,ready=0;
  while (srcp<srcc) {
    const char *line=src+srcp;
    int linec=0;
    while ((srcp<srcc)&&(src[srcp++]!=0x0a)) linec++;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    
    if (ready) { // "MIDI Devices" section started...
    
      if (!linec) break; // Stop on blank line.
    
      int dstc=ch_ossmidi_device_name_from_sndstat_line(dst,dsta,line,linec,devid);
      if (dstc>=0) return dstc;
      
    } else if ((linec==13)&&!memcmp(line,"Midi devices:",13)) {
      ready=1;
    }
  }
  return -1;
}

int ch_ossmidi_get_device_name(char *dst,int dsta,int devid) {
  void *src=0;
  int srcc=-1;
  if ((srcc=rb_file_read_pipesafe(&src,"/proc/asound/oss/sndstat"))<0) {
    if ((srcc=rb_file_read_pipesafe(&src,"/dev/sndstat"))<0) {
      return -1;
    }
  }
  int dstc=ch_ossmidi_device_name_from_sndstat(dst,dsta,src,srcc,devid);
  free(src);
  return dstc;
}
