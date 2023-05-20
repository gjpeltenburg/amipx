#ifndef _AMIPX_PRAGMAS_
#define _AMIPX_PRAGMAS_
#pragma amicall(AMIPX_Library, 0x1e, AMIPX_OpenSocket(a6,d0))
#pragma amicall(AMIPX_Library, 0x24, AMIPX_CloseSocket(a6,d0))
#pragma amicall(AMIPX_Library, 0x2a, AMIPX_ListenForPacket(a6,a0))
#pragma amicall(AMIPX_Library, 0x30, AMIPX_SendPacket(a6,a0))
#pragma amicall(AMIPX_Library, 0x36, AMIPX_GetLocalAddr(a6,a0))
#pragma amicall(AMIPX_Library, 0x3c, AMIPX_RelinquishControl(a6))
#pragma amicall(AMIPX_Library, 0x42, AMIPX_GetLocalTarget(a6,a0,a1))
#endif
