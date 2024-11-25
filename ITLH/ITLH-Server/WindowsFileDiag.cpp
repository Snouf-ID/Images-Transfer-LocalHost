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