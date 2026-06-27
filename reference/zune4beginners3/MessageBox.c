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

Object *app, *window;

int quit = 0;  

static struct Hook myhook;

static ULONG myfunc(struct Hook *hook, Object *object, APTR msg)
{
  // display simple message box
  MUI_Request(app, window, 0, "Notification", "OK", "Application will quit now");

  quit = 1; // set "quit" variable to 1 so main loop ends
  
  return 1;
}

int main(int argc,char *argv[])
{

     app = ApplicationObject,
        MUIA_Application_Title      , "MessageBox app",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

        SubWindow, window = WindowObject,
            MUIA_Window_Title, "Message box when quiting...",
            MUIA_Window_Width, 450,
	    MUIA_Window_Height, 70,     
                       
            WindowContents, 
            
            VGroup,
                 Child,
                    TextObject,
		    TextFrame, // can use TextFrame to display border our text
                    MUIA_Text_Contents, "This is static text inside frame",
                 End,  
            End,

        End,
    End;



    myhook.h_Entry = HookEntry;
    
    // we assign our "myfunc" function to myhook
    myhook.h_SubEntry = (HOOKFUNC)myfunc; 
   
   
    // when user closes window, we call hook called "myhook", to which we have previously assigned our custom "myfunc" function
    DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_CallHook, (IPTR)&myhook);

    
        
    // display "window" object
    set(window,MUIA_Window_Open,TRUE);
    
    ULONG sigs = 0;

    // main application loop
    while (DoMethod(app,MUIM_Application_NewInput,&sigs) != MUIV_Application_ReturnID_Quit)
    {
      if (sigs)
      {
        sigs = Wait(sigs | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C) break;
      }
      
      // when "quit" variable is set to 1 in "myfunc" function, 
      // we end main loop
      if (quit == 1) break;
    }
    
    // hide "window" object
    set(window,MUIA_Window_Open,FALSE);

    // dispose "app" object
    MUI_DisposeObject(app);
    
   
}