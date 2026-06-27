#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/alib.h>
#include <utility/hooks.h>

// in this demo, we create 2 windows at same time (at startup)

int main(int argc,char *argv[])
{
    Object *app,*window, *window2;
    
    app = ApplicationObject,
        MUIA_Application_Title      , "Two windows app",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "Window 1 title",
            MUIA_Window_Width, 240,
	    MUIA_Window_Height, 80,  
                       
            WindowContents, 
            
            VGroup,
                 Child,
                    TextObject,
                    MUIA_Text_Contents, "First window...",
                 End,  
            End,
         End,
             
           
        SubWindow,
            window2 = WindowObject,
            MUIA_Window_Title, "Window 2 title",
            MUIA_Window_Width, 200,
	    MUIA_Window_Height, 80,  
                       
            WindowContents, 
            
            VGroup,
                 Child,
                    TextObject,
                    MUIA_Text_Contents, "Second window...",
                 End,  
            End,
         End,  
             
        End;

   

    // when user closes one of the windows ("window" or "window2" object),
    // quit entire application ("app" object)
    
    DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
    DoMethod(window2,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);

     
        
    // display both windows
    set(window,MUIA_Window_Open,TRUE);
    set(window2,MUIA_Window_Open,TRUE);
    
    ULONG sigs = 0;

    while (DoMethod(app,MUIM_Application_NewInput,&sigs) != MUIV_Application_ReturnID_Quit)
    {
      if (sigs)
      {
        sigs = Wait(sigs | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C) break;
      }
    }
    
    // hide both windows
    set(window,MUIA_Window_Open,FALSE);
    set(window2,MUIA_Window_Open,FALSE);
    
    // dispose "app" object
    MUI_DisposeObject(app);
    
   
}