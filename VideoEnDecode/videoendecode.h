#include <Windows.h>
#include <gstreamer-0.10\gst\gst.h>

typedef struct _App App;

struct _App {
	GstElement *pipeline;
	GMainLoop *loop;
	GTimer *timer;
	//GstElement *appsrc;
};

class VideoBuffer {
public: 

	VideoBuffer();
	~VideoBuffer();
	void LockBuffer();
	void UnlockBuffer();
	void SetBuffer(GstBuffer *new_buffer, size_t new_size);

	//unsigned char *buffer;
	GstBuffer *buffer;
	unsigned int size;
	bool running_proc;
	bool updated;
	bool finishing;
	guint source_id;
	GstElement *appsrc;
	GstCaps *caps;

private:
	CRITICAL_SECTION critical_section;
};

class VideoEncoder {
public:
	VideoEncoder(VideoBuffer *vid_buf);
	~VideoEncoder();
	unsigned int encode();
	unsigned int decode();
	App s_app, decode_app;

private:
	VideoBuffer *video_buffer;
};

//TODO: Add VideoDecoder class

