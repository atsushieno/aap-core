#ifndef AAP_CORE_ALOOPERHELPER_H
#define AAP_CORE_ALOOPERHELPER_H

#include <sys/eventfd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <unistd.h>
#include <android/looper.h>
#include <aap/unstable/logging.h>

namespace aap {

    // similar to AMessage in stagefright.
    class ALooperMessage {
        ALooper* looper;
        int pipe_fds[2];
        int poll_index;

    protected:
        virtual void handleMessage() = 0;

    public:
        ALooperMessage(ALooper* looper) : looper(looper) {
            // Create the message pipe
            if (pipe(pipe_fds) != 0) {
                // handle error
            }

            poll_index = ALooper_addFd(looper, pipe_fds[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, ALooperMessage::handleMessage, this);
        }

        virtual ~ALooperMessage() {
            // Remove the read end of the pipe from the looper's file descriptor set
            ALooper_removeFd(looper, pipe_fds[0]);
            close(pipe_fds[0]);
            close(pipe_fds[1]);
        }

        void post() {
            // Write a byte to the write end of the pipe to wake up the looper
            uint64_t value = 1;
            write(pipe_fds[1], &value, sizeof(value));

            ALooper_wake(looper);
        }

        static int handleMessage(int fd, int events, void* data) {
            // Read the byte from the read end of the pipe to clear the event
            uint64_t value;
            read(fd, &value, sizeof(value));

            // Call the message handler function
            auto message = (ALooperMessage*) data;
            message->handleMessage();

            return 1; // loop continues
        }
    };
}

#endif //AAP_CORE_ALOOPERHELPER_H
