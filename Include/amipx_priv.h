/* include this file along with amipx_protos.h and amipx_pragmas.h,
   if you need them
*/
#ifndef _AMIPX_H_
#define _AMIPX_H_

#define AMIPX_PT_AUTO           0
#define AMIPX_PT_ETHERNET_II    1
#define AMIPX_PT_ETHERNET_SNAP  2
#define AMIPX_PT_ETHERNET_802_2 3
#define AMIPX_PT_ETHERNET_802_3 4

#include <devices/sana2.h>
#include <exec/libraries.h>
#include <exec/semaphores.h>
#include <utility/hooks.h>

#define AMIPX_QSIZE 8
#define AMIPX_READREQUESTS 4 
#define AMIPX_MAXCARDS 8
#define AMIPX_MAXROUTES 16
#define AMIPX_RIPPORT 0x0453 
#define AMIPX_RIPECBS 4

struct AMIPX_Address {
 UBYTE Network[4];
 UBYTE Node[6];
 UBYTE Socket[2];
};

struct AMIPX_SPX_Status {
 UBYTE State;
 UBYTE WatchDog;
 UWORD SrcConnID;
 UWORD DstConnID;
 UWORD SendSequence;
 UWORD ReceiveSequence;
 UWORD MaxReceiveSeqNoAck;
 UWORD AcknowledgedSequence;
 UWORD MaxSendSeqNoAck;
 UWORD Socket;
 UBYTE Bridge[6];
 struct AMIPX_Address Dst;
 UWORD Retransmits;
 UWORD RoundTrip;
 UWORD RetransmittedPackets;
 UWORD SuppressedPackets;
};

struct AMIPX_Route {
 ULONG Network;            /* Destination network                    */
 UWORD Ticks;              /* Number of ticks reported by the router */
 UWORD Hops;               /* Number of hops reported by the router  */
 UBYTE Card;               /* Network card to send with              */
 UBYTE Router[6];          /* Node to send to                        */
};
/* Special case: if Hops=0, the destination address is used instead of the
   router address (hops=0 means on the same segment)
*/

struct AMIPX_PacketHeader {
 UBYTE Checksum[2];
 UBYTE Length[2];
 UBYTE Tc;
 UBYTE Type;
 struct AMIPX_Address Dst;
 struct AMIPX_Address Src;
};

struct AMIPX_Fragment {
 BYTE *FragData;
 UWORD FragSize;
};

struct AMIPX_ECB {
 APTR Link;
 APTR ESR;
 UBYTE InUse;
 UBYTE CompletionCode; /* non-zero in case of error */
 UWORD Socket;
 struct AMIPX_ECB *NextECB;      // private - next ecb in list
 struct AMIPX_Library *AMIPX_Lib;// private 
 UBYTE CardNum;                  // private - Card ECB was received on
 UBYTE Flavour;                  // private - sendflavour at time of send
 UBYTE IsBroadcast;		 // private - broadcast on all cards
 UBYTE Reserved[5];
 UBYTE ImmedAddr[6];
 UWORD FragCount;
 struct AMIPX_Fragment Fragment[1]; // first fragment - more than one allowed
};

/* all of this is private */


struct AMIPX_Nic {
 int AddressSize;
 unsigned char DeviceName[128];
 unsigned char InTaskName[16];
 unsigned char OutTaskName[16];
 UBYTE Unit;
 UBYTE Address[SANA2_MAX_ADDR_BYTES];
 UBYTE NetAddress[4];
 UBYTE CardAddress[6];
 ULONG SendFlavour;
 struct Sana2DeviceQuery DevQuery;
 ULONG Speed;
 ULONG MTU;
 UBYTE InputBuffUse[AMIPX_READREQUESTS]; // selects which buffer to use
 UBYTE *InputBuff[AMIPX_READREQUESTS*2]; // one for Listen, one for Router
 struct IOSana2Req *InputReq[AMIPX_READREQUESTS];
 struct AMIPX_ECB *RoutedECB[AMIPX_READREQUESTS]; // for routing (send ECBs) 
 char InputReqP;
 struct AMIPX_ECB *FirstECB;
 struct AMIPX_ECB *LastECB;
 struct IOSana2Req *OutputReq;
 struct Task *InputTask;
 struct Task *OutputTask;
 ULONG EcbSignalMask;
 ULONG KillInSignalMask;
 ULONG KillOutSignalMask;
 struct Hook PacketHook;
};


struct AMIPX_Library {                  
 struct Library ml_Lib;
 unsigned long ml_SegList;
 unsigned long SendRequests;     // Number of times SendPacket was called
 UWORD SentOnSocket;             // Most recently used socket
 unsigned long Sent;             // Number of packets put in the transmit queues
 unsigned long ListenRequests;   
 UWORD ListenedOnSocket;         
 unsigned long Listened;
 unsigned long DroppedPackets;   // packets dropped because they weren't IPX
 UWORD LastReceivedOnSocket;    
 int CardCount;
 struct AMIPX_Nic Card[AMIPX_MAXCARDS]; 
 unsigned char RSrchTaskName[16];
 unsigned char ESRTaskName[16]; 
 int RouteTableSize[2]; 
 int Max0Hops;                   // max # of hops for Network 0 
 int MaxRoutes;                  // user configurable
 int MaxSockets;                 // user configurable - defaults to 64
 struct AMIPX_Route *RouteTable[2];
 UWORD *RouteIndex[2];
 UBYTE ActiveRouteTable;
 UBYTE TempRouteTable;
 struct SignalSemaphore *RouteSem;
 struct AMIPX_PacketHeader *RIPPacketHeader[AMIPX_RIPECBS];
 UBYTE *RIPPacket[AMIPX_RIPECBS];
 struct AMIPX_ECB *RIPECB[AMIPX_RIPECBS];
 int CurrentRIPECB;
 ULONG KillRouteSearchMask;
 ULONG RouteSearchInterval;
 struct Task *RouteSearchTask;
 struct Task *ESRTask;
 ULONG ESRSignalMask;
 ULONG KillESRSignalMask;
 struct AMIPX_ECB *ESR_Head;
 struct AMIPX_ECB *ESR_Tail;
 struct SignalSemaphore *ESRSem;
 UWORD IndexSize;
 UWORD *SockIndex;              // <-- MaxSockets UWORDs
 UWORD *SocketTable;            // <-- MaxSockets UWORDs
 struct AMIPX_ECB **WaitingECB; // <-- MaxSockets lists
 struct SignalSemaphore *SendSem;
 UBYTE e8022prefix[3];
 UBYTE DontRoute;
 UBYTE esprefix[8];
 UWORD NextSocket;
 UBYTE Inited;
 UBYTE QuakeFix;
 APTR SysBase;
 struct SignalSemaphore *SocketSem;
 struct AMIPX_ECB *RouteSearchECB;
 UBYTE *RouteSearchHeader;
};
   


#endif
