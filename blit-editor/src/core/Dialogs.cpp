#include "core/Dialogs.h"

#include "core/Files.h"
#include "core/Process.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>

// one IFileOpenDialog, picking folders or one kind of file
std::string RunOpenDialog(const std::string& title, const std::string& startFolder, bool folders,
                          const std::string& filterName, const std::string& filterPattern)
{
    std::string result;
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dialog = nullptr;

    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, (void**)&dialog)))
    {
        DWORD options = 0;
        std::wstring wideTitle = Widen(title);
        std::wstring wideName = Widen(filterName), widePattern = Widen(filterPattern);
        COMDLG_FILTERSPEC filter = { wideName.c_str(), widePattern.c_str() };

        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | (folders ? (DWORD)FOS_PICKFOLDERS : 0u));
        dialog->SetTitle(wideTitle.c_str());

        if (!folders && !filterPattern.empty())
        {
            dialog->SetFileTypes(1, &filter);
        }

        if (FolderExists(startFolder))
        {
            std::wstring start = Widen(startFolder);
            for (wchar_t& c : start) c = c == L'/' ? L'\\' : c;
            IShellItem* folder = nullptr;

            if (SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&folder))))
            {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        if (SUCCEEDED(dialog->Show(nullptr)))
        {
            IShellItem* item = nullptr;

            if (SUCCEEDED(dialog->GetResult(&item)))
            {
                PWSTR path = nullptr;

                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    result = CleanPath(Narrow(path));
                    CoTaskMemFree(path);
                }

                item->Release();
            }
        }

        dialog->Release();
    }

    if (SUCCEEDED(init))
    {
        CoUninitialize();
    }

    return result;
}

std::string PickFolder(const std::string& title, const std::string& startFolder)
{
    return RunOpenDialog(title, startFolder, true, "", "");
}

std::string PickFile(const std::string& title, const std::string& startFolder, const std::string& filterName,
                     const std::string& filterPattern)
{
    return RunOpenDialog(title, startFolder, false, filterName, filterPattern);
}

void OpenWithSystem(const std::string& path)
{
    std::wstring wide = Widen(path);
    for (wchar_t& c : wide) c = c == L'/' ? L'\\' : c;
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

#else

std::string PickFolder(const std::string&, const std::string&) { return ""; }
std::string PickFile(const std::string&, const std::string&, const std::string&, const std::string&) { return ""; }
void OpenWithSystem(const std::string&) {}

#endif
