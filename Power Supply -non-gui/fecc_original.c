// test/development controls program for STAR EPD FEE
//
// G. Visser, Indiana University, 12/2014

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
  if ((linkusb->rd=open("/dev/ttyUSB0",O_RDONLY|O_NONBLOCK)) == -1) {
    printf("device read open error: %s\n",strerror(errno));
    close(linkusb->wr);
    return -1;
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
	  printf("got newline, at %ld\n",end-rbuf);
	  *end = 0;
	  return 0;
	}
	else if ((rlen>0)&&(j==n)) {
	  printf("got exact\n");
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

int DS2413_post(char *wbuf, int data) {

  data = 0xfc | (data & 0x03);           // upper bits to 1 (see DS2413 datasheet)
  return sprintf(wbuf,"%02X%02XFFFF",    // send the byte & its complement, and 2 read frames (again see datasheet on this)
		 data,(~data)&0xff);
}

///////////////////////////////////////////////////////////////////////////////////////////
// write/read I2C device through DS2413 through LinkUSB
// i2cwbuf/i2crbuf format/usage similar to future FPS control FPGA implementation of
//   I2C through DS2413 through FPGA 1-wire master
///////////////////////////////////////////////////////////////////////////////////////////
int DS2413_talk_I2C(struct linkusb_dev *linkusb, uint64_t owaddr,
		    unsigned int *i2cwbuf, int length, unsigned int *i2crbuf, int do_timeout_prevention_hack) {
  int i,j,k,tmp;
  char wbuf[BUFLEN],*pwbuf=wbuf,rbuf[BUFLEN],*prbuf=rbuf;

#if (DBG_LEVEL>=2)
  printf("i2cwbuf");
  for(j=0;j<length;j++)
    printf("[%d]: %04x  ",j,i2cwbuf[j]);
  printf("\n");
#endif

  linkusb_talk(linkusb,"r",1,rbuf,-1);  // 1-wire reset, expect "P" response
  //printf("reset: got %d chars:\n%s\n",strlen(rbuf),rbuf);
  if (strcmp(rbuf,"P")) {
    printf("ERROR: 1-wire reset response: %s\n",rbuf);
    return -1;
  }

  pwbuf+=sprintf(pwbuf,"b55");                      // set LinkUSB byte mode and send match rom command
  for(k=0;k<8;k++)                              // send the address
    pwbuf+=sprintf(pwbuf,"%02X",(uint32_t) (owaddr>>8*k)&0xff);  // Note: UPPERCASE hexadecimal REQUIRED for all linkusb input!!
  pwbuf+=sprintf(pwbuf,"5A");                        // pio write command

  for(i=0;i<length;i++) {
    if (i2cwbuf[i]==OP_I2C_START) {
      // FORCE an extra stop condition here, this is needed only because program might have been killed while talking, do not do this in final FPS HW configuration
      pwbuf+=DS2413_post(pwbuf,0x1);     // take SCL high to prepare for stop condition
      pwbuf+=DS2413_post(pwbuf,0x3);     // stop (SDA goes high while SCL high)
      // start condition
      pwbuf+=DS2413_post(pwbuf,0x1);     // start condition (SDA goes low while SCL (still, from before) high)
      pwbuf+=DS2413_post(pwbuf,0x0);     // SDA and SCL low, prepare for data phase
    }
    else if (i2cwbuf[i]==OP_I2C_STOP) {
      pwbuf+=DS2413_post(pwbuf,0x1);     // take SCL high to prepare for stop condition
      pwbuf+=DS2413_post(pwbuf,0x3);     // stop (SDA goes high while SCL high)
    }
    else { // data read/write
      for(j=7;j>=0;j--) {
	pwbuf += DS2413_post(pwbuf,tmp = ((i2cwbuf[i]>>j)&0x1)<<1);   // set SDA to the data bit j
	pwbuf += DS2413_post(pwbuf, tmp|0x1);                         // SCL high, hold SDA
	//  11/13/17 THIS was what fixed the DS28CM00 response -- must put SDA high to avoid timeout! harmless (?hopefully?) to do this to other chips
	// BUT WAIT - actually we can only change SDA when SCL is low (with some hold time)
	// I should rewrite this to use an extra cycle; complicated and I am lazy for now, so no. But I put the hack as an option
	// and will call it with the hack enabled only when really necessary.
	// Later come back and rewrite the hack properly with an extra cycle (for hack call only).
	if (do_timeout_prevention_hack)
	  pwbuf += DS2413_post(pwbuf, 0x2);                             // SCL low, SDA high (regardless of the bit just sent - to avoid SDA timeout)
	else   // generally, we don't do that ~unsafe hack; only use it for DS28CM00 inital write to turn off the timeout
	  pwbuf += DS2413_post(pwbuf, tmp);                             // SCL low, hold SDA
      }
      pwbuf += DS2413_post(pwbuf,tmp = ((i2cwbuf[i]>>8)&0x1)<<1);   // set SDA to the ack bit write flag
      pwbuf += DS2413_post(pwbuf, tmp|0x1);                         // SCL high, hold SDA
      pwbuf += DS2413_post(pwbuf, tmp);                             // SCL low, hold SDA
    }
  }
  pwbuf += sprintf(pwbuf,"\n");                        // close LinkUSB byte mode

#if (DBG_LEVEL>=3)
  printf("Will send (%d chars):\n%s",strlen(wbuf),wbuf);
#endif
  linkusb_talk(linkusb,wbuf,strlen(wbuf),rbuf,-1);
#if (DBG_LEVEL>=3)
  printf("bytemode: got %d chars:\n%s\n",strlen(rbuf),rbuf);
#endif

  prbuf+=20;   // skip to after the write mode command (0x5A)
  int m=0;
  for(i=0;i<length;i++) {
    if (i2cwbuf[i]==OP_I2C_START) {
      prbuf+=32;                         // skip 4 [why 4? SEE ABOVE!]  (we don't check start cond, for now anyway)
    }
    else if (i2cwbuf[i]==OP_I2C_STOP) {
      prbuf+=16;                         // skip 2 (we don't check stop cond, for now anyway)
    }
    else { // data read/write
      i2crbuf[m]=0;
      for(j=7;j>=0;j--) {
	prbuf+=8;
	prbuf+=7;
	i2crbuf[m] |= ( (*prbuf&0x4)!=0 ? 1<<j : 0 );
	prbuf+=1;
	prbuf+=8;
      }
      prbuf+=8;
      prbuf+=7;
      i2crbuf[m] |= ( (*prbuf&0x4)!=0 ? 1<<8 : 0 );
      prbuf+=1;
      prbuf+=8;
      m++;
    }
  }
#if (DBG_LEVEL>=3)
  printf("used to %d\n",prbuf-rbuf);
#endif
#if (DBG_LEVEL>=2)
  printf("i2crbuf");
  for(j=0;j<length-2;j++)
    printf("[%d]: %04x  ",j,i2crbuf[j]);
  printf("\n");
#endif
  return 0;
}

int LTC2615_write_dac(struct linkusb_dev *linkusb, uint64_t owaddr, int i2caddr, int chan, int data) {
  unsigned int i2cwbuf[12],i2crbuf[12];    // this is _just_ enough - be careful if changing code below!
  // BUT IF INCREASING CHECK BUFLEN VALUE!!!!
  int k=0,j;

  if ((chan<0)||(chan>7)) {
    fprintf(stderr,"invalid channel number\n");
    return 1;
  }
  if (DBG_LEVEL>=1)
    printf("writing 0x%04x to LTC2615 channel %d at I2C address 0x%02x ..\n",data,chan,i2caddr);
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | 0x30 | chan;  // LTC2615 command byte (write and update one channel)
  i2cwbuf[k++] = OP_I2C_WRITE | ((data>>6)&0xff);     // upper 8 data bits
  i2cwbuf[k++] = OP_I2C_WRITE | ((data<<2)&0xfc);     // lower 6 data bits
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition

  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      fprintf(stderr,"NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }
  return 0;
}

// this is same exactly as LTC2615 (above) except there are only channels 0-3
int LTC2619_write_dac(struct linkusb_dev *linkusb, uint64_t owaddr, int i2caddr, int chan, int data) {
  unsigned int i2cwbuf[12],i2crbuf[12];    // this is _just_ enough - be careful if changing code below!
  // BUT IF INCREASING CHECK BUFLEN VALUE!!!!
  int k=0,j;

  if ((chan<0)||(chan>3)) {
    fprintf(stderr,"invalid channel number\n");
    return 1;
  }
  if (DBG_LEVEL>=1)
    printf("writing 0x%04x to LTC2615 channel %d at I2C address 0x%02x ..\n",data,chan,i2caddr);
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | 0x30 | chan;  // LTC2615 command byte (write and update one channel)
  i2cwbuf[k++] = OP_I2C_WRITE | ((data>>6)&0xff);     // upper 8 data bits
  i2cwbuf[k++] = OP_I2C_WRITE | ((data<<2)&0xfc);     // lower 6 data bits
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition

  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      fprintf(stderr,"NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }
  return 0;
}

int LTC2637dash8_write_dac(struct linkusb_dev *linkusb, uint64_t owaddr, int i2caddr, int chan, int data) {
  unsigned int i2cwbuf[12],i2crbuf[12];    // this is _just_ enough - be careful if changing code below!
  // BUT IF INCREASING CHECK BUFLEN VALUE!!!!
  int k=0,j;

  if ((chan<0)||(chan>7)) {
    fprintf(stderr,"invalid channel number\n");
    return 1;
  }
  if (DBG_LEVEL>=1)
    printf("writing 0x%02x to LTC2637-8 channel %d at I2C address 0x%02x ..\n",data,chan,i2caddr);

  // first do the external reference select (really only needed first time, but to be sure and for simplicity do it always)
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | 0x7f;  // LTC2637 command byte (select external reference)
  i2cwbuf[k++] = OP_I2C_WRITE | 0;     // dummy byte
  i2cwbuf[k++] = OP_I2C_WRITE | 0;     // dummy byte
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition
  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      fprintf(stderr,"NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }

  k=0;
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | 0x30 | chan;  // LTC2637 command byte (write and update one channel)
  i2cwbuf[k++] = OP_I2C_WRITE | (data&0xff);     // 8 data bits
  i2cwbuf[k++] = OP_I2C_WRITE | 0;     // dummy byte (more data in -10 or -12 parts)
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition

  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      fprintf(stderr,"NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }
  return 0;
}

int LTC2631dash12_write_dac(struct linkusb_dev *linkusb, uint64_t owaddr, int i2caddr, int data) {
  unsigned int i2cwbuf[12],i2crbuf[12];    // this is _just_ enough - be careful if changing code below!
  // BUT IF INCREASING CHECK BUFLEN VALUE!!!!
  int k=0,j;

  if (DBG_LEVEL>=1)
    printf("writing 0x%02x to LTC2631-12 at I2C address 0x%02x ..\n",data,i2caddr);

  // first do the external reference select (really only needed first time, but to be sure and for simplicity do it always)
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | 0x7f;  // LTC2631 command byte (select external reference)
  i2cwbuf[k++] = OP_I2C_WRITE | 0;     // dummy byte
  i2cwbuf[k++] = OP_I2C_WRITE | 0;     // dummy byte
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition
  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      fprintf(stderr,"NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }

  k=0;
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | 0x30;  // LTC2631 command byte (write and update)
  i2cwbuf[k++] = OP_I2C_WRITE | ((data>>4)&0xff);     // upper 8 data bits
  i2cwbuf[k++] = OP_I2C_WRITE | ((data<<4)&0xf0);     // lower 4 data bits
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition

  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      fprintf(stderr,"NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }
  return 0;
}

int PCF8574A_write(struct linkusb_dev *linkusb, uint64_t owaddr, int i2caddr, int data) {
  unsigned int i2cwbuf[12],i2crbuf[12];    // this is _just_ enough - be careful if changing code below!
  // BUT IF INCREASING CHECK BUFLEN VALUE!!!!
  int k=0,j;

  if (DBG_LEVEL>=1)
    printf("writing 0x%02x to PCF8574A at I2C address 0x%02x ..\n",data,i2caddr);
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | (data&0xff);  // immediate output data
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition

  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      fprintf(stderr,"NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }
  return 0;
}

int LTC2301_convert_read(struct linkusb_dev *linkusb, uint64_t owaddr, int i2caddr, unsigned int *adc) {
  unsigned int i2cwbuf[12],i2crbuf[12];    // this is _just_ enough - be careful if changing code below!
  // BUT IF INCREASING CHECK BUFLEN VALUE!!!!
  int k=0,j;

  if (DBG_LEVEL>=1)
    printf("convert and read from LTC2301 at I2C address 0x%02x ..\n",i2caddr);
  // do a conversion
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | 0x08;  // LTC2301 control byte
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition

  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      printf("NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }
  // a small delay could in principle be needed here, but in practice probably not

  // get the data
  k=0;
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1) | 0x01;  // I2C write flag bit is 1
  i2cwbuf[k++] = OP_I2C_READ_ACK;
  i2cwbuf[k++] = OP_I2C_READ_NACK;     // nack the 2nd read byte, is normal termination
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition

  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  if (i2crbuf[0]&0x100)
    printf("NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  *adc = ((i2crbuf[1]&0xff)<<4)|((i2crbuf[2]&0xf0)>>4);
  if (DBG_LEVEL>=1)
    printf("   LTC2301 reads 0x%04x.\n",*adc);
  return 0;
}

int PCA9541A_write(struct linkusb_dev *linkusb, uint64_t owaddr, int i2caddr, int cc, int data) {
  unsigned int i2cwbuf[12],i2crbuf[12];
  int k=0,j;

  if (DBG_LEVEL>=1)
    printf("writing to PCA9541A at I2C address 0x%02x ..\n",i2caddr);
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | (cc&0xff);  // command code (i.e. internal register address)
  i2cwbuf[k++] = OP_I2C_WRITE | (data&0xff);  // data to write to register
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition

  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      fprintf(stderr,"NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }
  return 0;
}

int DS28CM00_read(struct linkusb_dev *linkusb, uint64_t owaddr, uint64_t *serial) {
  unsigned int i2cwbuf[12],i2crbuf[12];    // this is _just_ enough - be careful if changing code below!
  // BUT IF INCREASING CHECK BUFLEN VALUE!!!!
  int k=0,j;
  int i2caddr=0x50;  // there are no other options for DS28CM00, so no point to an argument for this
  uint64_t y=0;

  // the first thing to do is make a write to control register to turn off the timeout
  // this only has to be done once, but harmless if repeated
  // we do this because reads still suffer from the timeout problem on SDA, although writes work ok (with SDA high trick)
  if (DBG_LEVEL>=1)
    printf("write DS28CM00 at I2C address 0x%02x ..\n",i2caddr);
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | 0x08;  // select control register
  i2cwbuf[k++] = OP_I2C_WRITE | 0x00;  // write 0x00
  i2cwbuf[k++] = OP_I2C_STOP;          // stop condition
  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,1);  // use the timeout prevention hack here only, at no other time
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      printf("NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }

  // and now, read 8 bytes of serial number (raw; not handling CRC and including 0x70 code as just more serial number)
  k=0;
  if (DBG_LEVEL>=1)
    printf("read from DS28CM00 at I2C address 0x%02x ..\n",i2caddr);
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = OP_I2C_WRITE | 0x00;  // internal address for read to follow
  i2cwbuf[k++] = OP_I2C_STOP;          // stop condition
  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      printf("NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }
  // get the data
  k=0;
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = OP_I2C_WRITE | ((i2caddr&0x7f)<<1) | 0x01;  // I2C write flag bit is 1
  for(j=0;j<7;j++)
    i2cwbuf[k++] = OP_I2C_READ_ACK;      // byte 0 - 6 to read w/ ack
  i2cwbuf[k++] = OP_I2C_READ_NACK;     // nack the last read byte (CRC), is normal termination
  i2cwbuf[k++] = OP_I2C_STOP;          // stop condition
  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  if (i2crbuf[0]&0x100)
    printf("NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);

  for(j=0;j<8;j++)
    y |= ((uint64_t) (i2crbuf[j+1] & 0xff)) << 8*j;
  *serial = y;
  // ought to check the CRC... but this is not implemented yet...
  if (DBG_LEVEL>=1)
    printf("   DS28CM00 reads 0x%016lx.\n",*serial);
  return 0;
}

//-----------------------------------------------------------------------------------------------------//
//-----------------------------------------------------------------------------------------------------//
// 4/14/2018 Adapting from EPD to FEC (forward emcal, test boards of run 17/18); keeping the same structure from EPD
struct dac_map_element {
  int i2c;  // i2c address of the proper LTC2619 chip on FEC FEE
  int chan; // dac channel
};
const struct dac_map_element map_setdac[4] = {
  {0x22, 0},{0x22,3},{0x22,1},{0x22,2} };
const int adc_select_rmon[4] = {
  0x37, 0xcf, 0xf7, 0xef };
const int adc_select_imon[4] = {
  0xb7, 0x8f, 0x97, 0xaf };
const int adc_select_vcomp = 0xd7;
int connected_fee;

int FEC_connect_FEE(struct linkusb_dev *linkusb, uint64_t owaddr, int ifee) {
  connected_fee=(ifee&0x0f);
  return PCA9541A_write(linkusb, owaddr, 0x70 | (ifee&0x0f), 0x01, 0x04);
  // perhaps it would be best to disconnect the other 15, to ensure there is no unwanted connections
  // "wastes" time but if we don't hop around between FEE more than we have to, it may be ok
}

int FEC_disconnect_FEE(struct linkusb_dev *linkusb, uint64_t owaddr) {   // we disconnect the one stored in connected_fee variable
  return PCA9541A_write(linkusb, owaddr, 0x70 | (connected_fee&0x0f), 0x01, 0x00);
}

int FEC_set_voltage(struct linkusb_dev *linkusb, uint64_t owaddr, int ichan, double voltage) {
  if ((ichan<0)||(ichan>15)) {
    fprintf(stderr,"invalid channel number\n");
    return 1;
  }
  int data = ((int) (voltage*(126./2200.)/4.096*16384))&0x3fff;
  printf("setting bias to %.3lf V (+compensation voltage) on channel %d ..\n",(((double) data)/16384)*4.096*2200./126.,ichan);
  return LTC2619_write_dac(linkusb,owaddr,map_setdac[ichan].i2c,map_setdac[ichan].chan,data);
}

int FEC_set_vslope(struct linkusb_dev *linkusb, uint64_t owaddr, double vslope) {
  int data = ((int) (4096*vslope/0.066))&0xfff;
  printf("setting bias compensation slope to %.2lf mV / deg C ..\n",(((double) data)/4096)*0.066*1000);
  return LTC2631dash12_write_dac(linkusb,owaddr,0x10,data);
}

double FEC_get_rmon(struct linkusb_dev *linkusb, uint64_t owaddr, int ichan) {
  unsigned int adc;
  if ((ichan<0)||(ichan>15)) {
    fprintf(stderr,"invalid channel number\n");
    return -999.;
  }
  if (PCF8574A_write(linkusb,owaddr,0x38,adc_select_rmon[ichan]))    // select this channel to ADC
    return -999.;
  if (LTC2301_convert_read(linkusb,owaddr,0x08,&adc))
    return -999.;
  return 4.096*((double) adc)/4096;
}

double FEC_get_imon(struct linkusb_dev *linkusb, uint64_t owaddr, int ichan) {
  unsigned int adc;
  if ((ichan<0)||(ichan>15)) {
    fprintf(stderr,"invalid channel number\n");
    return -999.;
  }
  if (PCF8574A_write(linkusb,owaddr,0x38,adc_select_imon[ichan]))    // select this channel to ADC
    return -999.;
  if (LTC2301_convert_read(linkusb,owaddr,0x08,&adc))
    return -999.;
  return 409.6*((double) adc)/4096;
}

double FEC_get_vcomp(struct linkusb_dev *linkusb, uint64_t owaddr) {
  unsigned int adc;
  if (PCF8574A_write(linkusb,owaddr,0x38,adc_select_vcomp))    // select this channel to ADC
    return -999.;
  if (LTC2301_convert_read(linkusb,owaddr,0x08,&adc))
    return -999.;
  return (4.096*((double) adc)/4096)*2.2;   // bias voltage regulator gain factor 2.2
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

  FEC_connect_FEE(&linkusb,owaddr,swaddr);     // connect to FEE at switch address 0
  DS28CM00_read(&linkusb, owaddr, &serial);
  printf("FEE %d serial number: 0x%016lx\n",swaddr,serial);

  //quick first tests
  for(k=0;k<4;k++)
    FEC_set_voltage(&linkusb,owaddr,k,58.0);//hacked for offset test 7/2 put it back!!  56.5);//57);
  FEC_set_vslope(&linkusb,owaddr,0.054);//0.054);

  //for(;;)
  for(k=0;k<4;k++) {
    printf("channel %2d: regulator control voltage = %5.3lf V, current monitor = %6.3lf uA\n",
	   k,FEC_get_rmon(&linkusb,owaddr,k),FEC_get_imon(&linkusb,owaddr,k));
  }
  printf("bias compensation voltage (applies to all channels) = %.3lf V\n",FEC_get_vcomp(&linkusb,owaddr));

  FEC_disconnect_FEE(&linkusb,owaddr);
  linkusb_close(&linkusb);
  return 0;
}
