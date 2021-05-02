#include "ch_internal.h"
#include "ch_ossmidi.h"
#include <rabbit/rb_fs.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/poll.h>
#include <sys/inotify.h>

int ch_ossmidi_get_device_name(char *dst,int dsta,int devid);

/* Globals.
 */

static struct {
  int init;
  int (*cb)(void *userdata,int devid,const void *src,int srcc);
  void *userdata;
  int fd;
  int refresh;
  char *root;
  int rootc;
  struct ch_ossmidi_device {
    int fd;
    char *path;
    int pathc;
    int devid;
  } *devicev;
  int devicec,devicea;
  void *pollfdv;
  int pollfdc,pollfda;
} ch_ossmidi={0};

/* Cleanup.
 */
 
static void ch_ossmidi_device_cleanup(struct ch_ossmidi_device *device) {
  if (device->fd>=0) close(device->fd);
  if (device->path) free(device->path);
}

void ch_ossmidi_quit() {
  if (!ch_ossmidi.init) return;
  if (ch_ossmidi.root) free(ch_ossmidi.root);
  if (ch_ossmidi.fd>=0) close(ch_ossmidi.fd);
  if (ch_ossmidi.devicev) {
    while (ch_ossmidi.devicec-->0) {
      ch_ossmidi_device_cleanup(ch_ossmidi.devicev+ch_ossmidi.devicec);
    }
    free(ch_ossmidi.devicev);
  }
  if (ch_ossmidi.pollfdv) free(ch_ossmidi.pollfdv);
  memset(&ch_ossmidi,0,sizeof(ch_ossmidi));
}

/* New.
 */

int ch_ossmidi_init(
  int (*cb)(void *userdata,int devid,const void *src,int srcc),
  void *userdata
) {
  const char *path="/dev/";
  int pathc=5;
  
  if (ch_ossmidi.init) return -1;
  memset(&ch_ossmidi,0,sizeof(ch_ossmidi));
  ch_ossmidi.init=1;
  
  ch_ossmidi.cb=cb;
  ch_ossmidi.userdata=userdata;
  
  if ((ch_ossmidi.fd=inotify_init())<0) {
    ch_ossmidi_quit();
    return -1;
  }
  
  if (!(ch_ossmidi.root=malloc(pathc+1))) {
    ch_ossmidi_quit();
    return -1;
  }
  memcpy(ch_ossmidi.root,path,pathc);
  ch_ossmidi.root[pathc]=0;
  ch_ossmidi.rootc=pathc;
  
  ch_ossmidi.refresh=1;
  
  if (inotify_add_watch(ch_ossmidi.fd,ch_ossmidi.root,IN_CREATE|IN_ATTRIB)<0) {
    ch_ossmidi_quit();
    return -1;
  }
  
  return 0;
}

/* Add a device if we don't already have it.
 */
 
static int ch_ossmidi_add_device(
  const char *path,int pathc,
  int devid
) {
  struct ch_ossmidi_device *device=ch_ossmidi.devicev;
  int i=ch_ossmidi.devicec;
  for (;i-->0;device++) {
    if (device->pathc!=pathc) continue;
    if (memcmp(device->path,path,pathc)) continue;
    return 0;
  }
  
  if (ch_ossmidi.devicec>=ch_ossmidi.devicea) {
    int na=ch_ossmidi.devicea+8;
    if (na>INT_MAX/sizeof(struct ch_ossmidi_device)) return -1;
    void *nv=realloc(ch_ossmidi.devicev,sizeof(struct ch_ossmidi_device)*na);
    if (!nv) return -1;
    ch_ossmidi.devicev=nv;
    ch_ossmidi.devicea=na;
  }
  
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0; // Don't call it an error, device might not be readable yet or whatever.
  
  device=ch_ossmidi.devicev+ch_ossmidi.devicec++;
  device->fd=fd;
  if (!(device->path=malloc(pathc+1))) {
    close(fd);
    ch_ossmidi.devicec--;
    return -1;
  }
  memcpy(device->path,path,pathc);
  device->path[pathc]=0;
  device->pathc=pathc;
  device->devid=devid;
  
  // Get the device name, best effort, sanitize it, and send the "hello" Sysex.
  char hello[256];
  int namec=ch_ossmidi_get_device_name(hello+1,sizeof(hello)-2,devid);
  if ((namec>0)&&(namec<=sizeof(hello)-2)) {
    char *name=hello+1;
    for (i=namec;i-->0;) {
      if ((name[i]<0x20)||(name[i]>0x7e)) name[i]='?';
    }
  } else {
    namec=0;
  }
  hello[0]=0xf0;
  hello[1+namec]=0xf7;
  if (ch_ossmidi.cb(ch_ossmidi.userdata,devid,hello,namec+2)<0) return -1;
  
  return 0;
}

/* Scan for devices.
 */
 
static int ch_ossmidi_scan_cb(
  const char *path,int pathc,
  const char *base,int basec
) {
  if (basec<5) return 0;
  if (memcmp(base,"midi",4)) return 0;
  //if (basec<7) return 0;
  //if (memcmp(base,"dmmidi",6)) return 0;
  int devid=0;
  int i=4; for (;i<basec;i++) {
    if ((base[i]<'0')||(base[i]>'9')) return 0;
    devid*=10;
    devid+=base[i]-'0';
  }
  if (ch_ossmidi_add_device(path,pathc,devid)<0) return -1;
  return 0;
}
 
static int ch_ossmidi_scan() {
  char subpath[1024];
  if (ch_ossmidi.rootc>=sizeof(subpath)) return -1;
  memcpy(subpath,ch_ossmidi.root,ch_ossmidi.rootc);
  int subpathc=ch_ossmidi.rootc;
  DIR *dir=opendir(ch_ossmidi.root);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    if (de->d_name[0]=='.') continue;
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    if (subpathc>=sizeof(subpath)-basec) continue;
    memcpy(subpath+subpathc,base,basec+1);
    if (ch_ossmidi_scan_cb(subpath,subpathc+basec,base,basec)<0) {
      closedir(dir);
      return -1;
    }
  }
  closedir(dir);
  return 0;
}

/* Add a pollfd to the list.
 */
 
static int ch_ossmidi_pollfd_add(int fd) {
  if (ch_ossmidi.pollfdc>=ch_ossmidi.pollfda) {
    int na=ch_ossmidi.pollfda+4;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(ch_ossmidi.pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    ch_ossmidi.pollfdv=nv;
    ch_ossmidi.pollfda=na;
  }
  struct pollfd *pollfd=((struct pollfd*)ch_ossmidi.pollfdv)+ch_ossmidi.pollfdc++;
  pollfd->fd=fd;
  pollfd->events=POLLIN|POLLERR|POLLHUP;
  pollfd->revents=0;
  return 0;
}

/* Read from inotify.
 */
 
static int ch_ossmidi_update_inotify() {
  char buf[1024];
  int bufc=read(ch_ossmidi.fd,buf,sizeof(buf));
  if (bufc<=0) {
    fprintf(stderr,"%.*s: Failed to read from inotify. Will not detect any new MIDI devices.\n",ch_ossmidi.rootc,ch_ossmidi.root);
    close(ch_ossmidi.fd);
    ch_ossmidi.fd=-1;
    return 0;
  }
  char subpath[1024];
  int subpathc=0;
  int bufp=0;
  while (bufp<=bufc-sizeof(struct inotify_event)) {
    struct inotify_event *event=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc-event->len) break;
    bufp+=event->len;
    
    const char *base=event->name;
    int basec=0;
    while ((basec<event->len)&&base[basec]) basec++;
    if (basec<5) continue;
    if (memcmp(base,"midi",4)) continue;
    int i=4,ok=1,devid=0;
    for (;i<basec;i++) {
      if ((base[i]<'0')||(base[i]>'9')) {
        ok=0;
        break;
      }
      devid*=10;
      devid+=base[i]-'0';
    }
    if (!ok) continue;
    
    if (!subpathc) {
      if (ch_ossmidi.rootc>sizeof(subpath)) return -1;
      memcpy(subpath,ch_ossmidi.root,ch_ossmidi.rootc);
      subpathc=ch_ossmidi.rootc;
    }
    if (subpathc+basec>=sizeof(subpath)) return -1;
    memcpy(subpath+subpathc,base,basec+1);
  
    if (ch_ossmidi_add_device(subpath,subpathc+basec,devid)<0) return -1;
  }
  return 0;
}

/* Drop device by fd.
 */
 
static void ch_ossmidi_drop_device(int fd) {
  int i=ch_ossmidi.devicec;
  while (i-->0) {
    struct ch_ossmidi_device *device=ch_ossmidi.devicev+i;
    if (device->fd==fd) {
      ch_ossmidi_device_cleanup(device);
      ch_ossmidi.devicec--;
      memmove(device,device+1,sizeof(struct ch_ossmidi_device)*(ch_ossmidi.devicec-i));
      return;
    }
  }
}

/* Update device.
 */

static int ch_ossmidi_update_device(
  int fd
) {
  int devid=0;
  struct ch_ossmidi_device *device=ch_ossmidi.devicev;
  int i=ch_ossmidi.devicec;
  for (;i-->0;device++) {
    if (device->fd==fd) {
      devid=device->devid;
      break;
    }
  }
  
  char buf[1024];
  int bufc=read(fd,buf,sizeof(buf));
  if (bufc<=0) {
    ch_ossmidi_drop_device(fd);
    return ch_ossmidi.cb(ch_ossmidi.userdata,devid,0,0);
  }
  
  return ch_ossmidi.cb(ch_ossmidi.userdata,devid,buf,bufc);
}

/* Update.
 */

int ch_ossmidi_update() {
  if (!ch_ossmidi.init) return 0;
  
  if (ch_ossmidi.refresh) {
    ch_ossmidi.refresh=0;
    if (ch_ossmidi_scan()<0) return -1;
  }
  
  ch_ossmidi.pollfdc=0;
  if (ch_ossmidi.fd>=0) {
    if (ch_ossmidi_pollfd_add(ch_ossmidi.fd)<0) return -1;
  }
  int i=ch_ossmidi.devicec;
  struct ch_ossmidi_device *device=ch_ossmidi.devicev+i-1;
  for (;i-->0;device--) {
    if (device->fd<0) {
      ch_ossmidi_device_cleanup(device);
      ch_ossmidi.devicec--;
      memmove(device,device+1,sizeof(struct ch_ossmidi_device)*(ch_ossmidi.devicec-i));
    } else {
      if (ch_ossmidi_pollfd_add(device->fd)<0) return -1;
    }
  }
  if (!ch_ossmidi.pollfdc) return 0;
  
  if (poll(ch_ossmidi.pollfdv,ch_ossmidi.pollfdc,0)<=0) return 0;
  
  struct pollfd *pollfd=ch_ossmidi.pollfdv;
  for (i=ch_ossmidi.pollfdc;i-->0;pollfd++) {
    if (!pollfd->revents) continue;
    if (pollfd->fd==ch_ossmidi.fd) {
      if (ch_ossmidi_update_inotify()<0) return -1;
    } else {
      if (ch_ossmidi_update_device(pollfd->fd)<0) return -1;
    }
  }
  
  return 0;
}
