#include <Windows.h>
#include <gstreamer-0.10\gst\gst.h>
#include <gst/app/gstappsink.h>

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
	VideoEncoder(VideoBuffer *vid_buf, VideoBuffer *rgb_buf);
	~VideoEncoder();
	unsigned int encode();
	unsigned int decode();
	App s_app, decode_app;

	static GstFlowReturn new_buffer(GstAppSink *sink, gpointer user_data);
	static GstFlowReturn new_buffer_list(GstAppSink *sink, gpointer user_data);
	static GstFlowReturn new_preroll (GstAppSink *sink, gpointer user_data);
	static gboolean read_data (VideoBuffer * video_buffer);
	static void start_feed (GstElement * pipeline, guint size, VideoBuffer * app);
	static void stop_feed (GstElement * pipeline, VideoBuffer * app);
	static gboolean bus_message (GstBus * bus, GstMessage * message, _App * app);
	static void print_structure(const GstStructure *structure, const char *title, const char *type_name);
	static void print_buffer(GstBuffer *buffer, const char *type_name);


private:
	VideoBuffer *video_buffer;
	VideoBuffer *rgb_buffer;
};

struct _VideoEncoder_Params {
	VideoBuffer *vid_buf_param;
	VideoBuffer *rgb_buf_param;
};

//TODO: Add VideoDecoder class

