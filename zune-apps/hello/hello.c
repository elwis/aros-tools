#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;

int main(void)
{
    Object *app, *window, *button;

    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase)
        return 1;

    app = ApplicationObject,
        MUIA_Application_Title, "Hello",
        SubWindow, window = WindowObject,
            MUIA_Window_Title, "Hello Zune",
            MUIA_Window_RootObject, button = SimpleButton("Hello AROS!"),
        End,
    End;

    if (app)
    {
        ULONG sigs = 0;
        SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
        DoMethod(button, MUIM_Notify, MUIA_Pressed, FALSE,
                 app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

        while (DoMethod(app, MUIM_Application_NewInput, &sigs)
               != MUIV_Application_ReturnID_Quit)
        {
            if (sigs)
                sigs = Wait(sigs | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C)
                break;
        }
        MUI_DisposeObject(app);
    }

    CloseLibrary(MUIMasterBase);
    return 0;
}