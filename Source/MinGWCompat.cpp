// Only compiled for MinGW builds. JUCE calls TaskDialogIndirect, which exists only in
// Common Controls v6. Linking it directly makes Windows refuse to load the plugin/app
// ("entry point not found") when the v5 comctl32 gets bound. Instead we provide the
// import slot ourselves and resolve the function at runtime.
#ifdef __MINGW32__
#include <windows.h>
#include <commctrl.h>

namespace
{
    using TaskDialogIndirectFn = HRESULT (WINAPI*) (const TASKDIALOGCONFIG*, int*, int*, BOOL*);

    HRESULT WINAPI taskDialogIndirectShim (const TASKDIALOGCONFIG* config, int* button, int* radio, BOOL* verify)
    {
        static TaskDialogIndirectFn fn = []() -> TaskDialogIndirectFn
        {
            if (auto* lib = LoadLibraryW (L"comctl32.dll"))
                return reinterpret_cast<TaskDialogIndirectFn> (reinterpret_cast<void*> (GetProcAddress (lib, "TaskDialogIndirect")));
            return nullptr;
        }();

        if (fn != nullptr)
            return fn (config, button, radio, verify);

        // Fallback: plain message box with the dialog's text.
        const int r = MessageBoxW (config != nullptr ? config->hwndParent : nullptr,
                                   config != nullptr ? config->pszContent : L"",
                                   config != nullptr ? config->pszWindowTitle : L"",
                                   MB_OK);
        if (button != nullptr) *button = r;
        if (radio != nullptr) *radio = 0;
        if (verify != nullptr) *verify = FALSE;
        return S_OK;
    }
}

extern "C" TaskDialogIndirectFn __imp_TaskDialogIndirect = &taskDialogIndirectShim;
#endif
