package org.androidaudioplugin;

interface AudioPluginInterface {

	void create(String pluginId, int sampleRate);

	boolean isPluginAlive();

	void prepare(int frameCount, int portCount, in long[] sharedMemoryFDs);
	void activate();
	void process(int timeoutInNanoseconds);
	void deactivate();
	int getStateSize();
	void getState(long sharedMemoryFD);
	void setState(long sharedMemoryFD, int size);
	
	void destroy();
}

