#pragma once
#include "model.hpp"
#include <array>
#include <optional>
#include <string_view>

namespace uw::updates {
inline constexpr char repository[] = "Galaxy-NCC-1701-D/codex-usage-widget-windows";
inline constexpr char apiHost[] = "api.github.com";
inline constexpr char downloadHost[] = "github.com";
inline constexpr std::size_t maxInstallerBytes = 64 * 1024 * 1024;
using Version = std::array<unsigned, 3>;
using Digest = std::array<unsigned char, 32>;

inline std::optional<Version> parseVersion(std::string_view text) {
    Version version{};
    for (std::size_t index = 0; index < version.size(); ++index) {
        auto end = text.find('.');
        if ((end == text.npos) != (index + 1 == version.size())) return std::nullopt;
        auto part = text.substr(0, end);
        if (part.empty() || part.size() > 5 || (part.size() > 1 && part.front() == '0'))
            return std::nullopt;
        for (char digit : part) {
            if (digit < '0' || digit > '9') return std::nullopt;
            version[index] = version[index] * 10 + static_cast<unsigned>(digit - '0');
        }
        if (version[index] > 65535) return std::nullopt;
        if (end != text.npos) text.remove_prefix(end + 1);
    }
    return version;
}

inline std::optional<Digest> parseDigest(std::string_view value) {
    if (value.size() != 71 || value.substr(0, 7) != "sha256:") return std::nullopt;
    value.remove_prefix(7);
    auto hex = [](char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    Digest digest{};
    for (std::size_t index = 0; index < digest.size(); ++index) {
        int high = hex(value[index * 2]);
        int low = hex(value[index * 2 + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        digest[index] = static_cast<unsigned char>(high * 16 + low);
    }
    return digest;
}

struct Release {
    std::string tag, notes, assetName;
    std::size_t size = 0;
    std::optional<Digest> digest;
    std::string page() const {
        return "https://" + std::string(downloadHost) + "/" + repository + "/releases/tag/" + tag;
    }
    std::string assetPath() const {
        return "/" + std::string(repository) + "/releases/download/" + tag + "/" + assetName;
    }
};

inline std::optional<Release> newerRelease(const Json& json, const Version& installed) {
    const auto& draft = json.at("draft");
    const auto& preview = json.at("prerelease");
    const auto& tag = json.at("tag_name").str;
    if (draft.type != Json::Boolean || preview.type != Json::Boolean || draft.number ||
        preview.number || tag.empty() || tag.front() != 'v') return std::nullopt;
    auto version = parseVersion(std::string_view(tag).substr(1));
    if (!version || *version <= installed) return std::nullopt;

    Release release;
    release.tag = tag;
    release.notes = json.at("body").str.substr(0, 12000);
    release.assetName = "UsageWidget-Setup-" + tag.substr(1) + "-x64.exe";
    for (const auto& asset : json.at("assets").arr) {
        if (asset.at("name").str != release.assetName || asset.at("state").str != "uploaded") continue;
        const auto& size = asset.at("size");
        if (size.type != Json::Number || size.number < 2 || size.number > maxInstallerBytes ||
            std::floor(size.number) != size.number) continue;
        release.size = static_cast<std::size_t>(size.number);
        release.digest = parseDigest(asset.at("digest").str);
        break;
    }
    return release;
}
}
