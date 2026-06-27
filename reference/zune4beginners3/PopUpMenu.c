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

Object *app,*window;
Object *menu1;
Object *menu2;
Object *btn;
Object *menu1item1;
Object *menu2item1;
Object *menu1item2sub1;
Object *menu1item2sub2;
  

static struct Hook myhook_menu1item1;
static struct Hook myhook_menu1item2sub1;
static struct Hook myhook_menu1item2sub2;
static struct Hook myhook_menu2item1;


static ULONG myfunc_menu1item1(struct Hook *hook, Object *object, APTR msg)
{
  // display simple message box
  MUI_Request(app, window, 0, "MessageBox Title", "OK", "You have choosen first item on 1st popup menu");
  return 11;
}

static ULONG myfunc_menu1item2sub1(struct Hook *hook, Object *object, APTR msg)
{
  // display simple message box
  MUI_Request(app, window, 0, "MessageBox Title", "OK", "You have choosen second item and 1st sub item on 1st popup menu");
  return 11;
}

static ULONG myfunc_menu1item2sub2(struct Hook *hook, Object *object, APTR msg)
{
  // display simple message box
  MUI_Request(app, window, 0, "MessageBox Title", "OK", "You have choosen second item and 2nd sub item on 1st popup menu");
  return 11;
}
static ULONG myfunc_menu2item1(struct Hook *hook, Object *object, APTR msg)
{
  // display simple message box
  MUI_Request(app, window, 0, "MessageBox Title", "OK", "You have choosen first item on 2st popup menu");
  return 11;
}

int main(int argc,char *argv[])
{

  // first popup menu
  menu1 = MenuitemObject, 
        MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,"Click here",
        MUIA_Family_Child, menu1item1 = MenuitemObject, MUIA_Menuitem_Title,"Item 1",End,
        MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,"Item 2",
        // next two are sub items
          MUIA_Family_Child, menu1item2sub1 = MenuitemObject, MUIA_Menuitem_Title,"Subitem 1",End,
          MUIA_Family_Child, menu1item2sub2 = MenuitemObject, MUIA_Menuitem_Title,"Subitem 2",End,
        End,
        End,
        End;
        
  // second popup menu
  menu2 = MenuitemObject, MUIA_Family_Child,  MenuitemObject,
        MUIA_Menuitem_Title, "Click here",
        MUIA_Family_Child, menu2item1 = MenuitemObject, MUIA_Menuitem_Title,"Item 1b",End,
        End,
        End;       
         
    app = ApplicationObject,
        MUIA_Application_Title      , "Popup menu demo app",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",

        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "Popup menu demo",
            MUIA_Window_Height, 300,
            MUIA_Window_Width, 400,
            WindowContents, 
            
            VGroup,
            Child,
                 TextObject,
                    // attach 1st popup menu to TextObject
                    MUIA_ContextMenu, menu1,
                    MUIA_Text_Contents, "Click here with right mouse button",
                 End,
                 Child,
                 btn = SimpleButton("...or here"),
                 End, 
            End,
         End;

    // attach 2nd popup menu to button
    set(btn, MUIA_ContextMenu, menu2);
   

    // attach all popup menu item responder functions to hooks, and then hooks to menu items
    
    myhook_menu1item1.h_Entry = HookEntry;
    myhook_menu1item1.h_SubEntry = (HOOKFUNC)myfunc_menu1item1;
    DoMethod(menu1item1, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_menu1item1);
        

    myhook_menu1item2sub1.h_Entry = HookEntry;
    myhook_menu1item2sub1.h_SubEntry = (HOOKFUNC)myfunc_menu1item2sub1;
    DoMethod(menu1item2sub1, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_menu1item2sub1);
     

    myhook_menu1item2sub2.h_Entry = HookEntry;
    myhook_menu1item2sub2.h_SubEntry = (HOOKFUNC)myfunc_menu1item2sub2;
    DoMethod(menu1item2sub2, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime , (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_menu1item2sub2);
   
    myhook_menu2item1.h_Entry = HookEntry;
    myhook_menu2item1.h_SubEntry = (HOOKFUNC)myfunc_menu2item1;
    DoMethod(menu2item1, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_menu2item1);





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