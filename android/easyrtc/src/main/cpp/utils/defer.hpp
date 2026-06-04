#include <utility>
template <typename F>
class Defer {
public:
    explicit Defer(F&& f)
        : func(std::forward<F>(f)), active(true) {}

    Defer(const Defer&) = delete;
    Defer& operator=(const Defer&) = delete;

    Defer(Defer&& other) noexcept
        : func(std::move(other.func)), active(other.active) {
        other.active = false;
    }

    ~Defer() {
        if (active) func();
    }

    void cancel() { active = false; }

private:
    F func;
    bool active;
};

template <typename F>
Defer<F> make_defer(F&& f) {
    return Defer<F>(std::forward<F>(f));
}
#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)

#define defer(code) auto CONCAT(_defer_, __LINE__) = make_defer([&](){code;})