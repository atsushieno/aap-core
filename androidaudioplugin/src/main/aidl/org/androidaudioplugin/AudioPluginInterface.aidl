package org.androidaudioplugin;
import org.androidaudioplugin.AudioPluginInterfaceCallback;

interface AudioPluginInterface {
    const int AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID = 1;
    const int AAP_BINDER_ERROR_CREATE_INSTANCE_FAILED = 2;
    const int AAP_BINDER_ERROR_UNEXPECTED_FEATURE_URI = 3;
    const int AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION = 10;
    const int AAP_BINDER_ERROR_MMAP_FAILED = 11;
    const int AAP_BINDER_ERROR_MMAP_NULL_RETURN = 12;
    const int AAP_BINDER_ERROR_INVALID_SHARED_MEMORY_FD = 20;
    const int AAP_BINDER_ERROR_CALLBACK_ALREADY_SET = 30;

    // ServiceConnection operations
	void setCallback(in AudioPluginInterfaceCallback callback);

    // Instance operations
	int beginCreate(String pluginId, int sampleRate);
	// add an AAP extension, with a URI, with an optional shared memory FD and its size dedicated to it.
	// For consistency in the future, the order of calls to `addExtension()` should be considered as significant
	// (i.e. the extension list is ordered). Any extension that could affect other extensions should be added earlier.
	void addExtension(int instanceID, String uri, in ParcelFileDescriptor sharedMemoryFD, int size);
	void endCreate(int instanceID);

	boolean isPluginAlive(int instanceID);

	void extension(int instanceID, String uri, int size);

	// Indicates thaat it begins "prepare" step, to plugin.
	// When received, plugin finishes port configuration.
	// Port configuration by extensions should be done before this call.
	void beginPrepare(int instanceID);
	void prepareMemory(int instanceID, int shmFDIndex, in ParcelFileDescriptor sharedMemoryFD);
	void endPrepare(int instanceID, int frameCount);
	void activate(int instanceID);
	void process(int instanceID, int frameCount, int timeoutInNanoseconds);
	void deactivate(int instanceID);
	
	void destroy(int instanceID);
}

