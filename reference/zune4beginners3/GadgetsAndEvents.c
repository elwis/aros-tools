#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/alib.h>
#include <utility/hooks.h>
#include <mui/TextEditor_mcc.h>
#include <stdio.h>
#include <stdlib.h>

#define SAVEDS
#define ASM

Object *obj_btn;
Object *obj_scroller;
Object *obj_slider;
Object *obj_progressbar;
Object *obj_numbutton;
Object *obj_chk;
Object *obj_chk2;
Object *obj_radios;
Object *obj_imgbtn;
Object *textbox;


char *radio_entries[] = {"Paris","Pataya","London","New-York","Reykjavik",NULL};    
 
static struct Hook myhook_button;
static struct Hook myhook_slider;
static struct Hook myhook_scroller;
static struct Hook myhook_numbutton;
static struct Hook myhook_checkmark;
static struct Hook myhook_checkmark2;
static struct Hook myhook_radios;
static struct Hook myhook_imgbutton;

static int line_cnt = 0;

void AppendTextToBox(char *txt)
{
  
  // here we get text from text object
  char *text = (char*) DoMethod(textbox, MUIM_TextEditor_ExportText);
  char newtext[1000];
  line_cnt++;
  if (line_cnt < 7)
    strcpy(newtext, text);
  else
  {
    // when there are more than 6 lines in 'textbox' object, clear it
  
    strcpy(newtext, "");
    line_cnt = 0;
  }
  
  // add 'txt' string to existing text from 'textbox' object
  char newline[2];
  newline[0] = 13; // code for new line
  newline[1] = NULL;
  strcat(newtext, newline);
  strcat(newtext, txt);
  
  // copy 'newtext' string to 'textbox' object
  set(textbox, MUIA_TextEditor_Contents , newtext);
}


// following functions are called when changing value of the gui objects

static ULONG func_button(struct Hook *hook, Object *object, APTR msg)
{
  int gauge_max = XGET(obj_progressbar, MUIA_Gauge_Max);
  int random = (rand() / (float) RAND_MAX) * gauge_max;
 
  
  printf("Progressbar value change to: %i ", random);
 
  static char text[255];
  sprintf(text, "Value: %i", random);
  
  // important! progressbar text must be changed prior to setting bar's value
  set(obj_progressbar, MUIA_Gauge_InfoText, text); 
  set(obj_progressbar, MUIA_Gauge_Current, random);
}

static ULONG func_slider(struct Hook *hook, Object *object, APTR msg)
{
  char text[255];
  sprintf(text, "Slider is set at %i", XGET(obj_slider,MUIA_Numeric_Value));
  AppendTextToBox(text);
}

static ULONG func_scroller(struct Hook *hook, Object *object, APTR msg)
{
  char text[255];
  sprintf(text, "Scrollbar is set at %i", XGET(obj_scroller,MUIA_Prop_First));
  AppendTextToBox(text);
}


static ULONG func_numbutton(struct Hook *hook, Object *object, APTR msg)
{
  char text[255];
  sprintf(text, "Numeric button is set at %i", XGET(obj_numbutton,MUIA_Numeric_Value));
  AppendTextToBox(text);

}


static ULONG func_radios(struct Hook *hook, Object *object, APTR msg)
{
   char text[255];
  sprintf(text, "Selected radio index is %i", XGET(obj_radios, MUIA_Radio_Active));
  AppendTextToBox(text);

}

static ULONG func_checkmark(struct Hook *hook, Object *object, APTR msg)
{
  char text[255];
  
  if (XGET(obj_chk, MUIA_Selected) == TRUE)
    AppendTextToBox("First checkmark set to TRUE");
  else
    AppendTextToBox("First checkmark set to FALSE");  
}


static ULONG func_checkmark2(struct Hook *hook, Object *object, APTR msg)
{
  char text[255];
  
  if (XGET(obj_chk2, MUIA_Selected) == TRUE)
    AppendTextToBox("Second checkmark set to TRUE");
  else
    AppendTextToBox("Second checkmark set to FALSE");  
}


static ULONG func_imgbutton(struct Hook *hook, Object *object, APTR msg)
{
  AppendTextToBox("Image button was clicked.");
}


static APTR MakeButton(UBYTE *Image, UBYTE Key, UBYTE *Help)
{
   return(MUI_NewObject("Dtpic.mui",
                        MUIA_Dtpic_Name,Image,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                        MUIA_ControlChar, Key,
                        MUIA_Background, MUII_ButtonBack,
                        MUIA_ShortHelp, Help,
                        ImageButtonFrame,
                        TAG_DONE));
}

int main(int argc,char *argv[])
{
    Object *app,*window;
  

    app = ApplicationObject,
        MUIA_Application_Title      , "Gadgets and events app",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "Gadgets and events",
            MUIA_Window_Width, 330,
            MUIA_Window_Height, 500,
            WindowContents, 
            
            VGroup,   
                 Child,
                    textbox = TextEditorObject,
                    MUIA_TextEditor_Contents, "Events are printed here...",
                 End, 
                 Child,
                   obj_btn = SimpleButton("Set progressbar value to random"),
                 Child,
                   obj_progressbar = GaugeObject,
                    MUIA_Gauge_Horiz, TRUE,
                   End,
                 Child,
                  obj_slider = SliderObject,
                  MUIA_Slider_Horiz, TRUE,
                  End,                              
                Child,
                  obj_scroller = ScrollbarObject,
                   MUIA_Group_Horiz, TRUE,
                End,
                Child,
                  obj_numbutton = NumericbuttonObject,
                End,
                Child,
                  obj_imgbtn = MakeButton("ImageButton.png", 'o', "\33uO\33npen Archive"),
            
		Child, HGroup, //GroupSpacing(10), MUIA_Group_SameWidth, TRUE,
                  
                  Child, MUI_MakeObject(MUIO_Label,"Checkmark 1",0),
	          Child, obj_chk = MUI_MakeObject(MUIO_Checkmark,"_Checmark"),           
		 
		  Child, MUI_MakeObject(MUIO_Label,"Checkmark 2",0),
                  Child, obj_chk2 = MUI_MakeObject(MUIO_Checkmark,"_Checma2"),           
		End, 			
	
		Child, obj_radios= RadioObject,GroupFrame,MUIA_Radio_Entries,radio_entries,MUIA_Radio_Active,1,
		End,
		End,
       End,
    End;
 
 
    // set min, max and current values for GUI objects
    
    set(obj_progressbar, MUIA_Gauge_Current, 50);
    set(obj_progressbar, MUIA_Gauge_Max, 100);
    set(obj_progressbar, MUIA_Gauge_InfoText, " ");

    set(obj_slider, MUIA_Numeric_Min, 0);
    set(obj_slider, MUIA_Numeric_Max, 30);
    set(obj_slider, MUIA_Numeric_Value, 0);
    
    set(obj_numbutton, MUIA_Numeric_Min, 10);
    set(obj_numbutton, MUIA_Numeric_Max, 20);
    set(obj_numbutton, MUIA_Numeric_Value, 0);
  
    int bar_size = 4;
    int scroller_max = 14;
    
    set(obj_scroller, MUIA_Prop_Visible, bar_size);
    set(obj_scroller, MUIA_Prop_Entries, scroller_max + bar_size); // max
    set(obj_scroller, MUIA_Prop_First, scroller_max / 2); // current index
    
    // when user closes our window ("window" object),
    // quit entire application ("app" object)
    DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);

     
    // connect event responding functions to hooks
    myhook_button.h_Entry = HookEntry;
    myhook_button.h_SubEntry = (HOOKFUNC)func_button;
    myhook_slider.h_Entry = HookEntry;
    myhook_slider.h_SubEntry = (HOOKFUNC)func_slider;
    myhook_scroller.h_Entry = HookEntry;
    myhook_scroller.h_SubEntry = (HOOKFUNC)func_scroller;
    myhook_numbutton.h_Entry = HookEntry;
    myhook_numbutton.h_SubEntry = (HOOKFUNC)func_numbutton;
    myhook_checkmark.h_Entry = HookEntry;
    myhook_checkmark.h_SubEntry = (HOOKFUNC)func_checkmark;
    myhook_checkmark2.h_Entry = HookEntry;
    myhook_checkmark2.h_SubEntry = (HOOKFUNC)func_checkmark2;
    myhook_radios.h_Entry = HookEntry;
    myhook_radios.h_SubEntry = (HOOKFUNC)func_radios;
    myhook_imgbutton.h_Entry = HookEntry;
    myhook_imgbutton.h_SubEntry = (HOOKFUNC)func_imgbutton;
    
    // Set textbox height, so it will fit to 9 rows
    set(textbox, MUIA_TextEditor_Rows, 9);

    // connect hooks to GUI objects
    DoMethod(obj_numbutton, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_numbutton);
    DoMethod(obj_btn, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_button);   
    DoMethod(obj_slider, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_slider);
    DoMethod(obj_scroller, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_scroller);
    DoMethod(obj_radios, MUIM_Notify, MUIA_Radio_Active, MUIV_EveryTime, (IPTR)app,2,MUIM_CallHook, (IPTR)&myhook_radios);
    DoMethod(obj_chk, MUIM_Notify, MUIA_Pressed, MUIV_EveryTime, (IPTR)app,2,MUIM_CallHook, (IPTR)&myhook_checkmark);
    DoMethod(obj_chk2, MUIM_Notify, MUIA_Pressed, MUIV_EveryTime, (IPTR)app,2,MUIM_CallHook, (IPTR)&myhook_checkmark2);
    DoMethod(obj_imgbtn, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app,2,MUIM_CallHook, (IPTR)&myhook_imgbutton);
  
        
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