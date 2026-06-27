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

int quit = 0;  
Object *app,*window, *window_dialog;
 
static struct Hook myhook_press_exit;
static struct Hook myhook_press_yes;
static struct Hook myhook_press_no;

// this function is called, when user closes primary (first) window
static ULONG myfunc_press_exit(struct Hook *hook, Object *object, APTR msg)
{
  
  // show question dialog
  set(window_dialog,MUIA_Window_Open,TRUE);
  return 1;
}

static ULONG myfunc_press_no(struct Hook *hook, Object *object, APTR msg)
{
  // hide question dialog
  set(window_dialog,MUIA_Window_Open,FALSE);
  return 1;
}

static ULONG myfunc_press_yes(struct Hook *hook, Object *object, APTR msg)
{
  // set "quit" variable to 1
  quit = 1;
  return 1;
}

// at startup main window is created and also question dialog window, but
// only first window is shown (dialog window is shown when we quit app)

int main(int argc,char *argv[])
{
   
    Object *btn_yes, *btn_no;

    app = ApplicationObject,
        MUIA_Application_Title      , "QuestionDialog app",
        MUIA_Application_Version    , "2.2",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASSx4",

        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "QuestionDialog demo",
            MUIA_Window_Width, 280,
	    MUIA_Window_Height, 80,     
                       
            WindowContents, 
            
            VGroup,
            
                 Child,
                    TextObject,
                    MUIA_Text_Contents, "This is main window...",
                 End,  
            End,
         End,
             
        // this is 2nd window (question box)
        SubWindow,
            window_dialog = WindowObject,
            MUIA_Window_Title, "Question box title",
	    MUIA_Window_Width, 240,
	    MUIA_Window_Height, 180,  
                       
            WindowContents, 
            
            VGroup,
                 Child,
                    TextObject,
                    MUIA_Text_Contents, "Do you want to quit application?",
                   End,  
                 Child,
                   btn_yes = SimpleButton("Yes"),
                 Child,
                   btn_no = SimpleButton("No"),
            End,
         End,  
             
        End;

   
   
    myhook_press_exit.h_Entry = HookEntry;
    // we assign our "myfunc_press_exit" function 
    myhook_press_exit.h_SubEntry = (HOOKFUNC)myfunc_press_exit; 
    // when user closes window, call "myhook_press_exit" hook
    DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_CallHook, (IPTR)&myhook_press_exit);

  
    myhook_press_no.h_Entry = HookEntry;
    myhook_press_no.h_SubEntry = (HOOKFUNC)myfunc_press_no; 
    // when user presses 'btn_no' button, call function 'myfunc_press_no'
    DoMethod(btn_no, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_press_no);
     
    myhook_press_yes.h_Entry = HookEntry;
    myhook_press_yes.h_SubEntry = (HOOKFUNC)myfunc_press_yes; 
    // when user presses 'btn_yes' button, call fuction 'myfunc_press_yes'
    DoMethod(btn_yes, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_press_yes);
     
        
    // display window
    set(window,MUIA_Window_Open,TRUE);
    
    
    ULONG sigs = 0;

    while (DoMethod(app,MUIM_Application_NewInput,&sigs) != MUIV_Application_ReturnID_Quit)
    {
      if (sigs)
      {
        sigs = Wait(sigs | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C) break;
      }
      if (quit == 1) break;
    }
    
    // hide both windows
    set(window,MUIA_Window_Open,FALSE);
    set(window_dialog,MUIA_Window_Open,FALSE);
    
    // dispose "app" object
    MUI_DisposeObject(app);
    
   
}