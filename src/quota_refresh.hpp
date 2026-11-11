#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

namespace uw::refresh {
inline constexpr unsigned minimumSeconds = 5;
inline constexpr unsigned maximumSeconds = 1800;
inline constexpr unsigned defaultSeconds = 120;

inline std::optional<unsigned> parseInterval(std::wstring_view input) {
    if (input.empty() || input.size() > 4) return std::nullopt;
    unsigned seconds = 0;
    for (wchar_t digit : input) {
        if (digit < L'0' || digit > L'9') return std::nullopt;
        seconds = seconds * 10 + static_cast<unsigned>(digit - L'0');
    }
    if (seconds < minimumSeconds || seconds > maximumSeconds) return std::nullopt;
    return seconds;
}

// The worker owns this clock. Reading the current interval at each decision
// also applies settings changes made while a request is in progress.
class Schedule {
    std::uint64_t completedAt = 0;
    std::uint64_t retryAt = 0;
    unsigned failures = 0;
public:
    void complete(std::uint64_t now, bool success) {
        completedAt = now;
        if (success) {
            failures = 0;
            retryAt = 0;
        } else {
            failures = std::min(failures + 1, 5u);
            retryAt = now + 30000ULL * (1u << (failures - 1));
        }
    }

    bool due(std::uint64_t now, unsigned seconds, bool requested) const {
        if (requested) return true;
        return seconds && now >= retryAt &&
            now >= completedAt + 1000ULL * seconds;
    }
};

inline unsigned staleAfter(unsigned automaticSeconds) {
    return automaticSeconds ? std::max(150u, automaticSeconds + 60u) : 150u;
}
}
