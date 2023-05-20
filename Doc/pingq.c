#include <clib/exec_protos.h>
#include <amipx.h>
#include <amipx_protos.h>
#include <amipx_pragmas.h>
#include <exec/tasks.h>

struct AMIPX_Library *AMIPX_Library;

struct myECB {
 struct AMIPX_ECB ECB;
 struct AMIPX_Fragment extra[2];
};
        
unsigned char pd[16]={0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x0c,
                      0x02,0x51,0x55,0x41,0x4b,0x45,0x00,0x03
                   };

/*****************************************************************************
 * This program fools Quake into thinking it found a second Quake node in    *
 * IPX game setup time.                                                      *
 *****************************************************************************/
struct Task *mytask;
ULONG mysigmask;

/* Aztec stuff to make a registerised function */
void myESR(BYTE,struct AMIPX_ECB *);
#pragma regcall(myESR(D0,A0))
void myESR(BYTE caller,struct AMIPX_ECB *ecb)
{
 /* simply wake up main */
 geta4(); // essential!

 Signal(mytask,mysigmask);
}

struct myECB LECB[4];
struct AMIPX_PacketHeader Lheader[4];
BYTE Ltime[4][4];
BYTE Luserdata[4][512];

main(int argc,char **argv)
{
 struct myECB ECB;
 unsigned char myaddrbuff[20];
 int t,lp;
 UWORD mysocket;
 int socknum,sigbit;
 struct AMIPX_PacketHeader header;
 BYTE time[4];
 BYTE userdata[512];
 int count;
 int ecbindex;

 socknum=26000; // default socket number

 mytask=FindTask(NULL);

 for(lp=0;lp<4;lp++) { /* set up 4 listen requests */
  for(t=0;t<sizeof(struct myECB);t++)
   ((char *)&(LECB[lp]))[t]=0;

  LECB[lp].ECB.Fragment[0].FragData=&(Lheader[lp]);
  LECB[lp].ECB.Fragment[0].FragSize=sizeof(struct AMIPX_PacketHeader);
  LECB[lp].ECB.FragCount=2;
  LECB[lp].ECB.Fragment[1].FragData=&(Luserdata[lp]);
  LECB[lp].ECB.Fragment[1].FragSize=16;
  LECB[lp].ECB.InUse=0x00;
 }

 for(t=0;t<10;t++)
  myaddrbuff[t]=10-t;

 for(t=0;t<4;t++)
  time[t]=0xff;

 for(t=0;t<16;t++)
  userdata[t]=pd[t];

 for(t=0;t<sizeof(ECB);t++)
  ((char *)&ECB)[t]=0;

 header.Checksum=0xffff;
 header.Length=0x2e;
 header.Tc=0;
 header.Type=4;

 ECB.ECB.Fragment[0].FragData=&header;
 ECB.ECB.Fragment[0].FragSize=sizeof(header);
 ECB.ECB.FragCount=2;
 ECB.ECB.Fragment[1].FragData=&userdata;
 ECB.ECB.Fragment[1].FragSize=16;
 ECB.ECB.InUse=0x00;
 for(t=0;t<6;t++)
  ECB.ECB.ImmedAddr[t]=0xff;

 AMIPX_Library=(struct AMIPX_Library *)OpenLibrary("amipx.library",0);

 if(AMIPX_Library) {
  printf("Library opened.\n");
  AMIPX_GetLocalAddr(myaddrbuff);
  for(t=0;t<10;t++)
   printf("%02x ",(unsigned int)myaddrbuff[t]);
  printf("\n");
  if((mysocket=AMIPX_OpenSocket(0))!=0) {
   printf("Socket %04x opened\n",(unsigned int)mysocket);
//   for(t=0;t<4;t++) {
//    header.Dst.Network[t]=myaddrbuff[t];
//   }

   for(t=0;t<6;t++) {
    header.Dst.Node[t]=0xff;
   }

   header.Dst.Socket=26000;
   ECB.ECB.Socket=mysocket;

/* allocate a signal to wake me up when an ecb has been processed */
   sigbit=AllocSignal(-1);
   if(sigbit!=-1)
    mysigmask = 1<<sigbit;

   printf("Now submitting Listen ECBs...\n");

   for(lp=0;lp<4;lp++) {
    LECB[lp].ECB.Socket=mysocket;

    AMIPX_ListenForPacket(&LECB[lp]);
   }

   for(lp=0,count=0;count<30;lp=(lp+1)%4,count++) {
    printf("Now submitting Send ECB...");
    if(sigbit!=-1) {
     printf("(Will Wait() for ecb)...");
     ECB.ECB.ESR=myESR;
     SetSignal(0L,mysigmask);
    }
    printf("Return value: %d\n",AMIPX_SendPacket(&ECB));
    printf("Now waiting for Send to return\n");

    while(ECB.ECB.InUse) {
     if(sigbit!=-1)
      Wait(mysigmask);
      ;
    }
    printf("CompletionCode: %02x\n",ECB.ECB.CompletionCode);
    printf("header : ");
    for(t=0;t<sizeof(header);t++)
     printf("%02x ",((unsigned char *)&header)[t]);
    printf("\n");
    printf("Waiting for Listen to return...\n");

    while(LECB[lp].ECB.InUse) 
      ;

    printf("Now submitting Send ECB...");
    if(sigbit!=-1) {
     printf("(Will Wait() for ecb)...");
     ECB.ECB.ESR=myESR;
     SetSignal(0L,mysigmask);
    }
    printf("Return value: %d\n",AMIPX_SendPacket(&ECB));
    printf("Now waiting for Send to return\n");

    while(ECB.ECB.InUse) {
     if(sigbit!=-1)
      Wait(mysigmask);
      ;
    }
    printf("CompletionCode: %02x\n",ECB.ECB.CompletionCode);
    printf("header : ");
    for(t=0;t<sizeof(header);t++)
     printf("%02x ",((unsigned char *)&header)[t]);
    printf("\n");
    printf("Waiting for Listen to return...\n");

    while(LECB[lp].ECB.InUse) 
      ;


    printf("header : ");
    for(t=0;t<sizeof(header);t++)
     printf("%02x ",((unsigned char *)&Lheader[lp])[t]);

    printf("\n");

    for(t=0;t<512 && t<(Lheader[lp].Length-30);t++)
     printf("%02x ",(unsigned char)Luserdata[lp][t]);

    printf("\n");
    AMIPX_ListenForPacket(&LECB[lp]); // resubmit
   }
   
   AMIPX_CloseSocket(mysocket);
  }
  else {
   printf("Panic! Socket not opened!\n");
  }
  if(sigbit!=-1)
   FreeSignal(sigbit); // CLI will want this back
  printf("Now exiting...\n");
  CloseLibrary(AMIPX_Library);
 }
}
