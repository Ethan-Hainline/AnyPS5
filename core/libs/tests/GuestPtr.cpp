#include "prx/libc/include/GuestPtr.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest pointer: %s\n", message);
    std::abort();
}

struct Payload {
    std::uint32_t first;
    std::uint64_t second;
};

void CheckMapped() {
    const Payload source{7, 0x1122334455667788ull};
    Payload copied{};
    Require(GuestPtr<const Payload>(&source).CopyIn(copied), "a readable guest pointer did not copy in");
    Require(std::memcmp(&copied, &source, sizeof(Payload)) == 0, "a copied-in value differs from its source");
    const Payload replacement{9, 0xaabbccddeeff0011ull};
    Require(GuestPtr<Payload>(&copied).CopyOut(replacement), "a writable guest pointer did not copy out");
    Require(std::memcmp(&copied, &replacement, sizeof(Payload)) == 0, "a copied-out value differs from its source");
}

void CheckFaults() {
    Payload value{};
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        const auto* pointer = reinterpret_cast<const void*>(address);
        Require(!GuestPtr<const Payload>(pointer).CopyIn(value), "an unreadable guest pointer was copied in");
        Require(!GuestPtr<Payload>(pointer).CopyOut(value), "an unwritable guest pointer was copied out");
    }
    const GuestPtr<const Payload> empty;
    Require(!empty.CopyIn(value), "an empty guest pointer was copied in");
    Require(empty.Address() == nullptr, "an empty guest pointer has an address");
}

}  // namespace

int main() {
    CheckMapped();
    CheckFaults();
    std::puts("guest pointer tests passed");
}
