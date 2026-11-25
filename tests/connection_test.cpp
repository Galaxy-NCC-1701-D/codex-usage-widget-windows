#include "../src/connection_mode.hpp"
#include "../src/runtime_paths.hpp"
#include "../src/transport.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace uw::connection;
    assert(readMode(L"", true) == Mode::Https);
    assert(readMode(L"", false) == Mode::WebSocket);
    for (auto preferred : {Mode::Https, Mode::WebSocket, Mode::DirectHttps}) {
        assert(readMode(setting(preferred), false) == preferred);
        StartupOrder order(preferred);
        assert(order.candidate() == preferred);
        auto first = order.candidate();
        assert(order.advance());
        auto second = order.candidate();
        assert(order.advance());
        auto third = order.candidate();
        assert(first != second && first != third && second != third && !order.advance());
    }
    assert(usesHttps(Mode::Https) && !direct(Mode::Https));
    assert(!usesHttps(Mode::WebSocket) && !direct(Mode::WebSocket));
    assert(usesHttps(Mode::DirectHttps) && direct(Mode::DirectHttps));

    const std::string full = R"({"id":1001,"result":{"rateLimits":{"primary":{"windowDurationMins":300,"usedPercent":100}}}})";
    assert(freshQuota(uw::Parser(full).parse(), 1001)); // A fully used window still counts.
    assert(!freshQuota(uw::Parser(full).parse(), 1002));
    assert(freshQuota(uw::Parser(R"({"id":4,"result":{"rateLimitsByLimitId":{"codex":{"secondary":{"windowDurationMins":10080,"usedPercent":0}}}}})").parse(), 4));
    for (const auto* payload : {
        R"({"id":4,"result":{}})",
        R"({"id":4,"error":{"code":401}})",
        R"({"id":4,"result":{"rateLimitResetCredits":{"availableCount":1}}})",
        R"({"id":4,"result":{"rateLimits":{"primary":{"windowDurationMins":300,"usedPercent":101}}}})",
        R"({"method":"account/rateLimits/updated","params":{"rateLimits":{"primary":{"windowDurationMins":300,"usedPercent":30}}}})"
    }) assert(!freshQuota(uw::Parser(payload).parse(), 4));

    using uw::runtime::environment;
    const std::vector<std::wstring> inherited = {L"PATH=original", L"https_proxy=http://127.0.0.1:8000",
        L"Http_Proxy=proxy", L"ALL_PROXY=other", L"no_PROXY=internal", L"USERPROFILE=unchanged"};
    auto block = environment(inherited, {}, L"", true);
    std::vector<std::wstring> variables;
    for (auto p = block.data(); *p; p += std::wcslen(p) + 1) variables.emplace_back(p);
    auto has = [&](const wchar_t* value) {
        return std::find(variables.begin(), variables.end(), value) != variables.end();
    };
    assert(variables.size() == inherited.size() && has(L"HTTPS_PROXY=") && has(L"HTTP_PROXY=") &&
        has(L"ALL_PROXY=") && has(L"NO_PROXY=*") && has(L"USERPROFILE=unchanged") && has(L"PATH=original"));
    assert(inherited[1] == L"https_proxy=http://127.0.0.1:8000");
    std::cout << "PASS startup order, fresh quota response matching and direct child proxy isolation\n";
}
