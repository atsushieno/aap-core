# AAP and requirements for realtime audio processing

Realtime audio processing is a must in generic audio processing in audio plugin framework, and AAP should not be exceptional. Currently there are some however some inevitable realtime blockers as long as we are on Android (which is the almost only reason why we use AAP). This documentation clarifies how we do and should tuckle realtime audio processing problems.

As explained shortly later, it is in theory impossible to get AAP to process audio in truly realtime manner. But we should try to minimize the impact of realtime blockes. It is why this documentation is done.

## Where we need realtime safety

### within audio processing loop in the audio thread

AAP is not very different from typical audio plugin formats in that it has the "realtime ready" state and the state otherwise. Whenever a host and plugin is at ACTIVE state, the audio processing is supposed to be RT-safe.

For both host and plugin, `process()` needs to be implemented in RT-safe manner. `activate()` and `deactivate()` switches those states, but they themselves do not necessarily have to be RT-safe (they would mostly be though).

### extensions

Each extension function is attributed as either RT-safe or non-RT-safe. (NOTE: even with C23 we cannot really define custom attributes, so we would only put code comments `RT_SAFE` or `RT_UNSAFE`.

Regardless of the attributed safety, each extension function should perform in RT-safe manner. Those marked as RT-safe should be simply implemented in RT-safe manner. Those extension functions that are marked as non-RT-safe still need to execute in asynchronous manner, i.e. :

- at INACTIVE state, we do not have to worry about RT-safety, thus Binder `extension()` method can be (synchronously) invoked and they can be implemented in AAPXS in simply synchronous way.
- at ACTIVE state, those non-RT-safe functions has to be transmitted as asynchronous MIDI Universal SysEx UMP. There might or might not be a reply Universal SysEx UMP to it in the returning processing result. When it was not performed within the same `process()` time slot, then response will be sent back to host in another `process()` cycle later.

Although note that this does not mean that typical plugin developers should care; they can implement AAP extensions in synchronous manner. AAPXS implementors (like ourselves as libandroidaudioplugin developers) should take care e.g. dispatching client requests to another non-RT thread, and correlate Universal SysEx responses.

Host developers should care about RT safety on each extension function. Those `RT_UNSAFE` functions are supposed to be invoked on non-RT thread otherwise it blocks execution until the function completes. The AAPXS would create a **BLOCKING** `std::mutex`, generate AAPXS SysEx8 and add to the plugin's input MIDI2 channel, then wait for reply and signal the mutex (it depends on each extension AAPXS developer, but the standard extensions are written by @atsushieno who is writing this document).

(Regarding IPC, we do not consider other channels between host and plugin than AAP's Binder; it is possible to establish another channel that is not based on Binder, or establish other Binder connection, but that would complicate the entire plugin connection management, at least within AAP framework itself. Third party developers could do whatever they want.)

## Inevitable realtime blockers

### (Ndk)Binder

As long as we need AAP plugin hosts (DAWs) and AAP plugins in different application processes, it is inevitable that we have to resort to some IPC framework, and we use Binder as the best optimal IPC framework on Android.

Unfortunately, we cannot use Binder in realtime manner whereas [binder itself is designed to be realtime ready](https://source.android.com/docs/core/architecture/hidl/binder-ipc#rt-priority). We will be suffered from its mutex locks.

While the entire Service framework is Dalvik only (it should not matter anyways), use of NdkBinder and native code is (almost) must. Any audio framework in native code (such as JUCE) would invoke audio processing from within Oboe audio callback, which is supposed to be RT-safe.

## realtime blockers that we could avoid

- JNI: `JavaVM` and `JNIEnv` brings in the entire Dalvik VM which is not RT-safe (GC, JIT, threads in mutex etc.). RT audio thread should not attach to JavaVM, which will stop the attached thread in blocking manner.
- (most of) system calls such as I/O or `std::mutex`
- excessive > O(1) operations; they make WCET unpredictable
- non-wait-free locks
- blocking thread dispatchers: calling async operation should be made wait-free. (For example we use `ALooper` with pipe `write()` which should be rt-safe.)

## design choices

### minimizing impact of extension async-ification

Many extension functions would not complete within an audio `process()` call and thus something needs to be async-ified at both plugin and host. An obvious approach to support async calls is to let extension developers define functions in callback manner i.e. pass callback function pointer that can be then invoked when asynchronous call ends e.g.

- `void* beginGetPresetCount() // returns asyncContext`
- `int32_t endGetPresetCount(void* asyncContext) // returns the actual preset count`

Do we need that?

For plugin side, it does not matter; the AAPXS SysEx8 approach makes it asynchronously doable without making changes to the function signature. It can return the async result at any time.

But the caller side needs to care, because it has to wait for the actual results from the plugin service to return *their* return value, and to achieve that it needs to block its thread until it is signaled with the return value from the plugin.

If we had extension functions to provide async calls, then AAPXS developers could simply convert the request to AAPXS SysEx8 request, send it, and once the reply has arrived (identified by `requestId`) then the AAPXS could receive async context and pass the result to callback, without blocking the caller.

But the change is too drastic. Could we just ask host developers assign a new thread that can block during the extension call? In theory yes, but that could result in such a situation that every track invokes `setPreset()` at the top of the song which would result in 100 "physically running" threads for 100 tracks. That is a bit too disastrous.

Another option was to have different set of extension functions for plugins and hosts e.g. plugins implement those sync functions, and hosts implement those async functions. That would minimize the effort at both sides (AAPXS developers are responsible anyways), but that is still looking like a bloated changes.

They led to the current consequence of the API (change post-0.7.8) to add `callback` 
argument to each asynchronous function. If each extension function is invoked without the callback, that means the host expects it as synchronous and the call to the extension funciton should immediately return the default value (like `0`). If it did not return the correct result, then it is either regarded as a failure or the host does not care about the result.


## external references

- Real-time audio programming 101: time waits for nothing http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
- (ADC '19) Real-time 101 - part I: Investigating the real-time problem space https://www.youtube.com/watch?v=Q0vrQFyAdWI and part II: https://www.youtube.com/watch?v=PoZAo2Vikbo&t=6s
  - https://github.com/hogliux/farbot/ , referenced from the session talk
- (ADC '22) Thread synchronisation in real-time audio processing with RCU (Read-Copy-Update) https://www.youtube.com/watch?v=7fKxIZOyBCE
  - https://github.com/crill-dev/crill , referenced from the session talk
- https://github.com/cradleapps/realtime_memory std::pmr implementation (referenced from the relevant ADC22 session talk, not published yet)


