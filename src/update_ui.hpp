#pragma once
// Included after the chat UI so installation can wait for ongoing work.
struct AppUpdateResult {
    std::optional<uw::updates::Release> release;
    std::unique_ptr<uw::updates::Installer> installer;
    std::wstring error;
    bool checking = false;
};

std::thread updateWorker;
std::optional<uw::updates::Release> availableRelease;
std::unique_ptr<uw::updates::Installer> downloadedInstaller;
std::wstring updatesMessage = L"Ready to check for updates.";
std::wstring launchedInstallerPath;
HANDLE installerProcess = nullptr;
bool updateBusy = false;
bool automaticUpdates = true;
std::time_t lastUpdateCheck = 0;
ULONGLONG nextUpdateAttempt = 0;

bool updateInstalling() {
    return installerProcess && WaitForSingleObject(installerProcess, 0) == WAIT_TIMEOUT;
}

void repaintSettings() {
    if (settingsWindow) InvalidateRect(settingsWindow, nullptr, FALSE);
}

void removePreviousInstallers() {
    const auto directory = dataDir + L"\\updates\\";
    WIN32_FIND_DATAW entry{};
    auto handle = FindFirstFileW((directory + L"update-{*}.exe").c_str(), &entry);
    if (handle == INVALID_HANDLE_VALUE) return;
    do {
        if (entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
            continue;
        std::wstring name = entry.cFileName;
        if (name.size() != 49 || name.compare(0, 8, L"update-{") != 0 ||
            name.compare(44, 5, L"}.exe") != 0) continue;
        GUID id{};
        if (SUCCEEDED(CLSIDFromString(name.substr(7, 38).c_str(), &id)))
            DeleteFileW((directory + name).c_str());
    } while (FindNextFileW(handle, &entry));
    FindClose(handle);
}

void loadUpdatePrefs() {
    removePreviousInstallers();
    automaticUpdates = GetPrivateProfileIntW(L"Updates", L"AutomaticCheck", 1,
                                              configPath.c_str()) != 0;
    wchar_t value[32]{};
    GetPrivateProfileStringW(L"Updates", L"LastSuccessfulCheck", L"0", value, 32, configPath.c_str());
    wchar_t* end = nullptr;
    auto saved = std::wcstoll(value, &end, 10);
    if (end && !*end && saved > 0 && saved <= std::time(nullptr)) lastUpdateCheck = saved;
    nextUpdateAttempt = GetTickCount64() + 5000;
}

void saveAutomaticUpdates() {
    bool next = !automaticUpdates;
    if (!WritePrivateProfileStringW(L"Updates", L"AutomaticCheck", next ? L"1" : L"0",
                                    configPath.c_str())) {
        updatesMessage = L"Could not save update preferences.";
        return;
    }
    automaticUpdates = next;
    if (automaticUpdates) nextUpdateAttempt = GetTickCount64();
}

void startUpdateWorker(bool checking) {
    if (updateBusy || updateInstalling()) return;
    auto release = availableRelease;
    if (!checking && (!release || !release->digest || !release->size)) return;
    updateBusy = true;
    updatesMessage = checking ? L"Checking GitHub for a stable release..." :
                                L"Downloading and verifying the installer...";
    repaintSettings();
    try {
        updateWorker = std::thread([checking, release = std::move(release)] {
            auto result = std::make_unique<AppUpdateResult>();
            result->checking = checking;
            try {
                if (checking) result->release = uw::updates::check(stopWorker);
                else result->installer = uw::updates::download(*release, dataDir + L"\\updates", stopWorker);
            } catch (const std::exception& error) {
                result->error = wide(error.what());
            }
            if (!stopWorker && PostMessageW(mainWindow, WM_APP_UPDATE, 0,
                                             reinterpret_cast<LPARAM>(result.get()))) result.release();
        });
    } catch (const std::system_error&) {
        updateBusy = false;
        nextUpdateAttempt = GetTickCount64() + 3600000;
        updatesMessage = L"Could not start the update worker.";
        repaintSettings();
    }
}

bool readyForUpgrade() {
    return !chatBusy && !resetBusy && !confirmResetAfterRead && !resetConfirmationOpen && pendingDeleteId.empty() &&
        chatInput.empty() && pendingAttachments.empty();
}

void installDownloadedUpdate() {
    if (!readyForUpgrade()) {
        updatesMessage = L"Finish the current operation or clear the draft, then install.";
        return;
    }
    saveCurrent();
    auto process = downloadedInstaller->launch();
    if (!process) {
        updatesMessage = L"Could not start the installer. Try the Release page.";
        return;
    }
    installerProcess = process;
    launchedInstallerPath = downloadedInstaller->path;
    downloadedInstaller.reset();
    updatesMessage = L"Installer opened. Complete the upgrade there.";
}

void downloadOrInstallUpdate() {
    if (updateBusy || updateInstalling() || !availableRelease || !availableRelease->digest) return;
    if (downloadedInstaller) {
        installDownloadedUpdate();
    } else if (!readyForUpgrade()) {
        updatesMessage = L"Finish the current operation or clear the draft, then update.";
    } else {
        startUpdateWorker(false);
    }
}

void receiveAppUpdate(AppUpdateResult& result) {
    if (updateWorker.joinable()) updateWorker.join();
    updateBusy = false;
    if (!result.error.empty()) {
        updatesMessage = result.error;
        if (result.checking) nextUpdateAttempt = GetTickCount64() + 3600000;
    } else if (result.checking) {
        lastUpdateCheck = std::time(nullptr);
        WritePrivateProfileStringW(L"Updates", L"LastSuccessfulCheck",
            std::to_wstring(lastUpdateCheck).c_str(), configPath.c_str());
        if (!result.release || !availableRelease || result.release->tag != availableRelease->tag ||
            result.release->digest != availableRelease->digest) downloadedInstaller.reset();
        availableRelease = std::move(result.release);
        if (!availableRelease) updatesMessage = L"You have the latest stable version.";
        else if (!availableRelease->digest)
            updatesMessage = L"Verified installer unavailable. Open the Release page.";
        else {
            updatesMessage = L"Stable update " + wide(availableRelease->tag) + L" is available.";
            if (!settingsWindow) {
                auto notification = tray;
                notification.uFlags = NIF_INFO;
                notification.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
                lstrcpynW(notification.szInfoTitle, L"Usage Widget update", ARRAYSIZE(notification.szInfoTitle));
                lstrcpynW(notification.szInfo, L"A stable update is available in Settings > Updates.",
                           ARRAYSIZE(notification.szInfo));
                Shell_NotifyIconW(NIM_MODIFY, &notification);
            }
        }
    } else if (result.installer) {
        downloadedInstaller = std::move(result.installer);
        installDownloadedUpdate();
    }
    repaintSettings();
}

void tickUpdates() {
    if (installerProcess && WaitForSingleObject(installerProcess, 0) != WAIT_TIMEOUT) {
        CloseHandle(installerProcess);
        installerProcess = nullptr;
        DeleteFileW(launchedInstallerPath.c_str());
        launchedInstallerPath.clear();
        updatesMessage = L"Installer closed. Check again or retry the update.";
        repaintSettings();
    }
    const auto now = std::time(nullptr);
    if (automaticUpdates && !updateBusy && !updateInstalling() &&
        GetTickCount64() >= nextUpdateAttempt &&
        (!lastUpdateCheck || now < lastUpdateCheck || now - lastUpdateCheck >= 86400))
        startUpdateWorker(true);
}
