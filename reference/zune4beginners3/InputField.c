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
Object *btn1;
Object *btn2;
Object *inputbox;

static struct Hook myhook1;
static struct Hook myhook2;

static ULONG myfunc1(struct Hook *hook, Object *object, APTR msg)
{
  set(inputbox, MUIA_String_Contents, "Button was clicked...");
  return 11;
}

static ULONG myfunc2(struct Hook *hook, Object *object, APTR msg)
{
  MUI_Request(app, window, 0, "Message box", "OK", XGET(inputbox, MUIA_String_Contents));
  return 11;
}



int main(int argc,char *argv[])
{

    app = ApplicationObject,
        MUIA_Application_Title      , "InputField demo app",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "InputField demo",
            MUIA_Window_Width, 360,
	    MUIA_Window_Height, 170,     
            WindowContents, 
            
            VGroup,
                 Child,
                 HGroup,
                   
                   Child,
                   btn1 = SimpleButton("Set some text"),
                   Child,
                   btn2 = SimpleButton("Show typed text in messagebox"),
                 End,
                 
	       Child,
                  inputbox = StringObject,
                  StringFrame,
                  MUIA_String_Contents, "This is content",
               End,  
            End,

        End,
    End;

   
    // assign myfunc1 function to myhook1
    myhook1.h_Entry = HookEntry;
    myhook1.h_SubEntry = (HOOKFUNC)myfunc1;
    // assign myhook1 to first button 'btn1'
    DoMethod(btn1, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook1);


    // assign myfunc2 function to myhook2
    myhook2.h_Entry = HookEntry;
    myhook2.h_SubEntry = (HOOKFUNC)myfunc2;
    // assign myhook2 to second button 'btn2'
    DoMethod(btn2, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook2);
              
   

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