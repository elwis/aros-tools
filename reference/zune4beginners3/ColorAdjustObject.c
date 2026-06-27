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
Object *clrobj;

static struct Hook myhook;

// When we click on button, choosen color values are displayed on it
static ULONG myfunc(struct Hook *hook, Object *object, APTR msg)
{
  char str[255];
  int red, green, blue;
  red = DecodeRedColor();
  green = DecodeGreenColor();
  blue = DecodeBlueColor();
  sprintf(str, "Red: %i Green: %i Blue: %i", red, green, blue);  
  set(btn, MUIA_Text_Contents, str);
  return 11;
}


int DecodeRedColor()
{
  int inpcolor = XGET(clrobj, MUIA_Coloradjust_Red);
  return (inpcolor & 0xFF000000) >> 24;
}

int DecodeGreenColor()
{
  int inpcolor = XGET(clrobj, MUIA_Coloradjust_Green);
  return (inpcolor & 0xFF000000) >> 24;
}
 
int DecodeBlueColor()
{
  int inpcolor = XGET(clrobj, MUIA_Coloradjust_Blue);
  return (inpcolor & 0xFF000000) >> 24;
}

ULONG EncodeColorChannel(int value)
{
  return ( value << 24 ) | 0x00FFFFFF ;
}



int main(int argc,char *argv[])
{
    Object *app,*window;
  
    app = ApplicationObject,
        MUIA_Application_Title      , "Color adjust demo app",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

        SubWindow,
            window = WindowObject,
       
            MUIA_Window_Height, 300,
            MUIA_Window_Width, 200,
            MUIA_Window_Title, "Color adjust object demo",
            // Important! Window size can be specified here and set
            // if you don't use MUIA_Window_ID for WindowObject
            // (in other case window size is set automatically and remembered
            // for next time you run app
            // not used here: MUIA_Window_ID   , MAKE_ID('C','L','S','2'),
            WindowContents, 
            
            VGroup,   
              Child,
              HGroup,
                   Child,
                     MUI_MakeObject(MUIO_Label,"Click button...", 0),
                   Child,
                     btn = SimpleButton("Click me..."),
              End,    
              Child,
                clrobj = ColoradjustObject,
              End,
            End,
       End,
    End;

    

    // when user closes our window ("window" object),
    // quit entire application ("app" object)
    DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);

     
    myhook.h_Entry = HookEntry;
    // attach 'myfunc' function to hook 'myhook'
    myhook.h_SubEntry = (HOOKFUNC)myfunc;

    // when 'btn' is presed, call hook 'myhook'
    DoMethod(btn, MUIM_Notify,  MUIA_Pressed, MUIV_EveryTime, (IPTR)app,2,MUIM_CallHook, (IPTR)&myhook);
  
 
    // Set red component to 255, green to 128 and blue to 50
    set(clrobj, MUIA_Coloradjust_Red, EncodeColorChannel(255));
    set(clrobj, MUIA_Coloradjust_Green, EncodeColorChannel(128));
    set(clrobj, MUIA_Coloradjust_Blue, EncodeColorChannel(50));

         
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