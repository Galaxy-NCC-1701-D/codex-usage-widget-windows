#include "../src/quota_refresh.hpp"
#include "../src/update_release.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace uw::refresh;
    assert(parseInterval(L"5") == 5);
    assert(parseInterval(L"1800") == 1800);
    for (auto input : {L"", L"4", L"1801", L"5.5", L"-5", L"5 sec", L"99999"})
        assert(!parseInterval(input));

    Schedule schedule;
    schedule.complete(100000, true);
    assert(!schedule.due(104999, 5, false));
    assert(schedule.due(105000, 5, false));
    assert(!schedule.due(105000, 1800, false));
    assert(!schedule.due(2000000, 0, false));
    assert(schedule.due(100001, 0, true));
    schedule.complete(105000, false);
    assert(!schedule.due(110000, 5, false));
    assert(schedule.due(135000, 5, false));
    schedule.complete(135000, false);
    assert(!schedule.due(194999, 5, false));
    assert(schedule.due(195000, 5, false));
    schedule.complete(195000, true);
    assert(schedule.due(200000, 5, false));
    assert(staleAfter(1800) > 1800);

    using namespace uw::updates;
    assert(parseVersion("1.10.0") > parseVersion("1.9.9"));
    for (auto input : {"1.2", "1.2.3-beta", "v1.2.3", "1.2.3/asset", "1.2.0003", "1.2.99999"})
        assert(!parseVersion(input));
    const std::string hash = "sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    assert(parseDigest(hash)->back() == 0xef);
    assert(!parseDigest(hash + "0"));
    assert(!parseDigest("sha256:" + std::string(64, 'g')));

    auto metadata = uw::Parser(R"({"tag_name":"v1.10.0","draft":false,"prerelease":false,"body":"Notes",
      "assets":[{"name":"UsageWidget-Setup-1.10.0-x64.exe","state":"uploaded","size":100,
      "browser_download_url":"https://example.invalid/test.exe",
      "digest":"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"}]})").parse();
    const auto installed = *parseVersion("1.7.2");
    auto release = newerRelease(metadata, installed);
    assert(release && release->tag == "v1.10.0" && release->digest && release->size == 100);
    assert(release->assetPath() == "/" + std::string(repository) +
        "/releases/download/v1.10.0/UsageWidget-Setup-1.10.0-x64.exe");
    assert(!newerRelease(metadata, *parseVersion("1.10.0")));
    metadata.obj["prerelease"].number = 1;
    assert(!newerRelease(metadata, installed));
    metadata.obj["prerelease"].number = 0;
    metadata.obj["draft"].number = 1;
    assert(!newerRelease(metadata, installed));
    metadata.obj["draft"].number = 0;
    metadata.obj["assets"].arr[0].obj["digest"].str.clear();
    assert(!newerRelease(metadata, installed)->digest);
    metadata.obj["assets"].arr[0].obj["name"].str = "UsageWidget-Setup-1.10.0-arm64.exe";
    assert(!newerRelease(metadata, installed)->size);

    std::cout << "PASS configurable quota schedule, request backoff, stable version and installer selection\n";
}
