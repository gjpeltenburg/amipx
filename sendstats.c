#include <clib/exec_protos.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <stdio.h>
#include "include/amipx_priv.h"

struct AMIPX_Library *AMIPX_Library;

/*****************************************************************************
 * This program keeps AMIPX open until it is sent a Ctrl-C                   *
 * The purpose of this is two-fold:                                          *
 *  1) it keeps router searches active between closing and opening IPX       *
 *     applications, so any program will know all routers, not just after    *
 *     a certain time has passed.                                            *
 *  2) AMIPX will route packets while open, not after it is closed. Now      *
 *     routing will remain active.                                           *
 *****************************************************************************/

main()
{
 AMIPX_Library=OpenLibrary("amipx.library",0);

 if(AMIPX_Library) {
  printf("Send requests: %d\n",AMIPX_Library->SendRequests);
  printf("Most recent send request was on socket number %d\n",
         AMIPX_Library->SentOnSocket);
  printf("Successes:     %d\n",AMIPX_Library->Sent);
  printf("Listen requests: %d\n",AMIPX_Library->ListenRequests);
  printf("Most recent listen request was on socket number %d\n",
         AMIPX_Library->ListenedOnSocket);
  printf("Successes:     %d\n",AMIPX_Library->Listened);
  printf("Dropped packets: %d\n",AMIPX_Library->DroppedPackets);
  printf("Last packet received was for socket %d\n",AMIPX_Library->LastReceivedOnSocket);
  CloseLibrary(AMIPX_Library);
 }
}
