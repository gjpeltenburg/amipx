#include <clib/exec_protos.h>
#include <amipx.h>
#include <amipx_protos.h>
#include <amipx_pragmas.h>
#include <exec/tasks.h>
#include <devices/timer.h>
#include <dos/dos.h>

struct AMIPX_Library *AMIPX_Library;

struct myECB {
 struct AMIPX_ECB ECB;
 struct AMIPX_Fragment extra[2];
};

unsigned char pd[12]={00,00,00,00,02,00,02,00,00,00,00,00
                   };

/*****************************************************************************
 * This program fools DOOM into thinking it found a second DOOM node in IPX  *
 * game setup time.                                                          *
 * All this program does is send the packet data above with a DOOM timeval   *
 * -1, and listening for packets. It keeps doing this until sent a CTRL-C.   *
 *****************************************************************************/
struct Task *mytask;
ULONG sendsigmask;
ULONG listensigmask;

/* Aztec stuff to make a registerised function */
void SendESR(BYTE,struct AMIPX_ECB *);
#pragma regcall(SendESR(D0,A0))
void SendESR(BYTE caller,struct AMIPX_ECB *ecb)
{
 /* simply wake up main */
 geta4(); // essential!

 Signal(mytask,sendsigmask);
}

/* Aztec stuff to make a registerised function */
void ListenESR(BYTE,struct AMIPX_ECB *);
#pragma regcall(ListenESR(D0,A0))
void ListenESR(BYTE caller,struct AMIPX_ECB *ecb)
{
 /* simply wake up main */
 geta4(); // essential!

 Signal(mytask,listensigmask);
}

struct myECB LECB[4];
struct AMIPX_PacketHeader Lheader[4];
BYTE Ltime[4][4];
BYTE Luserdata[4][512];

/* Aztec calls this function. default action is to exit the program */
void _abort()
{
}

main(int argc,char **argv)
{
 struct myECB ECB;
 unsigned char myaddrbuff[20];
 int t,lp;
 UWORD mysocket;
 int socknum,sendsigbit,listensigbit;
 ULONG sigret,timersigmask;
 struct AMIPX_PacketHeader header;
 BYTE time[4];
 BYTE userdata[512];
 int count;
 int ecbindex;
 int running,submitted_timerio;
 struct timerequest *TimerIO;
 struct MsgPort *TimerMP;
 struct Message *TimerMSG;
    
 socknum=0x869b;

 mytask=FindTask(NULL);

/* allocate a signal to wake me up when an ecb has been processed */
 sendsigbit=AllocSignal(-1);
 if(sendsigbit==-1) {
  printf("Cannot allocate signal!\n");
  return 0;
 }
 listensigbit=AllocSignal(-1);
 if(listensigbit==-1) {
  FreeSignal(sendsigbit);
  printf("Cannot allocate signal!\n");
  return 0;
 } 
 sendsigmask = 1<<sendsigbit;
 listensigmask = 1<<listensigbit;

 SetSignal(sendsigmask|listensigmask|timersigmask,0L);

 if(argc>1)
  sscanf(argv[1],"%x",&socknum);

 for(lp=0;lp<4;lp++) { /* set up 4 listen requests */
  for(t=0;t<sizeof(struct myECB);t++)
   ((char *)&(LECB[lp]))[t]=0;

  LECB[lp].ECB.ESR=ListenESR;
  LECB[lp].ECB.Fragment[0].FragData=&(Lheader[lp]);
  LECB[lp].ECB.Fragment[0].FragSize=sizeof(struct AMIPX_PacketHeader);
  LECB[lp].ECB.FragCount=3;
  LECB[lp].ECB.Fragment[1].FragData=&(Ltime[lp]);
  LECB[lp].ECB.Fragment[1].FragSize=4;
  LECB[lp].ECB.Fragment[2].FragData=&(Luserdata[lp]);
  LECB[lp].ECB.Fragment[2].FragSize=12;
  LECB[lp].ECB.InUse=0x00;
 }

 for(t=0;t<10;t++)
  myaddrbuff[t]=10-t;

 for(t=0;t<4;t++)
  time[t]=0xff;

 for(t=0;t<12;t++)
  userdata[t]=pd[t];

 for(t=0;t<sizeof(ECB);t++)
  ((char *)&ECB)[t]=0;

 header.Checksum=0xffff;
 header.Length=0x2e;
 header.Tc=0;
 header.Type=0;

 ECB.ECB.ESR=SendESR;
 ECB.ECB.Fragment[0].FragData=&header;
 ECB.ECB.Fragment[0].FragSize=sizeof(header);
 ECB.ECB.FragCount=3;
 ECB.ECB.Fragment[1].FragData=&time;
 ECB.ECB.Fragment[1].FragSize=4;
 ECB.ECB.Fragment[2].FragData=&userdata;
 ECB.ECB.Fragment[2].FragSize=12;
 ECB.ECB.InUse=0x00;
 for(t=0;t<6;t++)
  ECB.ECB.ImmedAddr[t]=0xff;

 AMIPX_Library=(struct AMIPX_Library *)OpenLibrary("amipx.library",0);

 lp=0; // is first Listen that will be satisfied (at least in AMIPX it is)
 submitted_timerio=0;

 if(AMIPX_Library) {
  TimerMP = CreateMsgPort();
  if(TimerMP) {
   TimerIO=
    (struct timerequest *)CreateIORequest(TimerMP,sizeof(struct timerequest));
   if(TimerIO) {
    if(OpenDevice((STRPTR)TIMERNAME,UNIT_VBLANK,(struct IORequest *)TimerIO,0L)){
     DeleteIORequest((struct IORequest *)TimerIO);
     TimerIO=NULL;
     DeleteMsgPort(TimerMP);
    }
   }                                      
  }
  printf("Library opened.\n");
  if(TimerIO) {
   printf("Timer.device opened.\n");
   AMIPX_GetLocalAddr(myaddrbuff);
   for(t=0;t<10;t++)
    printf("%02x ",(unsigned int)myaddrbuff[t]);
   printf("\n");
   if((mysocket=AMIPX_OpenSocket(socknum))!=0) {
    printf("Socket %04x opened\n",(unsigned int)mysocket);
    for(t=0;t<4;t++) {
     header.Dst.Network[t]=0; // all directly connected networks
    }

    for(t=0;t<6;t++) {
     header.Dst.Node[t]=0xff;
    }
    timersigmask=1<<TimerMP->mp_SigBit;
       
    SetSignal(timersigmask,0L);
       
    header.Dst.Socket=mysocket;
    ECB.ECB.Socket=mysocket;

    printf("Now submitting Listen ECBs...\n");

    for(lp=0;lp<4;lp++) {
     LECB[lp].ECB.Socket=mysocket;

     AMIPX_ListenForPacket(&LECB[lp]);
    }
    printf("Now sending the first packet...\n");

    running=1;
    lp=0;

    printf("Send: %d\n",AMIPX_SendPacket(&ECB));

    TimerIO->tr_time.tv_micro=0;
    TimerIO->tr_time.tv_secs=1;      // sleep for max. 1 sec
    TimerIO->tr_node.io_Command = TR_ADDREQUEST;
    SendIO((struct IORequest *) TimerIO);
    submitted_timerio=1;

    while(running) {

     sigret=Wait(timersigmask|sendsigmask|listensigmask|SIGBREAKF_CTRL_C);
     if(sigret & SIGBREAKF_CTRL_C) {
      running=0;
      printf("CTRL-C\n");
     }
     if(sigret & listensigmask) {
      while(LECB[lp].ECB.InUse)
       lp=(1+lp)%4;

      while(!LECB[lp].ECB.InUse) {
       printf("Packet received.\n");
       printf("header : ");
       for(t=0;t<sizeof(header);t++)
        printf("%02x ",((unsigned char *)&Lheader[lp])[t]);

       printf("\nTime : ");

       for(t=0;t<4;t++)
        printf("%02x ",(unsigned char)Ltime[lp][t]);

       printf("\n");
       for(t=0;t<512 && t<(Lheader[lp].Length-34);t++)
        printf("%02x ",(unsigned char)Luserdata[lp][t]);

       printf("\n");
       if(running) // no sense in submitting request if we have to quit anyway
        AMIPX_ListenForPacket(&LECB[lp]);

       lp=(1+lp)%4;
      }
     }
     if(running) {
      if(sigret & sendsigmask) {
       printf("Packet was transmitted OK.\n");
      }
      if(sigret & timersigmask) {
       if(!ECB.ECB.InUse) {
        printf("Send: %d\n",AMIPX_SendPacket(&ECB));
       }
       else {
        printf("Send has not returned yet!!!\n");
       }
       TimerIO->tr_time.tv_micro=0;
       TimerIO->tr_time.tv_secs=1;      // sleep for max. 1 sec
       TimerIO->tr_node.io_Command = TR_ADDREQUEST;
       SendIO((struct IORequest *) TimerIO);
      }
     }
    }
   }
   if(submitted_timerio) { // in use 
    AbortIO((struct IORequest *) TimerIO);
    WaitIO((struct IORequest *) TimerIO);
   }
   AMIPX_CloseSocket(mysocket);
   CloseDevice((struct IORequest *) TimerIO);
   DeleteIORequest((struct IORequest *)TimerIO);
  }
  else {
   printf("Panic! Socket not opened!\n");
  }
  FreeSignal(sendsigbit); // CLI will want this back
  FreeSignal(listensigbit);
  printf("Now exiting...\n");
  CloseLibrary(AMIPX_Library);
 }
}
