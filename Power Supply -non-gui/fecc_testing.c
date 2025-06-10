#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>    // for run 14 test log

const unsigned short OP_I2C_START     = 0x400;
const unsigned short OP_I2C_STOP      = 0x200;
const unsigned short OP_I2C_WRITE     = 0x100;
const unsigned short OP_I2C_READ_ACK  = 0x0ff;
const unsigned short OP_I2C_READ_NACK = 0x1ff;

struct linkusb_dev {
  int wr,rd;
};

#define BUFLEN  5000   // be careful it is enough! error checking not really implemented for this, I think
#define DBG_LEVEL 0 //1  //0
//#define IV_LOGFILE  "FPS_test_run14_SAVE.dat"

int linkusb_close(struct linkusb_dev *linkusb);
int linkusb_talk(struct linkusb_dev *linkusb, char *wbuf, int wlen, char *rbuf, int rlen);

int linkusb_open(struct linkusb_dev *linkusb, int unit) {
  char rbuf[BUFLEN];
  struct termios attr;

  // should use unit argument to make the filename; for now just one unit supported
  if ((linkusb->wr=open("/dev/ttyUSB0",O_WRONLY|O_SYNC)) == -1) {
    printf("device write open error: %s\n",strerror(errno));
    return -1;
  }
  else {
    printf("Write open successful\n");
  }

  if ((linkusb->rd=open("/dev/ttyUSB0",O_RDONLY|O_NONBLOCK)) == -1) {
    printf("device read open error: %s\n",strerror(errno));
    close(linkusb->wr);
    return -1;
  }
  else {
    printf("Read open successful\n");
  }

  /*   // this attempt to force a clean start doesn't really work quite right */
  /*   linkusb_talk(linkusb,"\n\n",2,rbuf,-1);   // close byte mode (in case it was left hanging), or else this is just junk command and '?' response */
  /*   printf("DBG: here on line 44\n"); */

  linkusb_talk(linkusb," ",1,rbuf,-1);    // check for connection & required firmware rev 1.6
  if (strcmp(rbuf,"LinkUSB V1.6")) {
    printf("ERROR: LinkUSB ID check failed, got %s\n",rbuf);
    linkusb_close(linkusb);
    return -1;
  }
  else {
    printf("LinkUSB V1.6, Check Succeeded\n");
  }

  // initial operation (if that was initial operation after power-up) is 9600 baud. but we want more... so:
  linkusb_talk(linkusb,"`",1,rbuf,0);   // command LinkUSB to 38400 baud
  usleep(100000);
  tcgetattr(linkusb->wr, &attr);
  cfsetospeed(&attr, B38400);
  tcsetattr(linkusb->wr, TCSAFLUSH, &attr);
  tcgetattr(linkusb->rd, &attr);
  cfsetispeed(&attr, B38400);
  tcsetattr(linkusb->rd, TCSAFLUSH, &attr);
  usleep(100000);
  linkusb_talk(linkusb," ",1,rbuf,-1);    // check for connection & required firmware rev 1.6
  if (strcmp(rbuf,"LinkUSB V1.6")) {
    printf("ERROR: LinkUSB ID check failed (after baud rate switch), got %s\n",rbuf);
    linkusb_close(linkusb);
    return -1;
  }
  else {
    printf("LinkUSB V1.6, Baud Rate Switch to 38400 Success\n");
  }
  // prepare for use with our 1-wire hardware
  linkusb_talk(linkusb,"\"r",2,rbuf,-1);  // set "iso" mode and issue the dummy 1-wire reset that seems necessary
  linkusb_talk(linkusb,"r",1,rbuf,-1);    // another 1-wire reset & expect a good response now
  if (strcmp(rbuf,"P"))
    printf("WARNING: On linkusb_open, got unexpected 1-wire reset response: %s\n",rbuf);

  return 0;
}

int linkusb_close(struct linkusb_dev *linkusb) {
  close(linkusb->wr);
  close(linkusb->rd);
  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// write to LinkUSB device and possibly read response
// call with rlen > 0 to expect some fixed length response
//           rlen < 0 to expect a variable-length, newline-terminated response
//                      (newline will be stripped here, any data after it may be lost)
//           rlen = 0 to expect no response
// the response will be put in rbuf, null-terminated
// for fixed length rbuf must be >rlen+1 chars, for variable length BUFLEN chars
///////////////////////////////////////////////////////////////////////////////////////////
int linkusb_talk(struct linkusb_dev *linkusb, char *wbuf, int wlen, char *rbuf, int rlen) {
  int n,j=0,k,timeout=0;
  char *end;

  write(linkusb->wr,wbuf,wlen);  // add error check of course
  if (rlen==0)
    return 0;
  n  = (rlen>0 ? rlen : BUFLEN-1);
  while (1) {
    printf("iter %d\n",timeout);
    // do something better here?  (select, poll, or epoll functions may be used?)
    // https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
    usleep(30000); // this delay is experimentally tuned to be about right to get ~20 chars in one iteration
    if (timeout++==66) {  // 33 should be enough for AD5694 writes. maybe reads.
      printf("read timeout\n");
      break;
    }
    if ((k=read(linkusb->rd,rbuf+j,n-j)) > 0) {
      j += k;
      printf("got %d, total %d\n",k,j);
      int m;
      for(m=0;m<k;m++)
    	printf("  at %d %d %c\n",m,rbuf[j-k+m],rbuf[j-k+m]);
    	if ((rlen<0)&&((end=strstr(rbuf,"\r\n"))!=NULL)) { // got newline on a variable read
        // Josh note: originally was "\r\n", but that forced timeout - check with Gerard
    	  //printf("got newline, at %d\n",end-rbuf);
    	  *end = 0;
    	  return 0;
    	}
    	else if ((rlen>0)&&(j==n)) {
    	  //printf("got exact\n");
    	  rbuf[j] = 0;
    	  return 0;
    	}
    }
    else if ((k<0)&&(errno!=EAGAIN)) {
      printf("read error: %s\n",strerror(errno));
      break;
    }
  }
  rbuf[0]=0;
  return -1;
}

int main(int argc, char *argv[])
{
  uint64_t owaddr = 0x3800000007EA673AULL;  // DS2413 adapter #1 for new FEE testing
  struct linkusb_dev linkusb;
  int k;
  uint64_t serial;
  int swaddr = 0;  // address on the switch on this FEE (or can loop over a set of them)

  if (linkusb_open(&linkusb,0))
    return -1;  // error messages in linkusb_open
}
