#include "WindowsFileDiag.h"

#include <windows.h>
#include <shobjidl.h>
#include <string>
#include <iostream>
#include <combaseapi.h>
#include <propvarutil.h>
#include <propsys.h>
#include <propkey.h>

//#define NTDDI_VERSION NTDDI_WIN10 // do we need this ?
//#define _WIN32_WINNT _WIN32_WINNT_WIN10 // in visual project property already -> it's for win10 code setup lib

std::string WindowsFileDiag::open_select_folder_diag_window()
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr))
    {
        std::cerr << "Init Com Error : " << std::hex << hr << std::endl;
        throw std::runtime_error("Can't init diag windows for selecting directory");
        return "";
    }

    IFileDialog* file_dialog = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileDialog, reinterpret_cast<void**>(&file_dialog));
    if (FAILED(hr))
    {
        std::cerr << "Create Com Error : " << std::hex << hr << std::endl;
        CoUninitialize();
        throw std::runtime_error("Can't create diag windows for selecting directory");
        return "";
    }

    DWORD options;
    hr = file_dialog->GetOptions(&options);
    if (SUCCEEDED(hr))
    {
        file_dialog->SetOptions(options | FOS_PICKFOLDERS);
    }

    hr = file_dialog->Show(NULL);
    if (FAILED(hr))
    {
        std::cerr << "No directory select or error : " << std::hex << hr << std::endl;
        file_dialog->Release();
        CoUninitialize();
        return "";
    }

    IShellItem* item = nullptr;
    hr = file_dialog->GetResult(&item);
    if (SUCCEEDED(hr))
    {
        PWSTR folder_path = nullptr;
        hr = item->GetDisplayName(SIGDN_FILESYSPATH, &folder_path);
        if (SUCCEEDED(hr))
        {
            std::wstring ws(folder_path);
            CoTaskMemFree(folder_path);
            item->Release();
            file_dialog->Release();
            CoUninitialize();

            // convert std::wstring en std::string
            // warning C4244: '=' : conversion de 'wchar_t' en 'char', perte possible de données
            return std::string(ws.begin(), ws.end());
        }
        item->Release();
    }

    file_dialog->Release();
    CoUninitialize();

    return "";
}

static void set_file_time(const std::string& file_path, const FILETIME& ft)
{
    HANDLE file_handle = CreateFileA(
        file_path.c_str(),
        FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file_handle == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Error : Can't open file for change file attributes." << std::endl;
        return;
    }

    if (!SetFileTime(file_handle, nullptr, nullptr, &ft))
    {
        std::cerr << "Error : Can't change file time attribut." << std::endl;
    }

    CloseHandle(file_handle);
}

void WindowsFileDiag::apply_last_modified_date_on_file(const std::string& file_path, double last_modified_time)
{
    FILETIME ft;
    uint64_t file_time_intervals = static_cast<uint64_t>(last_modified_time * 10000.0) + 116444736000000000ULL;
    ft.dwLowDateTime = static_cast<DWORD>(file_time_intervals);
    ft.dwHighDateTime = static_cast<DWORD>(file_time_intervals >> 32);

    set_file_time(file_path, ft);
}

void WindowsFileDiag::apply_metadata_date_on_file(const std::string& file_path)
{
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
    {
        std::cerr << "Error Init Com : " << std::hex << hr << std::endl;
        return;
    }

    std::wstring file_path_wstring(file_path.begin(), file_path.end());

    // Ouvrir le fichier avec le Property Store
    IPropertyStore* property_store = nullptr;
    hr = SHGetPropertyStoreFromParsingName(file_path_wstring.c_str(), nullptr, GPS_DEFAULT, IID_PPV_ARGS(&property_store));
    if (FAILED(hr))
    {
        std::cerr << "Error Accessing File Metadata : " << std::hex << hr << std::endl;
        CoUninitialize();
        return;
    }

    // Récupérer la date de prise de vue (PKEY_Photo_DateTaken)
    PROPVARIANT prop_var_date_taken;
    PropVariantInit(&prop_var_date_taken);
    hr = property_store->GetValue(PKEY_Photo_DateTaken, &prop_var_date_taken);
    if (!(SUCCEEDED(hr) && prop_var_date_taken.vt == VT_FILETIME))
    {
        return;
    }

    SYSTEMTIME utc_system_time, local_system_time;

    // Convertir FILETIME en SYSTEMTIME (UTC)
    if (!FileTimeToSystemTime(&prop_var_date_taken.filetime, &utc_system_time))
    {
        std::cerr << "Erreur lors de la conversion FILETIME en SYSTEMTIME." << std::endl;
        return;
    }

    // Convertir SYSTEMTIME (UTC) en SYSTEMTIME (Local) avec gestion des fuseaux horaires
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &utc_system_time, &local_system_time))
    {
        std::cerr << "Erreur lors de la conversion SYSTEMTIME UTC en heure locale." << std::endl;
        return;
    }

    //std::wcout << L"Date de prise de vue (locale) : "
    //    << local_system_time.wYear << L"-" << local_system_time.wMonth << L"-" << local_system_time.wDay << L" "
    //    << local_system_time.wHour << L":" << local_system_time.wMinute << L":" << local_system_time.wSecond
    //    << std::endl;

    FILETIME local_file_time;

    if (!SystemTimeToFileTime(&local_system_time, &local_file_time))
    {
        return;
    }

    PropVariantClear(&prop_var_date_taken);
    property_store->Release();
    CoUninitialize();

    set_file_time(file_path, local_file_time);
}
