// Standalone compatibility runtime for GPL-3 taskbar-grouping source.
#pragma once

#include <windows.h>

#include <cstdarg>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace winrt {
template <typename T>
class com_ptr {
  public:
    com_ptr() = default;
    com_ptr(std::nullptr_t) {}
    ~com_ptr() { reset(); }
    com_ptr(const com_ptr&) = delete;
    com_ptr& operator=(const com_ptr&) = delete;
    com_ptr(com_ptr&& other) noexcept : value_(std::exchange(other.value_, nullptr)) {}
    com_ptr& operator=(com_ptr&& other) noexcept {
        if (this != &other) {
            reset();
            value_ = std::exchange(other.value_, nullptr);
        }
        return *this;
    }
    T* get() const { return value_; }
    T* detach() { return std::exchange(value_, nullptr); }
    void** put_void() {
        reset();
        return reinterpret_cast<void**>(&value_);
    }
    explicit operator bool() const { return value_ != nullptr; }

  private:
    void reset() {
        if (value_) value_->Release();
        value_ = nullptr;
    }
    T* value_ = nullptr;
};
}  // namespace winrt

void Wh_Log(PCWSTR format, ...);
void SetStandaloneModule(HMODULE module);
void* ResolveDiaSymbol(HMODULE module, PCWSTR signature);
PCWSTR Wh_GetStringSetting(PCWSTR format, ...);
void Wh_FreeStringSetting(PCWSTR value);
int Wh_GetIntSetting(PCWSTR name);
BOOL Wh_SetFunctionHook(void* target, void* detour, void** original);
void Wh_ApplyHookOperations();

namespace WindhawkUtils {
struct SYMBOL_HOOK {
    std::vector<std::wstring> symbols;
    void** originalFunction;
    void* hookFunction;
    bool optional;

    template <typename Original>
    SYMBOL_HOOK(std::initializer_list<PCWSTR> names, Original** original)
        : symbols(names.begin(), names.end()),
          originalFunction(reinterpret_cast<void**>(original)),
          hookFunction(nullptr),
          optional(false) {}

    template <typename Original>
    SYMBOL_HOOK(std::initializer_list<PCWSTR> names,
                Original** original,
                std::nullptr_t,
                bool isOptional = false)
        : symbols(names.begin(), names.end()),
          originalFunction(reinterpret_cast<void**>(original)),
          hookFunction(nullptr),
          optional(isOptional) {}

    template <typename Original, typename Hook>
    SYMBOL_HOOK(std::initializer_list<PCWSTR> names,
                Original** original,
                Hook hook,
                bool isOptional = false)
        : symbols(names.begin(), names.end()),
          originalFunction(reinterpret_cast<void**>(original)),
          hookFunction(reinterpret_cast<void*>(hook)),
          optional(isOptional) {}
};

bool HookSymbols(HMODULE module, const SYMBOL_HOOK* hooks, size_t count);

template <typename Function>
BOOL SetFunctionHook(Function target, Function detour, Function* original) {
    return Wh_SetFunctionHook(reinterpret_cast<void*>(target),
                              reinterpret_cast<void*>(detour),
                              reinterpret_cast<void**>(original));
}
}  // namespace WindhawkUtils
