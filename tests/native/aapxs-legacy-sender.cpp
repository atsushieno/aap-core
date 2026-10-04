#include <cstdio>
#include <stdexcept>
#include <thread>
#include <future>
#include "legacy-aapxs-sender.h"
#include "aap/core/realtime.h"
using namespace aap::internal;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    LegacyAAPXSSender sender;
    int errors = 0, sends = 0;
    AAPXSRequestContext request{};
    request.callback_user_data = &errors;
    request.error_callback = [](void* context, void*, const char*) { ++*static_cast<int*>(context); };
    auto transmit = [&](const auto&) { ++sends; return true; };
    check(sender.enqueue(request, transmit), "queued");
    sender.poll(true, nullptr); check(sends == 0, "active sender waits for audio");
    { aap::RealtimeScope rt; check(sender.processingCompleted(), "processing requests eventfd wake"); }
    sender.poll(true, nullptr); check(sends == 1, "one send after progress");
    check(sender.enqueue(request, transmit), "second queued");
    sender.poll(true, nullptr); check(sends == 1, "next send needs later block");
    sender.processingCompleted(); sender.poll(true, nullptr); check(sends == 2, "later block drains reply");
    check(sender.enqueue(request, transmit, 0), "deadline queued");
    sender.poll(true, nullptr); check(errors == 1 && sends == 2, "stopped audio deadline expires independently");
    for (int i = 0; i < 255; ++i) check(sender.enqueue(request, transmit), "bounded slot available");
    check(!sender.enqueue(request, transmit), "limit enforced");
    { auto gate = sender.cancel("cancelled", nullptr);
      check(!sender.enqueue(request, transmit), "replacement refused during channel abort"); }
    check(errors == 256, "all cancelled exactly once");
    check(sender.enqueue(request, transmit), "reusable after cancellation");
    sender.poll(false, nullptr); check(sends == 3, "inactive sender does not need audio");
    check(sender.enqueue(request, transmit), "queued before close");
    { auto gate = sender.cancel("closed", nullptr, true); }
    check(errors == 257 && !sender.enqueue(request, transmit), "close rejects replacements");
    LegacyAAPXSSender callbacks;
    check(callbacks.enqueue(request, [&](const auto&) {
        // Binder can deliver on another thread before the transmitting IPC returns.
        auto cancellation = std::async(std::launch::async, [&] { auto gate = callbacks.cancel("reentrant", nullptr); });
        check(cancellation.wait_for(std::chrono::seconds(1)) == std::future_status::ready, "Binder callback can cancel while IPC is in progress");
        cancellation.get(); return true;
    }), "callback cancellation queued");
    callbacks.poll(false, nullptr);
    puts("PASS: bounded legacy Binder pacing, stopped-audio timeout and cancellation");
}
