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

Object *app,*window, *txt_obj;
Object *menu_new, *menu_open, *menu_cut_normal, *menu_cut_full, *menu_paste;

static struct Hook myhook_menu_new;
static struct Hook myhook_menu_open;
static struct Hook myhook_menu_cut_normal;
static struct Hook myhook_menu_cut_full;
static struct Hook myhook_menu_paste;


static ULONG myfunc_menu_new(struct Hook *hook, Object *object, APTR msg)
{
  set(txt_obj, MUIA_Text_Contents, "You have clicked on 'new' menu item");
}

static ULONG myfunc_menu_open(struct Hook *hook, Object *object, APTR msg)
{
  set(txt_obj, MUIA_Text_Contents, "You have clicked on 'open' menu item");
}

static ULONG myfunc_menu_cut_normal(struct Hook *hook, Object *object, APTR msg)
{
  set(txt_obj, MUIA_Text_Contents, "You have clicked on 'cut -> normal' menu item");
}

static ULONG myfunc_menu_cut_full(struct Hook *hook, Object *object, APTR msg)
{
  set(txt_obj, MUIA_Text_Contents, "You have clicked on 'cut -> full' menu item");
}

static ULONG myfunc_menu_paste(struct Hook *hook, Object *object, APTR msg)
{
  set(txt_obj, MUIA_Text_Contents, "You have clicked on 'paste' menu item");
}

int main(int argc,char *argv[])
{
    app = ApplicationObject,
        MUIA_Application_Title      , "App main menu",
        MUIA_Application_Version    , "1.0",
        MUIA_Application_Copyright  , "©2012",
        MUIA_Application_Author     , "Robert Negro",
        MUIA_Application_Description, "For learning purposes",
        MUIA_Application_Base       , "CLASS2",


        MUIA_Application_Menustrip, 
        
        // File menu
        MenuitemObject,
        MUIA_Family_Child, MenuitemObject,
        MUIA_Menuitem_Title, "File",
          MUIA_Family_Child, menu_new = MenuitemObject, MUIA_Menuitem_Title,"New",MUIA_Menuitem_Shortcut,"n",End,
          MUIA_Family_Child, menu_open = MenuitemObject, MUIA_Menuitem_Title,"Open",MUIA_Menuitem_Shortcut,"o",End,
        End,
           
        // Edit menu
        MUIA_Family_Child, MenuitemObject,
        MUIA_Menuitem_Title, "Edit",
          MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,"Cut",
            MUIA_Family_Child, menu_cut_normal = MenuitemObject, MUIA_Menuitem_Title,"Normal",MUIA_Menuitem_Shortcut,"c",End,
            MUIA_Family_Child, menu_cut_full = MenuitemObject, MUIA_Menuitem_Title,"Full",MUIA_Menuitem_Shortcut,"f",End,
          End,
          MUIA_Family_Child, menu_paste = MenuitemObject, MUIA_Menuitem_Title,"Paste",MUIA_Menuitem_Shortcut,"p",End,
        End,
        
	End,

        
        
        SubWindow,
            window = WindowObject,
            MUIA_Window_Title, "Main menu demo",
            MUIA_Window_Height, 300,
            MUIA_Window_Width, 400,
            WindowContents, 
            
            VGroup,
            Child,
                 txt_obj = TextObject,
                    MUIA_Text_Contents, "Some text...",
                 End,End,
            End,
         End;



    
    // attach menu responding functions to hooks, and then hooks to menu items
    myhook_menu_new.h_Entry = HookEntry;
    myhook_menu_new.h_SubEntry = (HOOKFUNC)myfunc_menu_new;
    DoMethod(menu_new, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_menu_new);
        
    myhook_menu_open.h_Entry = HookEntry;
    myhook_menu_open.h_SubEntry = (HOOKFUNC)myfunc_menu_open;
    DoMethod(menu_open, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_menu_open);
    
    myhook_menu_cut_normal.h_Entry = HookEntry;
    myhook_menu_cut_normal.h_SubEntry = (HOOKFUNC)myfunc_menu_cut_normal;
    DoMethod(menu_cut_normal, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_menu_cut_normal);
    
    myhook_menu_cut_full.h_Entry = HookEntry;
    myhook_menu_cut_full.h_SubEntry = (HOOKFUNC)myfunc_menu_cut_full;
    DoMethod(menu_cut_full, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_menu_cut_full);
  
    myhook_menu_paste.h_Entry = HookEntry;
    myhook_menu_paste.h_SubEntry = (HOOKFUNC)myfunc_menu_paste;
    DoMethod(menu_paste, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, (IPTR)app, 2, MUIM_CallHook, (IPTR)&myhook_menu_paste);


  

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