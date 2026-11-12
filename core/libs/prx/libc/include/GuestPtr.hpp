#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTPTR_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTPTR_HPP

#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <type_traits>

template <typename T>
class GuestPtr {
public:
    using Value = std::remove_const_t<T>;
    static_assert(std::is_trivially_copyable_v<Value>, "a guest pointer value must be trivially copyable");

    constexpr GuestPtr() = default;
    explicit constexpr GuestPtr(const void* address) : address(address) {}

    const void* Address() const { return address; }

    bool CopyIn(Value& value) const {
        return address != nullptr && GuestCopyIn_nid_no_patch(&value, address, sizeof(Value));
    }

    bool CopyOut(const Value& value) const {
        static_assert(!std::is_const_v<T>, "a const guest pointer cannot be written");
        return address != nullptr && GuestCopyOut_nid_no_patch(const_cast<void*>(address), &value, sizeof(Value));
    }

private:
    const void* address = nullptr;
};

#endif
