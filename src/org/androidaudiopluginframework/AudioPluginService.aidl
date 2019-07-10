package org.androidaudiopluginframework;

interface AudioPluginService {

	void create(String pluginId, int sampleRate);

	boolean isPluginAlive();

	void prepare(int frameCount, int bufferCount, in long[] bufferPointers);
	void activate();
	void process(int timeoutInNanoseconds);
	void deactivate();
	int getStateSize();
	void getState(long pointer);
	void setState(long pointer, int size);
	
	void destroy();
}

