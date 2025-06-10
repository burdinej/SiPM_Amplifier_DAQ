// test/development controls program for STAR EPD FEE
// Now controls for UCNtau SiPM Power Supply (June 4 2019)
// G. Visser, Indiana University, 12/2014
// J. Burdine, Indiana University, 06/2019

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>    // for run 14 test log

const unsigned short OP_I2C_START     = 0x400;
const unsigned short OP_I2C_STOP      = 0x200;
const unsigned short OP_I2C_WRITE     = 0x100;
const unsigned short OP_I2C_READ_ACK  = 0x0ff;
const unsigned short OP_I2C_READ_NACK = 0x1ff;
const unsigned int PCA_ADDR         = 0x41;
const unsigned int LTC2615_ADDR     = 0x10;
const unsigned int LTC2451_ADDR     = 0x14;

struct linkusb_dev {
  int wr,rd;
};

#define BUFLEN  5000   // be careful it is enough! error checking not really implemented for this, I think
#define DBG_LEVEL 3 //1  //0
//#define IV_LOGFILE  "FPS_test_run14_SAVE.dat"

int linkusb_close(struct linkusb_dev *linkusb);
int linkusb_talk(struct linkusb_dev *linkusb, char *wbuf, int wlen, char *rbuf, int rlen);

int linkusb_open(struct linkusb_dev *linkusb, int unit) {
  char rbuf[BUFLEN];
  struct termios attr;

  // should use unit argument to make the filename; for now just one unit supported
  if ((linkusb->wr=open("/dev/ttyLinkUSB",O_WRONLY|O_SYNC)) == -1) {
    printf("device write open error: %s\n",strerror(errno));
    return -1;
  }
  else {
    printf("Write open successful\n");
  }

  if ((linkusb->rd=open("/dev/ttyLinkUSB",O_RDONLY|O_NONBLOCK)) == -1) {
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
    if (timeout++ == 66) {  // 33 should be enough for AD5694 writes. maybe reads.
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
  printf("Will send (%ld chars):\n%s",strlen(wbuf),wbuf);
#endif
  linkusb_talk(linkusb,wbuf,strlen(wbuf),rbuf,-1);
#if (DBG_LEVEL>=3)
  printf("bytemode: got %ld chars:\n%s\n",strlen(rbuf),rbuf);
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
  printf("used to %ld\n",prbuf-rbuf);
#endif
#if (DBG_LEVEL>=2)
  printf("i2crbuf");
  for(j=0;j<length-2;j++)
    printf("[%d]: %04x  ",j,i2crbuf[j]);
  printf("\n");
#endif
  return 0;
}

// Write command to LTC2615, used to set voltage on all channels
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

// Writes to PCA9536 to control multiplexor
int PCA9536_write(struct linkusb_dev *linkusb, uint64_t owaddr, int i2caddr, int cc, int data) {
  unsigned int i2cwbuf[12],i2crbuf[12];
  int k=0,j;

  if (DBG_LEVEL>=1)
    printf("writing to PCA9536 at I2C address 0x%02x ..\n",i2caddr);
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = ((i2caddr&0x7f)<<1);  // I2C write flag bit is 0
  i2cwbuf[k++] = (cc&0xff);  // command code (i.e. internal register address)
  i2cwbuf[k++] = (data&0xff);  // data to write to register
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition

  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      fprintf(stderr,"NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }
  return 0;
}

// Reads from LTC2451 to communicate monitor values
unsigned int *LTC2451_read(struct linkusb_dev *linkusb, uint64_t owaddr, int i2caddr) {
  unsigned int *i2cwbuf = malloc(12);
  unsigned int *i2crbuf = malloc(12);
  int k=0,j;

  if (!i2cwbuf) {
      printf("\nProblem allocating memory, please abort.\n");
      return NULL;
  }

  if (!i2crbuf) {
      printf("\nProblem allocating memory, please abort.\n");
      return NULL;
  }

  if (DBG_LEVEL>=1)
    printf("writing to LTC2451 at I2C address 0x%02x ..\n",i2caddr);
  i2cwbuf[k++] = OP_I2C_START;         // start condition
  i2cwbuf[k++] = ((i2caddr&0x7f)<<1) | 0x01; // LTC2451 I2C address w/ read bit (1)
  i2cwbuf[k++] = OP_I2C_STOP;         // stop condition

  DS2413_talk_I2C(linkusb,owaddr,i2cwbuf,k,i2crbuf,0);
  for(j=0;j<k-2;j++) {
    if (i2crbuf[j]&0x100)
      fprintf(stderr,"NAK on I2C write (i2crbuf[%d]: %04x)\n",j,i2crbuf[j]);
  }
  return i2crbuf; // will later return the readbuffer
}

// Calls PCA write to control the 4 i/o pins and set the multiplexor output
// See 74HC4067 datasheet to see table of High and Low states corresponding
// to desired switch state
int multiplexor_control(struct linkusb_dev *linkusb, uint64_t owaddr, int chan, int i_or_ref) {
    // S0==P0, S1==P1, S2==P2, S3==P3
    int data;
    int cc = 0x01; // Sets internal register as output port register
    // Nested switch to set switch states
    switch (i_or_ref) {
      case 1: // User chose current monitoring
        switch (chan) { // Switch to determine chan choice, 8 options
          case 0:
            data = 0b11110000; //F0
            break;
          case 1:
            data = 0b11110001; //F1
            break;
          case 2:
            data = 0b11110010; //F2
            break;
          case 3:
            data = 0b11110011; //F3
            break;
          case 4:
            data = 0b11110100; //F4
            break;
          case 5:
            data = 0b11110101; //F5
            break;
          case 6:
            data = 0b11110110; //F6
            break;
          default:
            data = 0b11110111; //F7
        }
        break;
      default: // User chose "0", reference monitoring
        switch (chan) { // Switch to determine chan choice, 8 options
          case 0:
            data = 0b11111000; //F8
            break;
          case 1:
            data = 0b11111001; //F9
            break;
          case 2:
            data = 0b11111010; //FA
            break;
          case 3:
            data = 0b11111011; //FB
            break;
          case 4:
            data = 0b11111100; //FC
            break;
          case 5:
            data = 0b11111101; //FD
            break;
          case 6:
            data = 0b11111110; //FE
            break;
          default:
            data = 0b11111111; //FF
        }
    }

    PCA9536_write(linkusb, owaddr, PCA_ADDR, 0x03, 0b1110000);
    PCA9536_write(linkusb, owaddr, PCA_ADDR, cc, data);
    return 0;
}

// Takes user input to determine what channel monitoring to display
void monitor_channel(struct linkusb_dev *linkusb, uint64_t owaddr) {
  int chan;
  int i_or_ref;
  char *cur_or_ref;
  unsigned int *readBuff;
  int i = 0;

  printf("\nWhich channel would you like to monitor (0-7)?: ");
  scanf("%d", &chan);
  printf("\nWould you like to monitor channel %d's current or reference voltage?\n", chan);
  printf("Type 1 for current or 0 for reference voltage: ");
  scanf("%d", &i_or_ref);

  if (i_or_ref == 1) {
    cur_or_ref = "current";
  }
  else {
    cur_or_ref = "voltage";
  }

  printf("\nYou have selected to monitor channel %d's %s!\n\n", chan, cur_or_ref);

  multiplexor_control(linkusb, owaddr, chan, i_or_ref);
  readBuff = LTC2451_read(linkusb, owaddr, LTC2451_ADDR);

  if (readBuff) {
    for ( i = 0; i < 12; i++ ) {
      printf( "Arr[%d] : %04X\n", i, *(readBuff + i));
    }
    free(readBuff);
  }
  else {
    free(readBuff);
  }

  return ;
}

// Takes user input to set voltages on all channels
void set_voltage(struct linkusb_dev *linkusb, uint64_t owaddr) {
  int chan;
  double voltage;
  int bVolts;
  int allOrOne;
  int i;

  printf("\nDo you want to change a single channel's voltage, or all channels?\n");
  printf("Enter 1 for all, or 0 for only one channel: ");
  scanf("%d", &allOrOne);

  switch (allOrOne) {
    case 1 :

      for (i = 0; i < 8; i++) {
        chan = i;

        do { // Input validation for voltage
          printf("\nWhat is the desired voltage for channel %d? (0 to -32V Unsigned value): ", chan);
          scanf("%lf", &voltage);
          if ((voltage < 0) | (voltage > 32)) {
            printf("Error: Invalid voltage value.\n");
            printf("Please only enter values between 0 and -32, unsigned!\n");
          }
        } while ((voltage < 0) | (voltage > 32));

        bVolts = voltage / 32.0 * 16383;
        LTC2615_write_dac(linkusb, owaddr, LTC2615_ADDR, chan, bVolts);
        printf("\nYou have set channel %d to -%f volts!\n\n", chan, voltage);
      }
      break;

    case 0 :

      do { // Input validation for channel selection
        printf("What channel do you want to set the voltage for (0-7)?: ");
        scanf("%d", &chan);
        if ((chan < 0) | (chan > 7)) {
          printf("\nError: Invalid Channel\n");
          printf("Please select a channel between 0 and 7, inclusive!\n\n");
        }
      } while ((chan < 0) | (chan > 7));
      printf("You selected channel: %d\n", chan);

      do { // Input validation for voltage
        printf("\nWhat is the desired voltage for channel %d? (0 to -32V Unsigned value): ", chan);
        scanf("%lf", &voltage);
        if ((voltage < 0) | (voltage > 32)) {
          printf("\nError: Invalid voltage value.\n");
          printf("Please only enter values between 0 and -32, unsigned!\n");
        }
      } while ((voltage < 0) | (voltage > 32));
      printf("\nYou have set channel %d to -%f volts!\n\n", chan, voltage);

      bVolts = voltage / 32.0 * 16383; // Scaled from 4.096 to 32. Since 32 is reference in practice
      LTC2615_write_dac(linkusb, owaddr, LTC2615_ADDR, chan, bVolts);
      break;

    default :
      printf("Did you type a number besides 1 or 0?? Please don't!\n");
  }

  return ;
}

// Allows user to select board they want to interact with,
// then automatically loads the correct owaddr.
uint64_t choose_board() {
  int board = 0;
  uint64_t owaddr = 0;

  printf("\nWhich board would you like to interact with (Board 1,2,3 or 4)?\n");
  printf("Note each board has its own set of channels 0-7.\n");
  printf("\nBoard: ");

  scanf("%d", &board);

  switch (board) {
    case 1:
      owaddr = 0x96000000455A523AULL;
      printf("\nYou have chosen board %d!\n", board);
      break;
    case 2:
      owaddr = 0x7E000000456F3E3AULL;
      printf("\nYou have chosen board %d!\n", board);
      break;
    case 3:
      owaddr = 0xC70000004643AD3AULL;
      printf("\nYou have chosen board %d!\n", board);
      break;
    case 4:
      owaddr = 0x48000000463FCD3AULL;
      printf("\nYou have chosen board %d!\n", board);
      break;
    default:
      printf("\nYou didn't select a valid board (1,2,3 or 4).\nPlease restart program.\n");
  }
  return owaddr;
}

//-----------------------------------------------------------------------------------------------------//
//-----------------------------------------------------------------------------------------------------//



int main(int argc, char *argv[])
{
  uint64_t owaddr = 0x0;  // DS2413 addr
  struct linkusb_dev linkusb;
  int k;
  uint64_t serial;
  int swaddr = 0;  // address on the switch on this FEE (or can loop over a set of them)

  if (linkusb_open(&linkusb,0))
    return -1;  // error messages in linkusb_open

  owaddr = choose_board(); // select board and get DS2413 owaddr
  //set_voltage(&linkusb, owaddr);
  monitor_channel(&linkusb, owaddr);

  linkusb_close(&linkusb);
  return 0;
}
