package org.androidaudioplugin;

interface AudioPluginInterface {

	void create(String pluginId, int sampleRate);

	boolean isPluginAlive();

	void prepare(int frameCount, int portCount);
	void prepareMemory(int shmFDIndex, in ParcelFileDescriptor sharedMemoryFD);
	void activate();
	void process(int timeoutInNanoseconds);
	void deactivate();
	int getStateSize();
	void getState(in ParcelFileDescriptor sharedMemoryFD);
	void setState(in ParcelFileDescriptor sharedMemoryFD, int size);
	
	void destroy();
}

