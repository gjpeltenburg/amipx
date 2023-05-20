#ifndef _AMIPX_PRAGMAS_
#define _AMIPX_PRAGMAS_

#ifdef AZTEC_C
#pragma amicall(AMIPX_Library, 0x1e, AMIPX_OpenSocket(d0))
#pragma amicall(AMIPX_Library, 0x24, AMIPX_CloseSocket(d0))
#pragma amicall(AMIPX_Library, 0x2a, AMIPX_ListenForPacket(a0))
#pragma amicall(AMIPX_Library, 0x30, AMIPX_SendPacket(a0))
#pragma amicall(AMIPX_Library, 0x36, AMIPX_GetLocalAddr(a0))
#pragma amicall(AMIPX_Library, 0x3c, AMIPX_RelinquishControl())
#pragma amicall(AMIPX_Library, 0x42, AMIPX_GetLocalTarget(a0,a1))
#endif

#ifdef __SASC
#pragma libcall AMIPX_Library AMIPX_OpenSocket 1e 001
#pragma libcall AMIPX_Library AMIPX_CloseSocket 24 001
#pragma libcall AMIPX_Library AMIPX_ListenForPacket 2a 801
#pragma libcall AMIPX_Library AMIPX_SendPacket 30 801
#pragma libcall AMIPX_Library AMIPX_GetLocalAddr 36 801
#pragma libcall AMIPX_Library AMIPX_RelinquishControl 3c 0
#pragma libcall AMIPX_Library AMIPX_GetLocalTarget 42 9802
#endif

#endif
