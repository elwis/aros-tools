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

Object *btn;
Object *app,*window;

static struct Hook myhook;

// called when button is clicked
static ULONG myfunc(struct Hook *hook, Object *object, APTR msg)
{
  printf("Resizing...");
  
  // hideo "window" object before resizing
  set(window,MUIA_Window_Open,FALSE);
  set(window, MUIA_Window_Width, 500);
  set(window, MUIA_Window_Height, 200);
  
  // display "window" object
  set(window,MUIA_Window_Open,TRUE);
  
  
  return 11;
}


int main(int argc,char *argv[])
{

    app = ApplicationObject,
        MUIA_Application_Title      , "Window resizer app",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "Window Resizer",
            MUIA_Window_Width, 220,
            MUIA_Window_Height, 150,
            WindowContents, 
            
            VGroup,   
                 Child,
                   btn = SimpleButton("Click here to resize"),
                 
            End,

        End,
    End;

   

    // when user closes our window ("window" object),
    // quit entire application ("app" object)
    DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);



     
     
    // attach 'myfunc' function to hook 'myhook' 
    myhook.h_Entry = HookEntry;
    myhook.h_SubEntry = (HOOKFUNC)myfunc;

    // attach 'myhook' to button object 'btn', so it will 
    // be called, when button is pressed    
    DoMethod(btn, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook);
        
        
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