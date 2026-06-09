

#include <cstdint>
#include <functional>
#include <chrono>
#include <thread>

static void sleepInterruptibleUs(int64_t targetSleepUs,
                                 const std::function<bool()> &shouldBreak) {
    if (targetSleepUs <= 0) {
        return;
    }
    static constexpr int64_t kMaxSleepUsPerSlice = 2000;
    const auto sleepStart = std::chrono::steady_clock::now();

    while (true) {
        if (shouldBreak && shouldBreak()) {
            break;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
                now - sleepStart).count();
        if (elapsedUs >= targetSleepUs) {
            break;
        }
        const int64_t remainingUs = targetSleepUs - elapsedUs;
        const int64_t sliceUs = std::min(remainingUs, kMaxSleepUsPerSlice);
        std::this_thread::sleep_for(std::chrono::microseconds(sliceUs));
    }
}