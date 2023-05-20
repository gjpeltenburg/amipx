/* libsup.c	-- exec library routines for amipx 	*/
#include <ctype.h>
#include <exec/types.h>
#include <exec/lists.h>
#include <exec/resident.h>
#include <exec/semaphores.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <amipx_priv.h>
#include <amipx_protos_p.h>
#include <amipx_pragmas_p.h>
#include <clib/exec_protos.h>
#include <devices/timer.h>
#include <devices/sana2.h>
#include <utility/tagitem.h>
#include <clib/utility_protos.h>
#include <clib/dos_protos.h>
#include <exec/memory.h>
#include <utility/hooks.h>
#define MAX(x,y) ((x)<(y)?(y):(x))

// amiga.lib
struct Task *CreateTask( STRPTR name, long pri, APTR initPC,
        unsigned long stackSize );

int AMIPX_Init(struct AMIPX_Library *);
void AMIPX_Exit(struct AMIPX_Library *);
void AMIPX_cleanup(struct AMIPX_Library *);
void myInit(void);
long myOpen(void);
long myClose(void);
long myExpunge(void);
struct UtilityBase *UtilityBase;

#pragma amicall(AMIPX_Library, 0x06, myOpen())
#pragma amicall(AMIPX_Library, 0x0c, myClose())
#pragma amicall(AMIPX_Library, 0x12, myExpunge())

#pragma regcall(AMIPX_GetLocalAddrI(A6,A0))
#pragma regcall(AMIPX_SendPacketI(A6,A0))
#pragma regcall(AMIPX_SendPacketR(A6,A0))
#pragma regcall(AMIPX_OpenSocketI(A6,D0))
#pragma regcall(AMIPX_CloseSocketI(A6,D0))
#pragma regcall(AMIPX_ListenForPacketI(A6,A0))
#pragma regcall(AMIPX_RelinquishControlI(A6))
#pragma regcall(AMIPX_GetLocalTargetI(A6,A0,A1))

/* library initialization table, used for AUTOINIT libraries			*/
struct InitTable {
	unsigned long	it_DataSize;		  /* library data space size		*/
	void			**it_FuncTable;	  /* table of entry points			*/
	void 			*it_DataInit;	  /* table of data initializers		*/
	void			(*it_InitFunc)(void); /* initialization function to run	*/
};

void *libfunctab[] = {	/* my function table */
	myOpen,					/* standard open	*/
	myClose,				/* standard close	*/
	myExpunge,				/* standard expunge	*/
	0,

	/* ------	user UTILITIES */
        AMIPX_OpenSocket,
        AMIPX_CloseSocket,
        AMIPX_ListenForPacket,
        AMIPX_SendPacket,
        AMIPX_GetLocalAddr,
	AMIPX_RelinquishControl,
	AMIPX_GetLocalTarget,
	(void *)-1			/* end of function vector table */
};

struct InitTable myInitTab =  {
	sizeof (struct AMIPX_Library),
	libfunctab,
	0,		/* will initialize my data in funkymain()	*/
	myInit
};

#define MYREVISION	24	/* would be nice to auto-increment this	*/

char myname[] = "amipx.library";
char myid[] = "AMIPX v1.24 (17 Oct 1998)\r\n";

extern struct Resident	myRomTag;


long
_main(struct AMIPX_Library *AMIPX_Library, unsigned long seglist)
{
	AMIPX_Library->ml_SegList = seglist;

	/* ----- init. library structure  -----		*/
	AMIPX_Library->ml_Lib.lib_Node.ln_Type = NT_LIBRARY;
	AMIPX_Library->ml_Lib.lib_Node.ln_Name = (char *) myname;	
	AMIPX_Library->ml_Lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
	AMIPX_Library->ml_Lib.lib_Version = myRomTag.rt_Version;
	AMIPX_Library->ml_Lib.lib_Revision = MYREVISION;
	AMIPX_Library->ml_Lib.lib_IdString = (APTR) myid;
        AMIPX_Library->SysBase=*((APTR *)4L);
        AMIPX_Library->Inited=0;
}

long
myOpen(void)
{
	struct AMIPX_Library *AMIPX_Library;

        /* mark us as having another customer		*/
        if(AMIPX_Library->ml_Lib.lib_OpenCnt==0)
         if(!AMIPX_Init(AMIPX_Library)) 
          return 0L;

        AMIPX_Library->SysBase=*((APTR *)4L);

        AMIPX_Library->Inited=1;

 	AMIPX_Library->ml_Lib.lib_OpenCnt++;
        /* prevent delayed expunges (standard procedure)		*/
        AMIPX_Library->ml_Lib.lib_Flags &= ~LIBF_DELEXP;

        return ((long) AMIPX_Library);
}

long
myClose(void)
{
	struct AMIPX_Library *AMIPX_Library;
	long retval = 0;

	AMIPX_cleanup(AMIPX_Library);

	if ((--AMIPX_Library->ml_Lib.lib_OpenCnt) == 0) {
                AMIPX_Exit(AMIPX_Library);
 		if  (AMIPX_Library->ml_Lib.lib_Flags & LIBF_DELEXP) {
			/* no more people have me open,
  			 * and I have a delayed expunge pending
			 */
                        AMIPX_Library->Inited=0;
			retval = myExpunge(); /* return segment list	*/
		}
	}

	return (retval);
}

long
myExpunge(void)
{
	struct AMIPX_Library *AMIPX_Library;
	unsigned long seglist = 0;
	long libsize;
	extern struct Library *DOSBase;

	if (AMIPX_Library->ml_Lib.lib_OpenCnt == 0) {
		/* really expunge: remove libbase and freemem	*/
                
		seglist	= AMIPX_Library->ml_SegList;

		Remove(&AMIPX_Library->ml_Lib.lib_Node);
				/* i'm no longer an installed library	*/

		libsize = AMIPX_Library->ml_Lib.lib_NegSize+AMIPX_Library->ml_Lib.lib_PosSize;
		FreeMem((char *)AMIPX_Library-AMIPX_Library->ml_Lib.lib_NegSize, libsize);
		CloseLibrary(DOSBase);		/* keep the counts even */
	}
	else
		AMIPX_Library->ml_Lib.lib_Flags |= LIBF_DELEXP;

	/* return NULL or real seglist				*/
	return ((long) seglist);
}

BOOL AMIPX_PacketFilter(struct Hook *,struct IOSana2Req *,APTR);

#pragma regcall(AMIPX_PacketFilter(a0,a2,a1))
	    
BOOL AMIPX_PacketFilter(struct Hook *hook,struct IOSana2Req *req, 
                        APTR data)
{
 register unsigned char *fromcp;
 struct AMIPX_Nic *NIC;

 NIC=hook->h_Data;
    
 fromcp=data;
    
 switch(NIC->SendFlavour) {
  case AMIPX_PT_ETHERNET_II:
   return TRUE;  // raw IPX packet over packet type assigned to Novell
   break;
  case AMIPX_PT_ETHERNET_802_3:
   if(fromcp[0]==0xff && fromcp[1]==0xff)
    return TRUE; // raw 802.3 may not use checksum
   break;
  case AMIPX_PT_ETHERNET_802_2:
   if(fromcp[0]==0xe0)
    return TRUE; // ethernet 802.2 for Novell uses DSAP of 0xe0 in 802.2 header
   break;
  case AMIPX_PT_ETHERNET_SNAP:
   if(fromcp[0]==0xaa && fromcp[1]==0xaa && fromcp[2]==0x03
      && fromcp[3]==0 && fromcp[4]==0 && fromcp[5]==0 && fromcp[6]==0x81
      && fromcp[7]==0x37)
    return TRUE;
   break;
  default:
   return FALSE; /* unknown packet type is not accepted */
   break;
 }
 return FALSE;
}	 		

BOOL CopyFromECBToS2(VOID *,struct AMIPX_ECB *,ULONG);
BOOL CopyFromS2ToBuff(UBYTE *,VOID *,ULONG);
BOOL CopyNothing(VOID *,VOID *,ULONG);
/*
   AMIPX_searchsocket returns either the position of the socketnumber in the
   index (if found) or the position where it should be inserted (if not found)

   REMINDER: caller must have "Obtained" the socket table! 
*/

int AMIPX_searchsocket(struct AMIPX_Library *AMIPX_Lib,UWORD socket)
{
 int middle,stepsize;
 char table;
 char found;
 int tsock;

 if(AMIPX_Lib->IndexSize==0)
  return 0;

 middle=(AMIPX_Lib->IndexSize-1)>>1;

 found=0;

 stepsize=middle>>1;

 while(stepsize>0 && !found) {
  tsock=AMIPX_Lib->SocketTable[AMIPX_Lib->SockIndex[middle]];
  if(tsock==socket)
   found=1;
  else { 
   if(tsock<socket)
    middle+=stepsize;
   else 
    middle-=stepsize;
   stepsize=stepsize>>1;
  }
 }
 if(!found) {
  while(AMIPX_Lib->SocketTable[AMIPX_Lib->SockIndex[middle]]>socket
     && middle>0)
   middle--;
  while(AMIPX_Lib->SocketTable[AMIPX_Lib->SockIndex[middle]]<socket
     && middle<AMIPX_Lib->IndexSize)
   middle++;
 }

 return middle;
}

int AMIPX_searchroute(struct AMIPX_Library *AMIPX_Lib,int table,ULONG network)
{
 int middle,stepsize;
 char found;
 int tnet;

 if(AMIPX_Lib->RouteTableSize[table]==0) 
  return 0;
 
 middle=(AMIPX_Lib->RouteTableSize[table]-1)>>1;

 found=0;

 stepsize=middle>>1;

 while(stepsize>0 && !found) {
  tnet=AMIPX_Lib->RouteTable[table][AMIPX_Lib->RouteIndex[table][middle]].Network;
  if(tnet==network)
   found=1;
  else { 
   if(tnet<network)
    middle+=stepsize;
   else 
    middle-=stepsize;
   stepsize=stepsize>>1;
  }
 }
 if(!found) {
  while(AMIPX_Lib->RouteTable[table]
                   [AMIPX_Lib->RouteIndex[table][middle]].Network>network
        && middle>0)
   middle--;
  while(AMIPX_Lib->RouteTable[table]
                   [AMIPX_Lib->RouteIndex[table][middle]].Network<network
        && middle<AMIPX_Lib->RouteTableSize[table])
   middle++;
 }
 return middle;
}

/* AMIPX_addroute currently keeps just one route to any network */
void AMIPX_addroute(struct AMIPX_Library *AMIPX_Lib,ULONG Network,
                    UBYTE Card,UBYTE *Router,UWORD Ticks,UWORD Hops)
{
 int pos,routepos,t,table;

 table=(int)AMIPX_Lib->TempRouteTable;
// if(Hops==0) { // we have received word that we're on a certain network
//  AMIPX_Lib->Card[Card].NetAddress[0]=Network>>24;
//  AMIPX_Lib->Card[Card].NetAddress[1]=Network>>16&0xff;
//  AMIPX_Lib->Card[Card].NetAddress[2]=Network>>8&0xff;
//  AMIPX_Lib->Card[Card].NetAddress[3]=Network&0xff;
// }
 pos=AMIPX_searchroute(AMIPX_Lib,table,Network);
 if(pos<AMIPX_Lib->RouteTableSize[table]) {
  routepos=AMIPX_Lib->RouteIndex[table][pos];
  if(AMIPX_Lib->RouteTable[table][routepos].Network==Network) {
   if(AMIPX_Lib->RouteTable[table][routepos].Hops<Hops) {
    // in case this is a longer route to an already known network 
    return;
   }
   // in case this is a shorter or equal route, replace the existing one
   AMIPX_Lib->RouteTable[table][routepos].Hops=Hops;
   AMIPX_Lib->RouteTable[table][routepos].Card=Card;
   AMIPX_Lib->RouteTable[table][routepos].Ticks=Ticks;
   if(Hops>0)
    for(t=0;t<6;t++)
     AMIPX_Lib->RouteTable[table][routepos].Router[t]=Router[t];

   return;
  }
 }
 // in case there is no route to the network yet, insert one
 if(AMIPX_Lib->RouteTableSize[table]<AMIPX_Lib->MaxRoutes) { // still room
  for(t=0;t<AMIPX_Lib->MaxRoutes &&
      AMIPX_Lib->RouteTable[table][t].Network!=0xffffffffL;t++)
   ;
  routepos=t;
  if(routepos<AMIPX_Lib->MaxRoutes) { // this should never fail
   AMIPX_Lib->RouteTable[table][routepos].Network=Network;
   AMIPX_Lib->RouteTable[table][routepos].Hops=Hops;
   AMIPX_Lib->RouteTable[table][routepos].Card=Card;
   AMIPX_Lib->RouteTable[table][routepos].Ticks=Ticks;
   if(Hops>0)
    for(t=0;t<6;t++)
     AMIPX_Lib->RouteTable[table][routepos].Router[t]=Router[t]; 

   for(t=pos;t<AMIPX_Lib->RouteTableSize[table];t++)
    AMIPX_Lib->RouteIndex[table][t+1]=AMIPX_Lib->RouteIndex[table][t];

   AMIPX_Lib->RouteIndex[table][pos]=routepos;
   AMIPX_Lib->RouteTableSize[table]++;
   return; 
  }
 }
}

//void AMIPX_deleteroute(struct AMIPX_Library *AMIPX_Lib,ULONG Network)
//{
// int pos,t,table;
//
// table=(int)AMIPX_Lib->TempRouteTable;
// pos=AMIPX_searchroute(AMIPX_Lib,table,Network);
// 
// if(pos<AMIPX_Lib->RouteTableSize[table]) {
//  AMIPX_Lib->RouteTable[table][AMIPX_Lib->RouteIndex[pos]].Network=0xffffffffL;
//  AMIPX_Lib->RouteTableSize--;
//  for(t=pos+1;t<AMIPX_Lib->RouteTableSize[table];t++)
//   AMIPX_Lib->RouteIndex[table][t-1]=AMIPX_Lib->RouteIndex[table][t];
// }
//}

#pragma regcall(CopyNothing(a0,a1,d0))
BOOL CopyNothing(to,from,n)
VOID *to;
VOID *from;
ULONG n;
{
 return FALSE;
}
       
#pragma regcall(CopyFromECBToS2(a0,a1,d0))
       
BOOL CopyFromECBToS2(to,ecb,n)
VOID *to;
struct AMIPX_ECB *ecb;
ULONG n;
{
 int e2prefixsize=0;
 int esprefixsize=8;
 int e8022prefixsize=3;
 int e8023prefixsize=0;
 register int prefixsize;
 UBYTE *prefix;
 register int t;
 int offset,frag;
 register UBYTE *tocp;
 struct AMIPX_Library *AMIPX_Lib;

 AMIPX_Lib=ecb->AMIPX_Lib;

 tocp=(UBYTE *)to;

 switch(ecb->Flavour) {
  case AMIPX_PT_AUTO:
  case AMIPX_PT_ETHERNET_802_2:
   prefixsize=e8022prefixsize;
   prefix=AMIPX_Lib->e8022prefix;
   break;
  case AMIPX_PT_ETHERNET_802_3:
   prefixsize=e8023prefixsize;
   break;
  case AMIPX_PT_ETHERNET_SNAP:
   prefixsize=esprefixsize;
   prefix=AMIPX_Lib->esprefix;
   break;
  case AMIPX_PT_ETHERNET_II:
   prefixsize=e2prefixsize;
   break;
 }
 for(t=0;t<prefixsize;t++)
  tocp[t]=prefix[t];

 frag=0;
 t=0;
 offset=0;

 if(ecb->Flavour==AMIPX_PT_ETHERNET_802_3) {
  tocp[prefixsize]=0xff;
  tocp[prefixsize+1]=0xff;
  t=2;
 }
 while(t<n-prefixsize && frag<ecb->FragCount) {
  if(t-offset>=ecb->Fragment[frag].FragSize) {
   offset+=ecb->Fragment[frag].FragSize;
   frag++;
  }
  else {
   tocp[t+prefixsize]=ecb->Fragment[frag].FragData[t-offset];
   t++;
  }
 }

 return TRUE;
}
 
#pragma regcall(CopyFromS2ToBuff(a0,a1,d0))
 
/*********************************************************************
 * Notice the difference between the data structures used in a send  *
 * and in a receive: the argument to a send is the ecb itself, while *
 * in the receive, the argument is a contiguous block of memory.     *
 *********************************************************************/
BOOL CopyFromS2ToBuff(to,from,n)
UBYTE *to; 
VOID *from;
ULONG n;
{
 register UBYTE *fromcp;
 register int t;

 fromcp=from;

 for(t=0;t<n;t++)
  to[t]=fromcp[t];

 return TRUE;
}

void AMIPX_cleanup(struct AMIPX_Library *AMIPX_Lib)
{
// All cleanup that was needed in the past is now performed by tasks.
      
 return;
}

void AMIPX_initioreq(struct AMIPX_Nic *NIC,
                     struct IOSana2Req *dr,
                     struct IOSana2Req *sp,
                     void *data)
{
 struct IORequest *rp;
 int t;

 rp=(struct IORequest *)sp;
 rp->io_Device=dr->ios2_Req.io_Device;
 rp->io_Unit=dr->ios2_Req.io_Unit;
 rp->io_Command=S2_READORPHAN; // READ_ORPHAN for AUTO packettype
 rp->io_Flags=0;
 rp->io_Error=0;
 sp->ios2_WireError=0;
 sp->ios2_PacketType=0;        // insignificant because of S2_READORPHAN
 for(t=0;t<SANA2_MAX_ADDR_BYTES;t++) {
  sp->ios2_SrcAddr[t]=0;
  sp->ios2_DstAddr[t]=0;
 }
 sp->ios2_DataLength=0;
 sp->ios2_Data=data;
 sp->ios2_StatData=NULL;
 sp->ios2_BufferManagement=dr->ios2_BufferManagement;
}

struct AMIPX_Library *task_AMIPX_Lib;
struct AMIPX_Nic *tasknic;
struct Task *callingtask;
UBYTE taskcardnum;
ULONG tasksigmask;
int taskretval;
unsigned char *taskdevname;
int taskunitnumber;
unsigned char *tasknodeaddr;

void ESR(APTR,UBYTE,struct AMIPX_ECB *);
#pragma regcall(ESR(A1,D0,A0))
void ESR(APTR address,UBYTE caller,struct AMIPX_ECB *ecb)
{
#asm
 jsr (a1)
#endasm
}
void AMIPX_ESRTask(void)
{
 struct AMIPX_ECB *ecb;
 struct AMIPX_Library *AMIPX_Lib;
 int sigbit,sigbit2;
 char running;
 ULONG sigret;

 geta4();

 AMIPX_Lib=task_AMIPX_Lib;

 taskretval=1;

 sigbit=AllocSignal(-1);
 if(sigbit==-1) 
  taskretval=0;

 if(taskretval) {
  sigbit2=AllocSignal(-1);
  if(sigbit2==-1)
   taskretval=0;
 }
 if(taskretval) {
  AMIPX_Lib->KillESRSignalMask=1<<sigbit;
  AMIPX_Lib->ESRSignalMask=1<<sigbit2;
 }
 if(taskretval==0) {
  Signal(callingtask,tasksigmask);
  DeleteTask(NULL);
 }
 SetSignal(0L,AMIPX_Lib->KillESRSignalMask|AMIPX_Lib->ESRSignalMask);
 Signal(callingtask,tasksigmask);
 running=1;

 ObtainSemaphore(AMIPX_Lib->ESRSem);
 while(running) {
  while((ecb=AMIPX_Lib->ESR_Head)==NULL && running) {
   ReleaseSemaphore(AMIPX_Lib->ESRSem);
   sigret=Wait(AMIPX_Lib->KillESRSignalMask|AMIPX_Lib->ESRSignalMask);

   if(sigret&AMIPX_Lib->KillESRSignalMask) {
    running=0;
   }
   ObtainSemaphore(AMIPX_Lib->ESRSem);
  }
  if(ecb && running) { 
   AMIPX_Lib->ESR_Head=ecb->NextECB;
   if(AMIPX_Lib->ESR_Head==NULL)
    AMIPX_Lib->ESR_Tail=NULL;

   ReleaseSemaphore(AMIPX_Lib->ESRSem);
   if(ecb->ESR) {
    ESR(ecb->ESR,0xff,ecb);
    geta4();
   }
   ObtainSemaphore(AMIPX_Lib->ESRSem);
  }
 }
 ReleaseSemaphore(AMIPX_Lib->ESRSem);
 Signal(callingtask,tasksigmask); // tell AMIPX_Exit that we're done
 DeleteTask(NULL);
}

struct ECB2 {
  APTR Link;             /* RIP: Amipx library */
  APTR ESR;
  UBYTE InUse;
  UBYTE CompletionCode;  /* non-zero in case of error */
  UWORD Socket;
  struct AMIPX_ECB *NextECB;
  struct AMIPX_Library *AMIPX_Lib;
  UBYTE CardNum;
  UBYTE Reserver[4];
  UBYTE Flavour;
  UBYTE DWork[2];       /* private! */
  UBYTE ImmedAddr[6];
  UWORD FragCount;
  struct AMIPX_Fragment Fragment[2]; 
};

struct RIPEntry {
 ULONG Network;
 UWORD Hops;
 UWORD Ticks;
};

struct RIPPacket {
 UWORD RIPType;
 struct RIPEntry Entries[1];
};

// RIPESR handles all RIP packets for the standard RIP socket AMIPX_RIPPORT
// It handles both replies and requests
void AMIPX_RIPESR(UBYTE,struct ECB2 *);
#pragma regcall(AMIPX_RIPESR(D0,A0))
void AMIPX_RIPESR(UBYTE caller,struct ECB2 *ecb)
{
 struct AMIPX_Library *AMIPX_Lib;
 struct RIPPacket *RIPPacket;
 struct AMIPX_PacketHeader *PacketHeader;
 struct AMIPX_PacketHeader *OutPacketHeader;
 struct RIPPacket *RIPOutPacket;
 struct ECB2 *RIPECB;
 int actualentries,numentries,entry,pos,routepos,table,CardNum,t;
 
 geta4();

 if(ecb==NULL)
  return;

 AMIPX_Lib=ecb->Link; 

 // parse RIP packet and update routing table
 PacketHeader=(struct AMIPX_PacketHeader *)ecb->Fragment[0].FragData;
 RIPPacket=(struct RIPPacket *)ecb->Fragment[1].FragData;
 numentries=((PacketHeader->Length[0]*256+PacketHeader->Length[1])-32)/8;
 CardNum=ecb->CardNum;
 switch(RIPPacket->RIPType) {
  case 1:  // request - from clients
   actualentries=0;
   RIPECB=(struct ECB2 *)AMIPX_Lib->RIPECB[AMIPX_Lib->CurrentRIPECB];
   RIPOutPacket=
            (struct RIPPacket *)AMIPX_Lib->RIPPacket[AMIPX_Lib->CurrentRIPECB];
   OutPacketHeader=(struct AMIPX_PacketHeader *)
                   AMIPX_Lib->RIPPacketHeader[AMIPX_Lib->CurrentRIPECB];
   if(numentries>0) {
    if(RIPPacket->Entries[0].Network==0xffffffff) {
     // general request
     ObtainSemaphore(AMIPX_Lib->RouteSem);
     table=AMIPX_Lib->ActiveRouteTable;
     // repeat until whole table processed or max. of 50 entries reached
     for(pos=0;pos<AMIPX_Lib->RouteTableSize[table] && actualentries<50;pos++) {
      routepos=AMIPX_Lib->RouteIndex[table][pos];
      if(AMIPX_Lib->RouteTable[table][routepos].Card!=CardNum) {
       RIPOutPacket->Entries[actualentries].Network=
        AMIPX_Lib->RouteTable[table][routepos].Network;
       RIPOutPacket->Entries[actualentries].Hops=
        AMIPX_Lib->RouteTable[table][routepos].Hops+1;

       // transfer-time for 576 byte packet in ticks.
       RIPOutPacket->Entries[actualentries].Ticks=
        AMIPX_Lib->RouteTable[table][routepos].Ticks
        +(576*18*10/AMIPX_Lib->Card[CardNum].Speed); // about 18 ticks/sec
       actualentries++;
      }
     }
     ReleaseSemaphore(AMIPX_Lib->RouteSem);
    }
    else { // i.e. not a general request
     ObtainSemaphore(AMIPX_Lib->RouteSem);
     table=AMIPX_Lib->ActiveRouteTable;
     for(entry=0;entry<numentries && actualentries<50;entry++) {
      pos=AMIPX_searchroute(AMIPX_Lib,table,RIPPacket->Entries[entry].Network);
      if(pos<AMIPX_Lib->RouteTableSize[table]) {
       routepos=AMIPX_Lib->RouteIndex[table][pos];
       if(AMIPX_Lib->RouteTable[table][routepos].Network==
          RIPPacket->Entries[entry].Network &&
          AMIPX_Lib->RouteTable[table][routepos].Card!=CardNum) {
        RIPOutPacket->Entries[actualentries].Network=
         AMIPX_Lib->RouteTable[table][routepos].Network;
        RIPOutPacket->Entries[actualentries].Hops=
         AMIPX_Lib->RouteTable[table][routepos].Hops+1;

        // approximate transfer-time for 576 byte packet in ticks.
        RIPOutPacket->Entries[actualentries].Ticks=
         AMIPX_Lib->RouteTable[table][routepos].Ticks
         +(576*18*10/AMIPX_Lib->Card[CardNum].Speed); // about 18 ticks/sec
        actualentries++;
       } // if network found
      } // if pos<tablesize...
     } // for...
     ReleaseSemaphore(AMIPX_Lib->RouteSem);
    }
   }
   if(actualentries>0) {
    if(!RIPECB->InUse) {// check for overrun
     *((UWORD *)(&(OutPacketHeader->Length[0])))=
              actualentries*sizeof(struct RIPEntry)+sizeof(UWORD)+30;
     for(t=0;t<12;t++)
      ((unsigned char *)(&(OutPacketHeader->Dst)))[t]=
           ((unsigned char *)(&(PacketHeader->Src)))[t];

     for(t=0;t<4;t++) // tell the client which network it is on
      OutPacketHeader->Dst.Network[t]=AMIPX_Lib->Card[CardNum].NetAddress[t];

     RIPECB->ESR=NULL;
     RIPECB->Socket=AMIPX_RIPPORT;
     RIPECB->FragCount=2;
     RIPECB->Fragment[0].FragData=(BYTE *)OutPacketHeader;
     RIPECB->Fragment[0].FragSize=30;
     RIPECB->Fragment[1].FragData=(BYTE *)RIPOutPacket;
     RIPECB->Fragment[1].FragSize=sizeof(UWORD)+
                                  actualentries*sizeof(struct RIPEntry);
     OutPacketHeader->Checksum[0]=OutPacketHeader->Checksum[1]=0xff;
     OutPacketHeader->Type=1; // RIP packet
     OutPacketHeader->Tc=0;   // same network, ain't it
     RIPOutPacket->RIPType=2; // reply

     for(t=0;t<4;t++)
      OutPacketHeader->Src.Network[t]=AMIPX_Lib->Card[CardNum].NetAddress[t];

     for(t=0;t<6;t++)
       OutPacketHeader->Src.Node[t]=AMIPX_Lib->Card[CardNum].CardAddress[t];
     
     OutPacketHeader->Src.Socket[0]=AMIPX_RIPPORT>>8;
     OutPacketHeader->Src.Socket[1]=AMIPX_RIPPORT&0xff;

     RIPECB->CardNum=0xff;

     AMIPX_SendPacketR(AMIPX_Lib,RIPECB);
     AMIPX_Lib->CurrentRIPECB=(AMIPX_Lib->CurrentRIPECB+1)%AMIPX_RIPECBS;
    }
   }
   break;
  case 2:  // reply - from router
   ObtainSemaphore(AMIPX_Lib->RouteSem);
   for(entry=0;entry<numentries;entry++) { // process all networks
//    Alert(RIPPacket->Entries[entry].Network);
    AMIPX_addroute(AMIPX_Lib,RIPPacket->Entries[entry].Network,
                             ecb->CardNum,PacketHeader->Src.Node,
                             RIPPacket->Entries[entry].Ticks,
                             RIPPacket->Entries[entry].Hops);
   }
   ReleaseSemaphore(AMIPX_Lib->RouteSem);
   break;
 }

 // repost
 AMIPX_ListenForPacketI(AMIPX_Lib,(struct AMIPX_ECB *)ecb);
 return;
}

void AMIPX_RouteSearchTask(void)
{
 struct ECB2 *ecb;
 struct AMIPX_Library *AMIPX_Lib;
 struct ECB2 lecb[4];
 int sigbit,sigbit2,t,t2,CardNum;
 struct AMIPX_PacketHeader *packetheader;
 struct RIPPacket *rip;
 char running;
 ULONG sigretval,sigmask;
 ULONG Network;
 struct timerequest *TimerIO;
 struct MsgPort *TimerMP;
 struct Message *TimerMSG;
 UBYTE *spd;
 UBYTE *ph[4];
 UBYTE *pd[4];

 geta4();

 AMIPX_Lib=task_AMIPX_Lib;

 spd=(UBYTE *)(&lecb[0]);
 for(t=0;t<4*sizeof(struct ECB2);t++)
  spd[t]=0;

 taskretval=1;

 sigbit=AllocSignal(-1);
 if(sigbit==-1) 
  taskretval=0;

 for(t=0;t<4;t++) 
  ph[t]=pd[t]=NULL;

 AMIPX_Lib->RouteSearchECB=NULL;
 AMIPX_Lib->RouteSearchHeader=NULL;

 if(taskretval) {
  AMIPX_Lib->KillRouteSearchMask=1<<sigbit;
  if(AMIPX_OpenSocketI(AMIPX_Lib,AMIPX_RIPPORT)==0)  // cannot open RIP socket
   taskretval=0;
 }
 spd=NULL;
 if(taskretval) {
  if((spd=(UBYTE *)AllocVec(sizeof(UBYTE)*10,MEMF_PUBLIC|MEMF_CLEAR))==NULL)
   taskretval=0;
 }
 if((AMIPX_Lib->RouteSearchHeader=
   (UBYTE *)AllocVec(sizeof(UBYTE)*30,MEMF_PUBLIC|MEMF_CLEAR))==NULL)
    taskretval=0;

 if(taskretval) {
  for(t=0;t<4;t++) {
   ph[t]=(UBYTE *)AllocVec(sizeof(UBYTE)*30,MEMF_PUBLIC|MEMF_CLEAR);
   pd[t]=(UBYTE *)AllocVec(sizeof(UBYTE)*1500,MEMF_PUBLIC|MEMF_CLEAR);
   if(ph[t]==NULL || pd[t]==NULL) 
    taskretval=0;
  }
 }
 if(taskretval) {
  if((AMIPX_Lib->RouteSearchECB=AllocVec(sizeof(struct ECB2),
                                         MEMF_PUBLIC|MEMF_CLEAR))==NULL)
    taskretval=0;
 }
 if(taskretval) {
  TimerMP = CreateMsgPort();
  if(TimerMP) {
   TimerIO=
      (struct timerequest *)CreateIORequest(TimerMP,sizeof(struct timerequest));
   if(TimerIO) {
    if(OpenDevice((STRPTR)TIMERNAME,UNIT_VBLANK,(struct IORequest *)TimerIO,0L)){
     DeleteIORequest((struct IORequest *)TimerIO);
     TimerIO=NULL;
     DeleteMsgPort(TimerMP);
     taskretval=0;
    }
   }                                      
  }
 }
 if(taskretval==0) {
  if(AMIPX_Lib->RouteSearchHeader)
   FreeVec(AMIPX_Lib->RouteSearchHeader);
  AMIPX_Lib->RouteSearchHeader=NULL;
  if(AMIPX_Lib->RouteSearchECB)
   FreeVec(AMIPX_Lib->RouteSearchECB);
  AMIPX_Lib->RouteSearchECB=NULL;
  
  for(t=0;t<4;t++) {
   if(ph[t])
    FreeVec(ph[t]);
   if(pd[t])
    FreeVec(pd[t]);
  }
  if(spd)
   FreeVec(spd);
  Signal(callingtask,tasksigmask);
  DeleteTask(NULL);
 }
 Signal(callingtask,tasksigmask);
 for(t=0;t<sizeof(ecb);t++)
  ((unsigned char *)&ecb)[t]=0;
 for(t2=0;t2<4;t2++)
  for(t=0;t<sizeof(struct AMIPX_ECB);t++)
   ((unsigned char *)&(lecb[t2]))[t]=0;

 AMIPX_Lib->RouteTableSize[0]=0;
 AMIPX_Lib->RouteTableSize[1]=0;

 AMIPX_Lib->ActiveRouteTable=0;
 AMIPX_Lib->TempRouteTable=1;

 for(t=0;t<AMIPX_Lib->MaxRoutes;t++) { // clear Route Table
  AMIPX_Lib->RouteTable[0][t].Network=0xffffffffL;
  AMIPX_Lib->RouteTable[1][t].Network=0xffffffffL;
 }
 for(t=0;t<AMIPX_Lib->CardCount;t++) { // add direct networks
  Network=AMIPX_Lib->Card[t].NetAddress[0]<<24|
          AMIPX_Lib->Card[t].NetAddress[1]<<16|
          AMIPX_Lib->Card[t].NetAddress[2]<<8|
          AMIPX_Lib->Card[t].NetAddress[3];
  AMIPX_addroute(AMIPX_Lib,Network,t,NULL,0,0);
 }

 running=1;

 Signal(callingtask,tasksigmask);

 rip=(struct RIPPacket *)spd;

 for(t=0;t<4;t++) {
  lecb[t].Socket=AMIPX_RIPPORT;
  lecb[t].ESR=AMIPX_RIPESR;
  lecb[t].Link=AMIPX_Lib;
  lecb[t].FragCount=2;
  lecb[t].Fragment[0].FragSize=30;
  lecb[t].Fragment[0].FragData=(BYTE *)ph[t];
  lecb[t].Fragment[1].FragSize=1500;
  lecb[t].Fragment[1].FragData=(BYTE *)pd[t];
 }
 for(t=0;t<4;t++)
  AMIPX_ListenForPacketI(AMIPX_Lib,(struct AMIPX_ECB *)&(lecb[t]));

 sigmask=1<<TimerMP->mp_SigBit;

 SetSignal(AMIPX_Lib->KillRouteSearchMask,0L);

 while(running) {
  ObtainSemaphore(AMIPX_Lib->RouteSem);
// switch tables
  AMIPX_Lib->ActiveRouteTable=AMIPX_Lib->TempRouteTable;
  AMIPX_Lib->TempRouteTable=1-AMIPX_Lib->ActiveRouteTable;

// initialise the next table
  for(t=0;t<AMIPX_Lib->MaxRoutes;t++)
   AMIPX_Lib->RouteTable[AMIPX_Lib->TempRouteTable][t].Network=0xffffffffL;

  AMIPX_Lib->RouteTableSize[AMIPX_Lib->TempRouteTable]=0;
  for(t=0;t<AMIPX_Lib->CardCount;t++) { // add direct networks
   Network=AMIPX_Lib->Card[t].NetAddress[0]<<24|
           AMIPX_Lib->Card[t].NetAddress[1]<<16|
           AMIPX_Lib->Card[t].NetAddress[2]<<8|
           AMIPX_Lib->Card[t].NetAddress[3];
   AMIPX_addroute(AMIPX_Lib,Network,t,NULL,0,0);
  }
  ReleaseSemaphore(AMIPX_Lib->RouteSem);

// send router search broadcasts on every direct network 
  if(AMIPX_Lib->MaxRoutes>AMIPX_Lib->CardCount) {
   ecb=(struct ECB2*)AMIPX_Lib->RouteSearchECB;
   packetheader=AMIPX_Lib->RouteSearchHeader;
   if(ecb) { // i.e. card initialised allright
    if(!ecb->InUse) { // if still not processed, don't submit again
     ecb->Socket=AMIPX_RIPPORT;
     for(t=0;t<4;t++)
      packetheader->Dst.Network[t]=0; // all direct network connections
     for(t=0;t<6;t++)
      packetheader->Dst.Node[t]=0xff;
     packetheader->Dst.Socket[0]=AMIPX_RIPPORT>>8;
     packetheader->Dst.Socket[1]=AMIPX_RIPPORT&0xff;
     ecb->ESR=NULL;
     ecb->FragCount=2;
     ecb->Fragment[0].FragData=packetheader;
     ecb->Fragment[0].FragSize=30;
     ecb->Fragment[1].FragData=(BYTE *)spd;
     ecb->Fragment[1].FragSize=10;
     packetheader->Checksum[0]=packetheader->Checksum[1]=0xff;
     packetheader->Length[0]=0;
     packetheader->Length[1]=40;
     packetheader->Type=4;
     packetheader->Tc=0;
     for(t=0;t<4;t++)
      packetheader->Src.Network[t]=AMIPX_Lib->Card[CardNum].NetAddress[t];

     for(t=0;t<6;t++)
      packetheader->Src.Node[t]=AMIPX_Lib->Card[CardNum].CardAddress[t];
     
     packetheader->Src.Socket[0]=AMIPX_RIPPORT>>8;
     packetheader->Src.Socket[1]=AMIPX_RIPPORT&0xff;
     rip->RIPType=1;           // request
     rip->Entries[0].Network=0xffffffff;  // any
     rip->Entries[0].Hops=0xffff;
     rip->Entries[0].Ticks=0xffff;
     AMIPX_SendPacketI(AMIPX_Lib,(struct AMIPX_ECB *)ecb);
    }
   }
  }
// now go to sleep for .. secs
  TimerIO->tr_time.tv_micro=0;
  TimerIO->tr_time.tv_secs=AMIPX_Lib->RouteSearchInterval;
  TimerIO->tr_node.io_Command = TR_ADDREQUEST;
  SendIO((struct IORequest *) TimerIO);

  sigretval=Wait(AMIPX_Lib->KillRouteSearchMask|sigmask);
  if(sigretval & AMIPX_Lib->KillRouteSearchMask) { // told to quit?
   running=0;
   if(!(sigretval & sigmask)) { // timer not expired?
    AbortIO((struct IORequest *)TimerIO);
    WaitIO((struct IORequest *)TimerIO);
   }
  }
  if(sigretval & sigmask) {
   WaitIO((struct IORequest *) TimerIO);
  }
 }
 CloseDevice((struct IORequest *) TimerIO);
 DeleteIORequest((struct IORequest *)TimerIO);
 DeleteMsgPort(TimerMP);
 AMIPX_CloseSocketI(AMIPX_Lib,AMIPX_RIPPORT);
 if(AMIPX_Lib->RouteSearchHeader)
  FreeVec(AMIPX_Lib->RouteSearchHeader);
 AMIPX_Lib->RouteSearchHeader=NULL;
 if(AMIPX_Lib->RouteSearchECB)
  FreeVec(AMIPX_Lib->RouteSearchECB);
 AMIPX_Lib->RouteSearchECB=NULL;

 for(t=0;t<4;t++) {
  FreeVec(ph[t]);
  FreeVec(pd[t]);
 }
 FreeVec(spd);
 Signal(callingtask,tasksigmask); // tell AMIPX_Exit that we're done
 DeleteTask(NULL);
}
   
void AMIPX_ListenTask(void)
{
 struct AMIPX_Nic *NIC;
 struct AMIPX_Library *AMIPX_Lib;
 struct UtilityBase *UtilityBase;
 struct IOSana2Req *req;
 char running;
 int e2prefixsize=0;
 int esprefixsize=8;
 int e8022prefixsize=3;
 int e8023prefixsize=0;
 register int prefixsize,n;
 UWORD Flavour;
 register UBYTE *prefix;
 register int t,socket;
 register unsigned long SockNum;
 UBYTE table;
 struct AMIPX_ECB *ecb;
 int frag,offset,inqueue;
 UBYTE sigbit;
 UBYTE isbroadcast;
 ULONG ptype;
 ULONG DstNet;
 ULONG sigmask,sigret;
 unsigned char *fromcp;
 char in_Src,in_Dst,ok;
 struct TagItem *mytags;
 struct MsgPort *Sana2Port;
 struct IOSana2Req *DeviceReq;
 struct IOSana2Req *ControlReq;
 struct AMIPX_PacketHeader *ph;
 UBYTE CardNum,for_us;
 
 geta4();

 CardNum=taskcardnum;

 AMIPX_Lib=task_AMIPX_Lib;

 UtilityBase=(struct UtilityBase *)OpenLibrary((APTR)"Utility.library",0L);

 NIC=tasknic;

 Sana2Port=CreateMsgPort();
 if(Sana2Port==NULL)
  taskretval=0;
 else
  taskretval=1;

 if(taskretval) {
  sigbit=AllocSignal(-1);
  if(sigbit==-1) 
   taskretval=0;
 }
 if(taskretval==0) {
  if(Sana2Port)
   DeleteMsgPort(Sana2Port);

  Signal(callingtask,tasksigmask);
  DeleteTask(NULL);
 }
 NIC->KillInSignalMask=1<<sigbit;
 
 taskretval=1;

 DeviceReq=NULL;
 ControlReq=NULL;

 if(taskretval) {
  mytags=AllocateTagItems((ULONG)3);
  if(mytags==NULL) {
   taskretval=0;
  }
 }

 if(taskretval) {
  mytags[0].ti_Tag=S2_CopyToBuff;
  mytags[0].ti_Data=(ULONG)CopyFromS2ToBuff;
  mytags[1].ti_Tag=S2_CopyFromBuff;
  mytags[1].ti_Data=(ULONG)CopyNothing;
  mytags[2].ti_Tag=TAG_DONE; 
  mytags[2].ti_Data=NULL;

  NIC=tasknic;

  ControlReq=(struct IOSana2Req *)
    CreateIORequest(Sana2Port,sizeof(struct IOSana2Req));

  DeviceReq=(struct IOSana2Req *)
    CreateIORequest(Sana2Port,sizeof(struct IOSana2Req));

  if(DeviceReq) 
   DeviceReq->ios2_BufferManagement=mytags;
 
  if(ControlReq==NULL || DeviceReq==NULL) 
   taskretval=0;
 }

 if(taskretval) {
  if(OpenDevice(taskdevname,taskunitnumber,(struct IORequest *)DeviceReq,0L))
   taskretval=0;
 }

 FreeTagItems(mytags);
 CloseLibrary((struct Library *)UtilityBase);

 for(t=0;t<AMIPX_READREQUESTS;t++) {
  NIC->InputReq[t]=NULL;
  NIC->InputBuffUse[t]=0; // first buffer, not t+AMIPX_READREQUESTS
  NIC->InputBuff[t]=NULL;
  NIC->InputBuff[t+AMIPX_READREQUESTS]=NULL; // extra buffer for routing
  NIC->RoutedECB[t]=NULL;
 }
 if(!AMIPX_Lib->DontRoute) {
  for(t=0;t<AMIPX_READREQUESTS && taskretval;t++) {
   if((NIC->RoutedECB[t]=
               AllocVec(sizeof(struct AMIPX_ECB),MEMF_PUBLIC|MEMF_CLEAR))==NULL)
    taskretval=0;
  }
 }

 if(taskretval==0) {
  for(t=0;t<AMIPX_READREQUESTS;t++) {
   if(NIC->RoutedECB[t]) {
    FreeVec(NIC->RoutedECB[t]);
    NIC->RoutedECB[t]=NULL;
   }
  }
  if(ControlReq)
   DeleteIORequest((struct IORequest *)ControlReq);
  if(DeviceReq)
   DeleteIORequest((struct IORequest *)DeviceReq);

  Signal(callingtask,tasksigmask);
  DeleteTask(NULL);
 }

 AMIPX_initioreq(NIC,DeviceReq,ControlReq,NULL);

 ControlReq->ios2_Req.io_Command=S2_GETSTATIONADDRESS;

 DoIO((struct IORequest *)ControlReq);

 in_Src=0;
 in_Dst=0;

 for(t=0;t<6;t++) {
  if(ControlReq->ios2_SrcAddr[t])
   in_Src=1;
  if(ControlReq->ios2_DstAddr[t])
   in_Dst=1;
 }
// ignoring SrcAddress - device can only be configured once anyway
 in_Src=0;

 if(!in_Src) {
  if(in_Dst) {
   for(t=0;t<6;t++)
    ControlReq->ios2_SrcAddr[t]=ControlReq->ios2_DstAddr[t];
  }
  else {
   if(tasknodeaddr)
    for(t=0;t<6;t++)
     ControlReq->ios2_SrcAddr[t]=tasknodeaddr[t];
  }
 }

 ControlReq->ios2_Req.io_Command=S2_DEVICEQUERY;
 ControlReq->ios2_StatData=&NIC->DevQuery;
 NIC->DevQuery.SizeAvailable=sizeof(NIC->DevQuery);
 NIC->DevQuery.DevQueryFormat=0;
 NIC->DevQuery.DeviceLevel=0;
 DoIO((struct IORequest *)ControlReq);
 NIC->AddressSize=NIC->DevQuery.AddrFieldSize>>3;
 if(NIC->DevQuery.AddrFieldSize&7>0)
  NIC->AddressSize++;

 NIC->MTU=NIC->DevQuery.MTU;
 NIC->Speed=NIC->DevQuery.BPS;

 ControlReq->ios2_Req.io_Command=S2_CONFIGINTERFACE;
 DoIO((struct IORequest *)ControlReq);
 for(t=0;t<6;t++)
  NIC->CardAddress[t]=ControlReq->ios2_SrcAddr[t];

 for(t=0;t<SANA2_MAX_ADDR_BYTES;t++)
  NIC->Address[t]=ControlReq->ios2_SrcAddr[t];

 NIC->InputReqP=0;
 for(t=0;t<AMIPX_READREQUESTS && taskretval;t++) {
  NIC->InputReq[t]=(struct IOSana2Req *)
    CreateIORequest(Sana2Port,sizeof(struct IOSana2Req));

  if(NIC->InputReq[t]==NULL) {
   taskretval=0;
  }
  else {
   NIC->InputBuff[t]=AllocVec(NIC->MTU,MEMF_CLEAR|MEMF_PUBLIC);
   if(!AMIPX_Lib->DontRoute) {
    NIC->InputBuff[t+4]=AllocVec(NIC->MTU,MEMF_CLEAR|MEMF_PUBLIC);
   }
   NIC->InputReq[t]->ios2_Data=NIC->InputBuff[t];
   NIC->InputBuffUse[t]=0;
   if(NIC->InputBuff[t]==NULL || 
      (NIC->InputBuff[t+AMIPX_READREQUESTS]==NULL && !AMIPX_Lib->DontRoute)) {
    taskretval=0;
   }
  }
 }

 if(taskretval==0) {
  for(t=0;t<AMIPX_READREQUESTS;t++) {
   if(NIC->RoutedECB[t]) {
    FreeVec(NIC->RoutedECB[t]);
    NIC->RoutedECB[t]=NULL;
   }
  }
  for(t=0;t<AMIPX_READREQUESTS;t++) {
   if(NIC->InputReq[t]) {
    DeleteIORequest(NIC->InputReq[t]);
    NIC->InputReq[t]=NULL;
   }
  }
  for(t=0;t<AMIPX_READREQUESTS*2;t++) {
   if(NIC->InputBuff[t]) {
    FreeVec(NIC->InputBuff[t]);
    NIC->InputBuff[t]=NULL;
   }
  }
  CloseDevice((struct IORequest *)DeviceReq);
  DeleteIORequest((struct IORequest *)DeviceReq);
  DeleteIORequest((struct IORequest *)ControlReq);
  Signal(callingtask,tasksigmask);
  DeleteTask(NULL);
 }
 running=1;
 ptype=1; // for all IEEE802.x frames
 if(NIC->SendFlavour==AMIPX_PT_ETHERNET_II)
  ptype = 33079;

 SetSignal(0L,NIC->KillInSignalMask|sigmask);
  
 for(t=0;t<AMIPX_READREQUESTS;t++) {
  AMIPX_initioreq(NIC,DeviceReq,NIC->InputReq[t],NIC->InputBuff[t]);
  NIC->InputReq[t]->ios2_Req.io_Command=CMD_READ;
  NIC->InputReq[t]->ios2_PacketType=ptype;
  NIC->InputReq[t]->ios2_DataLength=NIC->MTU;
  NIC->InputReq[t]->ios2_Data=NIC->InputBuff[t];
  SendIO((struct IORequest *)NIC->InputReq[t]);
 }
 Signal(callingtask,tasksigmask); // Let AMIPX_InitNIC know we're up and running

 sigmask=1<<Sana2Port->mp_SigBit;
 while(running) { // will get a signal from AMIPX_Exit
  while(running
        && CheckIO((struct IORequest *)NIC->InputReq[NIC->InputReqP])==NULL) {
   sigret=Wait(NIC->KillInSignalMask|sigmask);

   if(sigret&NIC->KillInSignalMask)
    running=0;
  }
  if(running) {
   WaitIO((struct IORequest *)NIC->InputReq[NIC->InputReqP]);
   req=NIC->InputReq[NIC->InputReqP];

   fromcp=req->ios2_Data;

   ok=0;
   switch(NIC->SendFlavour) {
    case AMIPX_PT_ETHERNET_II:
      prefixsize=e2prefixsize;
      ok=1;
      break;
     case AMIPX_PT_ETHERNET_802_3:
      prefixsize=e8023prefixsize;
      if(fromcp[0]==0xff && fromcp[1]==0xff)
       ok=1;
      break;
     case AMIPX_PT_ETHERNET_802_2:
      prefixsize=e8022prefixsize;
      if(fromcp[0]==0xe0)
       ok=1;
      break;
     case AMIPX_PT_ETHERNET_SNAP:
      prefixsize=esprefixsize;
      if(fromcp[0]==0xaa && fromcp[1]==0xaa && fromcp[2]==0x03
         && fromcp[3]==0 && fromcp[4]==0 && fromcp[5]==0 && fromcp[6]==0x81
         && fromcp[7]==0x37)
       ok=1;
      break;
     default:
      prefixsize=0;
      break;
   }
   if(!ok) {
    AMIPX_Lib->DroppedPackets++;
   }
   else {
    for_us=1;

    ph=(struct AMIPX_PacketHeader *)(fromcp+prefixsize);

    for(t=0;t<4 && ph->Dst.Network[t]==NIC->NetAddress[t];t++)
     ; 

    n=ph->Length[0]*256+ph->Length[1];

    if(t<4) 
     for_us=0;

    DstNet=ph->Dst.Network[0]<<24+ph->Dst.Network[1]<<16
           +ph->Dst.Network[2]<<8+fromcp[3];

    isbroadcast=0;

    if((ph->Dst.Node[0]&ph->Dst.Node[1]&ph->Dst.Node[2]
       &ph->Dst.Node[3]&ph->Dst.Node[4]&ph->Dst.Node[5])==0xff)
     isbroadcast=1;
 
    if(!for_us) {
     if(DstNet==0) {
      for(t=0;t<6 && ph->Dst.Node[t]==NIC->CardAddress[t];t++)
       ;
      if(t==6 || isbroadcast) {
       for(t=0;t<4;t++)  // tell IPX user who sent the packet
        ph->Src.Network[t]=NIC->NetAddress[t]; 

       for_us=1; // accept local broadcast as a valid network
      }
     }
    }

    if(!for_us) {
     for(t=0;t<4 && ph->Dst.Network[t]==AMIPX_Lib->Card[0].NetAddress[t];t++)
      ; // check if it is meant for our primary network interface
     if(t==4) {
      for(t=0;t<6 && ph->Dst.Node[t]==AMIPX_Lib->Card[0].CardAddress[t];t++)
       ;
      if(t==6 || isbroadcast)
       for_us=1;
     }
    }
/////////////////////////////////////////////////////////////////////////////
// The following test is used to determine one of the following reasons to //
// route the packet:                                                       //
//  1) The packet is meant for a different network, and we may have been   //
//     asked to route the packet. (!for_us)                                //
//  2) A packet has been broadcast on network 0 and we're configured to    //
//     concatenate that network - however, we must NEVER route RIP packets //
//     because routers will not know who sent the packet, and cannot reply //
//  3) It is a propagated packet (type 0x14)                               //
///////////////////////////////////////////////////////////////////////////// 
    if(!for_us || (ph->Tc<AMIPX_Lib->Max0Hops &&
                   (ph->Type==0x14 ||(DstNet==0 && ph->Type!=1)))) {
     if(!AMIPX_Lib->DontRoute && ph->Tc<16) { // only route if allowed; no more
                                             // than 16 hops allowed (why?)
/////////////////////////////////////////////////////////////////////////////
//                                                                         //
//                               ROUTING HERE                              //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////
      ecb=NIC->RoutedECB[NIC->InputReqP];
      if(!ecb->InUse) { // don't overrun a send
       // assign received packet data (minus prefix) to the ECB;
       ecb->ESR=NULL;
       ecb->Link=NULL;
       ecb->Fragment[0].FragData=(BYTE *)(fromcp+prefixsize);
       ecb->Fragment[0].FragSize=n-prefixsize;
       ecb->FragCount=1;
       ecb->CardNum=CardNum; // prevent sending on the same network card
       // swap receive buffer with alternative (which is guaranteed to be free)
       NIC->InputBuffUse[NIC->InputReqP]=1-NIC->InputBuffUse[NIC->InputReqP];
       NIC->InputReq[NIC->InputReqP]->ios2_Data=
              NIC->InputBuff[NIC->InputReqP+AMIPX_READREQUESTS
                            * NIC->InputBuffUse[NIC->InputReqP]];
       NIC->InputReq[NIC->InputReqP]->ios2_DataLength=NIC->MTU;
       ph->Tc++; // increment hop count
       // submit send ECB using SendPacketR (does not change src field in header)
       AMIPX_SendPacketR(AMIPX_Lib,ecb);
      }  // ecb->InUse
     } // !DontRoute
    } // !for_us...
    if(for_us) { // meant for this network, therefore meant for us 
     SockNum=fromcp[prefixsize+16]<<8|fromcp[prefixsize+17];
     if(SockNum!=0) { // socket number 0 is invalid
      ObtainSemaphore(AMIPX_Lib->SocketSem); // wait for release of socket table
      AMIPX_Lib->LastReceivedOnSocket=SockNum;
      if((t=AMIPX_searchsocket(AMIPX_Lib,(UWORD)SockNum))<AMIPX_Lib->IndexSize) {
       socket=AMIPX_Lib->SockIndex[t];
       if((int)AMIPX_Lib->SocketTable[socket]==(int)(UWORD)SockNum) {
        ecb=AMIPX_Lib->WaitingECB[socket];
        if(ecb) {
         ecb->InUse=0xFA; // processing
 
         AMIPX_Lib->WaitingECB[socket]=ecb->NextECB;
       
         frag=0;
         t=0;
         offset=0;
  
         while(t+prefixsize<n && frag<ecb->FragCount) {
          if(t-offset>=ecb->Fragment[frag].FragSize) {
           offset+=ecb->Fragment[frag].FragSize;
           frag++;
          }
          else {
           ecb->Fragment[frag].FragData[t-offset]=fromcp[t+prefixsize];
           t++;
          }
         }

         if(NIC->SendFlavour==AMIPX_PT_AUTO)
          NIC->SendFlavour=Flavour;

         ecb->CompletionCode=0;
         ecb->CardNum=CardNum;
         ecb->InUse=0;
         ecb->NextECB=NULL;

         if(ecb->ESR) {
          ObtainSemaphore(AMIPX_Lib->ESRSem);
          if(AMIPX_Lib->ESR_Tail) {
           AMIPX_Lib->ESR_Tail->NextECB=ecb;
           AMIPX_Lib->ESR_Tail=ecb;
          }
          else {
           AMIPX_Lib->ESR_Head=ecb;
           AMIPX_Lib->ESR_Tail=ecb;
          }
          ReleaseSemaphore(AMIPX_Lib->ESRSem);
          Signal(AMIPX_Lib->ESRTask,AMIPX_Lib->ESRSignalMask); // wake up ESRTask
         }
        }
       }  
      }
      ReleaseSemaphore(AMIPX_Lib->SocketSem);
     }
    }
   }
  
   ptype=1; // for all IEEE802.x frames
   if(NIC->SendFlavour==AMIPX_PT_ETHERNET_II)
    ptype = 33079;
   SendIO((struct IORequest *)NIC->InputReq[NIC->InputReqP]);
   NIC->InputReqP=(NIC->InputReqP+1)%AMIPX_READREQUESTS;
  }
 }
 for(t=0;t<AMIPX_READREQUESTS;t++)
  AbortIO((struct IORequest *)NIC->InputReq[t]);

 for(t=0;t<AMIPX_READREQUESTS;t++) {
  WaitIO((struct IORequest *)NIC->InputReq[t]);
  DeleteIORequest(NIC->InputReq[t]);
 }
 for(t=0;t<AMIPX_READREQUESTS*2;t++) {
  FreeVec(NIC->InputBuff[t]);
  NIC->InputBuff[t]=NULL;
 }
 for(t=0;t<AMIPX_READREQUESTS;t++) {
  FreeVec(NIC->RoutedECB[t]);
  NIC->RoutedECB[t]=NULL;
 }
  
 CloseDevice((struct IORequest *)DeviceReq);
 DeleteIORequest((struct IORequest *)DeviceReq);
 DeleteIORequest((struct IORequest *)ControlReq);
 DeleteMsgPort(Sana2Port);
 Signal(callingtask,tasksigmask); // tell AMIPX_HaltNIC that we're done
 DeleteTask(NULL);
}

void AMIPX_SendTask(void)
{
 struct AMIPX_Nic *NIC;
 struct AMIPX_Nic *tNIC;
 struct AMIPX_Library *AMIPX_Lib; 
 struct UtilityBase *UtilityBase;
 int t,t2;
 char running;
 int ptype;
 BYTE sigbit,sigbit2;
 ULONG sigmask,sigret;
 UBYTE CardNum;
 struct AMIPX_ECB *ecb;
 int datalength,frag;
 int sendprefixsize;
 int e2prefixsize=0;
 int esprefixsize=8;
 int e8022prefixsize=3;
 int e8023prefixsize=0;
 struct TagItem *mytags;
 struct MsgPort *Sana2Port;
 struct IOSana2Req *DeviceReq;
 struct IOSana2Req *ControlReq;
 struct AMIPX_PacketHeader *ph;

 geta4();

 CardNum=taskcardnum;

 UtilityBase=(struct UtilityBase *)OpenLibrary((APTR)"Utility.library",0L);

 AMIPX_Lib=task_AMIPX_Lib;

 DeviceReq=NULL;
 ControlReq=NULL;

 Sana2Port=CreateMsgPort();

 taskretval=0;

 if(Sana2Port)
  taskretval=1;

 if(taskretval) {
  mytags=AllocateTagItems((ULONG)3);
  if(mytags==NULL) {
   taskretval=0;
  }
 }
 if(taskretval) {
  mytags[0].ti_Tag=S2_CopyToBuff;
  mytags[0].ti_Data=(ULONG)CopyNothing;
  mytags[1].ti_Tag=S2_CopyFromBuff;
  mytags[1].ti_Data=(ULONG)CopyFromECBToS2;
  mytags[2].ti_Tag=TAG_DONE; 
  mytags[2].ti_Data=NULL;

  NIC=tasknic;

  ControlReq=(struct IOSana2Req *)
    CreateIORequest(Sana2Port,sizeof(struct IOSana2Req));

  DeviceReq=(struct IOSana2Req *)
    CreateIORequest(Sana2Port,sizeof(struct IOSana2Req));
  if(DeviceReq) 
   DeviceReq->ios2_BufferManagement=mytags;
 
  NIC->OutputReq=(struct IOSana2Req *)
    CreateIORequest(Sana2Port,sizeof(struct IOSana2Req));

  if(NIC->OutputReq==NULL || ControlReq==NULL || DeviceReq==NULL) 
   taskretval=0;
 }
 if(taskretval) {
  sigbit=AllocSignal(-1);
  sigbit2=AllocSignal(-1);
  if(sigbit==-1 || sigbit2==-1)
   taskretval=0;
 }
 if(taskretval) {
  if(OpenDevice(taskdevname,taskunitnumber,(struct IORequest *)DeviceReq,0L))
   taskretval=0;
 }
 FreeTagItems(mytags);
 CloseLibrary((struct Library *)UtilityBase);
 if(taskretval==0) {
  if(NIC->OutputReq)
   DeleteIORequest((struct IORequest *)NIC->OutputReq);
  if(ControlReq)
   DeleteIORequest((struct IORequest *)ControlReq);
  if(DeviceReq)
   DeleteIORequest((struct IORequest *)DeviceReq);

  NIC->OutputReq=NULL;
  Signal(callingtask,tasksigmask);
  DeleteTask(NULL);
 }
 AMIPX_initioreq(NIC,DeviceReq,ControlReq,NULL);

 for(t=0;t<SANA2_MAX_ADDR_BYTES;t++)
     ControlReq->ios2_SrcAddr[t]=NIC->Address[t];

 ControlReq->ios2_Req.io_Command=S2_CONFIGINTERFACE;
 DoIO((struct IORequest *)ControlReq);

 NIC->EcbSignalMask=1<<sigbit;
 NIC->KillOutSignalMask=1<<sigbit2;
 NIC->FirstECB=NULL;
 NIC->LastECB=NULL;

 running=1;

 Signal(callingtask,tasksigmask); // Let AMIPX_InitNIC know we're up and running

 sigmask=1<<Sana2Port->mp_SigBit;
 ObtainSemaphore(AMIPX_Lib->SendSem);
 while(running) { 
  while((ecb=NIC->FirstECB)==NULL && running) {
   ReleaseSemaphore(AMIPX_Lib->SendSem);
   SetSignal(0L,NIC->KillOutSignalMask|NIC->EcbSignalMask);
   sigret=Wait(NIC->KillOutSignalMask|NIC->EcbSignalMask); // wait for ECB
   ObtainSemaphore(AMIPX_Lib->SendSem);

   if(sigret&NIC->KillOutSignalMask) // have we been killed?
    running=0;
  }
  if(running) {
   ecb->AMIPX_Lib=AMIPX_Lib;
   ecb->Flavour=NIC->SendFlavour;
   ecb->InUse=0xfa; // processing
   AMIPX_initioreq(NIC,DeviceReq,NIC->OutputReq,ecb);
   NIC->FirstECB=ecb->NextECB;
   if(ecb->NextECB==NULL)
    NIC->LastECB=NULL;

   NIC->OutputReq->ios2_Data=ecb;
   ReleaseSemaphore(AMIPX_Lib->SendSem);

   datalength=0;

   for(frag=0;frag<ecb->FragCount;frag++) 
    datalength+=ecb->Fragment[frag].FragSize;

   ecb->Flavour=NIC->SendFlavour;

   switch(ecb->Flavour) {
    case AMIPX_PT_ETHERNET_II:
     sendprefixsize=e2prefixsize;
     break;
    case AMIPX_PT_ETHERNET_SNAP:
     sendprefixsize=esprefixsize;
     break;
    default:
    case AMIPX_PT_ETHERNET_802_2:
     sendprefixsize=e8022prefixsize;
     break;
    case AMIPX_PT_ETHERNET_802_3:
     sendprefixsize=e8023prefixsize;
     break;
   }

   for(t=0;t<SANA2_MAX_ADDR_BYTES;t++)
    NIC->OutputReq->ios2_SrcAddr[t]=NIC->Address[t];

   t=0;
   for(t=0;t<NIC->AddressSize;t++)
    NIC->OutputReq->ios2_DstAddr[t]=0;

   t2=0;

   if(NIC->AddressSize<6)  // use last AddressSize bytes (e.g. Liana)
    t2=6-NIC->AddressSize; 

   for(t=0;t2<6;t2++,t++)
    NIC->OutputReq->ios2_DstAddr[t]=ecb->ImmedAddr[t2];

   ph=(struct AMIPX_PacketHeader *)ecb->Fragment[0].FragData;
   datalength=ph->Length[0]*256+ph->Length[1];

   NIC->OutputReq->ios2_DataLength=datalength+sendprefixsize;
   NIC->OutputReq->ios2_Req.io_Command=CMD_WRITE;
   if((ecb->ImmedAddr[0]&ecb->ImmedAddr[1]&ecb->ImmedAddr[2]&
       ecb->ImmedAddr[3]&ecb->ImmedAddr[4]&ecb->ImmedAddr[5])==0xff)
    NIC->OutputReq->ios2_Req.io_Command=S2_BROADCAST;

   NIC->OutputReq->ios2_PacketType=datalength+sendprefixsize;
   if(NIC->SendFlavour == AMIPX_PT_ETHERNET_II)
    NIC->OutputReq->ios2_PacketType=33079;

   SetSignal(0L,sigmask);

   sigret=sigmask;

   if(ecb->CardNum!=CardNum) {
              // if this is not a broadcast originating from this card
    SendIO((struct IORequest *)NIC->OutputReq);
    while(running && ((sigret=Wait(NIC->KillOutSignalMask|sigmask))&sigmask)==0){
     if(sigret&NIC->KillOutSignalMask)
      running=0;
     SetSignal(0L,NIC->KillOutSignalMask|sigmask);
    }
   }
   if(sigret&sigmask) { // got a signal from the device
    if(ecb->CardNum!=CardNum) // won't have transmitted if same card received it
     WaitIO((struct IORequest *)NIC->OutputReq);

    ecb->CompletionCode=0;
    ecb->NextECB=NULL;
    if(ecb->IsBroadcast) {
     if(CardNum>=AMIPX_Lib->CardCount-1) { // if I'm the last card, it has been
      ecb->InUse=0;                       // sent on all cards.
     }
     else { // put in queue for next network card

      ObtainSemaphore(AMIPX_Lib->SendSem);

      ecb->NextECB=NULL;

      if(AMIPX_Lib->Card[CardNum+1].LastECB)
       AMIPX_Lib->Card[CardNum+1].LastECB->NextECB=ecb;

      AMIPX_Lib->Card[CardNum+1].LastECB=ecb;

      if(AMIPX_Lib->Card[CardNum+1].FirstECB==NULL)
       AMIPX_Lib->Card[CardNum+1].FirstECB=ecb;

      ecb->InUse=0xff;
      ecb->CompletionCode=0;
      ReleaseSemaphore(AMIPX_Lib->SendSem);
      Signal(AMIPX_Lib->Card[CardNum+1].OutputTask,
             AMIPX_Lib->Card[CardNum+1].EcbSignalMask);

     }
    }
    else {
     ecb->InUse=0;
    }
    if(ecb->ESR && ecb->InUse==0) { // if there is an ESR and we've finished
     ObtainSemaphore(AMIPX_Lib->ESRSem);
     if(AMIPX_Lib->ESR_Tail) {
      AMIPX_Lib->ESR_Tail->NextECB=ecb;
      AMIPX_Lib->ESR_Tail=ecb;
     }
     else {
      AMIPX_Lib->ESR_Head=ecb;
      AMIPX_Lib->ESR_Tail=ecb;
     }
     ReleaseSemaphore(AMIPX_Lib->ESRSem);
     Signal(AMIPX_Lib->ESRTask,AMIPX_Lib->ESRSignalMask); // wake up ESRTask
    }
   }
   else { // told to quit, so cancel I/O
    AbortIO((struct IORequest *)NIC->OutputReq);
    WaitIO((struct IORequest *)NIC->OutputReq);
    ecb->CompletionCode=0xfc;
    ecb->InUse=0;
   }
   ObtainSemaphore(AMIPX_Lib->SendSem);
  }
 } // while(running)
 ReleaseSemaphore(AMIPX_Lib->SendSem);
 CloseDevice((struct IORequest *)DeviceReq);
 DeleteIORequest((struct IORequest *)ControlReq);
 DeleteIORequest((struct IORequest *)DeviceReq);
 DeleteIORequest((struct IORequest *)NIC->OutputReq);
 DeleteMsgPort(Sana2Port);
 Signal(callingtask,tasksigmask); // tell AMIPX_HaltNIC that we're done
 FreeSignal(sigbit);
 DeleteTask(NULL);
}

int AMIPX_InitNIC(struct AMIPX_Library *AMIPX_Library,struct AMIPX_Nic *NIC,
                  int cardno)
{
 struct IOSana2Req *sp; 
 int t,t2,error;
 struct TagItem *mytags;
 unsigned char *qd;
 struct SysBase *SysBase;
 char in_Dst,in_Src;
 int sigbit;
 struct MsgPort *Sana2Port;
 struct IOSana2Req *DeviceReq;

 SysBase=AMIPX_Library->SysBase;

 DeviceReq=NULL;
 Sana2Port=NULL;
 tasknodeaddr=NIC->Address;

 task_AMIPX_Lib=AMIPX_Library; // for initialisation of send and listen task

 UtilityBase = (struct UtilityBase *)OpenLibrary((APTR)"utility.library",0L);
 if(UtilityBase==NULL) 
  return 0;

 for(t=0;t<sizeof(struct Hook);t++)
  ((UBYTE *)&(NIC->PacketHook))[t]=0;

 NIC->PacketHook.h_Entry=AMIPX_PacketFilter;
 NIC->PacketHook.h_SubEntry=AMIPX_PacketFilter;
 NIC->PacketHook.h_Data=NIC;
    
 mytags=AllocateTagItems((ULONG)4);
 if(mytags==NULL) {
  CloseLibrary((struct Library *)UtilityBase);
  return 0;
 }
 mytags[0].ti_Tag=S2_CopyToBuff;
 mytags[0].ti_Data=(ULONG)CopyNothing;
 mytags[1].ti_Tag=S2_CopyFromBuff;
 mytags[1].ti_Data=(ULONG)CopyNothing;
 mytags[2].ti_Tag=S2_PacketFilter;
 mytags[2].ti_Data=(ULONG)(&(NIC->PacketHook));
 mytags[3].ti_Tag=TAG_DONE; 
 mytags[3].ti_Data=NULL;

 taskcardnum=cardno;

 if((Sana2Port=CreateMsgPort())!=NULL) {
  DeviceReq=CreateIORequest(Sana2Port,sizeof(struct IOSana2Req));
  if(DeviceReq) {
   DeviceReq->ios2_BufferManagement=mytags;

/////////////////////////////////////////////////////////////////////////////
// Device has to be initially opened by a Process in order to load it into //
// memory. That's what happens here.                                       //
// Offspring will force it to remain loaded.                               //
/////////////////////////////////////////////////////////////////////////////
   if(OpenDevice((APTR)NIC->DeviceName,(ULONG)NIC->Unit,
                 (struct IORequest *)DeviceReq,0)==0) {

    NIC->FirstECB=NULL;
    NIC->LastECB=NULL;
 
    FreeTagItems(mytags);

    callingtask=FindTask(NULL);
    tasknic=NIC;
    taskdevname=NIC->DeviceName;
    taskunitnumber=NIC->Unit;

    sigbit=AllocSignal(-1); // signal for feedback from offspring
    if(sigbit!=-1) {
     tasksigmask=1<<sigbit;
     taskretval=0;  // 'return' value from child startup
     SetSignal(0L,tasksigmask);
     NIC->InputTask=CreateTask((STRPTR)NIC->InTaskName,5L,AMIPX_ListenTask,4096);
     if(NIC->InputTask) {
      Wait(tasksigmask);
      if(taskretval) {
       taskretval=0;
       SetSignal(0L,tasksigmask);
       NIC->OutputTask=CreateTask((STRPTR)NIC->OutTaskName,5L,AMIPX_SendTask,4096);
       if(NIC->OutputTask) {
        Wait(tasksigmask);
        if(taskretval) {
         CloseLibrary((struct Library *)UtilityBase);
         CloseDevice((struct IORequest *)DeviceReq); // close the device: offspring have it open  
         DeleteIORequest((struct IORequest *)DeviceReq);
         DeleteMsgPort(Sana2Port);
         FreeSignal(sigbit);
         return 1;
        }
        // else
        NIC->OutputTask=NULL;
       }
       Signal(NIC->InputTask,NIC->KillInSignalMask); // tell child to quit
       Wait(tasksigmask); // wait for child to die
      }
      NIC->InputTask=NULL;
     }
     FreeSignal(sigbit);
    }
    CloseLibrary((struct Library *)UtilityBase);
    CloseDevice((struct IORequest *)DeviceReq);
    DeleteIORequest(DeviceReq);
    DeleteMsgPort(Sana2Port);
    return 0;
   }
   else {
    DeleteIORequest(DeviceReq);
    DeleteMsgPort(Sana2Port);
    FreeTagItems(mytags);
    CloseLibrary((struct Library *)UtilityBase);
    return 0;
   }
  }
  else {
   if(DeviceReq)
    DeleteIORequest(DeviceReq);

   DeleteMsgPort(Sana2Port);
   FreeTagItems(mytags);
   CloseLibrary((struct Library *)UtilityBase);
   return 0;
  }
 }
 else {
  FreeTagItems(mytags);
  CloseLibrary((struct Library *)UtilityBase);
  return 0;
 }
}

int AMIPX_HaltNIC(struct AMIPX_Library *AMIPX_Library,struct AMIPX_Nic *NIC)
{
 struct SysBase *SysBase;
 int sigbit;

 SysBase=AMIPX_Library->SysBase;

 sigbit=AllocSignal(-1); // signal for feedback from offspring
 if(sigbit) {
  tasksigmask=1<<sigbit;
  callingtask=FindTask(NULL);
  if(NIC->InputTask) {
   Signal(NIC->InputTask,NIC->KillInSignalMask); // tell listentask to quit
   Wait(tasksigmask); // wait till listentask has cleaned up
  }
  if(NIC->OutputTask) {
   Signal(NIC->OutputTask,NIC->KillOutSignalMask);   // tell sendtask to quit
   Wait(tasksigmask); // wait till sendtask has cleaned up
  }
  FreeSignal(sigbit);
 }
 return 0;
}

unsigned long AMIPX_Atoi(char *s)
{
 unsigned long value;
 int pos;

 value=0;

 for(pos=0;s[pos]==' ';pos++)
  ;

 for(;s[pos]>='0' && s[pos]<='9';pos++) {
  value*=10;
  value+=s[pos]-'0';
 }
 return value;
}

int AMIPX_strcmp(char *,char *);

#pragma regcall(AMIPX_strcmp(A0,A1))

int AMIPX_strcmp(char *x,char *y)
{
 register int strpos;

 for(strpos=0;x[strpos]==y[strpos] && x[strpos];strpos++)
   ;

 return x[strpos]-y[strpos];
} 

struct nodeaddr_s {
 UWORD empty;
 ULONG value;
};
 
int AMIPX_Init(struct AMIPX_Library *AMIPX_Library)
{

 struct DosLibrary *DosLibrary;
 BPTR cfgfile;
 unsigned char *mybuffer;
 ULONG mybufferlength=2048; 
 ULONG flavour;
 ULONG bytesread;
 ULONG Network;
 ULONG max0hops;
 ULONG maxsockets;
 unsigned char devname[256];
 unsigned char line[256];
 unsigned char keyword[256];
 struct nodeaddr_s nodeaddr;
 unsigned char *nodeaddrp;
 int sockentry,t,t2,t3,t4,unit,netnum,card,sigbit;
 static UBYTE esprefix[8]={0xaa,0xaa,0x03,0x00,0x00,0x00,0x81,0x37};
 static UBYTE e8022prefix[3]={0xe0,0xe0,0x03};
 struct SysBase *SysBase;
 int strpos;
 int numcards,maxnet,interval,cardno;
 char OK;

 AMIPX_Library->CardCount=0;
 AMIPX_Library->QuakeFix=0;

 SysBase=AMIPX_Library->SysBase;

 nodeaddrp=NULL;

 for(t=0;t<AMIPX_RIPECBS;t++) {
  AMIPX_Library->RIPECB[t]=NULL;
  AMIPX_Library->RIPPacket[t]=NULL;
  AMIPX_Library->RIPPacketHeader[t]=NULL;
 }
 for(card=0;card<AMIPX_MAXCARDS;card++)
  for(t=0;t<10;t++)
   AMIPX_Library->Card[card].Address[t]=t;

 AMIPX_Library->SocketSem = NULL;
 AMIPX_Library->SendSem = NULL;
 AMIPX_Library->RouteSem = NULL;
 AMIPX_Library->ESRSem = NULL;
 AMIPX_Library->ESR_Head=NULL;
 AMIPX_Library->ESR_Tail=NULL;

 AMIPX_Library->SocketSem  = AllocVec(sizeof(struct SignalSemaphore),
                                      MEMF_PUBLIC|MEMF_CLEAR);

 if(AMIPX_Library->SocketSem==NULL)
  return 0;

 AMIPX_Library->SendSem  = AllocVec(sizeof(struct SignalSemaphore),
                                      MEMF_PUBLIC|MEMF_CLEAR);

 if(AMIPX_Library->SendSem==NULL) {
  FreeVec(AMIPX_Library->SocketSem);
  return 0;
 }
 AMIPX_Library->RouteSem  = AllocVec(sizeof(struct SignalSemaphore),
                                      MEMF_PUBLIC|MEMF_CLEAR);

 if(AMIPX_Library->RouteSem==NULL) {
  FreeVec(AMIPX_Library->SendSem);
  FreeVec(AMIPX_Library->SocketSem);
  return 0;
 }

 AMIPX_Library->ESRSem  = AllocVec(sizeof(struct SignalSemaphore),
                                      MEMF_PUBLIC|MEMF_CLEAR);

 if(AMIPX_Library->ESRSem==NULL) {
  FreeVec(AMIPX_Library->RouteSem);
  FreeVec(AMIPX_Library->SendSem);
  FreeVec(AMIPX_Library->SocketSem);
  return 0;
 }

 AMIPX_Library->SocketTable=NULL;
 AMIPX_Library->WaitingECB=NULL;
 AMIPX_Library->IndexSize=0;
 AMIPX_Library->SockIndex=NULL;

 AMIPX_Library->NextSocket=0x4000;    
 AMIPX_Library->RouteSearchInterval=30;
 AMIPX_Library->DontRoute=0;
 
 DosLibrary = (struct DosLibrary *)OpenLibrary((APTR)"dos.library",37L);

 AMIPX_Library->Max0Hops=0;
 mybuffer=(unsigned char *)AllocVec(mybufferlength,MEMF_PUBLIC);
 numcards=1;
 maxnet=16;
 for(cardno=0;cardno<AMIPX_MAXCARDS;cardno++) {
  sprintf(AMIPX_Library->Card[cardno].InTaskName,"amipx_lib%di",cardno);
  sprintf(AMIPX_Library->Card[cardno].OutTaskName,"amipx_lib%do",cardno);
 }
 AMIPX_Library->Card[0].Unit=0;
 for(t2=0;t2<4;t2++)
  AMIPX_Library->Card[0].NetAddress[t2]=0;

 strcpy(AMIPX_Library->Card[0].DeviceName,"devs:networks/ariadne.device");
 AMIPX_Library->Card[0].SendFlavour=AMIPX_PT_ETHERNET_802_2;

 AMIPX_Library->MaxSockets=64; // old default

 if(mybuffer) {
  cfgfile=Open((APTR)"env:amipx.prefs",MODE_OLDFILE);

  if(cfgfile) {
   bytesread=Read(cfgfile,mybuffer,mybufferlength);
   
   t2=0;
   for(t=0;t<bytesread;t++) {
    if(mybuffer[t]=='\n') {
     line[t2]='\0';

     sscanf(line,"%s",keyword);

     for(t2=0;keyword[t2];t2++)
      if(isupper(keyword[t2]))
       keyword[t2]=tolower(keyword[t2]);

     if(strcmp(keyword,"quakefix")==0)
      AMIPX_Library->QuakeFix=1;

     if(strcmp(keyword,"network")==0) {  // network number (ULONG)
      sscanf(line,"%*s%ld",&netnum);
      AMIPX_Library->Card[numcards-1].NetAddress[0]=netnum>>24 & 0xff;
      AMIPX_Library->Card[numcards-1].NetAddress[1]=netnum>>16 & 0xff;
      AMIPX_Library->Card[numcards-1].NetAddress[2]=netnum>>8  & 0xff;
      AMIPX_Library->Card[numcards-1].NetAddress[3]=netnum     & 0xff;
     }
     
     if(strcmp(keyword,"device")==0) {  // device path and name
      sscanf(line,"%*s%s",devname);
      strcpy(AMIPX_Library->Card[numcards-1].DeviceName,devname);
     }

     if(strcmp(keyword,"max0hops")==0) {     // maximum hops for Network 0
      sscanf(line,"%*s%ld",&max0hops);
      if(max0hops>=0)
       AMIPX_Library->Max0Hops=max0hops;
     }

     if(strcmp(keyword,"maxsockets")==0) {   // maximum number of sockets
      sscanf(line,"%*s%ld",&maxsockets);
      if(maxsockets>=1)
       AMIPX_Library->MaxSockets=maxsockets;
     }

     if(strcmp(keyword,"unit")==0) {     // unit number
      sscanf(line,"%*s%ld",&unit);
      if(unit>=0)
       AMIPX_Library->Card[numcards-1].Unit=unit;
     }
     if(strcmp(keyword,"node")==0) {    // node address (if no factory address)
      sscanf(line,"%*s%ld",&nodeaddr.value);
      nodeaddr.empty=0;
      nodeaddrp=(unsigned char *)&nodeaddr;
      for(t2=0;t2<sizeof(nodeaddr);t2++)
       AMIPX_Library->Card[numcards-1].Address[t2]=nodeaddrp[t2];
      for(;t2<SANA2_MAX_ADDR_BYTES;t2++)
       AMIPX_Library->Card[numcards-1].Address[t2]=0;
     }
     if(strcmp(keyword,"rsinterval")==0) { // router scan interval (secs)
      sscanf(line,"%*s%d",&interval);
      if(interval<15)
       interval=15;
      AMIPX_Library->RouteSearchInterval=interval;
     }
     if(strcmp(keyword,"maxnet")==0) {  // max number of entries in route table
      sscanf(line,"%*s%d",&maxnet);
     }
     if(strcmp(keyword,"ethernet_ii")==0) 
      AMIPX_Library->Card[numcards-1].SendFlavour=AMIPX_PT_ETHERNET_II;
     
     if(strcmp(keyword,"ethernet_802.2")==0)
      AMIPX_Library->Card[numcards-1].SendFlavour=AMIPX_PT_ETHERNET_802_2;
     
     if(strcmp(keyword,"ethernet_802.3")==0)
      AMIPX_Library->Card[numcards-1].SendFlavour=AMIPX_PT_ETHERNET_802_3;
     
     if(strcmp(keyword,"ethernet_snap")==0)
      AMIPX_Library->Card[numcards-1].SendFlavour=AMIPX_PT_ETHERNET_SNAP;

     if(strcmp(keyword,"auto")==0)
      AMIPX_Library->Card[numcards-1].SendFlavour=AMIPX_PT_AUTO;

     if(strcmp(keyword,"----")==0)
      numcards++;

     if(strcmp(keyword,"dontroute")==0)
      AMIPX_Library->DontRoute=1;

     t2=0;
    }
    else {
     line[t2]=mybuffer[t];
     if(t2<255) // don't overwrite innocent memory due to long line
      t2++;
    }
   }
   Close(cfgfile);
  }
  FreeVec(mybuffer);

  if(cfgfile) {
   AMIPX_Library->WaitingECB=(struct AMIPX_ECB **)
               AllocVec(AMIPX_Library->MaxSockets*sizeof(struct AMIPX_ECB *),
                        MEMF_PUBLIC|MEMF_CLEAR);

   if(AMIPX_Library->WaitingECB==NULL)
    cfgfile=NULL;
  }

  if(cfgfile) {
   AMIPX_Library->SocketTable=(UWORD *)
               AllocVec(AMIPX_Library->MaxSockets*sizeof(UWORD),
                        MEMF_PUBLIC|MEMF_CLEAR);
   if(AMIPX_Library->SocketTable==NULL) {
    cfgfile=NULL;
    FreeVec(AMIPX_Library->WaitingECB);
    AMIPX_Library->WaitingECB=NULL;
   }
  }
  if(cfgfile) {
   AMIPX_Library->SockIndex=(UWORD *)
               AllocVec(AMIPX_Library->MaxSockets*sizeof(UWORD),
                        MEMF_PUBLIC|MEMF_CLEAR);
   if(AMIPX_Library->SockIndex==NULL) {
    cfgfile=NULL;
    FreeVec(AMIPX_Library->SocketTable);
    AMIPX_Library->SocketTable=NULL;
    FreeVec(AMIPX_Library->WaitingECB);
    AMIPX_Library->WaitingECB=NULL;
   }
  }
	     


  if(AMIPX_Library->DontRoute)
   numcards=1;
 
  AMIPX_Library->CardCount=numcards;

  if(cfgfile!=NULL) {
   OK=1;
   for(t=0;t<AMIPX_RIPECBS && OK;t++) {
    if((AMIPX_Library->RIPECB[t]=
          AllocVec(sizeof(struct ECB2),MEMF_PUBLIC|MEMF_CLEAR))==NULL)
     OK=0;
   }
   if(OK) {
    for(t=0;t<AMIPX_RIPECBS && OK;t++) {
     if((AMIPX_Library->RIPPacket[t]=
             AllocVec(sizeof(UWORD)+50*sizeof(struct RIPEntry),
                      MEMF_PUBLIC|MEMF_CLEAR))==NULL)
      OK=0;
    }
   }
   if(OK) {
    for(t=0;t<AMIPX_RIPECBS && OK;t++) {
     if((AMIPX_Library->RIPPacketHeader[t]=
             AllocVec(30,MEMF_PUBLIC|MEMF_CLEAR))==NULL)
      OK=0;
    }
   }
   if(!OK) {
    for(t=0;t<AMIPX_RIPECBS;t++) {
     if(AMIPX_Library->RIPECB[t])
      FreeVec(AMIPX_Library->RIPECB[t]);
     if(AMIPX_Library->RIPPacket[t])
      FreeVec(AMIPX_Library->RIPPacket[t]);
     if(AMIPX_Library->RIPPacketHeader[t])
      FreeVec(AMIPX_Library->RIPPacketHeader[t]);
     AMIPX_Library->RIPPacketHeader[t]=NULL;
     AMIPX_Library->RIPPacket[t]=NULL;
     AMIPX_Library->RIPECB[t]=NULL;
    }
   }
   AMIPX_Library->CurrentRIPECB=0;
   if(!OK) {
    CloseLibrary((struct Library *)DosLibrary);
    return 0;
   }
  }

  if(cfgfile!=NULL) {
   AMIPX_Library->RouteTable[0]=(struct AMIPX_Route *)
              AllocVec(sizeof(struct AMIPX_Route)*(maxnet+numcards),
                       MEMF_PUBLIC|MEMF_CLEAR);
   AMIPX_Library->RouteIndex[0]=
      (UWORD *)AllocVec(sizeof(UWORD)*(maxnet+numcards),
                                      MEMF_PUBLIC|MEMF_CLEAR);
   AMIPX_Library->RouteTable[1]=(struct AMIPX_Route *)
              AllocVec(sizeof(struct AMIPX_Route)*(maxnet+numcards),
                       MEMF_PUBLIC|MEMF_CLEAR);
   AMIPX_Library->RouteIndex[1]=
      (UWORD *)AllocVec(sizeof(UWORD)*(maxnet+numcards),
                                      MEMF_PUBLIC|MEMF_CLEAR);
   if(AMIPX_Library->RouteTable[0]==NULL || AMIPX_Library->RouteIndex[0]==NULL
   || AMIPX_Library->RouteTable[1]==NULL || AMIPX_Library->RouteIndex[1]==NULL){
    if(AMIPX_Library->RouteIndex[0])
     FreeVec(AMIPX_Library->RouteIndex[0]);
    if(AMIPX_Library->RouteTable[0])
     FreeVec(AMIPX_Library->RouteTable[0]);
    if(AMIPX_Library->RouteIndex[1])
     FreeVec(AMIPX_Library->RouteIndex[1]);
    if(AMIPX_Library->RouteTable[1])
     FreeVec(AMIPX_Library->RouteTable[1]);
    AMIPX_Library->RouteTable[0]=NULL;
    AMIPX_Library->RouteIndex[0]=NULL;
    AMIPX_Library->RouteTable[1]=NULL;
    AMIPX_Library->RouteIndex[1]=NULL;
    cfgfile=NULL;
   }
  }
  if(cfgfile==NULL) {
   for(t=0;t<AMIPX_RIPECBS;t++) {
    if(AMIPX_Library->RIPECB[t])
     FreeVec(AMIPX_Library->RIPECB[t]);
    if(AMIPX_Library->RIPPacket[t])
     FreeVec(AMIPX_Library->RIPPacket[t]);
    if(AMIPX_Library->RIPPacketHeader[t])
     FreeVec(AMIPX_Library->RIPPacketHeader[t]);
    AMIPX_Library->RIPPacketHeader[t]=NULL;
    AMIPX_Library->RIPPacket[t]=NULL;
    AMIPX_Library->RIPECB[t]=NULL;
   }
   if(AMIPX_Library->SocketTable)
    FreeVec(AMIPX_Library->SocketTable);
   if(AMIPX_Library->SockIndex)
    FreeVec(AMIPX_Library->SockIndex);
   if(AMIPX_Library->WaitingECB)
    FreeVec(AMIPX_Library->WaitingECB);
   if(AMIPX_Library->ESRSem) 
    FreeVec(AMIPX_Library->ESRSem);
   if(AMIPX_Library->SocketSem) 
    FreeVec(AMIPX_Library->SocketSem);
   if(AMIPX_Library->SendSem) 
    FreeVec(AMIPX_Library->SendSem);
   if(AMIPX_Library->RouteSem) 
    FreeVec(AMIPX_Library->RouteSem);
   CloseLibrary((struct Library *)DosLibrary);
   return 0;
  }
 }
 else {
  if(AMIPX_Library->ESRSem) 
   FreeVec(AMIPX_Library->ESRSem);
  if(AMIPX_Library->SocketSem) 
   FreeVec(AMIPX_Library->SocketSem);
  if(AMIPX_Library->SendSem) 
   FreeVec(AMIPX_Library->SendSem);
  if(AMIPX_Library->RouteSem) 
   FreeVec(AMIPX_Library->RouteSem);
  CloseLibrary((struct Library *)DosLibrary);
  for(t=0;t<AMIPX_RIPECBS;t++) {
   if(AMIPX_Library->RIPECB[t])
    FreeVec(AMIPX_Library->RIPECB[t]);
   if(AMIPX_Library->RIPPacket[t])
    FreeVec(AMIPX_Library->RIPPacket[t]);
   if(AMIPX_Library->RIPPacketHeader[t])
    FreeVec(AMIPX_Library->RIPPacketHeader[t]);
   AMIPX_Library->RIPPacketHeader[t]=NULL;
   AMIPX_Library->RIPPacket[t]=NULL;
   AMIPX_Library->RIPECB[t]=NULL;
  }
  if(AMIPX_Library->SocketTable)
   FreeVec(AMIPX_Library->SocketTable);
  if(AMIPX_Library->SockIndex)
   FreeVec(AMIPX_Library->SockIndex);
  if(AMIPX_Library->WaitingECB)
   FreeVec(AMIPX_Library->WaitingECB);
  return 0;
 }

 InitSemaphore(AMIPX_Library->SocketSem);
 InitSemaphore(AMIPX_Library->SendSem);
 InitSemaphore(AMIPX_Library->RouteSem);
 InitSemaphore(AMIPX_Library->ESRSem);
 for(t=0;t<8;t++)
  AMIPX_Library->esprefix[t]=esprefix[t];
 for(t=0;t<3;t++)
  AMIPX_Library->e8022prefix[t]=e8022prefix[t];

 CloseLibrary((struct Library *)DosLibrary);

 AMIPX_Library->ESRTask=NULL;
 AMIPX_Library->RouteSearchTask=NULL;
 AMIPX_Library->MaxRoutes=maxnet+numcards;

 OK=1;

 for(cardno=0;cardno<numcards && OK;cardno++)
  if(!AMIPX_InitNIC(AMIPX_Library,&(AMIPX_Library->Card[cardno]),cardno))
   OK=0;

 strcpy(AMIPX_Library->ESRTaskName,"amipx_lib_esr");
 strcpy(AMIPX_Library->RSrchTaskName,"amipx_lib_rsrch");

 if(OK) {
  callingtask=FindTask(NULL);
  sigbit=AllocSignal(-1); // signal for feedback from offspring
  if(sigbit!=-1) {
   tasksigmask=1<<sigbit;
   taskretval=0;  // 'return' value from child startup
   SetSignal(0L,tasksigmask);
   AMIPX_Library->ESRTask=CreateTask((STRPTR)AMIPX_Library->ESRTaskName,0L,
                                     AMIPX_ESRTask,4096);
   if(AMIPX_Library->ESRTask) {
    Wait(tasksigmask);
    if(taskretval) {
     if(AMIPX_Library->DontRoute) {
      AMIPX_Library->RouteTableSize[0]=0;
      AMIPX_Library->RouteTableSize[1]=0;

      AMIPX_Library->ActiveRouteTable=0;
      AMIPX_Library->TempRouteTable=0;

      for(t=0;t<AMIPX_Library->MaxRoutes;t++) { // clear Route Table
       AMIPX_Library->RouteTable[0][t].Network=0xffffffffL;
       AMIPX_Library->RouteTable[1][t].Network=0xffffffffL;
      }
      for(t=0;t<AMIPX_Library->CardCount;t++) { // add direct networks
       Network=AMIPX_Library->Card[t].NetAddress[0]<<24|
               AMIPX_Library->Card[t].NetAddress[1]<<16|
               AMIPX_Library->Card[t].NetAddress[2]<<8|
               AMIPX_Library->Card[t].NetAddress[3];
       AMIPX_addroute(AMIPX_Library,Network,t,NULL,0,0);
      }
      FreeSignal(sigbit);
      return 1;
     }
     else {
      taskretval=0;  // 'return' value from child startup
      SetSignal(0L,tasksigmask);
      AMIPX_Library->RouteSearchTask=
          CreateTask((STRPTR)AMIPX_Library->RSrchTaskName,0L,
                       AMIPX_RouteSearchTask,4096);
      if(AMIPX_Library->RouteSearchTask) { // submits ecbs, so NIC must be up
       Wait(tasksigmask);
       if(taskretval) {
        FreeSignal(sigbit);
        return 1;
       }
      }
     }
     // RouteSearchTask failed to start properly
     SetSignal(0L,tasksigmask);
     Signal(AMIPX_Library->ESRTask,AMIPX_Library->KillESRSignalMask);
     Wait(tasksigmask);
    }
   }
   FreeSignal(sigbit);
  }
 }
 for(;cardno>=0;cardno--)
  AMIPX_HaltNIC(AMIPX_Library,&(AMIPX_Library->Card[cardno]));
 FreeVec(AMIPX_Library->SendSem);
 FreeVec(AMIPX_Library->SocketSem);
 FreeVec(AMIPX_Library->RouteSem);
 FreeVec(AMIPX_Library->ESRSem);
 AMIPX_Library->RouteSem=NULL;
 AMIPX_Library->SendSem=NULL;
 AMIPX_Library->SocketSem=NULL;
 AMIPX_Library->ESRSem=NULL;
 if(AMIPX_Library->SocketTable)
  FreeVec(AMIPX_Library->SocketTable);
 if(AMIPX_Library->SockIndex)
  FreeVec(AMIPX_Library->SockIndex);
 if(AMIPX_Library->WaitingECB)
  FreeVec(AMIPX_Library->WaitingECB);
 AMIPX_Library->SockIndex=NULL;
 AMIPX_Library->SocketTable=NULL;
 AMIPX_Library->WaitingECB=NULL;
 for(t=0;t<AMIPX_RIPECBS;t++) {
  if(AMIPX_Library->RIPECB[t])
   FreeVec(AMIPX_Library->RIPECB[t]);
  if(AMIPX_Library->RIPPacket[t])
   FreeVec(AMIPX_Library->RIPPacket[t]);
  if(AMIPX_Library->RIPPacketHeader[t])
   FreeVec(AMIPX_Library->RIPPacketHeader[t]);
  AMIPX_Library->RIPPacketHeader[t]=NULL;
  AMIPX_Library->RIPPacket[t]=NULL;
  AMIPX_Library->RIPECB[t]=NULL;
 }
 return 0;
}

void AMIPX_Exit(struct AMIPX_Library *AMIPX_Library)
{
 int t,sigbit;
 struct AMIPX_SRequest *rp;
 struct AMIPX_SRequest *op;
 unsigned char *qd;
 struct SysBase *SysBase;

 SysBase=AMIPX_Library->SysBase;

 for(t=AMIPX_Library->CardCount-1;t>=0;t--)
  AMIPX_HaltNIC(AMIPX_Library,&(AMIPX_Library->Card[t]));

 sigbit=AllocSignal(-1);
 callingtask=FindTask(NULL);
 if(sigbit!=-1) {
  tasksigmask=1<<sigbit;
  if(AMIPX_Library->ESRTask) {
   SetSignal(0L,tasksigmask);
   Signal(AMIPX_Library->ESRTask,AMIPX_Library->KillESRSignalMask);
   Wait(tasksigmask);
  }
  if(AMIPX_Library->RouteSearchTask) {
   SetSignal(0L,tasksigmask);
   Signal(AMIPX_Library->RouteSearchTask,AMIPX_Library->KillRouteSearchMask);
   Wait(tasksigmask);
  }
  FreeSignal(sigbit);
 }
 AMIPX_Library->ESRTask=NULL;
 AMIPX_Library->RouteSearchTask=NULL;

 if(AMIPX_Library->SocketSem) {
  FreeVec(AMIPX_Library->SocketSem);
 }
 if(AMIPX_Library->SendSem) {
   FreeVec(AMIPX_Library->SendSem);
 }
 if(AMIPX_Library->RouteSem) {
  FreeVec(AMIPX_Library->RouteSem);
 }
 if(AMIPX_Library->ESRSem) {
  FreeVec(AMIPX_Library->ESRSem);
 }
 AMIPX_Library->SocketSem=NULL;
 AMIPX_Library->SendSem=NULL;
 AMIPX_Library->RouteSem=NULL;
 AMIPX_Library->ESRSem=NULL;

 if(AMIPX_Library->RouteIndex[0])
  FreeVec(AMIPX_Library->RouteIndex[0]);
 if(AMIPX_Library->RouteTable[0])
  FreeVec(AMIPX_Library->RouteTable[0]);
 if(AMIPX_Library->RouteIndex[1])
  FreeVec(AMIPX_Library->RouteIndex[1]);
 if(AMIPX_Library->RouteTable[1])
  FreeVec(AMIPX_Library->RouteTable[1]);
 AMIPX_Library->RouteTable[0]=NULL;
 AMIPX_Library->RouteIndex[0]=NULL;
 AMIPX_Library->RouteTable[1]=NULL;
 AMIPX_Library->RouteIndex[1]=NULL;

 if(AMIPX_Library->SocketTable)
  FreeVec(AMIPX_Library->SocketTable);
 if(AMIPX_Library->SockIndex)
  FreeVec(AMIPX_Library->SockIndex);
 if(AMIPX_Library->WaitingECB)
  FreeVec(AMIPX_Library->WaitingECB);
 AMIPX_Library->SockIndex=NULL;
 AMIPX_Library->SocketTable=NULL;
 AMIPX_Library->WaitingECB=NULL;

 for(t=0;t<AMIPX_RIPECBS;t++) {
  if(AMIPX_Library->RIPECB[t])
   FreeVec(AMIPX_Library->RIPECB[t]);
  if(AMIPX_Library->RIPPacket[t])
   FreeVec(AMIPX_Library->RIPPacket[t]);
  if(AMIPX_Library->RIPPacketHeader[t])
   FreeVec(AMIPX_Library->RIPPacketHeader[t]);
  AMIPX_Library->RIPPacketHeader[t]=NULL;
  AMIPX_Library->RIPPacket[t]=NULL;
  AMIPX_Library->RIPECB[t]=NULL;
 }

 AMIPX_Library->ESR_Head=NULL;
 AMIPX_Library->ESR_Tail=NULL;
}

#pragma regcall(AMIPX_GetLocalAddr(A6,A0))
#pragma regcall(AMIPX_SendPacket(A6,A0))
#pragma regcall(AMIPX_OpenSocket(A6,D0))
#pragma regcall(AMIPX_CloseSocket(A6,D0))
#pragma regcall(AMIPX_ListenForPacket(A6,A0))
#pragma regcall(AMIPX_RelinquishControl(A6))
#pragma regcall(AMIPX_GetLocalTarget(A6,A0,A1))

UWORD AMIPX_GetLocalTarget(struct AMIPX_Library *AMIPX_Library,
			  UBYTE *internetwaddr,UBYTE *localtarget)
{
 return AMIPX_GetLocalTargetI(AMIPX_Library,internetwaddr,localtarget);
}

UWORD AMIPX_GetLocalTargetI(struct AMIPX_Library *AMIPX_Library,
			  UBYTE *internetwaddr,UBYTE *localtarget)
{
 int t;
 int indexpos,tablepos;
 int table;
 ULONG DstNet;

 struct SysBase *SysBase;

 SysBase=AMIPX_Library->SysBase;
 
 AMIPX_cleanup(AMIPX_Library);
 
/* only if on same network segment */

 DstNet=internetwaddr[0]<<24+internetwaddr[1]<<16+internetwaddr[2]<<8+
        internetwaddr[3];
 if(DstNet==0) { // local network is always successful
  for(t=0;t<6;t++)
   localtarget[t]=internetwaddr[t+4];
  return 0;
 }

 ObtainSemaphore(AMIPX_Library->RouteSem);

 table=AMIPX_Library->ActiveRouteTable;
 indexpos=AMIPX_searchroute(AMIPX_Library,table,DstNet);
 if(indexpos<AMIPX_Library->RouteTableSize[table]) {
  tablepos=AMIPX_Library->RouteIndex[table][indexpos];
  if(AMIPX_Library->RouteTable[table][tablepos].Network==DstNet) {
   for(t=0;t<6;t++)
    localtarget[t]=internetwaddr[t+4];
   if(AMIPX_Library->RouteTable[table][tablepos].Hops>0) {
    for(t=0;t<6;t++)
     localtarget[t]=AMIPX_Library->RouteTable[table][tablepos].Router[t];
   }
   ReleaseSemaphore(AMIPX_Library->RouteSem);
   return 0;
  }
 }
 ReleaseSemaphore(AMIPX_Library->RouteSem);

 return 0xfa; // unsuccessful
}

VOID AMIPX_RelinquishControl(struct AMIPX_Library *AMIPX_Library)
{
 AMIPX_RelinquishControlI(AMIPX_Library);
}

VOID AMIPX_RelinquishControlI(struct AMIPX_Library *AMIPX_Library)
{
 AMIPX_cleanup(AMIPX_Library);
}

VOID AMIPX_GetLocalAddr(struct AMIPX_Library *AMIPX_Library,
                        UBYTE *addressarray)
{
 AMIPX_GetLocalAddrI(AMIPX_Library,addressarray);
}

VOID AMIPX_GetLocalAddrI(struct AMIPX_Library *AMIPX_Library,
                        UBYTE *addressarray)
{
 int t;
 struct SysBase *SysBase;
 SysBase=AMIPX_Library->SysBase;

 AMIPX_cleanup(AMIPX_Library);
 for(t=0;t<4;t++)
  addressarray[t]=AMIPX_Library->Card[0].NetAddress[t];
 for(t=0;t<6;t++)
  addressarray[t+4]=AMIPX_Library->Card[0].CardAddress[t];
}

UWORD AMIPX_SendPacket(struct AMIPX_Library *AMIPX_Library,
                       struct AMIPX_ECB *ECB)
{
 struct AMIPX_PacketHeader *ph;
 int t;
 unsigned total;

 ph=(struct AMIPX_PacketHeader *)ECB->Fragment[0].FragData;
 if(AMIPX_Library->QuakeFix) { // fix for quake, since Length should be filled in by 'user'
  total=0;
  for(t=0;t<ECB->FragCount;t++)
   total+=ECB->Fragment[t].FragSize;

  ph->Length[0]=total>>8;
  ph->Length[1]=total&0xff;
  ph->Checksum[0]=0xff;
  ph->Checksum[1]=0xff;
 }
 AMIPX_Library->SendRequests++;
 AMIPX_Library->SentOnSocket=ECB->Socket;
 return AMIPX_SendPacketI(AMIPX_Library,ECB);
}

UWORD AMIPX_SendPacketI(struct AMIPX_Library *AMIPX_Library,
                       struct AMIPX_ECB *ECB)
{
 int t2;
 struct AMIPX_PacketHeader *ph;

 ph=(struct AMIPX_PacketHeader *)ECB->Fragment[0].FragData;

 for(t2=0;t2<4;t2++)
  ph->Src.Network[t2]=AMIPX_Library->Card[0].NetAddress[t2];

 for(t2=0;t2<6;t2++)
  ph->Src.Node[t2]=AMIPX_Library->Card[0].CardAddress[t2];

 ph->Src.Socket[0]=(ECB->Socket>>8)&0xff;
 ph->Src.Socket[1]=(ECB->Socket)   &0xff;

 ph->Tc=0;
 ECB->CardNum=0xff; // will need to be changed if more than 255 cards are supported

 return AMIPX_SendPacketR(AMIPX_Library,ECB);
}

UWORD AMIPX_SendPacketR(struct AMIPX_Library *AMIPX_Library,
                       struct AMIPX_ECB *ECB)
{
 int sendpackettype;
 int sendprefixsize;
 struct IOSana2Req *sendpacket;
 struct AMIPX_SRequest *treq;
 UBYTE *userdata;
 int t,t2,broadcast,datalength;
 int e2prefixsize=0;
 int esprefixsize=8;
 int e8022prefixsize=3;
 int e8023prefixsize=0;
 int frag,socket;
 UBYTE *sendprefix;
 struct SysBase *SysBase;
 struct AMIPX_PacketHeader *ph;
 ULONG DstNetwork;
 UBYTE DstRouter[6];
 UBYTE CardNum,OK;
 UWORD Hops;
 UWORD Ticks;
 int table,pos;

 SysBase=AMIPX_Library->SysBase;

 AMIPX_cleanup(AMIPX_Library);

 ph=(struct AMIPX_PacketHeader *)(ECB->Fragment[0].FragData);

 OK=0;

 ObtainSemaphore(AMIPX_Library->SocketSem); 
 if(AMIPX_searchsocket(AMIPX_Library,ECB->Socket)>=AMIPX_Library->IndexSize) {
  ReleaseSemaphore(AMIPX_Library->SocketSem);
  ECB->CompletionCode=0xfd;
  return 1;
 }
 ReleaseSemaphore(AMIPX_Library->SocketSem);

 DstNetwork=ph->Dst.Network[0]<<24|ph->Dst.Network[1]<<16+
            ph->Dst.Network[2]<<8|ph->Dst.Network[3];

 ECB->IsBroadcast=0;

 if(DstNetwork==0) {
  ECB->IsBroadcast=1;
  for(t2=0;t2<6;t2++)
   ECB->ImmedAddr[t2]=ph->Dst.Node[t2];
 }

 CardNum=0;

 if(!ECB->IsBroadcast) {
  ObtainSemaphore(AMIPX_Library->RouteSem);

  table=(int)AMIPX_Library->ActiveRouteTable;

  t=AMIPX_searchroute(AMIPX_Library,table,DstNetwork);

  if(t<AMIPX_Library->RouteTableSize[table]) {
   pos=AMIPX_Library->RouteIndex[table][t];
   if(AMIPX_Library->RouteTable[table][pos].Network==DstNetwork) {
    CardNum=AMIPX_Library->RouteTable[table][pos].Card;
    Hops=AMIPX_Library->RouteTable[table][pos].Hops;
    Ticks=AMIPX_Library->RouteTable[table][pos].Ticks;
    if(Hops>0) {
     for(t2=0;t2<6;t2++) 
      ECB->ImmedAddr[t2]=AMIPX_Library->RouteTable[table][pos].Router[t2];
    }
    else {
     for(t2=0;t2<6;t2++)
      ECB->ImmedAddr[t2]=ph->Dst.Node[t2];
    }

    OK=1;
   }
  }
  ReleaseSemaphore(AMIPX_Library->RouteSem);
 }
 if(OK ||ECB->IsBroadcast) {
  ObtainSemaphore(AMIPX_Library->SendSem);

  ECB->NextECB=NULL;

  if(AMIPX_Library->Card[CardNum].LastECB)
   AMIPX_Library->Card[CardNum].LastECB->NextECB=ECB;

  AMIPX_Library->Card[CardNum].LastECB=ECB;

  if(AMIPX_Library->Card[CardNum].FirstECB==NULL)
   AMIPX_Library->Card[CardNum].FirstECB=ECB;

  ECB->InUse=0xff;
  ECB->CompletionCode=0;
  ReleaseSemaphore(AMIPX_Library->SendSem);
  Signal(AMIPX_Library->Card[CardNum].OutputTask,
         AMIPX_Library->Card[CardNum].EcbSignalMask);
  AMIPX_Library->Sent++;
  return 0;
 }

 ECB->CompletionCode=0xfe; // undeliverable because we don't know the network
 ECB->InUse=0;
 return 0;
}

UWORD AMIPX_OpenSocket(struct AMIPX_Library *AMIPX_Library,
                      UWORD socknum)
{
 return AMIPX_OpenSocketI(AMIPX_Library,socknum);
}

UWORD AMIPX_OpenSocketI(struct AMIPX_Library *AMIPX_Library,
                      UWORD socknum)
{
 int t,freepos,socket;
 UWORD maxsocknum; 
 struct SysBase *SysBase;

 SysBase=AMIPX_Library->SysBase;


 AMIPX_cleanup(AMIPX_Library);

 ObtainSemaphore(AMIPX_Library->SocketSem);
 freepos=-1;
 maxsocknum=0;

 if(AMIPX_Library->IndexSize==AMIPX_Library->MaxSockets) { // i.e. table full
  ReleaseSemaphore(AMIPX_Library->SocketSem);
  return 0;
 }
 if(socknum==0) 
  socknum=AMIPX_Library->NextSocket++; // starts at 0x4000

 socket=AMIPX_searchsocket(AMIPX_Library,socknum);
 if(socket<AMIPX_Library->IndexSize) {
  if(AMIPX_Library->SocketTable[AMIPX_Library->SockIndex[socket]]
                    ==socknum) { // i.e. socket already open
   ReleaseSemaphore(AMIPX_Library->SocketSem);
   return 0;
  }
 }
 for(t=0;t<AMIPX_Library->MaxSockets && AMIPX_Library->SocketTable[t]!=0;t++)
  ;
 
 freepos=t;

 if(freepos==AMIPX_Library->MaxSockets) {
  ReleaseSemaphore(AMIPX_Library->SocketSem);
  return 0;
 }

 AMIPX_Library->SocketTable[freepos]=socknum;
 AMIPX_Library->WaitingECB[freepos]=NULL;

 for(t=0;t<socket;t++)
  AMIPX_Library->SockIndex[t]=AMIPX_Library->SockIndex[t];

 for(;t<AMIPX_Library->IndexSize;t++)
  AMIPX_Library->SockIndex[t+1]=AMIPX_Library->SockIndex[t];

 AMIPX_Library->SockIndex[socket]=freepos;

 AMIPX_Library->IndexSize++;

 ReleaseSemaphore(AMIPX_Library->SocketSem);

 return socknum;
}

VOID AMIPX_CloseSocket(struct AMIPX_Library *AMIPX_Library,
                       UWORD socknum)
{
 AMIPX_CloseSocketI(AMIPX_Library,socknum);
}

VOID AMIPX_CloseSocketI(struct AMIPX_Library *AMIPX_Library,
                       UWORD socknum)
{
 int t,socket,ipos;
 struct AMIPX_SRequest *sreq;
 struct AMIPX_SRequest *nreq;
 struct AMIPX_ECB *ecb;
 struct SysBase *SysBase;

 SysBase=AMIPX_Library->SysBase;

 AMIPX_cleanup(AMIPX_Library);

 if(socknum==0)
  return;

 ObtainSemaphore(AMIPX_Library->SocketSem);

 ipos=AMIPX_searchsocket(AMIPX_Library,socknum);
 if(ipos<AMIPX_Library->IndexSize) {
  socket=AMIPX_Library->SockIndex[ipos];
  if(AMIPX_Library->SocketTable[socket]==socknum) {
   for(t=0;t<ipos;t++)
    AMIPX_Library->SockIndex[t]=AMIPX_Library->SockIndex[t];

   for(;t<AMIPX_Library->IndexSize-1;t++)
    AMIPX_Library->SockIndex[t]=AMIPX_Library->SockIndex[t+1];
   AMIPX_Library->IndexSize--;

   AMIPX_cleanup(AMIPX_Library);

   AMIPX_Library->SocketTable[socket]=0;
   AMIPX_Library->WaitingECB[socket]=NULL;
  }
 }
 ReleaseSemaphore(AMIPX_Library->SocketSem);

 return;
}

UWORD AMIPX_ListenForPacket(struct AMIPX_Library *AMIPX_Library,
                       struct AMIPX_ECB *ECB)
{
 AMIPX_Library->ListenRequests++;
 AMIPX_Library->ListenedOnSocket=ECB->Socket;
 return AMIPX_ListenForPacketI(AMIPX_Library,ECB);
}

UWORD AMIPX_ListenForPacketI(struct AMIPX_Library *AMIPX_Library,
                       struct AMIPX_ECB *ECB)
{
 int t,t2,ts,table;
 int socknum,knownsocket,socket;
 unsigned long retval;
 struct AMIPX_ECB *tecb;
 struct AMIPX_ECB *oecb;
 struct AMIPX_Socket *sock;
 UBYTE *userpacket;
 struct AMIPX_Library *AMIPX_Lib;
 struct SysBase *SysBase;

 SysBase=AMIPX_Library->SysBase;

 AMIPX_cleanup(AMIPX_Library);

 AMIPX_Lib=AMIPX_Library;

 ObtainSemaphore(AMIPX_Library->SocketSem);
 t=AMIPX_searchsocket(AMIPX_Library,(int)ECB->Socket);
 if(t<AMIPX_Library->IndexSize) {
  socket=AMIPX_Library->SockIndex[t];
  if(AMIPX_Library->SocketTable[socket]==ECB->Socket) {
   oecb=NULL;
   for(tecb=AMIPX_Library->WaitingECB[socket];tecb;tecb=tecb->NextECB) {
    if(tecb==ECB) {/* i.e. ListenPacket called twice */
     ReleaseSemaphore(AMIPX_Library->SocketSem);
     return 0;
    }
    if(tecb->NextECB==NULL)
     oecb=tecb;
   }
   ECB->NextECB=NULL;
   ECB->AMIPX_Lib=AMIPX_Library;

   if(oecb) { // i.e. there are already ECBs in the queue
    oecb->NextECB=ECB;
   }
   else {
    AMIPX_Library->WaitingECB[socket]=ECB;
   } 
   ECB->CompletionCode=0;
   ECB->InUse=0xfe;
   AMIPX_Library->Listened++; 
   ReleaseSemaphore(AMIPX_Library->SocketSem);
/* now out of critical section */
   return 0;
  }
 }
 ReleaseSemaphore(AMIPX_Library->SocketSem);
 return 0xff;
}
