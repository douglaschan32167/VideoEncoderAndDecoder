

// VideoEnDecode.cpp : Defines the entry point for the console application.
//

// cppgstreamer.cpp : main project file.
#pragma comment(lib, "gstreamer-0.10.lib")
#pragma comment(lib, "glib-2.0.lib")
#pragma comment(lib, "gobject-2.0.lib")
#pragma comment(lib, "gstapp-0.10.lib")
#pragma comment(lib, "gstvideo-0.10.lib")

#include "stdafx.h"
#include "videoendecode.h"
#include <iostream>
#include <Windows.h>
#include <gstreamer-0.10\gst\gst.h>
#include <gst/app/gstappsink.h>
#include <process.h>
#include <glib.h>

using namespace std;
using namespace System;



static void print_structure(const GstStructure *structure, const char *title, const char *type_name) {
	if(structure)
	{
		::g_printerr("%s: %s[%s] ", title, type_name, ::gst_structure_get_name(structure));
		for(int i = 0; i < ::gst_structure_n_fields(structure); i++) {
			if(i != 0) {
				::printf_s(", ");
			}
			const char *name = ::gst_structure_nth_field_name(structure, i);
			GType type = ::gst_structure_get_field_type(structure, name);
			::g_printerr("%s[%s]", name, ::g_type_name(type));
		}
	}

	::g_printerr("\n");
}

static void print_buffer(GstBuffer *buffer, const char *type_name) {
	GstCaps *caps = ::gst_buffer_get_caps(buffer);

	unsigned int sz = buffer->size;
	::g_printerr("print_buffer size:%d stnum:%d ", buffer->size, ::gst_caps_get_size(caps));

	for(unsigned int j = 0; j < ::gst_caps_get_size(caps); j++) {
		const GstStructure *structure = ::gst_caps_get_structure(caps, j);
		print_structure(structure, "appsinkcallback", type_name);
	}

	::g_printerr("\n");
}


// Callback functions to AppSink
static GstFlowReturn new_buffer_list(GstAppSink *sink, gpointer user_data) {
	GstBufferList *list = gst_app_sink_pull_buffer_list (sink);
	GstBufferListIterator *it = gst_buffer_list_iterate (list);
	GstBuffer *buffer;
	while (gst_buffer_list_iterator_next_group (it)) {
		while ((buffer = gst_buffer_list_iterator_next (it)) != NULL) {
			print_buffer(buffer, "new_buffer_list");
		}
	}
	gst_buffer_list_iterator_free (it);
	return GST_FLOW_OK;
}
static GstFlowReturn new_preroll (GstAppSink *sink, gpointer user_data) {
	GstBuffer *buffer =  gst_app_sink_pull_preroll (sink);
	if (buffer) {
		print_buffer(buffer, "preroll");

		GstCaps *caps = ::gst_buffer_get_caps(buffer);
		::g_printerr("preroll : caps ->: %s", ::gst_caps_to_string(caps));
	}
	return GST_FLOW_OK;
}
// when the appsink output one new frame, new_buffer() is called
static GstFlowReturn new_buffer(GstAppSink *sink, gpointer user_data) {
	GstBuffer *buffer =  gst_app_sink_pull_buffer (sink);
	if (buffer) {
		print_buffer(buffer, "buffer");

		VideoBuffer *rbuffer = reinterpret_cast< VideoBuffer * >(user_data);
		unsigned char *buf = reinterpret_cast< unsigned char * >(buffer->data);
		rbuffer->SetBuffer(buf, buffer->size);
	}
	return GST_FLOW_OK;
}


VideoBuffer::VideoBuffer()
	: buffer(new unsigned char[sizeof(char) * 8]),
	size(0),
	running_proc(false),
	updated(false)
  {
	  ::InitializeCriticalSection(&critical_section);
	  cout << "Initialized Critical section" << endl;
  }

VideoBuffer::~VideoBuffer() {
	::DeleteCriticalSection(&critical_section);
	if (buffer) {
		delete[] buffer;
	}
}

void VideoBuffer::LockBuffer() {
	cout << "trying to lock buffer" << endl; 
	::EnterCriticalSection(&critical_section); // error occurs on this line. 
	cout << "buffer locked" << endl;
}

void VideoBuffer::UnlockBuffer() {
	::LeaveCriticalSection(&critical_section);
}

void VideoBuffer::SetBuffer(unsigned char *new_buffer, size_t new_size) {
	LockBuffer();
	if (new_size != size) {
		if (buffer) {
			delete[] buffer;
		}
		buffer = new unsigned char[new_size];
		cout << "setting buffer size" << endl;
		size = new_size;
	}
	memset(buffer, size, 1);
	memcpy_s(new_buffer, size, buffer, size);
	updated = true;
	UnlockBuffer();
}

VideoEncoder::VideoEncoder(VideoBuffer *vid_buf) {
	video_buffer = vid_buf;
	cout << "creating video encoder" << endl;
	//TODO: complete
}

VideoEncoder::~VideoEncoder() {
	if (video_buffer != NULL) {
		delete video_buffer;
		video_buffer = NULL;
	}
	//TODO: complete
}

// Thread function to retrieve received data
unsigned int _stdcall process_thread(void *lpvoid)
{
	//Error has to do with calling LockBuffer() inside here. so cast maybe?
	unsigned char color[3];
	VideoBuffer *vid_buffer = reinterpret_cast< VideoBuffer * >(lpvoid);
	cout << "in process_thread" << endl;

	while(vid_buffer->running_proc)
	{
		cout << (vid_buffer->updated ? "true" : "false");
		if(vid_buffer->updated)
		{
			cout << "in if statement" << endl;
			vid_buffer->LockBuffer();
			::memcpy_s(color, sizeof(unsigned char)*3, vid_buffer->buffer, sizeof(unsigned char)*3);
			::g_printerr("Received data : [%d %d %d]\n", color[0], color[1], color[2]);
			vid_buffer->updated = false;
			vid_buffer->UnlockBuffer();
		}
		Sleep(1000);
	}

	return 0;
}


unsigned int VideoEncoder::encode() {

	//TODO: complete and delete the test code above.
	
	cout << "starting encoding" << endl;
	App *app = &s_app;
	GError *error = NULL;
	GstElement *encode_pipeline, *video_src, *ffmpegcolorspace, *mp4encoder, *ffmpegcolorspace2, *appsink;
	gst_init(NULL, NULL);
	encode_pipeline = gst_pipeline_new("encode_pipeline");
	app->pipeline = encode_pipeline;
	app->loop = g_main_loop_new(NULL, TRUE);

	video_src = gst_element_factory_make("autovideosrc", "video_src");
	ffmpegcolorspace = gst_element_factory_make("ffmpegcolorspace", "ffmpegcolorspace");
	mp4encoder = gst_element_factory_make("ffenc_mpeg4", "mp4encoder");
	ffmpegcolorspace2 = gst_element_factory_make("ffmpegcolorspace", "ffmpegcolorspace2");
	appsink = gst_element_factory_make("appsink", "appsink");

	GstAppSinkCallbacks cb = {NULL, new_preroll, new_buffer, new_buffer_list,  {NULL}};
	gst_app_sink_set_callbacks (GST_APP_SINK(appsink), &cb,  video_buffer, NULL);

	gst_bin_add_many((GstBin *) encode_pipeline, video_src, ffmpegcolorspace, mp4encoder, appsink, NULL);
	if (! gst_element_link_many(video_src, ffmpegcolorspace, mp4encoder, appsink, NULL)) {
		cout << "error linking" << endl;
		Sleep(1000);
		exit(1);
	}

	video_buffer->running_proc = true;
	HANDLE thread = (HANDLE)::_beginthreadex(NULL, 0, process_thread, video_buffer, 0, NULL);
	cout << "set to playing" << endl;
	gst_element_set_state (encode_pipeline, GST_STATE_PLAYING);
	if (!encode_pipeline) {
		fprintf (stderr, "Parse error: %s\n", error->message);
		 exit(1);
	}
	g_main_loop_run(app->loop);
	// Finish thread
	video_buffer->running_proc = false;
	::WaitForSingleObject(thread, INFINITE);
	while(1) {};
	return 0;
}

unsigned int VideoEncoder::decode() {
	App *app = &decode_app;
	GError *error = NULL;
	GstElement *decode_pipeline, *appsrc, *mp4decoder, *ffmpegcolorspace, *autovideosink;
	gst_init(NULL, NULL);
	app->loop = g_main_loop_new(NULL, TRUE);
	app->timer = g_timer_new();

	decode_pipeline = gst_pipeline_new("decode_pipeline");
	appsrc = gst_element_factory_make("autovideosrc", "appsrc"); //TODO: Change this to appsrc.
	ffmpegcolorspace = gst_element_factory_make("ffmpegcolorspace", "ffmpegcolorspace");
	autovideosink = gst_element_factory_make("autovideosink", "autovideosink");

	gst_bin_add_many((GstBin *) decode_pipeline, appsrc, ffmpegcolorspace, autovideosink, NULL);
	if (! gst_element_link_many(appsrc, ffmpegcolorspace, autovideosink, NULL)) {
		cout << "could not link decode elements" << endl;
		Sleep(1000);
		return 1;
	}
	
	gst_element_set_state (decode_pipeline, GST_STATE_PLAYING);
	if (!decode_pipeline) {
		fprintf (stderr, "Parse error: %s\n", error->message);
		 exit(1);
	}
	while(1){};
	return 1;
}

unsigned int _stdcall encode_task(void *video_buffer) {
	VideoBuffer *vb = (VideoBuffer *) video_buffer;
	VideoEncoder encoder(vb);
	encoder.encode();
	return 1;
}

unsigned int _stdcall decode_task(void *video_buffer) {
	VideoBuffer *vb = (VideoBuffer *) video_buffer;
	VideoEncoder decoder(vb);
	decoder.decode();
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	//is the error because the video buffer is defined here?
	VideoBuffer *test = new VideoBuffer();
	VideoEncoder ve(test);
	ve.decode();
	//HANDLE thread = (HANDLE)::_beginthreadex(NULL, 0, encode_task, test, 0, NULL);
	cout << "hi" << endl;
	Sleep(1000);
	while(1) {};
	return 0;
}

