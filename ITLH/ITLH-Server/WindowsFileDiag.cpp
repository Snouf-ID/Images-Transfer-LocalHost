#include "WindowsFileDiag.h"

#include <windows.h>
#include <shobjidl.h> // Pour IFileDialog
#include <string>
#include <iostream>
#include <combaseapi.h> // Pour CoInitialize et CoUninitialize

std::string WindowsFileDiag::select_folder()
{
    // Initialiser COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        std::cerr << "Impossible d'initialiser COM. Erreur : " << std::hex << hr << std::endl;
        return "";
    }

    // Créer l'objet FileDialog
    IFileDialog* pFileDialog = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileDialog, reinterpret_cast<void**>(&pFileDialog));
    if (FAILED(hr)) {
        std::cerr << "Impossible de créer la boîte de dialogue. Erreur : " << std::hex << hr << std::endl;
        CoUninitialize();
        return "";
    }

    // Configuration pour sélectionner un dossier
    DWORD options;
    hr = pFileDialog->GetOptions(&options);
    if (SUCCEEDED(hr)) {
        pFileDialog->SetOptions(options | FOS_PICKFOLDERS);
    }

    //// Définir un titre personnalisé
    //hr = pFileDialog->SetTitle(L"Veuillez sélectionner un dossier pour sauvegarder les images");
    //if (FAILED(hr)) {
    //    std::cerr << "Impossible de définir le titre." << std::endl;
    //}

    // Afficher la boîte de dialogue
    hr = pFileDialog->Show(NULL);
    if (FAILED(hr)) {
        std::cerr << "Aucun dossier sélectionné ou erreur. Erreur : " << std::hex << hr << std::endl;
        pFileDialog->Release();
        CoUninitialize();
        return "";
    }

    // Récupérer le chemin du dossier sélectionné
    IShellItem* pItem = nullptr;
    hr = pFileDialog->GetResult(&pItem);
    if (SUCCEEDED(hr)) {
        PWSTR folderPath = nullptr;
        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &folderPath);
        if (SUCCEEDED(hr)) {
            std::wstring ws(folderPath);
            CoTaskMemFree(folderPath);
            pItem->Release();
            pFileDialog->Release();
            CoUninitialize();

            // Convertir std::wstring en std::string
            return std::string(ws.begin(), ws.end());
        }
        pItem->Release();
    }

    // Libérer les ressources COM
    pFileDialog->Release();
    CoUninitialize();

    return "";
}

void WindowsFileDiag::apply_last_modified(const std::string& file_path, double last_modified_time)
{
    // Convertir le timestamp en structure time_t
    auto file_time = static_cast<std::time_t>(last_modified_time / 1000.0);

    HANDLE file_handle = CreateFileA(
        file_path.c_str(),
        FILE_WRITE_ATTRIBUTES,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file_handle == INVALID_HANDLE_VALUE) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier pour modifier les attributs." << std::endl;
        return;
    }

    // Convertir time_t en FILETIME
    FILETIME ft;
    ULARGE_INTEGER ull;
    ull.QuadPart = static_cast<uint64_t>(file_time) * 10000000ULL + 116444736000000000ULL;
    ft.dwLowDateTime = static_cast<DWORD>(ull.QuadPart);
    ft.dwHighDateTime = static_cast<DWORD>(ull.QuadPart >> 32);

    // Appliquer la date de modification
    if (!SetFileTime(file_handle, nullptr, nullptr, &ft)) {
        std::cerr << "Erreur : Impossible de modifier la date du fichier." << std::endl;
    }

    CloseHandle(file_handle);
}


// Fonction pour convertir un timestamp en ms depuis Unix epoch en FILETIME
static FILETIME convertToFileTime(uint64_t timestamp_ms) {
    // Convertir de millisecondes en "100-nanosecond intervals"
    uint64_t intervals = timestamp_ms * 10000ULL + 116444736000000000ULL;

    FILETIME ft;
    ft.dwLowDateTime = static_cast<DWORD>(intervals);
    ft.dwHighDateTime = static_cast<DWORD>(intervals >> 32);
    return ft;
}

void WindowsFileDiag::setFileCreationTime(const std::string& filePath, uint64_t creationTimeMs) {
    // Ouvrir le fichier avec les privilèges nécessaires
    HANDLE fileHandle = CreateFileA(
        filePath.c_str(),
        FILE_WRITE_ATTRIBUTES,  // Autorisation pour modifier les attributs
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (fileHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "Impossible d'ouvrir le fichier. Code d'erreur : " << GetLastError() << std::endl;
        return;
    }

    // Convertir le timestamp en FILETIME
    FILETIME creationTime = convertToFileTime(creationTimeMs);

    // Modifier uniquement la date de création
    if (!SetFileTime(fileHandle, &creationTime, nullptr, nullptr)) {
        std::cerr << "Impossible de modifier la date de création. Code d'erreur : " << GetLastError() << std::endl;
    }
    else {
        std::cout << "Date de création modifiée avec succès." << std::endl;
    }

    // Fermer le handle du fichier
    CloseHandle(fileHandle);
}