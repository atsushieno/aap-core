#ifndef AAP_CORE_ALOOPERMESSAGE_H
#define AAP_CORE_ALOOPERMESSAGE_H

#include <sys/eventfd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <unistd.h>
#include <android/looper.h>
#include <aap/unstable/logging.h>
#include <pthread.h>
#include <memory>
#include <functional>
#include "aap/unstable/utility.h"

namespace aap {

    // similar to AMessage in stagefright.
    class ALooperMessage {
    public:
        std::function<void()> handleMessage{nullptr};
    };

    class NonRealtimeLoopRunner {
        ALooper* looper;
        void* preallocated_space;
        int32_t preallocated_space_size;
        int32_t preallocated_space_next_pos{0};
        int pipe_fds[2];

    public:
        NonRealtimeLoopRunner(ALooper* looper, int32_t preallocatedSpaceSize)
        : looper(looper), preallocated_space_size(preallocatedSpaceSize) {
            if (pipe(pipe_fds) != 0)
                AAP_ASSERT_FALSE;
            else if (!ALooper_addFd(looper, pipe_fds[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, handleMessage, nullptr))
                AAP_ASSERT_FALSE;

            preallocated_space = calloc(1, preallocated_space_size);
            if (preallocated_space == 0)
                AAP_ASSERT_FALSE;
        }

        ~NonRealtimeLoopRunner() {
            ALooper_removeFd(looper, pipe_fds[0]);
            close(pipe_fds[0]);
            close(pipe_fds[1]);

            free(preallocated_space);
        }

        void* getLooper() { return looper; }

        // returns an unsafe pointer within the preallocated buffer space.
        // Since we cannot guess if the object is still in use, it blindly goes back to the head
        // of the address.
        template<typename T>
        void create(void** ptr) {
            int32_t size = sizeof(T);
            if (preallocated_space_next_pos + size >= preallocated_space_size) {
                preallocated_space_next_pos = size;
                // from zero again
                *ptr = preallocated_space;
            } else {
                preallocated_space_next_pos += size;
                *ptr = (uint8_t *) preallocated_space + preallocated_space_next_pos;
            }
        }

        void postMessage(ALooperMessage *message) {
            // pass the pointer to the message here. Restore it by `read()`.
            void** value = (void**) &message;
            write(pipe_fds[1], value, sizeof(void**));
        }

        static int handleMessage(int fd, int events, void* data) {
            // Read the byte from the read end of the pipe to clear the event
            void* value;
            read(fd, &value, sizeof(void**));

            // Call the message handler function
            auto message = (ALooperMessage*) value;
            if (!message) {
                AAP_ASSERT_FALSE;
                return 1; // loop continues anyway
            }
            message->handleMessage();

            return 1; // loop continues
        }
    };
}

#endif //AAP_CORE_ALOOPERMESSAGE_H
