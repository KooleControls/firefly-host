#pragma once
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <algorithm>

template <typename Interface, typename... Types>
class UnionStorage
{
public:
    UnionStorage() : type_id(-1) {}
    ~UnionStorage() { Destroy(); }

    UnionStorage(const UnionStorage &) = delete;
    UnionStorage &operator=(const UnionStorage &) = delete;

    template <typename T, typename... Args>
    T& Emplace(Args &&...args)
    {
        static_assert(IsAllowedType<T>(), "Type not allowed in UnionStorage");
        Destroy();
        new (reinterpret_cast<T *>(&storage)) T(std::forward<Args>(args)...);
        type_id = TypeIdOf<T>();
        return GetAs<T>();
    }

    Interface &Get()
    {
        assert(type_id >= 0 && "Invalid access to empty storage");
        return DispatchGet(type_id);
    }

    const Interface &Get() const
    {
        assert(type_id >= 0 && "Invalid access to empty storage");
        return DispatchGet(type_id);
    }

    template <typename T>
    T &GetAs()
    {
        static_assert(IsAllowedType<T>(), "Type not allowed in UnionStorage");
        assert(type_id == TypeIdOf<T>() && "Type mismatch in GetAs()");
        return *reinterpret_cast<T *>(&storage);
    }

    template <typename T>
    const T &GetAs() const
    {
        static_assert(IsAllowedType<T>(), "Type not allowed in UnionStorage");
        assert(type_id == TypeIdOf<T>() && "Type mismatch in GetAs() const");
        return *reinterpret_cast<const T *>(&storage);
    }

    template <typename T>
    bool IsType() const
    {
        static_assert(IsAllowedType<T>(), "Type not allowed in UnionStorage");
        return type_id == TypeIdOf<T>();
    }

    template <typename T>
    static constexpr int TypeIdOf()
    {
        static_assert(IsAllowedType<T>(), "Type not allowed in UnionStorage");
        return IndexOf<T, Types...>;
    }

    int GetTypeId() const { return type_id; }

    bool IsSet() const { return type_id >= 0; }

    void Clear() { Destroy(); }

private:
    static constexpr size_t max_size = std::max({sizeof(Types)...});
    static constexpr size_t max_align = std::max({alignof(Types)...});
    alignas(max_align) std::byte storage[max_size];
    int type_id;

    void Destroy()
    {
        if (type_id < 0)
            return;
        DispatchDestroy(type_id);
        type_id = -1;
    }

    Interface &DispatchGet(int id)
    {
        Interface *ptr = nullptr;
        ((id == TypeIdOf<Types>() ? ptr = reinterpret_cast<Types *>(&storage) : 0), ...);
        assert(ptr && "Invalid type ID in DispatchGet");
        return *ptr;
    }

    const Interface &DispatchGet(int id) const
    {
        const Interface *ptr = nullptr;
        ((id == TypeIdOf<Types>() ? ptr = reinterpret_cast<const Types *>(&storage) : 0), ...);
        assert(ptr && "Invalid type ID in DispatchGet (const)");
        return *ptr;
    }

    void DispatchDestroy(int id)
    {
        ((id == TypeIdOf<Types>() ? reinterpret_cast<Types *>(&storage)->~Types() : void()), ...);
    }

    template <typename T>
    static constexpr bool IsAllowedType()
    {
        return (std::is_same_v<T, Types> || ...);
    }

    template <typename T, typename... Ts>
    static constexpr int IndexOf = []
    {
        int index = 0;
        ((std::is_same_v<T, Ts> ? true : (++index, false)) || ...);
        return index;
    }();
};
