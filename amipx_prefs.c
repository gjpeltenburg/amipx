#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <exec/exec.h>
#include <exec/lists.h>
#include <clib/exec_protos.h>
#include <dos/dos.h>
#include <libraries/asl.h>
#include <clib/intuition_protos.h>
#include <clib/utility_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/asl_protos.h>
#include <clib/graphics_protos.h>
#include <pragma3/intuition_lib.h>
#include <pragma3/utility_lib.h>
#include <pragma3/gadtools_lib.h>
#include <pragma3/asl_lib.h>
#include <pragma3/graphics_lib.h>
#include <utility/tagitem.h>
#include <graphics/text.h>

#include <devices/sana2.h>

struct Library *IntuiLib;
struct IntuitionBase *IntuitionBase;
struct GadToolsBase *GadToolsBase;
struct UtilityBase *UtilityBase;
struct AslBase *AslBase;
struct Library *GfxLib;
struct GfxBase *GfxBase;
struct Window *Window;
struct Screen *Screen;
struct Task *TC;
struct IntuiMessage *IMsg;
struct MenuItem *MenuPtr;
struct Message *EMsg;
struct DosLibrary *DosLibrary;
BPTR cfgfile;
unsigned char *mybuffer;
ULONG mybufferlength=2048; 
ULONG bytesread;

#define MAXCARDS 8
// these variables will be written to the prefs file in some way
int cardcount;
ULONG flavour[MAXCARDS];
unsigned char devname[MAXCARDS][256];
int unit[MAXCARDS],netnum[MAXCARDS];
ULONG nodeaddr[MAXCARDS];
int maxnet;
int dontroute;
int interval;
int max0hops;
int maxsockets;
int quakefix;


// Network card testing stuff
struct MsgPort *sanaport;
struct Sana2DeviceQuery mydevquery;
BYTE *prefix;
int sendprefixsize;
ULONG sendpackettype;
BYTE myaddress[SANA2_MAX_ADDR_BYTES];
int  myaddresssize;
ULONG myMTU;

BOOL mybufferrw(VOID *,VOID *,ULONG);
BOOL mypacketfilterreg(struct Hook *,struct IOSana2Req *, APTR);

#pragma regcall(mybufferrw(a0,a1,d0))

BOOL mybufferrw(to,from,n)
VOID *to;
VOID *from;
ULONG n;
{
 register ULONG t;
    
// geta4();
 
 if(n>(ULONG)MAXPACKETSIZE) 
  return FALSE;
     
 for(t=0;t<n;t++)
  ((unsigned char *)to)[t]=((unsigned char *)from)[t];
     
 return TRUE;
}

int packetfiltercalled;
int packetsreceived;
    
#pragma regcall(mypacketfilterreg(a0,a2,a1))
    
BOOL mypacketfilterreg(struct Hook *packethook,struct IOSana2Req *req,
                       APTR data)
{
 int *calls;
	
 calls=(int *)packethook->h_Data;
 (*calls)++;
	
 return TRUE;
}






























struct Gadget *GadgetContext;
struct Gadget *TabGadgetList;
struct Gadget *MiscGadgetList;
struct Gadget *DirectGadgetList;
struct Gadget *RoutingGadgetList;

struct NewGadget newgad;

struct TextAttr topaz8={
 (UBYTE *)"topaz.font",
 8,
 0,0
};

struct NewWindow newwin = {
 0,0,
 500,100,
 0,
 1,
 CYCLEIDCMP|BUTTONIDCMP|STRINGIDCMP|LISTVIEWIDCMP|TEXTIDCMP,
 WINDOWDRAG|WINDOWDEPTH|GIMMEZEROZERO|ACTIVATE,
 NULL,
 NULL,
 (APTR)"AMIPX Preferences v1.24",
 NULL,
 NULL,
 0,0,
 0,0,
 WBENCHSCREEN
};
char netnumstr[MAXCARDS][62];

char e2text[]="Ethernet II";
char e8022text[]="Ethernet 802.2";
char estext[]="Ethernet 802.2 SNAP";
char e8023text[]="Ethernet 802.3";

char *labels[5]={
 e2text,
 e8022text,
 estext,
 e8023text,
 NULL
};

char *framekey[5]={
 "ETHERNET_II",
 "ETHERNET_802.2",
 "ETHERNET_SNAP",
 "ETHERNET_802.3",
 NULL
};

readprefs()
{
 unsigned char line[256];
 unsigned char keyword[256];
 int t,t2,cardnum;

 maxnet=16;
 interval=45;
 cardcount=0;
 cardnum=0;
 dontroute=0;
 max0hops=0;
 maxsockets=64;
 unit[cardnum]=0;
 netnum[cardnum]=0;
 strcpy(devname[cardnum],"");
 flavour[cardnum]=1;
 quakefix=0;
 
 mybuffer=(unsigned char *)AllocVec(mybufferlength,MEMF_PUBLIC);
 if(mybuffer) {
  cfgfile=Open("env:amipx.prefs",MODE_OLDFILE);
	 
  if(cfgfile) {
   bytesread=Read(cfgfile,mybuffer,mybufferlength);
   Close(cfgfile);
   cardcount=1; 
   t2=0;
   for(t=0;t<bytesread;t++) {
    if(mybuffer[t]=='\n') {
     line[t2]='\0';
     sscanf(line,"%s",keyword);
     for(t2=0;keyword[t2];t2++)
      if(islower(keyword[t2]))
       keyword[t2]=toupper(keyword[t2]);
			      
     if(strcmp(keyword,"NETWORK")==0) 
      sscanf(line,"%*s%lu",&netnum[cardnum]);
			           
     if(strcmp(keyword,"DEVICE")==0) 
      sscanf(line,"%*s%s",devname[cardnum]);
			           
     if(strcmp(keyword,"UNIT")==0) 
      sscanf(line,"%*s%d",&unit[cardnum]);

     if(strcmp(keyword,"NODE")==0)
      sscanf(line,"%*s%d",&nodeaddr[cardnum]);
			           
     if(strcmp(keyword,framekey[0])==0) 
      flavour[cardnum]=0;
			           
     if(strcmp(keyword,framekey[1])==0)
      flavour[cardnum]=1;
			           
     if(strcmp(keyword,framekey[2])==0)
      flavour[cardnum]=2;
			           
     if(strcmp(keyword,framekey[3])==0)
      flavour[cardnum]=3;

     if(strcmp(keyword,"DONTROUTE")==0)
      dontroute=1;

     if(strcmp(keyword,"MAXNET")==0)
      sscanf(line,"%*s%d",&maxnet);

     if(strcmp(keyword,"MAX0HOPS")==0)
      sscanf(line,"%*s%d",&max0hops);

     if(strcmp(keyword,"MAXSOCKETS")==0)
      sscanf(line,"%*s%d",&maxsockets);

     if(strcmp(keyword,"QUAKEFIX")==0)
      quakefix=1;

     if(strcmp(keyword,"RSINTERVAL")==0) {
      sscanf(line,"%*s%d",&interval);
      if(interval<15)
       interval=15;
     }

     if(strcmp(keyword,"----")==0) {
      cardnum++;
      cardcount++;
      unit[cardnum]=0;
      netnum[cardnum]=0;
      strcpy(devname[cardnum],"");
      flavour[cardnum]=1;
     }
     t2=0;
    }
    else {
     line[t2]=mybuffer[t];
     if(t2<255) // don't overwrite innocent memory due to long line
      t2++;
     
    }
   }
   FreeVec(mybuffer);
  }
 }				      
}

writeprefs(char *filename)
{
 int t;

 mybuffer=(unsigned char *)AllocVec(mybufferlength,MEMF_PUBLIC);
    
 if(mybuffer) {
  cfgfile=Open(filename,MODE_NEWFILE);
	 
  if(cfgfile) {
   if(dontroute) {
    sprintf(mybuffer,"DONTROUTE\n");
    Write(cfgfile,mybuffer,strlen(mybuffer));
   }
   if(quakefix) {
    sprintf(mybuffer,"QUAKEFIX\n");
    Write(cfgfile,mybuffer,strlen(mybuffer));
   }
   sprintf(mybuffer,"MAXNET %d\n",maxnet);
   Write(cfgfile,mybuffer,strlen(mybuffer));
   sprintf(mybuffer,"RSINTERVAL %d\n",interval);
   Write(cfgfile,mybuffer,strlen(mybuffer));
   sprintf(mybuffer,"MAX0HOPS %d\n",max0hops);
   Write(cfgfile,mybuffer,strlen(mybuffer));
   sprintf(mybuffer,"MAXSOCKETS %d\n",maxsockets);
   Write(cfgfile,mybuffer,strlen(mybuffer));
   
   for(t=0;t<cardcount;t++) {
    if(t>0) {
     sprintf(mybuffer,"----\n");
     Write(cfgfile,mybuffer,strlen(mybuffer));
    }
    sprintf(mybuffer,"NETWORK %d\nDEVICE %s\nUNIT %d\n%s\nNODE %d\n",
            netnum[t],devname[t],unit[t],framekey[flavour[t]],nodeaddr[t]);
    Write(cfgfile,mybuffer,strlen(mybuffer));
   }
   
   Close(cfgfile);
    
  }  
  FreeVec(mybuffer);
 }
}

void freenetlist(struct List *netlist)
{
 struct Node *node;
 struct Node *oldnode;

 node=netlist->lh_Head;

 while(node = RemHead(netlist)) {
  FreeVec(node);
 }
}

void buildnetlist(struct List *netlist)
{
 struct Node *node;
 int t;

 freenetlist(netlist);
 node=(struct Node *)4;
 for(t=0;t<cardcount && node;t++) {
  sprintf(netnumstr[t],"%d",netnum[t]);
  node=(struct Node *)AllocVec(sizeof(struct Node),MEMF_PUBLIC|MEMF_CLEAR);
  if(node) {
   node->ln_Name=netnumstr[t];
   node->ln_Type=254;
   node->ln_Pri=0;
   AddTail(netlist,node);
  }
 }
 if(node && t<MAXCARDS) {
  sprintf(netnumstr[t],"<end>");
  node=(struct Node *)AllocVec(sizeof(struct Node),MEMF_PUBLIC|MEMF_CLEAR);
  if(node) {
   node->ln_Name=netnumstr[t];
   node->ln_Type=254;
   node->ln_Pri=0;
   AddTail(netlist,node);
  }
 }
}
 
main()
{
 int einde; 
 ULONG Class;
 ULONG Code;
 APTR vi;
 struct Screen *WBScreen;
 struct TagItem *tags;
 struct Gadget *SaveGadget;
 struct Gadget *CancelGadget;
 struct Gadget *UseGadget;
 struct Gadget *FrameGadget;
 struct Gadget *DeviceTextGadget;
 struct Gadget *DevicePickGadget;
 struct Gadget *UnitGadget;
 struct Gadget *NetnumGadget;
 struct Gadget *NodeGadget;
 struct Gadget *InGad;
 struct Gadget *DontRouteGadget;
 struct Gadget *QuakeFixGadget;
 struct Gadget *MaxNetGadget;
 struct Gadget *Max0HopsGadget;
 struct Gadget *NICSelGadget;
 struct Gadget *NICAddGadget;
 struct Gadget *NICDelGadget;
 struct Gadget *RoutingTabGadget;
 struct Gadget *DirectTabGadget;
 struct Gadget *IntervalGadget;
 struct Gadget *MaxSocketsGadget;
 struct Gadget *PageBorder;
 struct Gadget *MiscTabGadget;
 struct Gadget *Tab1;
 struct Gadget *Tab2;
 struct Gadget *Tab3;
 struct Gadget *CurrentTab;
 struct Gadget *LastTabGadget;
 struct Gadget *TabGad1;
 struct Gadget *TabGad2;
 struct Gadget *TabGad3;
 struct Gadget *TabGad;
 struct FileRequester *filerequest;
 struct TextAttr titlefontsize;
 ULONG cardnum;
 struct List *netlist;
 int t,m_redraw;

 DosLibrary = (struct DosLibrary *)OpenLibrary((APTR)"dos.library",37L);

 IntuiLib=OpenLibrary("intuition.library",0L);

 IntuitionBase = (struct IntuitionBase *)IntuiLib;

 GfxBase = (struct GfxBase *)OpenLibrary("graphics.library",0L);

 AslBase = (struct AslBase *)OpenLibrary("asl.library",0L);

 if(AslBase==NULL) {
  CloseLibrary(DosLibrary);
  CloseLibrary(IntuiLib);
  CloseLibrary((struct Library *)GfxBase);
  return 0;
 }
 GadToolsBase = (struct GadToolsBase *)OpenLibrary("gadtools.library",0L);

 if(GadToolsBase==NULL) {
  CloseLibrary(AslBase);
  CloseLibrary(DosLibrary);
  CloseLibrary(IntuiLib);
  CloseLibrary((struct Library *)GfxBase);
  return 0;
 }
 UtilityBase = (struct UtilityBase *)OpenLibrary("utility.library",0L);
 if(UtilityBase==NULL) {
  CloseLibrary(AslBase);
  CloseLibrary(DosLibrary);
  CloseLibrary(IntuiLib);
  CloseLibrary(GadToolsBase);
  CloseLibrary((struct Library *)GfxBase);
  return 0;
 }
 netlist=(struct List *)AllocVec(sizeof(struct List),MEMF_PUBLIC|MEMF_CLEAR);
 tags=AllocateTagItems(6);
 if(tags == NULL||netlist==NULL) {
  if(netlist)
   FreeVec(netlist);
  if(tags)
   FreeTagItems(tags);
  CloseLibrary(AslBase);
  CloseLibrary(UtilityBase);
  CloseLibrary(GadToolsBase);
  CloseLibrary(IntuiLib);
  CloseLibrary(DosLibrary);
  CloseLibrary((struct Library *)GfxBase);
  return 0;
 }
 NewList(netlist);

 WBScreen=(struct Screen *)OpenWorkBench();

 AskFont(&(WBScreen->RastPort),&titlefontsize);

 newwin.Height=titlefontsize.ta_YSize+159;
 newwin.Width=504;

 Window=OpenWindow(&newwin);

 cardnum=0;

 if(Window==NULL) {
  CloseLibrary(AslBase);
  CloseLibrary(DosLibrary);
  CloseLibrary(UtilityBase);
  CloseLibrary(GadToolsBase);
  CloseLibrary(IntuiLib);
  CloseLibrary((struct Library *)GfxBase);
  return 0;
 }
 vi=NULL;

 GadgetContext=CreateContext(&TabGadgetList);
 if(GadgetContext) {
  GadgetContext=CreateContext(&MiscGadgetList);
 }
 if(GadgetContext) {
  GadgetContext=CreateContext(&DirectGadgetList);
 }
 if(GadgetContext) {
  GadgetContext=CreateContext(&RoutingGadgetList);
 }
 
 if(GadgetContext) {
  readprefs();
  buildnetlist(netlist);
  vi=GetVisualInfoA(Window->WScreen,NULL);

  tags[0].ti_Tag=TAG_DONE;
  tags[0].ti_Data=NULL;
  newgad.ng_LeftEdge=4;
  newgad.ng_TopEdge=5;
  newgad.ng_Width=100;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Interfaces";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=71;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  DirectTabGadget=CreateGadgetA(BUTTON_KIND,TabGadgetList,&newgad,tags);
  DirectTabGadget->Flags|=GFLG_GADGHNONE; // don't highlight
  tags[0].ti_Tag=TAG_DONE;
  tags[0].ti_Data=NULL;
  newgad.ng_LeftEdge=104;
  newgad.ng_TopEdge=5;
  newgad.ng_Width=100;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Routing";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=72;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  RoutingTabGadget=CreateGadgetA(BUTTON_KIND,DirectTabGadget,&newgad,tags);
  RoutingTabGadget->Flags|=GFLG_GADGHNONE; // don't highlight
  tags[0].ti_Tag=TAG_DONE;
  tags[0].ti_Data=NULL;
  newgad.ng_LeftEdge=204;
  newgad.ng_TopEdge=5;
  newgad.ng_Width=100;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Misc.";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=73;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  MiscTabGadget=CreateGadgetA(BUTTON_KIND,RoutingTabGadget,&newgad,tags);
  MiscTabGadget->Flags|=GFLG_GADGHNONE; // don't highlight
  tags[0].ti_Tag=TAG_DONE;
  tags[0].ti_Data=NULL;
  newgad.ng_LeftEdge=4;
  newgad.ng_TopEdge=138;
  newgad.ng_Width=80;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Save";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=1;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  SaveGadget=CreateGadgetA(BUTTON_KIND,MiscTabGadget,&newgad,tags);
  tags[0].ti_Tag=TAG_DONE;
  tags[0].ti_Data=NULL;
  newgad.ng_LeftEdge=412;
  newgad.ng_TopEdge=138;
  newgad.ng_Width=80;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Cancel";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=2;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  CancelGadget=CreateGadgetA(BUTTON_KIND,SaveGadget,&newgad,tags);
  tags[0].ti_Tag=TAG_DONE;
  tags[0].ti_Data=NULL;
  newgad.ng_LeftEdge=208;
  newgad.ng_TopEdge=138;
  newgad.ng_Width=80;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Use";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=8;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  UseGadget=CreateGadgetA(BUTTON_KIND,CancelGadget,&newgad,tags);
  LastTabGadget=UseGadget;
 
  tags[0].ti_Tag=GTLV_Labels;
  tags[0].ti_Data=(ULONG)netlist;
  tags[1].ti_Tag=GTLV_ShowSelected;
  tags[1].ti_Data=NULL;
  tags[2].ti_Tag=GTLV_Selected;
  tags[2].ti_Data=0;
  tags[3].ti_Tag=TAG_DONE;
  tags[3].ti_Data=NULL;
  newgad.ng_LeftEdge=8;
  newgad.ng_TopEdge=21;
  newgad.ng_Width=80;
  newgad.ng_Height=51;
  newgad.ng_GadgetText="";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=13;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  NICSelGadget=CreateGadgetA(LISTVIEW_KIND,DirectGadgetList,&newgad,tags);
  tags[0].ti_Tag=GTIN_Number;
  tags[0].ti_Data=netnum[0];
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  newgad.ng_LeftEdge=242;
  newgad.ng_TopEdge=21;
  newgad.ng_Width=50;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Network number :";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=3;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  NetnumGadget=CreateGadgetA(INTEGER_KIND,NICSelGadget,&newgad,tags);
  tags[0].ti_Tag=GTST_String;
  tags[0].ti_Data=(ULONG)devname[0];
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  newgad.ng_LeftEdge=242;
  newgad.ng_TopEdge=37;
  newgad.ng_Width=230;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Device :";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=4;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  DeviceTextGadget=CreateGadgetA(STRING_KIND,NetnumGadget,&newgad,tags);
  tags[0].ti_Tag=TAG_DONE;
  tags[0].ti_Data=NULL;
  newgad.ng_LeftEdge=474;
  newgad.ng_TopEdge=37;
  newgad.ng_Width=14;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="?";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=5;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  DevicePickGadget=CreateGadgetA(BUTTON_KIND,DeviceTextGadget,&newgad,tags);
  tags[0].ti_Tag=GTIN_Number;
  tags[0].ti_Data=unit[0];
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  newgad.ng_LeftEdge=242;
  newgad.ng_TopEdge=53;
  newgad.ng_Width=50;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Unit :";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=6;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  UnitGadget=CreateGadgetA(INTEGER_KIND,DevicePickGadget,&newgad,tags);
  tags[0].ti_Tag=GTCY_Labels;
  tags[0].ti_Data=(ULONG)labels;
  tags[1].ti_Tag=GTCY_Active;
  tags[1].ti_Data=flavour[0];
  tags[2].ti_Tag=TAG_DONE;
  tags[2].ti_Data=NULL;
  newgad.ng_LeftEdge=242;
  newgad.ng_TopEdge=69;
  newgad.ng_Width=180;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Frame type :";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=7;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  FrameGadget=CreateGadgetA(CYCLE_KIND,UnitGadget,&newgad,tags);
  tags[0].ti_Tag=GTIN_Number;
  tags[0].ti_Data=nodeaddr[0];
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  newgad.ng_LeftEdge=352;
  newgad.ng_TopEdge=21;
  newgad.ng_Width=136;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Node :";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=4;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  NodeGadget=CreateGadgetA(INTEGER_KIND,FrameGadget,&newgad,tags);
  tags[0].ti_Tag=TAG_DONE;
  tags[0].ti_Data=NULL;
  newgad.ng_LeftEdge=8;
  newgad.ng_TopEdge=69;
  newgad.ng_Width=40;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Add";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=14;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  NICAddGadget=CreateGadgetA(BUTTON_KIND,NodeGadget,&newgad,tags);
  tags[0].ti_Tag=TAG_DONE;
  tags[0].ti_Data=NULL;
  newgad.ng_LeftEdge=48;
  newgad.ng_TopEdge=69;
  newgad.ng_Width=40;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Del";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=15;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  NICDelGadget=CreateGadgetA(BUTTON_KIND,NICAddGadget,&newgad,tags);

  tags[0].ti_Tag=GTIN_Number;
  tags[0].ti_Data=maxsockets;
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  newgad.ng_LeftEdge=168;
  newgad.ng_TopEdge=21;
  newgad.ng_Width=50;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Max. # of sockets :";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=10;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  MaxSocketsGadget=CreateGadgetA(INTEGER_KIND,MiscGadgetList,&newgad,tags);
  tags[0].ti_Tag=GTCB_Checked;
  tags[0].ti_Data=quakefix;
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  newgad.ng_LeftEdge=168;
  newgad.ng_TopEdge=37;
  newgad.ng_Width=50;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Quake fix";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=12;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  QuakeFixGadget=CreateGadgetA(CHECKBOX_KIND,MaxSocketsGadget,&newgad,tags);

  tags[0].ti_Tag=GTIN_Number;
  tags[0].ti_Data=interval;
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  newgad.ng_LeftEdge=168;
  newgad.ng_TopEdge=54;
  newgad.ng_Width=50;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Search Interval :";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=10;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  IntervalGadget=CreateGadgetA(INTEGER_KIND,RoutingGadgetList,&newgad,tags);
  tags[0].ti_Tag=GTIN_Number;
  tags[0].ti_Data=max0hops;
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  newgad.ng_LeftEdge=168;
  newgad.ng_TopEdge=70;
  newgad.ng_Width=50;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Max. hops net 0 :";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=10;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  Max0HopsGadget=CreateGadgetA(INTEGER_KIND,IntervalGadget,&newgad,tags);
  tags[0].ti_Tag=GTIN_Number;
  tags[0].ti_Data=maxnet;
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  newgad.ng_LeftEdge=168;
  newgad.ng_TopEdge=38;
  newgad.ng_Width=50;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Max. Routes :";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=11;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  MaxNetGadget=CreateGadgetA(INTEGER_KIND,Max0HopsGadget,&newgad,tags);
  tags[0].ti_Tag=GTCB_Checked;
  tags[0].ti_Data=dontroute;
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  newgad.ng_LeftEdge=168;
  newgad.ng_TopEdge=22;
  newgad.ng_Width=50;
  newgad.ng_Height=14;
  newgad.ng_GadgetText="Disable routing";
  newgad.ng_TextAttr=&topaz8;
  newgad.ng_GadgetID=12;
  newgad.ng_Flags=0;
  newgad.ng_VisualInfo=vi;
  newgad.ng_UserData=NULL;
  DontRouteGadget=CreateGadgetA(CHECKBOX_KIND,MaxNetGadget,&newgad,tags);

  Tab1=DirectGadgetList;
  TabGad1=DirectTabGadget;
  Tab2=RoutingGadgetList;
  TabGad2=RoutingTabGadget;
  Tab3=MiscGadgetList;
  TabGad3=MiscTabGadget;
  AddGList(Window,TabGadgetList,-1,-1,NULL);
  CurrentTab=Tab1;
  TabGad=TabGad1;
  AddGList(Window,CurrentTab,-1,-1,NULL);
  tags[0].ti_Tag=GT_VisualInfo;
  tags[0].ti_Data=(ULONG)vi;
  tags[1].ti_Tag=TAG_DONE;
  tags[1].ti_Data=NULL;
  RefreshGadgets(TabGadgetList,Window,NULL);
  GT_RefreshWindow(Window,NULL);

  einde=0;
  m_redraw=1;

  while(!einde) {
   if(m_redraw) {
    tags[0].ti_Tag=GT_VisualInfo;
    tags[0].ti_Data=(ULONG)vi;
    tags[1].ti_Tag=TAG_DONE;
    tags[1].ti_Data=NULL;
    DrawBevelBoxA(Window->RPort,4,18,488,118,tags);
    GT_BeginRefresh(Window);    // force gadget refresh
    GT_EndRefresh(Window,TRUE);
    EraseRect(Window->RPort,
              TabGad->LeftEdge+2,18,
              TabGad->Width+TabGad->LeftEdge-2,18);
    m_redraw=0;
   }
   WaitPort(Window->UserPort);
   IMsg=GT_GetIMsg(Window->UserPort);
   if(IMsg) {
    InGad=(struct Gadget *)IMsg->IAddress;
    Class=IMsg->Class;
    Code=IMsg->Code;
    switch(Class) {
     case IDCMP_REFRESHWINDOW:
      GT_ReplyIMsg(IMsg);
      m_redraw=1;
      break;
     case IDCMP_GADGETUP:
      switch(InGad->GadgetID) {
       case 71:
       case 72:
       case 73:
        GT_ReplyIMsg(IMsg);
        RemoveGList(Window,CurrentTab,-1); // Remove all page-bound gadgets 
        LastTabGadget->NextGadget=NULL;    // pre v35
        EraseRect(Window->RPort,6,19,488,134);
        switch(InGad->GadgetID) {
         case 71:
          CurrentTab = Tab1;
          TabGad = TabGad1;
          break;
         case 72:
          CurrentTab = Tab2;
          TabGad = TabGad2;
          break;
         case 73:
          CurrentTab = Tab3;
          TabGad = TabGad3;
          break;
         default:
          break;
        }
        AddGList(Window,CurrentTab,-1,-1,NULL); // Add gadgets from page
        RefreshGadgets(CurrentTab,Window,NULL); // redraw
        GT_RefreshWindow(Window,NULL);
        m_redraw=1;
        break;
       case 5:
        GT_ReplyIMsg(IMsg);
        tags[0].ti_Tag=ASL_Dir;
        tags[0].ti_Data=(ULONG)"DEVS:Networks";
        tags[1].ti_Tag=TAG_DONE;
        tags[1].ti_Data=NULL;
        filerequest=AllocAslRequest(ASL_FileRequest,tags);
        if(filerequest) {
         if(AslRequest(filerequest,NULL)) {
          strcpy(devname[cardnum],filerequest->fr_Drawer);
          AddPart(devname[cardnum],filerequest->fr_File,256);
          FreeAslRequest(filerequest);
          tags[0].ti_Tag=GTST_String;
	  tags[0].ti_Data=(ULONG)devname[cardnum];
	  GT_SetGadgetAttrsA(DeviceTextGadget,Window,NULL,tags);
          GT_RefreshWindow(Window,NULL);
          m_redraw=1;
         }
         else
          FreeAslRequest(filerequest);
        }
        break;
       case 13: // listview
       case 14: // add
       case 15: // del
        GT_ReplyIMsg(IMsg);
        
        netnum[cardnum]=((struct StringInfo *)NetnumGadget->SpecialInfo)->LongInt;
 	unit[cardnum]=((struct StringInfo *)UnitGadget->SpecialInfo)->LongInt;
        strcpy(devname[cardnum],
                 ((struct StringInfo *)DeviceTextGadget->SpecialInfo)->Buffer);
  	nodeaddr[cardnum]=((struct StringInfo *)NodeGadget->SpecialInfo)->LongInt;

        tags[0].ti_Tag=GTCY_Active;
        tags[0].ti_Data=(ULONG)&flavour[cardnum];
        tags[1].ti_Tag=TAG_DONE;
        tags[1].ti_Data=0;
        GT_GetGadgetAttrsA(FrameGadget,Window,NULL,tags);

        if(InGad->GadgetID==13) {
         tags[0].ti_Tag=GTLV_Selected;
         tags[0].ti_Data=(ULONG)&cardnum;
         tags[1].ti_Tag=TAG_DONE;
         tags[1].ti_Data=0;
         GT_GetGadgetAttrsA(NICSelGadget,Window,NULL,tags);
         if(cardnum<0)
          cardnum=0;
	}

        switch(InGad->GadgetID) {
         case 13:
          tags[0].ti_Tag=GTST_String;
          tags[0].ti_Data=(ULONG)devname[cardnum];
          GT_SetGadgetAttrsA(DeviceTextGadget,Window,NULL,tags);
          tags[0].ti_Tag=GTIN_Number;
          tags[0].ti_Data=(ULONG)unit[cardnum];
          GT_SetGadgetAttrsA(UnitGadget,Window,NULL,tags);
          tags[0].ti_Tag=GTCY_Active;
          tags[0].ti_Data=(ULONG)flavour[cardnum];
          GT_SetGadgetAttrsA(FrameGadget,Window,NULL,tags);
          tags[0].ti_Tag=GTIN_Number;
          tags[0].ti_Data=(ULONG)nodeaddr[cardnum];
          GT_SetGadgetAttrsA(NodeGadget,Window,NULL,tags);
          tags[0].ti_Tag=GTIN_Number;
          tags[0].ti_Data=(ULONG)netnum[cardnum];
          GT_SetGadgetAttrsA(NetnumGadget,Window,NULL,tags);
          break;
	 case 14: // Add
          if(cardcount<MAXCARDS) {
           for(t=cardcount;t>cardnum;t--) {
            strcpy(devname[t],devname[t-1]);
            unit[t]=unit[t-1];
            flavour[t]=flavour[t-1];
            netnum[t]=netnum[t-1];
            nodeaddr[t]=nodeaddr[t-1];
	   }
           strcpy(devname[cardnum],"");
           unit[cardnum]=0;
           flavour[cardnum]=0;
           netnum[cardnum]=0;
           nodeaddr[cardnum]=0;
           cardcount++;
           tags[0].ti_Tag=GTST_String;
           tags[0].ti_Data=(ULONG)devname[cardnum];
           GT_SetGadgetAttrsA(DeviceTextGadget,Window,NULL,tags);
           tags[0].ti_Tag=GTIN_Number;
           tags[0].ti_Data=(ULONG)unit[cardnum];
           GT_SetGadgetAttrsA(UnitGadget,Window,NULL,tags);
           tags[0].ti_Tag=GTCY_Active;
           tags[0].ti_Data=(ULONG)flavour[cardnum];
           GT_SetGadgetAttrsA(FrameGadget,Window,NULL,tags);
           tags[0].ti_Tag=GTIN_Number;
           tags[0].ti_Data=(ULONG)nodeaddr[cardnum];
           GT_SetGadgetAttrsA(NodeGadget,Window,NULL,tags);
           tags[0].ti_Tag=GTIN_Number;
           tags[0].ti_Data=(ULONG)netnum[cardnum];
           GT_SetGadgetAttrsA(NetnumGadget,Window,NULL,tags);
	  }
          break;
         case 15: // Del
          if(cardcount>1 && cardnum<cardcount) {
           for(t=cardnum;t<cardcount-1;t++) {
            strcpy(devname[t],devname[t+1]);
            unit[t]=unit[t+1];
            flavour[t]=flavour[t+1];
            netnum[t]=netnum[t+1];
            nodeaddr[t]=nodeaddr[t+1];
           }
           cardcount--;
           strcpy(devname[cardcount],"");
           unit[cardcount]=0;
           flavour[cardcount]=0;
           netnum[cardcount]=0;
           nodeaddr[cardcount]=0;
           if(cardnum>=cardcount)
            cardnum--;
           tags[0].ti_Tag=GTST_String;
           tags[0].ti_Data=(ULONG)devname[cardnum];
           GT_SetGadgetAttrsA(DeviceTextGadget,Window,NULL,tags);
           tags[0].ti_Tag=GTIN_Number;
           tags[0].ti_Data=(ULONG)unit[cardnum];
           GT_SetGadgetAttrsA(UnitGadget,Window,NULL,tags);
           tags[0].ti_Tag=GTCY_Active;
           tags[0].ti_Data=(ULONG)flavour[cardnum];
           GT_SetGadgetAttrsA(FrameGadget,Window,NULL,tags);
           tags[0].ti_Tag=GTIN_Number;
           tags[0].ti_Data=(ULONG)nodeaddr[cardnum];
           GT_SetGadgetAttrsA(NodeGadget,Window,NULL,tags);
           tags[0].ti_Tag=GTIN_Number;
           tags[0].ti_Data=(ULONG)netnum[cardnum];
           GT_SetGadgetAttrsA(NetnumGadget,Window,NULL,tags);
	  }
          break;
	}
       case 3:
        if(InGad->GadgetID==3) {
         GT_ReplyIMsg(IMsg);
         netnum[cardnum]=((struct StringInfo *)NetnumGadget->SpecialInfo)->LongInt;
	}
        buildnetlist(netlist);
        tags[0].ti_Tag=GTLV_Selected;
        tags[0].ti_Data=cardnum;
        tags[1].ti_Tag=GTLV_Labels;
        tags[1].ti_Data=(ULONG)netlist;
        tags[2].ti_Tag=TAG_DONE;
        tags[2].ti_Data=0;
        GT_SetGadgetAttrsA(NICSelGadget,Window,NULL,tags);
        GT_RefreshWindow(Window,NULL);
        m_redraw=1;
	break;
       case 4:
       case 6:
       case 7:
        GT_ReplyIMsg(IMsg);
        break;
       case 8:
       case 1:
        GT_ReplyIMsg(IMsg);
        einde=1;
        
        max0hops=((struct StringInfo *)Max0HopsGadget->SpecialInfo)->LongInt;
        netnum[cardnum]=((struct StringInfo *)NetnumGadget->SpecialInfo)->LongInt;
 	unit[cardnum]=((struct StringInfo *)UnitGadget->SpecialInfo)->LongInt;
        strcpy(devname[cardnum],
                 ((struct StringInfo *)DeviceTextGadget->SpecialInfo)->Buffer);

        tags[0].ti_Tag=GTCY_Active;
        tags[0].ti_Data=(ULONG)&flavour[cardnum];
        tags[1].ti_Tag=TAG_DONE;
        tags[1].ti_Data=0;
        GT_GetGadgetAttrsA(FrameGadget,Window,NULL,tags);
        tags[0].ti_Tag=GTCB_Checked;
        tags[0].ti_Data=(ULONG)&dontroute;
        tags[1].ti_Tag=TAG_DONE;
        tags[1].ti_Data=0;
        GT_GetGadgetAttrsA(DontRouteGadget,Window,NULL,tags);
        tags[0].ti_Tag=GTCB_Checked;
        tags[0].ti_Data=(ULONG)&quakefix;
        tags[1].ti_Tag=TAG_DONE;
        tags[1].ti_Data=0;
        GT_GetGadgetAttrsA(QuakeFixGadget,Window,NULL,tags);
        
  	nodeaddr[cardnum]=((struct StringInfo *)NodeGadget->SpecialInfo)->LongInt;
        interval=((struct StringInfo *)IntervalGadget->SpecialInfo)->LongInt;
        maxnet=((struct StringInfo *)MaxNetGadget->SpecialInfo)->LongInt;
        maxsockets=((struct StringInfo *)MaxSocketsGadget->SpecialInfo)->LongInt;

        if(InGad->GadgetID==1) 
         writeprefs("envarc:amipx.prefs");

        writeprefs("env:amipx.prefs");
        break;
       case 2:
        einde=1;
        GT_ReplyIMsg(IMsg);
        break;
      }
      break;
     default:
      GT_ReplyIMsg(IMsg);
      break;
    }
   }
  }
  CloseWindow(Window);
  freenetlist(netlist);
  FreeTagItems(tags);
  FreeGadgets(MiscGadgetList);
  FreeGadgets(DirectGadgetList);
  FreeGadgets(RoutingGadgetList);
 }
 CloseLibrary(AslBase);
 CloseLibrary(UtilityBase);
 CloseLibrary(GadToolsBase);
 CloseLibrary(IntuiLib);
 CloseLibrary(DosLibrary);
 CloseLibrary((struct Library *)GfxBase);
 return 0;
}
