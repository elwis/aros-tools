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

int main(int argc,char *argv[])
{
   Object *app,*window;
  
   app = ApplicationObject,
        MUIA_Application_Title      , "Frames demo app",
        MUIA_Application_Version    , "1",
        MUIA_Application_Copyright  , "©w012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLJS2",

        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "Frames demo",
            MUIA_Window_Width, 400,
            MUIA_Window_Height, 200,
            WindowContents, 
            
            VGroup,   
                Child,
                    SimpleButton("Button 1"),
                    
                Child, ColGroup(1), GroupFrameT("Frame 1"), // create 1st frame                 
 	          Child,	
 	          HGroup, // display following 2 buttons horizontally
 	            Child, SimpleButton("1st button inside frame 1"),           
	            Child, SimpleButton("2st button inside frame 1"),   
	          End,        
		End,  
		
	        Child, ColGroup(1), GroupFrameT("Frame 2"), // create 2nd frame                
 	          Child,
 	          VGroup, // display following GUI elements vertically aligned
 	            Child,
  	              TextObject,
                      MUIA_Text_Contents, "Label inside second frame!",
                    End,
 	            Child, SimpleButton("1st button inside frame 2"),   	             	           
 	            Child, SimpleButton("2st button inside frame 2"),
 	            Child, SimpleButton("3st button inside frame 2"),
 	           End,
 	           End,
 	             
		
		End,
        End,
    End;

   

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