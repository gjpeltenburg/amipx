/* include this file along with amipx_protos.h and amipx_pragmas.h,
   if you need them
*/
#ifndef _AMIPX_PROTOS_
#define _AMIPX_PROTOS_

WORD AMIPX_OpenSocket(UWORD socknum);
VOID AMIPX_CloseSocket(UWORD socknum);
WORD AMIPX_ListenForPacket(struct AMIPX_ECB *ECB);
WORD AMIPX_SendPacket(struct AMIPX_ECB *ECB);
VOID AMIPX_GetLocalAddr(UBYTE addressarray[10]);
WORD AMIPX_GetLocalTarget(UBYTE addressarray[12],UBYTE localtarget[6]);
VOID AMIPX_RelinquishControl(void);
#endif
