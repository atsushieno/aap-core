#ifndef AAP_CORE_BUSES_H
#define AAP_CORE_BUSES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include <stdint.h>
#include <stdbool.h>

#define AAP_BUSES_EXTENSION_URI "urn://androidaudioplugin.org/extensions/buses/v1"

/*
 * Buses extension.
 *
 * A bus is a logical connection: an audio bus has one buffer per channel, and an event bus has
 * one UMP buffer. Buses are indexed separately for each (kind, direction) pair, and the main bus
 * is always at index 0.
 *
 * The plugin implements this extension only when it knows its own bus layout (e.g. plugin format
 * wrappers). Otherwise the framework provides the layout on behalf of the plugin.
 *
 * Event buses are managed by the framework: the main event input and output always exist, for
 * every plugin. Plugins report audio buses only (`kind` is always AAP_BUS_KIND_AUDIO so far).
 */

#define AAP_MAX_BUS_NAME_CHARS 64
#define AAP_MAX_BUS_LAYOUT_CHARS 32
#define AAP_MAX_BUSES 32

// The host may leave the bus disabled.
#define AAP_BUS_FLAG_OPTIONAL 1
// An optional bus that is enabled unless the host disables it.
#define AAP_BUS_FLAG_ENABLED_BY_DEFAULT 2
// An event bus that consumes (input) or produces (output) note events.
#define AAP_BUS_FLAG_NOTES 4

// Reserved bus IDs for the framework-managed main event buses.
#define AAP_BUS_ID_MAIN_EVENT_INPUT 0xFFFF0000
#define AAP_BUS_ID_MAIN_EVENT_OUTPUT 0xFFFF0001

typedef struct aap_bus_info_t {
    // Stable across plugin versions; hosts persist it for routing. It is not the position.
    uint32_t id;
    enum aap_bus_kind kind;
    enum aap_port_direction direction;
    enum aap_bus_role role;
    char name[AAP_MAX_BUS_NAME_CHARS];
    // audio only
    int32_t channel_count;
    // audio only: one of "mono", "stereo", "lcr", "quad", "5.1", "7.1", "7.1.4", "ambisonic(order)", "discrete(n)".
    char layout[AAP_MAX_BUS_LAYOUT_CHARS];
    // AAP_BUS_FLAG_*
    uint32_t flags;
    bool enabled;
} aap_bus_info_t;

typedef struct aap_bus_layout_request_t {
    int32_t count;
    struct {
        uint32_t id;
        bool enabled;
        int32_t channel_count;
        char layout[AAP_MAX_BUS_LAYOUT_CHARS];
    } buses[AAP_MAX_BUSES];
} aap_bus_layout_request_t;

typedef struct aap_buses_extension_t {
    void* aapxs_context;
    RT_UNSAFE int32_t (*get_bus_count) (struct aap_buses_extension_t* ext, AndroidAudioPlugin* plugin,
                                        enum aap_bus_kind kind, enum aap_port_direction direction);
    RT_UNSAFE aap_bus_info_t (*get_bus) (struct aap_buses_extension_t* ext, AndroidAudioPlugin* plugin,
                                         enum aap_bus_kind kind, enum aap_port_direction direction, int32_t index);
    // Optional (may be NULL). Only when the plugin is not active. Returns false and leaves the layout
    // unchanged if the requested layout is not supported. (Not used by hosts yet.)
    RT_UNSAFE bool (*apply_layout) (struct aap_buses_extension_t* ext, AndroidAudioPlugin* plugin,
                                    const aap_bus_layout_request_t* request);
} aap_buses_extension_t;

#define AAP_BUSES_CHANGED_NAMES 1
#define AAP_BUSES_CHANGED_LAYOUT 2

typedef struct aap_buses_host_extension_t {
    void* aapxs_context;
    // Notifies the host that the bus names (AAP_BUSES_CHANGED_NAMES) or the layout
    // (AAP_BUSES_CHANGED_LAYOUT) changed. (Not delivered to hosts yet.)
    RT_SAFE void (*notify_buses_changed) (struct aap_buses_host_extension_t* ext, AndroidAudioPluginHost* host, uint32_t flags);
} aap_buses_host_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif //AAP_CORE_BUSES_H
