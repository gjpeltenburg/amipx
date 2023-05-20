/* include this file along with amipx_protos.h and amipx_pragmas.h,
   if you need them
*/
#ifndef _AMIPX_PROTOS_
#define _AMIPX_PROTOS_

UWORD AMIPX_OpenSocket(struct AMIPX_Library *AMIPX_Library,UWORD socknum);
VOID AMIPX_CloseSocket(struct AMIPX_Library *AMIPX_Library,UWORD socknum);
UWORD AMIPX_ListenForPacket(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_SendPacket(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
VOID AMIPX_GetLocalAddr(struct AMIPX_Library *AMIPX_Library,UBYTE addressarray[10]); 
VOID AMIPX_RelinquishControl(struct AMIPX_Library *AMIPX_Library);
UWORD AMIPX_GetLocalTarget(struct AMIPX_Library *AMIPX_Library,UBYTE *,UBYTE *);
// Available from version 2
UWORD AMIPX_GetIPXFlags(struct AMIPX_Library *AMIPX_Library);
UWORD AMIPX_SendPacketCS(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
VOID AMIPX_GenerateCS(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_VerifyCS(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
VOID AMIPX_ScheduleIPX(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
VOID AMIPX_CancelIPX(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
VOID AMIPX_ScheduleSpecial(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_GetIntervalMarker(struct AMIPX_Library *AMIPX_Library);
UWORD AMIPX_Disconnect(struct AMIPX_Library *AMIPX_Library,UBYTE *Dst);

UWORD AMIPX_CheckForSPX(struct AMIPX_Library *AMIPX_Library);
ULONG AMIPX_ConnectSPX(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_ListenSPXConn(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_TerminateSPX(struct AMIPX_Library *AMIPX_Library,UWORD connection);
UWORD AMIPX_AbortSPX(struct AMIPX_Library *AMIPX_Library,UWORD connection);
UWORD AMIPX_GetSPXStatus(struct AMIPX_Library *AMIPX_Library,struct AMIPX_SPX_Status *status);
UWORD AMIPX_ListenForSPX(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_SendSPX(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
ULONG AMIPX_ConnectSPX(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_OpenLASocket(struct AMIPX_Library *AMIPX_Library,UWORD socknum,APTR func);

// Internal versions because Aztec C fails to use local AMIPX_Library as base
UWORD AMIPX_OpenSocketI(struct AMIPX_Library *AMIPX_Library,UWORD socknum);
VOID AMIPX_CloseSocketI(struct AMIPX_Library *AMIPX_Library,UWORD socknum);
UWORD AMIPX_ListenForPacketI(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_SendPacketI(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_SendPacketR(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
VOID AMIPX_GetLocalAddrI(struct AMIPX_Library *AMIPX_Library,UBYTE addressarray[10]); 
VOID AMIPX_RelinquishControlI(struct AMIPX_Library *AMIPX_Library);
UWORD AMIPX_GetLocalTargetI(struct AMIPX_Library *AMIPX_Library,UBYTE *,UBYTE *);
// Available from version 2
UWORD AMIPX_CheckForSPXI(struct AMIPX_Library *AMIPX_Library);
ULONG AMIPX_ConnectSPXI(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_ListenSPXConnI(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_TerminateSPXI(struct AMIPX_Library *AMIPX_Library,UWORD connection);
UWORD AMIPX_AbortSPXI(struct AMIPX_Library *AMIPX_Library,UWORD connection);
UWORD AMIPX_GetSPXStatusI(struct AMIPX_Library *AMIPX_Library,struct AMIPX_SPX_Status *status);
UWORD AMIPX_ListenForSPXI(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_SendSPXI(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
ULONG AMIPX_ConnectSPXI(struct AMIPX_Library *AMIPX_Library,struct AMIPX_ECB *ECB);
UWORD AMIPX_OpenLASocketI(struct AMIPX_Library *AMIPX_Library,UWORD socknum,APTR func);
#endif
