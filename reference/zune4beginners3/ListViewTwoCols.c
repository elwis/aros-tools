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

Object *btn_delete;
Object *btn_insert;
Object *btn_clear;
Object *btn_change;
Object *listview;
Object *app,*window,*window_inputbox;
Object *listobj;
Object *btn_inputbox_ok ;
Object *inputfield;
Object *inputfield2;
Object *label;
Object *label2;

char *header1 = "Name";
char *header2 = "City";
BOOL change_string_mode;


struct list_entry
{
  char *column1;
  char *column2;
};

// string1 and 2 are for first and second column
void AddNewItemToList(char *string1, char *string2)
{
  struct list_entry *le = malloc(sizeof(struct list_entry));
  le->column1 = malloc(255);
  le->column2 = malloc(255);
  	
  strcpy(le->column1, string1);
  strcpy(le->column2, string2);
  DoMethod(listobj,MUIM_List_InsertSingle,le,MUIV_List_Insert_Bottom);
  
  int entries_count = XGET(listobj, MUIA_List_Entries);
  set(listobj, MUIA_List_Active, entries_count );
}


void RemoveItemFromList(int pos)
{
  struct list_entry *selected_item_pointer;
  // get pointer to string for given position
  DoMethod(listobj,MUIM_List_GetEntry,pos, &selected_item_pointer);
  // remove string from list
  DoMethod(listobj,MUIM_List_Remove,pos);
  // free memory, allocated for string
  free(selected_item_pointer->column1);
  free(selected_item_pointer->column2);
  free(selected_item_pointer);
  
}



static struct Hook myhook_btninsert;
static struct Hook myhook_btndelete;
static struct Hook myhook_btnclear;
static struct Hook myhook_btnchange;
static struct Hook myhook_listclick;
static struct Hook myhook_btninputboxok;
static struct Hook myhook_close_inputbox_window;
static struct Hook myhook_listdblclick;

// called when selecting item
static ULONG myfunc_listclick(struct Hook *hook, Object *object, APTR msg)
{
  int pos = XGET(listobj, MUIA_List_Active);
  if (pos < 0) return;
  static char str[255];
  sprintf(str, "You selected item with index: %i", pos);
  set(label, MUIA_Text_Contents , str);
  return 11;
}


// called when double clicking item
static ULONG myfunc_listdblclick(struct Hook *hook, Object *object, APTR msg)
{
  int pos = XGET(listobj, MUIA_List_Active);
  if (pos < 0) return;
  static char str[255];
  sprintf(str, "You double clicked on item with index %i", pos);
  set(label2, MUIA_Text_Contents , str);
  return 11;
}


// 
static ULONG myfunc_btn_close_inputbox_window(struct Hook *hook, Object *object, APTR msg)
{
  set( window_inputbox ,MUIA_Window_Open, FALSE); // hide window with inputfield
  return 11;
}



// this function is called, when we click on button OK (on window with inputfield)
static ULONG myfunc_btn_inputbox_ok(struct Hook *hook, Object *object, APTR msg)
{
  if (change_string_mode == FALSE) // if add new item button was clicked
  {
    AddNewItemToList(XGET(inputfield,MUIA_String_Contents), XGET(inputfield2,MUIA_String_Contents));
  }
  else // if change selected item button was clicked
  {
    struct list_entry *selected_item_pointer;
    int pos = XGET(listobj, MUIA_List_Active);
    if (pos < 0) return;
    // get pointer to string for given position
    DoMethod(listobj,MUIM_List_GetEntry,pos, &selected_item_pointer);
    strcpy(selected_item_pointer->column1, XGET(inputfield, MUIA_String_Contents));
    strcpy(selected_item_pointer->column2, XGET(inputfield2, MUIA_String_Contents));
 
    DoMethod(listobj,MUIM_List_Redraw,MUIV_List_Redraw_All);
  }
  
  set(inputfield, MUIA_String_Contents, ""); // clear text in inputfield
  set(inputfield2, MUIA_String_Contents, ""); // clear text in inputfield 2

  set( window_inputbox ,MUIA_Window_Open, FALSE); // hide window with inputfield
  return 11;
}

// this function is called when we click on button "Insert at bottom"
static ULONG myfunc_btninsert(struct Hook *hook, Object *object, APTR msg)
{
  change_string_mode = FALSE; // we are in insert string mode
  set( window_inputbox ,MUIA_Window_Open,TRUE); // show window with inputfield
  return 11;
}

static ULONG myfunc_btndelete(struct Hook *hook, Object *object, APTR msg)
{
  int pos = XGET(listobj, MUIA_List_Active);
  if (pos < 0) return;
  RemoveItemFromList(pos);
  return 11;
}




static ULONG myfunc_btnclear(struct Hook *hook, Object *object, APTR msg)
{
  int entries_count = XGET(listobj, MUIA_List_Entries);
  int n;
  for (n = 0; n < entries_count; n++)
  {
    RemoveItemFromList(0);
  }
  return 11;
}


static ULONG myfunc_btnchange(struct Hook *hook, Object *object, APTR msg)
{
  int pos = XGET(listobj, MUIA_List_Active);
  if (pos < 0) return;

  change_string_mode = TRUE; // we are in change item string mode
  set( window_inputbox ,MUIA_Window_Open,TRUE); // show window with inputfield
  return 11;
}


struct Hook hook_display;



// this is called just before displaying items in list
AROS_UFH3(void, display_function,
AROS_UFHA(struct Hook*, h, A0),
AROS_UFHA(char **, strings, A2),     
AROS_UFHA(struct list_entry*, entry, A1))
{
  AROS_USERFUNC_INIT
  
  if (entry)
  {
    strings[0] = entry->column1;
    strings[1] = entry->column2;
    strings[2] = NULL;
  }
  else // when entry is NULL, we must supply 'strings' array with header
  {
    strings[0] = header1;
    strings[1] = header2;
    strings[2] = NULL;
  }
  AROS_USERFUNC_EXIT
}

int main(int argc,char *argv[])
{
    
    // hook_display hook is used with conjunction with display_function to show items in list
    hook_display.h_Entry = HookEntry;
    hook_display.h_SubEntry = (HOOKFUNC) display_function;


    app = ApplicationObject,
        MUIA_Application_Title      , "ListViewObject with 2 columns",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

	// window with inputbox 
	SubWindow,
	  window_inputbox = WindowObject,
            MUIA_Window_Title, "Enter two strings",
            MUIA_Window_Width, 300,
            MUIA_Window_Height, 150,
            WindowContents, 
            
            VGroup,
             
              Child,
              TextObject,
                  MUIA_Text_Contents, "Name:",
              End,
	      Child,
                  inputfield = StringObject,
                  StringFrame,
                  MUIA_String_Contents, "",
              End,
              Child,
                  TextObject,
                  MUIA_Text_Contents, "City:",
              End,
              Child,
                  inputfield2 = StringObject,
                  StringFrame,
                  MUIA_String_Contents, "",
              End,
              Child,
              btn_inputbox_ok = SimpleButton("OK"),
                      
            End,	  
        End,  		
        // main window
        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "ListViewObject with 2 columns",
            MUIA_Window_Width, 200,
            MUIA_Window_Height, 150,
            WindowContents, 
            
            VGroup,
            	  Child,
                  HGroup,  
                  Child,
                   btn_insert = SimpleButton("Insert at bottom"),
                  Child,
                   btn_delete = SimpleButton("Delete selected item"),
                  Child,
                   btn_clear = SimpleButton("Clear listview"),
                  Child,
                   btn_change = SimpleButton("Edit selected item"),
                  End,
                  Child,
                   label = TextObject,
                   MUIA_Text_Contents, "None item was selected",
                 End,
                 Child,
                   label2 = TextObject,
                   MUIA_Text_Contents, "None item was double clicked",
                 End,
                 
                 Child,
                 listview = ListviewObject,
                 MUIA_Listview_List,listobj=ListObject,
                   InputListFrame,
                   MUIA_List_DisplayHook, &hook_display, // specify hook we use
                   MUIA_List_Title, TRUE,
                   MUIA_List_Format, ",,",
                   End,
                 End,
                 
             
            End,

        End,
    End;


    // when user closes our window ("window" object),
    // quit entire application ("app" object)
    DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app, 2, MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);

    
    myhook_close_inputbox_window.h_Entry = HookEntry;
    myhook_close_inputbox_window.h_SubEntry = (HOOKFUNC)myfunc_btn_close_inputbox_window;
    
    // when user closes inputbox window, call 'myfunc_btn_close_inputbox_window' via hook
    DoMethod(window_inputbox,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app,2, MUIM_CallHook, (IPTR) &myhook_close_inputbox_window);    
 
 
    myhook_btninputboxok.h_Entry = HookEntry;
    myhook_btninputboxok.h_SubEntry = (HOOKFUNC)myfunc_btn_inputbox_ok;
    DoMethod(btn_inputbox_ok, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_btninputboxok);
     
    
    myhook_btninsert.h_Entry = HookEntry;
    myhook_btninsert.h_SubEntry = (HOOKFUNC)myfunc_btninsert;
    DoMethod(btn_insert, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_btninsert);
      
    myhook_btndelete.h_Entry = HookEntry;
    myhook_btndelete.h_SubEntry = (HOOKFUNC)myfunc_btndelete;
    DoMethod(btn_delete, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_btndelete);
           
    myhook_btnclear.h_Entry = HookEntry;
    myhook_btnclear.h_SubEntry = (HOOKFUNC)myfunc_btnclear;
    DoMethod(btn_clear, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_btnclear);
   
    myhook_btnchange.h_Entry = HookEntry;
    myhook_btnchange.h_SubEntry = (HOOKFUNC)myfunc_btnchange;
    DoMethod(btn_change, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_btnchange);
   
   
   
       
    // display "window" object
    set(window,MUIA_Window_Open,TRUE);
    
    
    // start with some entries
    AddNewItemToList("John Soter", "New York");
    AddNewItemToList("Ben Affleck", "San Francisco");

    myhook_listclick.h_Entry = HookEntry;
    myhook_listclick.h_SubEntry = (HOOKFUNC)myfunc_listclick;
    DoMethod(listobj, MUIM_Notify, MUIA_Listview_SelectChange, TRUE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_listclick);
    
    myhook_listdblclick.h_Entry = HookEntry;
    myhook_listdblclick.h_SubEntry = (HOOKFUNC)myfunc_listdblclick;
    DoMethod(listview, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_listdblclick);
 


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
 
    // free memory for list item strings
    int entries_count = XGET(listobj, MUIA_List_Entries);
    int n;
    for (n = 0; n < entries_count; n++)
    {
      RemoveItemFromList(0);
    }
    
    
    // dispose "app" object
    MUI_DisposeObject(app);
    
   
}