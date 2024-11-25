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

    // Cr�er l'objet FileDialog
    IFileDialog* pFileDialog = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileDialog, reinterpret_cast<void**>(&pFileDialog));
    if (FAILED(hr)) {
        std::cerr << "Impossible de cr�er la bo�te de dialogue. Erreur : " << std::hex << hr << std::endl;
        CoUninitialize();
        return "";
    }

    // Configuration pour s�lectionner un dossier
    DWORD options;
    hr = pFileDialog->GetOptions(&options);
    if (SUCCEEDED(hr)) {
        pFileDialog->SetOptions(options | FOS_PICKFOLDERS);
    }

    //// D�finir un titre personnalis�
    //hr = pFileDialog->SetTitle(L"Veuillez s�lectionner un dossier pour sauvegarder les images");
    //if (FAILED(hr)) {
    //    std::cerr << "Impossible de d�finir le titre." << std::endl;
    //}

    // Afficher la bo�te de dialogue
    hr = pFileDialog->Show(NULL);
    if (FAILED(hr)) {
        std::cerr << "Aucun dossier s�lectionn� ou erreur. Erreur : " << std::hex << hr << std::endl;
        pFileDialog->Release();
        CoUninitialize();
        return "";
    }

    // R�cup�rer le chemin du dossier s�lectionn�
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

    // Lib�rer les ressources COM
    pFileDialog->Release();
    CoUninitialize();

    return "";
}