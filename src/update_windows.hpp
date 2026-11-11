#pragma once
#include "update_release.hpp"
#include "version.h"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <objbase.h>
#include <atomic>
#include <memory>

namespace uw::updates {
class InternetHandle {
    HINTERNET handle;
public:
    explicit InternetHandle(HINTERNET value) : handle(value) {}
    ~InternetHandle() { if (handle) WinHttpCloseHandle(handle); }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    operator HINTERNET() const { return handle; }
};

inline std::wstring asciiWide(std::string_view text) {
    return {text.begin(), text.end()};
}

// All network requests run off the window thread, with normal certificate
// validation, finite WinHTTP timeouts and bounded response sizes.
template<class Receive>
DWORD get(const char* host, const std::string& path, bool api, std::size_t limit,
          const std::atomic<bool>& stop, Receive receive) {
    const auto started = GetTickCount64();
    InternetHandle session(WinHttpOpen(L"UsageWidget/" UW_VERSION_W,
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session || !WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000))
        throw std::runtime_error("Could not start the GitHub connection.");
    auto server = asciiWide(host);
    InternetHandle connection(WinHttpConnect(session, server.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) throw std::runtime_error("Could not connect to GitHub.");
    auto route = asciiWide(path);
    InternetHandle request(WinHttpOpenRequest(connection, L"GET", route.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) throw std::runtime_error("Could not create the GitHub request.");
    DWORD redirect = api ? WINHTTP_OPTION_REDIRECT_POLICY_NEVER :
        WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    DWORD maxRedirects = 3;
    if (!WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)) ||
        !WinHttpSetOption(request, WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS,
                         &maxRedirects, sizeof(maxRedirects)))
        throw std::runtime_error("Could not configure GitHub redirects.");
    const wchar_t* headers = api ? L"Accept: application/vnd.github+json\r\n" :
        WINHTTP_NO_ADDITIONAL_HEADERS;
    if (stop || !WinHttpSendRequest(request, headers, api ? static_cast<DWORD>(-1) : 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr))
        throw std::runtime_error("GitHub connection failed. Check your network or proxy.");
    DWORD status = 0;
    DWORD length = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX))
        throw std::runtime_error("GitHub returned an invalid HTTP response.");
    if (status != 200) return status;
    char bytes[32768];
    std::size_t total = 0;
    for (;;) {
        if (stop || GetTickCount64() - started > 120000)
            throw std::runtime_error("GitHub request timed out or was cancelled.");
        DWORD read = 0;
        if (!WinHttpReadData(request, bytes, sizeof(bytes), &read))
            throw std::runtime_error("GitHub transfer was interrupted.");
        if (!read) return status;
        if (read > limit - total) throw std::runtime_error("GitHub response is larger than expected.");
        total += read;
        receive(bytes, read);
    }
}

inline std::optional<Release> check(const std::atomic<bool>& stop) {
    std::string response;
    DWORD status = get(apiHost, "/repos/" + std::string(repository) + "/releases/latest",
        true, 1024 * 1024, stop, [&](const char* bytes, DWORD read) { response.append(bytes, read); });
    if (status == 404) throw std::runtime_error("No public stable release was found.");
    if (status == 403 || status == 429)
        throw std::runtime_error("GitHub limited update checks. Try again later.");
    if (status != 200) throw std::runtime_error("GitHub update check failed (HTTP " +
        std::to_string(status) + ").");
    auto json = Parser(response).parse();
    const auto& tag = json.at("tag_name").str;
    if (tag.empty() || tag.front() != 'v' || !parseVersion(std::string_view(tag).substr(1)) ||
        json.at("draft").type != Json::Boolean || json.at("prerelease").type != Json::Boolean)
        throw std::runtime_error("GitHub returned an unsupported release version.");
    return newerRelease(json, *parseVersion(UW_VERSION));
}

class Sha256 {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
public:
    Sha256() {
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            throw std::runtime_error("Windows SHA-256 is unavailable.");
        if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            throw std::runtime_error("Could not create the SHA-256 checker.");
        }
    }
    ~Sha256() { BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm, 0); }
    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;
    void add(unsigned char* data, DWORD length) {
        if (BCryptHashData(hash, data, length, 0) < 0)
            throw std::runtime_error("Could not hash the installer.");
    }
    Digest finish() {
        Digest digest{};
        if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
            throw std::runtime_error("Could not finish installer verification.");
        return digest;
    }
};

struct Installer {
    std::wstring path;
    HANDLE file = INVALID_HANDLE_VALUE;
    bool started = false;
    ~Installer() {
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        if (!started && !path.empty()) DeleteFileW(path.c_str());
    }
    HANDLE launch() {
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        std::wstring command = L"\"" + path + L"\"";
        if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, nullptr, &startup, &process)) return nullptr;
        CloseHandle(process.hThread);
        started = true;
        return process.hProcess;
    }
};

inline std::unique_ptr<Installer> download(const Release& release, const std::wstring& directory,
                                           const std::atomic<bool>& stop) {
    if (!release.digest || !release.size) throw std::runtime_error("Installer verification data is missing.");
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        throw std::runtime_error("Could not create the update folder.");

    GUID id{};
    wchar_t token[40]{};
    if (FAILED(CoCreateGuid(&id)) || !StringFromGUID2(id, token, 40))
        throw std::runtime_error("Could not create a unique installer filename.");
    auto installer = std::make_unique<Installer>();
    auto path = directory + L"\\update-" + token + L".exe";
    installer->file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
    if (installer->file == INVALID_HANDLE_VALUE) throw std::runtime_error("Could not write the installer.");
    installer->path = std::move(path);
    std::size_t total = 0;
    DWORD status = get(downloadHost, release.assetPath(), false, release.size, stop,
        [&](const char* data, DWORD count) {
            DWORD written = 0;
            if (!WriteFile(installer->file, data, count, &written, nullptr) || written != count)
                throw std::runtime_error("Could not save the installer.");
            total += written;
        });
    if (status != 200 || total != release.size)
        throw std::runtime_error("The installer download is incomplete or unavailable.");
    if (!FlushFileBuffers(installer->file)) throw std::runtime_error("Could not flush the installer to disk.");
    CloseHandle(installer->file);
    // Keep this read-only handle open until launch. It denies writes and
    // replacement while verifying the exact bytes Windows will execute.
    installer->file = CreateFileW(installer->path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (installer->file == INVALID_HANDLE_VALUE) throw std::runtime_error("Could not verify the saved installer.");
    Sha256 sha;
    unsigned char buffer[32768];
    std::size_t checked = 0;
    for (;;) {
        DWORD count = 0;
        if (stop || !ReadFile(installer->file, buffer, sizeof(buffer), &count, nullptr))
            throw std::runtime_error("Installer verification was interrupted.");
        if (!count) break;
        if ((!checked && (count < 2 || buffer[0] != 'M' || buffer[1] != 'Z')) ||
            count > release.size - checked) throw std::runtime_error("Invalid installer file.");
        checked += count;
        sha.add(buffer, count);
    }
    if (checked != release.size || sha.finish() != *release.digest)
        throw std::runtime_error("Installer SHA-256 mismatch. The installer was blocked.");
    return installer;
}
}
