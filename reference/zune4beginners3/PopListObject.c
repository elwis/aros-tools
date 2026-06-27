#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/alib.h>
#include <utility/hooks.h>

#define SAVEDS
#define ASM
static struct Hook myhook_poplist;

static char *cc[] = { "allow all", "allow only numbers", "allow only letters", NULL};

Object *objpop;
Object *inputbox;
Object *app,*window;
    
static ULONG func_poplist_select_item(struct Hook *hook, Object *object, APTR msg)
{
  char *selected_item_from_list = XGET(objpop, MUIA_String_Contents);
  
  // set filter for inputbox (based on selected string on poplist object)
  if (!strcmp(selected_item_from_list, "allow only letters"))
  {
    set(inputbox,MUIA_String_Accept, "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPLKJHGFDSAZXCVBNM"); 
  }
  else if (!strcmp(selected_item_from_list, "allow only numbers"))
  {
    set(inputbox,MUIA_String_Accept, "0123456789");
  }
  else if (!strcmp(selected_item_from_list, "allow all"))
  {
    set(inputbox,MUIA_String_Reject, "");
  }
  
}



int main(int argc,char *argv[])
{

  

    app = ApplicationObject,
        MUIA_Application_Title      , "PoplistObject and TextObject filter",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "PoplistObject and TextObject filter",
            MUIA_Window_Width, 350,
	    MUIA_Window_Height, 370,     
            WindowContents, 
            
            VGroup,   
 		Child,		   
                     inputbox = StringObject,
                     StringFrame,
                     MUIA_String_Contents, "Enter text here",
                   End, 
                   Child,
                   TextObject,
                     MUIA_Text_Contents, "Input box filter for typing:",
                   End, 
                   Child,
                   objpop= PoplistObject,
                   MUIA_Popstring_String, StringObject, StringFrame,End,
                   MUIA_Popstring_Button, PopButton(MUII_PopUp),
                   MUIA_Poplist_Array, (IPTR)cc,
                   End,
                 End,
        End,
    End;
    
    
    myhook_poplist.h_Entry = HookEntry;
    myhook_poplist.h_SubEntry = (HOOKFUNC)func_poplist_select_item;
    
    
    // set default item for poplist
    set(objpop, MUIA_String_Contents,"all");
    // disable writing text into poplist object
    set(objpop,MUIA_String_Accept, "");
   
   
    DoMethod(objpop, MUIM_Notify,MUIA_String_Contents , MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_poplist);   
   
   
    // when user closes our window ("window" object),
    // quit entire application ("app" object)
    DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);

     
    
        
    // display "window" object
    set(window,MUIA_Window_Open,TRUE);
    
    ULONG sigs = 0;

    while (DoMethod(app,MUIM_Application_NewInput,&sigs) != MUIV_Application_ReturnID_Quit)
    {
      if (sigs)
      {
        sigs = Wait(sigs | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C) break;
      }
    }
    
    // hide "window" object
    set(window,MUIA_Window_Open,FALSE);

    // dispose "app" object
    MUI_DisposeObject(app);
    
   
}