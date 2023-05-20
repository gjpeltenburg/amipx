#include <clib/exec_protos.h>
#include <exec/tasks.h>
#include <dos/dos.h>

struct Library *AMIPX_Library;

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
  Wait(SIGBREAKF_CTRL_C);
  CloseLibrary(AMIPX_Library);
 }
}
