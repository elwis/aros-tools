#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/alib.h>
#include <utility/hooks.h>
#include <proto/asl.h>
#define SAVEDS
#define ASM

Object *btn;
Object *app,*window;


static struct FileRequester *filereq;
static struct Hook myhook;

// 'myfunc' function is called when we click on button
static ULONG myfunc(struct Hook *hook, Object *object, APTR msg)
{

   // create and show FileRequester dialog box
   filereq = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
        ASLFR_TitleText,     "Select a file",
        ASLFR_DoPatterns,    TRUE,
        ASLFR_InitialDrawer, "S:",
        TAG_END);
   
   if (!filereq)
   {
     // display message box
     MUI_Request(app, window, 0, "Error", "OK", "Failed to create FileRequester dialog");
     return;
   }
    
    if (AslRequest(filereq, NULL)) // if we choosen OK 
    {
      char filename[255];
      char dir[255];
      strcpy(dir, filereq->rf_Dir);
      strcpy(filename, filereq->rf_File);
      
      char full_filename[255];
      strcpy(full_filename, dir);
      strcat(full_filename, filename);
 
      char msg[1000];
      sprintf(msg, "You selected dir: %s, filename: %s, full filename: %s", dir, filename, full_filename);
      // display message box
      MUI_Request(app, window, 0, "Notification", "OK", msg);
    }
    else // if we choosen CANCEL
    {
      // display message box
      MUI_Request(app, window, 0, "Notification", "OK", "FileRequester dialog canceled");

    }

  return 11;
}


int main(int argc,char *argv[])
{

    app = ApplicationObject,
        MUIA_Application_Title      , "FileRequester dialog app",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "FileRequester dialog demo",
            MUIA_Window_Width, 300,
            MUIA_Window_Height, 150,
            WindowContents, 
            
            VGroup,   
                 Child,
                   btn = SimpleButton("Open dialog box"),
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