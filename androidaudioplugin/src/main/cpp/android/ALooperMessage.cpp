#include "ALooperMessage.h"
#include <queue>

namespace aap {

    std::queue<std::unique_ptr<ALooperMessage>> queue;

    void ALooperMessage::post(std::unique_ptr<ALooperMessage> msg) {
        queue.push(std::move(msg));
        queue.back()->post();
    }

    std::unique_ptr<ALooperMessage> ALooperMessage::dequeue() {
        auto ret = std::move(queue.front());
        queue.pop();
        return ret;
    }

}