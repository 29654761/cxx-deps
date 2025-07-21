#pragma once

#include <functional>
#include <shared_mutex>
#include <optional>
#include <utility>

namespace sys
{

    template <typename Signature>
    class mutex_function;

    template <typename R, typename... Args>
    class mutex_function<R(Args...)> {
    public:
        mutex_function() = default;

        void set(std::function<R(Args...)> func) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            func_ = std::move(func);
        }

        void clear() {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            func_ = nullptr;
        }

        bool valid() const {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            return static_cast<bool>(func_);
        }

        R invoke(Args... args)
        {
            std::function<R(Args...)> local;
            {
                std::shared_lock<std::shared_mutex> lock(mutex_);
                local = func_;
            }

            if (local) {
                if constexpr (std::is_void_v<R>) {
                    local(std::forward<Args>(args)...);
                }
                else {
                    return local(std::forward<Args>(args)...);
                }
            }
            return R();
        }

    private:
        std::function<R(Args...)> func_;
        mutable std::shared_mutex mutex_;


    };

}
