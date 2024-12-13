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

/**
 * @class WindowsFileDiag
 * @brief Provides utilities for file and folder selection dialogs on Windows.
 *
 * This method opens a folder selection dialog using the Windows COM-based File Dialog API.
 * It allows the user to select a folder and retrieves the absolute path as a `std::string`.
 *
 * The function initializes the COM library, creates and configures a `FileOpenDialog` instance,
 * and presents it to the user. If a folder is successfully selected, its path is retrieved
 * via the `IShellItem` interface and converted from `std::wstring` to `std::string`.
 * The function ensures all COM resources are properly released before returning.
 *
 * @return A `std::string` containing the absolute path of the selected folder.
 * If no folder is selected or an error occurs, an empty string is returned.
 *
 * @exception std::runtime_error Thrown if COM initialization or dialog creation fails.
 *
 * @note This implementation uses `FOS_PICKFOLDERS` to configure the dialog for folder selection.
 * Conversion from `std::wstring` to `std::string` may result in data loss for non-ASCII folder paths.
 * Consider using `std::wstring` if Unicode support is required.
 *
 * @warning Ensure the calling thread is compatible with COM (STA or MTA). Failing to properly initialize
 * or uninitialize COM can lead to undefined behavior.
 */
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

/**
 * @brief Sets the last modified time of a file using a `FILETIME` structure.
 *
 * This function opens a file specified by its path and updates its last modified time.
 * It uses the Windows API `SetFileTime` to apply the provided `FILETIME` value to the file.
 *
 * @param file_path The path to the file whose attributes are to be modified.
 * @param ft The `FILETIME` structure containing the desired last modified time.
 *
 * @note Ensure the caller has the necessary write permissions for the file.
 * The file handle is closed after the operation to prevent resource leaks.
 *
 * @warning If the file cannot be opened or its attributes cannot be modified,
 * an error message is logged to `std::cerr`, and no changes are made.
 */
static void set_file_creation_time(const std::string& file_path, const FILETIME& ft)
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

    if (!SetFileTime(file_handle, &ft, nullptr, nullptr))
    {
        std::cerr << "Error : Can't change file time attribut." << std::endl;
    }

    CloseHandle(file_handle);
}

/**
 * @brief Updates the last modified time of a file based on a Unix timestamp.
 *
 * Converts the given Unix timestamp (in milliseconds since epoch) to a `FILETIME` format
 * and applies it as the file's last modified time. This is done by calling
 * the `set_file_time` function internally.
 *
 * @param file_path The path to the file to be updated.
 * @param last_modified_time The last modified timestamp in milliseconds since Unix epoch.
 *
 * @note The function assumes the provided timestamp is in UTC and converts it to `FILETIME`
 * using the Windows epoch offset.
 *
 * @see set_file_time()
 */
void WindowsFileDiag::apply_last_modified_date_on_file(const std::string& file_path, double last_modified_time)
{
    FILETIME ft;
    uint64_t file_time_intervals = static_cast<uint64_t>(last_modified_time * 10000.0) + 116444736000000000ULL;
    ft.dwLowDateTime = static_cast<DWORD>(file_time_intervals);
    ft.dwHighDateTime = static_cast<DWORD>(file_time_intervals >> 32);

    set_file_creation_time(file_path, ft);
}

/**
 * @brief Applies the "Date Taken" metadata as the file's last modified time.
 *
 * This function retrieves the "Date Taken" metadata from the specified file's properties
 * using the Windows Property Store API. It converts the metadata from `FILETIME` to `SYSTEMTIME`,
 * and then applies it as the file's last modified time.
 *
 * The steps performed are as follows:
 * - Initializes COM for the current thread.
 * - Converts the file path from a `std::string` to a `std::wstring`.
 * - Opens the file's property store to access its metadata.
 * - Retrieves the `PKEY_Photo_DateTaken` property, which contains the "Date Taken" information, in `FILETIME` format.
 * - Converts the `FILETIME` to `SYSTEMTIME` (UTC).
 * - Converts the `SYSTEMTIME` to `FILETIME` for application to the file.
 * - Releases all resources and uninitializes COM.
 * - Updates the file's creation time to match the "Date Taken" value.
 *
 * @param file_path The path to the file whose metadata will be used to set the last modified time.
 *
 * @note This function assumes that the "Date Taken" metadata is available as a `FILETIME`.
 * It applies the metadata as the file's last modified time, regardless of the current local time zone.
 *
 * @warning Ensure that the calling thread is initialized with COM. The function handles all necessary COM initialization and cleanup.
 *
 * @exception std::runtime_error Thrown if COM initialization or metadata retrieval fails.
 */
bool WindowsFileDiag::apply_metadata_date_on_file(const std::string& file_path)
{
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
    {
        std::cerr << "Error Init Com : " << std::hex << hr << std::endl;
        return false;
    }

    // convert string to wstring, warning !
    std::wstring file_path_wstring(file_path.begin(), file_path.end());

    // Ouvrir le fichier avec le Property Store
    IPropertyStore* property_store = nullptr;
    hr = SHGetPropertyStoreFromParsingName(file_path_wstring.c_str(), nullptr, GPS_DEFAULT, IID_PPV_ARGS(&property_store));
    //SHGetPropertyStoreFromParsingName call can generate warning print like : "Corrupt JPEG data: 2809223 extraneous bytes before marker 0xd9", it's not a pb, file open correctly on windows after
    if (FAILED(hr))
    {
        // we can save file that fail at this position : "JPEG datastream contains no image" print and hr is ERROR. This path indicate file can't be open by windows after.
        std::cerr << "Error Accessing File Metadata : " << std::hex << hr << std::endl;
        CoUninitialize();
        return false;
    }

    // Récupérer la date de prise de vue (PKEY_Photo_DateTaken)
    PROPVARIANT prop_var_date_taken;
    PropVariantInit(&prop_var_date_taken);
    hr = property_store->GetValue(PKEY_Photo_DateTaken, &prop_var_date_taken);
    if (!(SUCCEEDED(hr) && prop_var_date_taken.vt == VT_FILETIME))
    {
        //std::cerr << "Error Get PKEY_Photo_DateTaken Property." << std::endl;
        return true;
    }

    SYSTEMTIME system_time;

    // Convertir FILETIME en SYSTEMTIME (UTC)
    if (!FileTimeToSystemTime(&prop_var_date_taken.filetime, &system_time))
    {
        //std::cerr << "Error convert FILETIME en SYSTEMTIME." << std::endl;
        return true;
    }

    FILETIME local_file_time;

    // TODO : really need FileTime -> SystemTime -> FileTime ???
    if (!SystemTimeToFileTime(&system_time, &local_file_time))
    {
        //std::cerr << "Error convert SYSTEMTIME in FILETIME." << std::endl;
        return true;
    }

    PropVariantClear(&prop_var_date_taken);
    property_store->Release();
    CoUninitialize();

    set_file_creation_time(file_path, local_file_time);

    return true;
}
