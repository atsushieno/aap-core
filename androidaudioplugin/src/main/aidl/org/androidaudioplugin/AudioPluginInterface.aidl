package org.androidaudioplugin;

interface AudioPluginInterface {

	int beginCreate(String pluginId, int sampleRate);
	void addExtension(int instanceID, String uri, in ParcelFileDescriptor sharedMemoryFD, int size);
	void endCreate(int instanceID);

	boolean isPluginAlive(int instanceID);

	int getStateSize(int instanceID);
	void getState(int instanceID, in ParcelFileDescriptor sharedMemoryFD);
	void setState(int instanceID, in ParcelFileDescriptor sharedMemoryFD, int size);

	void prepare(int instanceID, int frameCount, int portCount);
	void prepareMemory(int instanceID, int shmFDIndex, in ParcelFileDescriptor sharedMemoryFD);
	void activate(int instanceID);
	void process(int instanceID, int timeoutInNanoseconds);
	void deactivate(int instanceID);
	
	void destroy(int instanceID);
}

