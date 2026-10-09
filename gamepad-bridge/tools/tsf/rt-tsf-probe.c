// Read the RT2800 TSF registers over usbfs (vendor request USB_MULTI_READ=7 at TSF_TIMER_DW0 0x111c, 8 bytes = DW0+DW1;
// kernel rt2x00usb.h:54, rt2800.h:979-985, rt2800lib.c rt2800_get_tsf) and compare with the patched mac80211 sysfs tsf.
#include <errno.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
static uint64_t now_us(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000000ull+t.tv_nsec/1000;}
static int usb_tsf(int fd, uint64_t* out){
  uint8_t b[8]; struct usbdevfs_ctrltransfer c={.bRequestType=0xC0,.bRequest=7,.wValue=0,.wIndex=0x111c,.wLength=8,.timeout=100,.data=b};
  errno=0; int r=ioctl(fd,USBDEVFS_CONTROL,&c); if(r!=8){fprintf(stderr,"ioctl r=%d errno=%s\n",r,strerror(errno)); return -1;}
  *out=(uint64_t)(b[0]|b[1]<<8|b[2]<<16|(uint32_t)b[3]<<24) | (uint64_t)(b[4]|b[5]<<8|b[6]<<16|(uint32_t)b[7]<<24)<<32; return 0;}
int main(int argc,char**argv){
  const char* dev=argc>1?argv[1]:"/dev/bus/usb/001/005"; const char* sys=argc>2?argv[2]:"/sys/class/net/wlp0s20f0u2/tsf";
  int fd=open(dev,O_RDWR); if(fd<0){perror("open usb");return 1;}
  int sf=open(sys,O_RDONLY);
  for(int i=0;i<10;i++){
    uint64_t s0=0,s1=0,u=0; uint64_t t0=now_us();
    if(sf>=0) pread(sf,&s0,8,0);
    uint64_t t1=now_us(); int r=usb_tsf(fd,&u); uint64_t t2=now_us();
    if(sf>=0) pread(sf,&s1,8,0); uint64_t t3=now_us();
    if(r){perror("usb control");return 1;}
    printf("sysfs %llu  usb %llu  sysfs %llu | usb-sysfs0 %+lld us, usb read took %llu us\n",(unsigned long long)s0,(unsigned long long)u,(unsigned long long)s1,(long long)(u-s0),(unsigned long long)(t2-t1));
    usleep(200000);}
}
