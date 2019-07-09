package aap;

interface IAudioPluginService {

	bool isPluginAlive();

	void prepare(int frameCount, int bufferCount, in long[] bufferPointers);
	void activate();
	void process();
	void deactivate();
	int getStateSize();
	void getState(long pointer);
	void setState(long pointer, int size);
}

