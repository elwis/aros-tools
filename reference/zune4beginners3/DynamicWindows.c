#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/alib.h>
#include <utility/hooks.h>

Object *app, *window, *btn;


#define MAX_DYNAMIC_WINDOWS 200


static struct Hook myhook;
static ULONG myfunc(struct Hook *hook, Object *object, APTR msg)
{
  CreateAdditionalWindow();
  return 11;
}

Object *dynamic_windows[MAX_DYNAMIC_WINDOWS];
int dynamic_window_count = 0;

void ReleaseDynamicWindows()
{
  int n;
  for (n = 0; n < dynamic_window_count; n++)
  {
    set(dynamic_windows[n], MUIA_Window_Open, FALSE);
    DoMethod(app, OM_REMMEMBER, dynamic_windows[n]);
  }
}

void CreateAdditionalWindow()
{
  Object * new_window;
  char win_txt[255];
  sprintf(win_txt, "This is window with index %i", dynamic_window_count);
        new_window = WindowObject,
            MUIA_Window_Title, "Dynamic window",
            MUIA_Window_Width, 200,
	    MUIA_Window_Height, 70,     
            WindowContents, 
            
            VGroup,
                 Child,
                    TextObject,
                    MUIA_Text_Contents, win_txt,
                 End,  
            End,
         End;
         
        if (!new_window)
        {
          printf("Failed to create new window!");
          return;
        }
        else
        {
          printf("Window sucessfully created");
        }
        
        // add newly created window to application 
        DoMethod(app, OM_ADDMEMBER, new_window);
        
        // when we close any of the additional windows, entire application quits
        DoMethod(new_window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
    
        // display window
        set(new_window,MUIA_Window_Open,TRUE);
        
        // store pointer to our new window, so we can later release it
        dynamic_windows[dynamic_window_count++] = new_window;
}


int main(int argc,char *argv[])
{

    app = ApplicationObject,
        MUIA_Application_Title      , "Dynamic Windows app",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "Dynamic Windows Demo",
            MUIA_Window_Width, 200,
	    MUIA_Window_Height, 70,                     
            WindowContents, 
            
            VGroup,
                 Child,
                    TextObject,
                    
                    MUIA_Text_Contents, "First window...",
                 End, 
                 Child,
                 btn = SimpleButton("Create new window"),
                 
            End,
         End,
    End;
             
    // closing first window quits application
    DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);

     
    // attach 'myfunc' function to hook 'myhook' 
    myhook.h_Entry = HookEntry;
    myhook.h_SubEntry = (HOOKFUNC)myfunc;

    // attach 'myhook' to button object 'btn', so it will 
    // be called, when button is pressed    
    DoMethod(btn, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook);
        
        
    // display main window
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
    
    // hide main window
    set(window,MUIA_Window_Open,FALSE);
   
    ReleaseDynamicWindows();
    
    // dispose "app" object
    MUI_DisposeObject(app);
    
   
}