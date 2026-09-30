#ifndef AAP_CORE_HOSTING_HOST_AAPXS_REQUEST_QUEUE_H
#define AAP_CORE_HOSTING_HOST_AAPXS_REQUEST_QUEUE_H

#include "aap/core/host/plugin-instance.h"

namespace aap::internal {

struct HostAAPXSRequest {
    const void* owner;
    aapxs_host_ipc_sender send;
    void* send_context;
    const char* uri;
    int32_t instance_id;
    int32_t opcode;
    int32_t request_id;
    aapxs_completion_callback callback;
    void* callback_data;
    void* callback_plugin_or_host;
    aapxs_error_callback error_callback;

    void sendNow() const {
        send(send_context, uri, instance_id, opcode, request_id, callback, callback_data, callback_plugin_or_host, error_callback);
    }
};

// Sends plugin-initiated (host extension) AAPXS requests over IPC on a non-RT worker thread,
// so that plugins can call host extensions from any thread, including the audio thread.
class HostAAPXSRequestQueue {
public:
    enum class EnqueueResult { Queued, Full, OwnerClosed };

    static HostAAPXSRequestQueue& getInstance();

    // Starts the worker thread if not yet. Not RT-safe; call it before the first enqueue().
    void start();
    // RT-safe.
    EnqueueResult enqueue(const HostAAPXSRequest& request);
    // Drops pending requests from `owner` and rejects further ones, waiting for an in-flight send to finish.
    void closeOwner(const void* owner);
    // Accepts requests from `owner` again (the address may be reused by a new instance).
    void forgetOwner(const void* owner);
};

}

#endif //AAP_CORE_HOSTING_HOST_AAPXS_REQUEST_QUEUE_H
