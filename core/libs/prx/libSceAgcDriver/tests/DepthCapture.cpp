#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::DepthTarget;

constexpr std::uint32_t Magic = 0x48545044u;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "depth capture: %s\n", message);
    std::abort();
}

template <typename T>
void Append(std::vector<std::byte>& bytes, const T& value) {
    const auto* first = reinterpret_cast<const std::byte*>(&value);
    bytes.insert(bytes.end(), first, first + sizeof(value));
}

std::vector<std::byte> Header(std::uint32_t count) {
    std::vector<std::byte> bytes;
    Append(bytes, Magic);
    Append(bytes, static_cast<std::uint32_t>(sizeof(DepthTarget)));
    Append(bytes, count);
    return bytes;
}

void AppendPlane(std::vector<std::byte>& bytes, const DepthTarget& target, std::int32_t format, std::span<const std::byte> depth) {
    Append(bytes, target);
    Append(bytes, format);
    Append(bytes, static_cast<std::uint64_t>(depth.size()));
    bytes.insert(bytes.end(), depth.begin(), depth.end());
}

std::filesystem::path Write(const std::string& name, const std::vector<std::byte>& bytes) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path;
}

template <typename TAction>
bool Throws(TAction&& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

DepthTarget SampleTarget() {
    DepthTarget target{};
    target.address = 0x100000;
    target.extent = {64, 64};
    target.format = VK_FORMAT_D16_UNORM;
    target.clearDepth = 1.0f;
    target.samples = 1;
    return target;
}

}

int main() {
    const auto target = SampleTarget();
    const std::vector<std::byte> depth(64u * 64u * 2u, std::byte{0x5a});
    const auto valid = [&] {
        auto bytes = Header(1);
        AppendPlane(bytes, target, VK_FORMAT_D16_UNORM, depth);
        return bytes;
    };

    const auto path = Write("anyps5-depth-capture-valid.bin", valid());
    Require(AgcDriverLoadSavedDepth_nid_postfix(path.string().c_str()) == 1, "a valid depth.bin did not load");
    std::filesystem::remove(path);
    Require(AgcDriverLoadSavedDepth_nid_postfix(path.string().c_str()) == 0, "a missing depth.bin did not load as empty");

    auto badMagic = valid();
    badMagic[0] = std::byte{0};
    const auto badMagicPath = Write("anyps5-depth-capture-bad-magic.bin", badMagic);
    Require(Throws([&] { AgcDriverLoadSavedDepth_nid_postfix(badMagicPath.string().c_str()); }), "a bad magic was accepted");
    std::filesystem::remove(badMagicPath);

    auto badTarget = valid();
    badTarget[4] = std::byte{0};
    const auto badTargetPath = Write("anyps5-depth-capture-bad-target.bin", badTarget);
    Require(Throws([&] { AgcDriverLoadSavedDepth_nid_postfix(badTargetPath.string().c_str()); }), "a bad target size was accepted");
    std::filesystem::remove(badTargetPath);

    const auto tooManyPath = Write("anyps5-depth-capture-too-many.bin", Header(4097));
    Require(Throws([&] { AgcDriverLoadSavedDepth_nid_postfix(tooManyPath.string().c_str()); }), "an oversized plane count was accepted");
    std::filesystem::remove(tooManyPath);

    auto truncated = Header(1);
    AppendPlane(truncated, target, VK_FORMAT_D16_UNORM, depth);
    truncated.resize(truncated.size() - 3);
    const auto truncatedPath = Write("anyps5-depth-capture-truncated.bin", truncated);
    Require(Throws([&] { AgcDriverLoadSavedDepth_nid_postfix(truncatedPath.string().c_str()); }), "a truncated plane was accepted");
    std::filesystem::remove(truncatedPath);

    auto badFormat = Header(1);
    AppendPlane(badFormat, target, VK_FORMAT_R8G8B8A8_UNORM, depth);
    const auto badFormatPath = Write("anyps5-depth-capture-bad-format.bin", badFormat);
    Require(Throws([&] { AgcDriverLoadSavedDepth_nid_postfix(badFormatPath.string().c_str()); }), "an unsupported host format was accepted");
    std::filesystem::remove(badFormatPath);

    auto multisample = target;
    multisample.samples = 4;
    auto multisampleBytes = Header(1);
    AppendPlane(multisampleBytes, multisample, VK_FORMAT_D16_UNORM, depth);
    const auto multisamplePath = Write("anyps5-depth-capture-multisample.bin", multisampleBytes);
    Require(Throws([&] { AgcDriverLoadSavedDepth_nid_postfix(multisamplePath.string().c_str()); }), "a multisample plane was accepted");
    std::filesystem::remove(multisamplePath);

    auto badBytes = Header(1);
    AppendPlane(badBytes, target, VK_FORMAT_D16_UNORM, std::span<const std::byte>(depth).first(depth.size() - 2));
    const auto badBytesPath = Write("anyps5-depth-capture-bad-bytes.bin", badBytes);
    Require(Throws([&] { AgcDriverLoadSavedDepth_nid_postfix(badBytesPath.string().c_str()); }), "a wrong plane size was accepted");
    std::filesystem::remove(badBytesPath);

    std::puts("depth capture tests passed");
}
