#include "file_dialog.h"
#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
std::string pickFolderDialog(const char* title) {
    std::string out;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        if (title) { wchar_t w[256]; MultiByteToWideChar(CP_UTF8, 0, title, -1, w, 256); dlg->SetTitle(w); }
        if (SUCCEEDED(dlg->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    char buf[1024];
                    WideCharToMultiByte(CP_UTF8, 0, path, -1, buf, sizeof(buf), nullptr, nullptr);
                    out = buf;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dlg->Release();
    }
    if (SUCCEEDED(hr)) CoUninitialize();
    return out;
}
#else
std::string pickFolderDialog(const char*) { return ""; }
#endif
