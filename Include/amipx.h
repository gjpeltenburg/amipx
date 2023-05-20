/* include this file along with amipx_protos.h and amipx_pragmas.h,
   if you need them
*/
#ifndef _AMIPX_H_
#define _AMIPX_H_


#include <exec/libraries.h>

struct AMIPX_Address {
 UBYTE Network[4];
 UBYTE Node[6];
 UWORD Socket;
};


/* Beware of the UWORD fields in the packet header: they have the same 
   byte ordering as the Amiga and must NOT be byte-swapped. PC's must do that.
*/
struct AMIPX_PacketHeader {
 UWORD Checksum;
 UWORD Length;
 UBYTE Tc;
 UBYTE Type;
 struct AMIPX_Address Dst;
 struct AMIPX_Address Src;
};

struct AMIPX_Fragment {
 UBYTE *FragData;
 UWORD FragSize;
};


/* Note that you may define an ECB with any number of fragments */

struct AMIPX_ECB {
 APTR Link;             /* Amipx does not use this   */
 APTR ESR;
 UBYTE InUse;
 UBYTE CompletionCode;   /* non-zero in case of error */
 UWORD Socket;
 UBYTE IPXWork[4];	/* private! */
 UBYTE DWork[12];        /* private! */
 UBYTE ImmedAddr[6];
 UWORD FragCount;
 struct AMIPX_Fragment Fragment[1]; // first fragment - more than one allowed
};


struct AMIPX_Library {                  
 struct Library ml_Lib;
};
   


#endif
