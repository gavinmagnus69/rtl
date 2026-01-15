#pragma once

#include <functional>
#include <future>


namespace rtl {
namespace stp {

struct TaskOptions {
    bool is_periodic{false};
    size_t periodic_interval_ms{0};
};

using Task = std::function<void()>;

struct IExecutor {
    virtual ~IExecutor() = default;
    template <typename Func, typename... Args>
    auto submit(Func&& func, Args&&... args, TaskOptions opt) -> std::future<typename std::invoke_result<Func, Args...>::type> {
        using ReturnType = typename std::invoke_result<Func, Args...>::type;
        auto boundPackagedTaskPtr = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        auto returnFuture = boundPackagedTaskPtr->get_future();
        if (!post(std::move([boundPackagedTaskPtr]() mutable { (*boundPackagedTaskPtr)(); }), opt)) {
            // failed to put task into executor
            std::promise<ReturnType> p;
            p.set_exception(std::make_exception_ptr(std::runtime_error("submit rejected by executor")));
            return p.get_future();
        }
        return returnFuture;
    }
protected:
    virtual bool post(Task&& task, TaskOptions) = 0;
};

}; // namespace stp
} // namespace rtl