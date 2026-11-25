#pragma once
#include "model.hpp"
#include <array>
#include <string_view>

namespace uw::connection {
enum class Mode { Https, WebSocket, DirectHttps };
enum class Startup { Off, Checking, Selected, Failed, Manual };

inline bool usesHttps(Mode mode) { return mode != Mode::WebSocket; }
inline bool direct(Mode mode) { return mode == Mode::DirectHttps; }

inline const wchar_t* label(Mode mode) {
    switch (mode) {
    case Mode::Https: return L"HTTPS";
    case Mode::WebSocket: return L"Default (WebSocket)";
    case Mode::DirectHttps: return L"HTTPS Direct";
    }
    return L"HTTPS";
}

inline const wchar_t* setting(Mode mode) {
    switch (mode) {
    case Mode::Https: return L"https";
    case Mode::WebSocket: return L"websocket";
    case Mode::DirectHttps: return L"https-direct";
    }
    return L"https";
}

inline Mode readMode(std::wstring_view value, bool legacyHttps) {
    if (value == L"https-direct") return Mode::DirectHttps;
    if (value == L"websocket") return Mode::WebSocket;
    if (value == L"https") return Mode::Https;
    return legacyHttps ? Mode::Https : Mode::WebSocket;
}

class StartupOrder {
    std::array<Mode, 3> modes{};
    std::size_t index = 0;
public:
    explicit StartupOrder(Mode preferred) {
        modes[0] = preferred;
        std::size_t next = 1;
        for (auto mode : {Mode::Https, Mode::WebSocket, Mode::DirectHttps}) {
            if (mode != preferred) modes[next++] = mode;
        }
    }
    Mode candidate() const { return modes[index]; }
    bool advance() {
        if (index + 1 == modes.size()) return false;
        ++index;
        return true;
    }
};

inline bool freshQuota(const Json& reply, int requestId) {
    const auto& id = reply.at("id");
    if (id.type != Json::Number || id.number != requestId || reply.has("error") ||
        reply.at("result").type != Json::Object) return false;
    const auto quota = extract(reply);
    return quota.five.available || quota.week.available;
}
}
