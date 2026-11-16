// aap-api.js
// Public JS facade for the AAP automation controller, exposed as globalThis.aap.
//
// The __aap_* functions are native bindings (implemented in AapJsControllerRuntime.cpp) and are
// NOT a stable API. Always drive automation through aap.* instead.
//
// This mirrors uapmd-app's uapmd-api.js in spirit, but the vocabulary is the AAP host API only
// (no sequencer/timeline concepts). Offline rendering (aap.render.*) is intentionally absent for
// now and will be added on top of this surface later.

// Wraps a created plugin instance with the per-instance operations. Each plugin service numbers its
// own instances, so the plugin ID and the instance ID together identify one.
class PluginInstance {
    constructor(pluginId, instanceId) {
        this.pluginId = pluginId;
        this.instanceId = instanceId;
    }

    // Lifecycle
    prepare(frameCount, sampleRate) { __aap_instance_prepare(this.pluginId, this.instanceId, frameCount, sampleRate); return this; }
    activate() { __aap_instance_activate(this.pluginId, this.instanceId); return this; }
    process(frameCount) { __aap_instance_process(this.pluginId, this.instanceId, frameCount); return this; }
    deactivate() { __aap_instance_deactivate(this.pluginId, this.instanceId); return this; }
    destroy() { __aap_instance_destroy(this.pluginId, this.instanceId); }
    fillAudioInputs(seed, amplitude) { __aap_instance_fill_audio_inputs(this.pluginId, this.instanceId, seed, amplitude); return this; }
    copyAudioOutputsTo(destination) {
        if (!destination || typeof destination.pluginId !== 'string' || typeof destination.instanceId !== 'number')
            throw new Error("copyAudioOutputsTo() expects an AAP plugin instance");
        __aap_instance_copy_audio_outputs_to_inputs(this.pluginId, this.instanceId, destination.pluginId, destination.instanceId);
        return this;
    }
    getAudioOutputStats() { return __aap_instance_get_audio_output_stats(this.pluginId, this.instanceId); }
    addEventUmpInput(words) { __aap_instance_add_event_ump_input_hex(this.pluginId, this.instanceId, umpWordsToHex(words)); return this; }
    sleepMs(milliseconds) { __aap_sleep_ms(milliseconds); return this; }

    // Parameters
    getParameterCount() { return __aap_instance_get_parameter_count(this.pluginId, this.instanceId); }
    getParameters() { return __aap_instance_get_parameters(this.pluginId, this.instanceId); }
    getParameterValue(index) { return __aap_instance_get_parameter_value(this.pluginId, this.instanceId, index); }

    // Presets
    getPresetCount() { return __aap_instance_get_preset_count(this.pluginId, this.instanceId); }
    getPresetName(index) { return __aap_instance_get_preset_name(this.pluginId, this.instanceId, index); }
    setPreset(index) { __aap_instance_set_preset(this.pluginId, this.instanceId, index); return this; }

    // State (opaque blob, base64-encoded across the JS boundary)
    getState() { return __aap_instance_get_state(this.pluginId, this.instanceId); }
    setState(base64) { __aap_instance_set_state(this.pluginId, this.instanceId, base64); return this; }

    // GUI
    createGui() { return __aap_instance_create_gui(this.pluginId, this.instanceId); }
    showGui(guiInstanceId) { __aap_instance_show_gui(this.pluginId, this.instanceId, guiInstanceId); return this; }
    hideGui(guiInstanceId) { __aap_instance_hide_gui(this.pluginId, this.instanceId, guiInstanceId); return this; }
    destroyGui(guiInstanceId) { __aap_instance_destroy_gui(this.pluginId, this.instanceId, guiInstanceId); return this; }
}

function umpWordsToHex(words) {
    if (!Array.isArray(words))
        throw new Error("addEventUmpInput() expects an array of 32-bit UMP words");
    return words.map((word) => {
        var w = Number(word) >>> 0;
        return w.toString(16).padStart(8, "0");
    }).join("");
}

// Resolve a plugin's owning package name from the provider-fed discovery catalog,
// so create() can bind the right Android service before instancing.
function packageOfPlugin(pluginId) {
    try {
        const list = JSON.parse(__aap_get_plugins());
        for (const p of list)
            if (p.pluginId === pluginId) return p.packageName;
    } catch (e) { /* no catalog / parse error -> caller proceeds without auto-connect */ }
    return null;
}

globalThis.aap = {
    // Liveness check for the transport.
    ping: () => ({ pong: __aap_ping() }),

    // Runtime status (e.g. whether a native client is attached).
    runtimeInfo: () => __aap_runtime_info(),

    // Discovery — the installed plugin list, fed in JVM-side by the provider.
    discovery: {
        getPlugins: () => JSON.parse(__aap_get_plugins())
    },

    // Instance creation + lookup.
    instancing: {
        // Bind a plugin's Android service by package name. AAP must connect the service before
        // instancing; create() does this automatically, but it is also exposed explicitly.
        connect: (packageName) => __aap_connect_service(packageName),

        create: (pluginId) => {
            // Auto-connect the plugin's service first (resolve its package from discovery).
            const pkg = packageOfPlugin(pluginId);
            if (pkg) __aap_connect_service(pkg);
            return new PluginInstance(pluginId, __aap_instance_create(pluginId));
        }
    },

    // Wrap an existing instance, by its plugin ID and instance ID.
    instance: (pluginId, instanceId) => new PluginInstance(pluginId, instanceId)
};
