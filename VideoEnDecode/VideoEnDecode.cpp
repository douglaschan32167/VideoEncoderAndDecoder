

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
#include <fstream>
#include <sstream>
#include <Windows.h>
#include <gstreamer-0.10\gst\gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <process.h>
#include <glib.h>

using namespace std;
using namespace System;
ofstream file ("packet_sizes.txt", ios::out);

const int WIDTH = 640;
const int HEIGHT = 480;

void VideoEncoder::print_structure(const GstStructure *structure, const char *title, const char *type_name) {
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

void VideoEncoder::print_buffer(GstBuffer *buffer, const char *type_name) {
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
GstFlowReturn VideoEncoder::new_buffer_list(GstAppSink *sink, gpointer user_data) {
	GstBufferList *list = gst_app_sink_pull_buffer_list (sink);
	GstBufferListIterator *it = gst_buffer_list_iterate (list);
	GstBuffer *buffer;
	while (gst_buffer_list_iterator_next_group (it)) {
		while ((buffer = gst_buffer_list_iterator_next (it)) != NULL) {
			//print_buffer(buffer, "new_buffer_list");
		}
	}
	gst_buffer_list_iterator_free (it);
	return GST_FLOW_OK;
}
GstFlowReturn VideoEncoder::new_preroll (GstAppSink *sink, gpointer user_data) {
	GstBuffer *buffer =  gst_app_sink_pull_preroll (sink);
	if (buffer) {
		//print_buffer(buffer, "preroll");

		GstCaps *caps = ::gst_buffer_get_caps(buffer);
		::g_printerr("preroll : caps ->: %s", ::gst_caps_to_string(caps));
	}
	return GST_FLOW_OK;
}

// This method is only for collecting information to make a graph. Usually this will not be called.
static void writeSizeToFile(size_t size) {
	//ofstream file ("packet_sizes.txt", ios::out);
	stringstream ss;
	ss  << size;
	file << ss.str();
	file << "\t";
	file << " ";
	//file.close();

}
// when the appsink outputs one new frame, new_buffer() is called
// is this running faster than decode? Is that why there is the delay?
GstFlowReturn VideoEncoder::new_buffer(GstAppSink *sink, gpointer user_data) {
	GstBuffer *buffer =  gst_app_sink_pull_buffer (sink);
	if (buffer && buffer->size < 60000) {
		//print_buffer(buffer, "buffer");//should I make rbuffer->buffer a GstBuffer?
		//cout << "in new buffer" << endl;
		VideoBuffer *rbuffer = reinterpret_cast< VideoBuffer * >(user_data);
		//unsigned char *buf = reinterpret_cast< unsigned char * >(buffer->data);
		rbuffer->SetBuffer(buffer, buffer->size);
		//writeSizeToFile(buffer->size);
		rbuffer->caps = buffer->caps;
	}
	return GST_FLOW_OK;
}

//Creator function for VideoBuffer. 
VideoBuffer::VideoBuffer()
	: //buffer(NULL),
	//size(0),
	running_proc(false),
	updated(false),
	finishing(false)
  {
	  ::InitializeCriticalSection(&critical_section);
	  buffer = gst_buffer_new();
	  gpointer buffer_data = g_malloc(WIDTH * HEIGHT * 3);
	  GST_BUFFER_DATA(buffer) = (guint8*)buffer_data;
	  GST_BUFFER_SIZE(buffer) = WIDTH * HEIGHT * 3;
	  size = WIDTH * HEIGHT * 3;
	  GST_BUFFER_FREE_FUNC(buffer) = ::g_free;
	  cout << "Initialized Critical section" << endl;
  }

VideoBuffer::~VideoBuffer() {
	::DeleteCriticalSection(&critical_section);
	if (buffer) {
		delete[] buffer;
	}
}

void VideoBuffer::LockBuffer() {
	::EnterCriticalSection(&critical_section);
}

void VideoBuffer::UnlockBuffer() {
	::LeaveCriticalSection(&critical_section);
}

void VideoBuffer::SetBuffer(GstBuffer *new_buffer, size_t new_size) {
	LockBuffer();
	/*cout << "set buffer" << endl;
	if (new_size != size) {
		if (buffer) {
			delete buffer;
		}
		gpointer new_data = g_malloc(new_size);
		GST_BUFFER_DATA(buffer) = (guint8*)new_data;
		cout << "setting buffer size" << endl;
		buffer->size = new_size;
	}
	//memset(buffer, size, 1);
	memcpy_s(new_buffer, buffer->size, buffer, buffer->size);*/
	buffer = new_buffer; // can I do this? I feel like something will go wrong here.
	//memcpy_s(new_buffer, new_size, buffer, new_size);
	updated = true;
	UnlockBuffer();
}

VideoEncoder::VideoEncoder(VideoBuffer *vid_buf) {
	video_buffer = vid_buf;
	init_called = false;
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
			//::memcpy_s(color, sizeof(unsigned char)*3, vid_buffer->buffer, sizeof(unsigned char)*3);
			//::g_printerr("Received data in process thread : [%d %d %d]\n", color[0], color[1], color[2]);
			//vid_buffer->updated = false;
			vid_buffer->UnlockBuffer();
		}
		Sleep(1000);
	}

	return 0;
}
// Callback function for Appsrc to notify updating buffer
gboolean VideoEncoder::read_data (VideoBuffer * video_buffer, App *app) 
{ 
    GstFlowReturn ret; 
	if(video_buffer->finishing)
	{
		cout << "finishing" << endl;
		::g_signal_emit_by_name (video_buffer->appsrc, "end-of-stream", &ret);
		return FALSE;
	}
 
    //ms = g_timer_elapsed(app->timer, NULL); 
    if (video_buffer->updated) {//ms > 1.0/20.0) { 
		gboolean ok = TRUE;

		video_buffer->LockBuffer();
		unsigned char color[3];
		::memcpy_s(color, sizeof(unsigned char)*3, video_buffer->buffer, sizeof(unsigned char)*3);
		//printf("Buffer size: %d\n", video_buffer->buffer->size);
		::g_printerr("Received data in read_data : [%d %d %d]\n", color[0], color[1], color[2]);
		//cout << video_buffer->buffer << endl;
		g_signal_emit_by_name (video_buffer->appsrc, "push-buffer", video_buffer->buffer, &ret);
		video_buffer->updated = false;
		video_buffer->UnlockBuffer();

		//Sleep(300);
		
		if (ret != GST_FLOW_OK)
		{
			/* some error, stop sending data */
			ok = FALSE;
		}
        
		//::g_timer_start(app->timer); 
 
        return ok; 
    } 
 
    return TRUE; 
} 
 

// Callback function to print bus message
gboolean VideoEncoder::bus_message (GstBus * bus, GstMessage * message, _App * app) 
{
	cout << "bus message" << endl;
  GST_DEBUG ("got message %s", 
      gst_message_type_get_name (GST_MESSAGE_TYPE (message))); 
 
	switch (GST_MESSAGE_TYPE (message)) { 
	case GST_MESSAGE_STATE_CHANGED:
		GstState oldstate, newstate;

		::gst_message_parse_state_changed(message, &oldstate, &newstate, NULL);
		g_printerr("[%s]; %s -> %s\n", GST_OBJECT_NAME(message->src), 
			::gst_element_state_get_name(oldstate), 
			::gst_element_state_get_name(newstate));
		break;
	case GST_MESSAGE_ERROR: { 
		GError *err = NULL; 
		gchar *dbg_info = NULL; 
 
		gst_message_parse_error (message, &err, &dbg_info); 
		g_printerr ("ERROR from element %s: %s\n", 
			GST_OBJECT_NAME (message->src), err->message); 
		g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none"); 
		g_error_free (err); 
		g_free (dbg_info); 
		g_main_loop_quit (app->loop); 
		break; 
	} 
	case GST_MESSAGE_EOS: 
		g_main_loop_quit (app->loop); 
		break; 
	default: 
		const GstStructure *structure = message->structure;
		print_structure(structure, "def busmessage", ::gst_message_type_get_name(message->type));

		break; 
	} 
	return TRUE; 
}


/* This signal callback is called when appsrc needs data, we add an idle handler 
* to the mainloop to start pushing data into the appsrc */ 
void VideoEncoder::start_feed (GstElement * pipeline, guint size, VideoBuffer * app) 
{ 
	cout << "in start feed" << endl;
  //if (app->source_id == 0) { 
	printf("start feed");
    app->source_id = g_idle_add ((GSourceFunc) read_data, app);
	cout << app->source_id << endl;
  //} 
} 
 
/* This callback is called when appsrc has enough data and we can stop sending. 
* We remove the idle handler from the mainloop */ 
void VideoEncoder::stop_feed (GstElement * pipeline, VideoBuffer * app) 
{ 
  if (app->source_id != 0) { 
    g_source_remove (app->source_id); 
    app->source_id = 0; 
  } 
} 

//The function that encodes the video data. 
unsigned int VideoEncoder::encode() {

	//TODO: complete and delete the test code above.
	
	cout << "starting encoding" << endl;
	App *app = &s_app;
	GError *error = NULL;
	GstElement *encode_pipeline, *video_src, *ffmpegcolorspace, *mp4encoder, *ffmpegcolorspace2, *appsink;
	gst_init(NULL, NULL);
	init_called = true;
	encode_pipeline = gst_pipeline_new("encode_pipeline");
	app->pipeline = encode_pipeline;
	app->loop = g_main_loop_new(NULL, TRUE);

	video_src = gst_element_factory_make("autovideosrc", "video_src");
	ffmpegcolorspace = gst_element_factory_make("ffmpegcolorspace", "ffmpegcolorspace");
	mp4encoder = gst_element_factory_make("ffenc_mpeg4", "mp4encoder");//fenc_mpeg4
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

	video_buffer->caps = gst_app_sink_get_caps((GstAppSink *) appsink);
	video_buffer->running_proc = true;
	//HANDLE thread = (HANDLE)::_beginthreadex(NULL, 0, process_thread, video_buffer, 0, NULL);
	cout << "set to playing" << endl;
	gst_element_set_state (encode_pipeline, GST_STATE_PLAYING);
	if (!encode_pipeline) {
		fprintf (stderr, "Parse error: %s\n", error->message);
		 exit(1);
	}
	g_main_loop_run(app->loop);
	// Finish thread
	video_buffer->running_proc = false; //comment out for more info.
	//::WaitForSingleObject(thread, INFINITE);
	while(1) {};
	return 0;
}

unsigned int VideoEncoder::decode() {
//	while (!init_called) {
//		Sleep(1200);
//		cout << "waiting" << endl;
//	}
	Sleep(2300); // use SetEvent
	GstBus *bus;
	App *app = &decode_app;
	GError *error = NULL;
	GstElement *decode_pipeline, *appsrc, *mp4decoder, *ffmpegcolorspace, *autovideosink;
	gst_init(NULL, NULL);
	app->loop = g_main_loop_new(NULL, TRUE);
	app->timer = g_timer_new();

	decode_pipeline = gst_pipeline_new("decode_pipeline");
	app->pipeline = decode_pipeline;
	video_buffer->appsrc = gst_element_factory_make("appsrc", "appsrc"); //TODO: Change this to appsrc.
	ffmpegcolorspace = gst_element_factory_make("ffmpegcolorspace", "ffmpegcolorspace");
	mp4decoder = gst_element_factory_make("ffdec_mpeg4", "mp4decoder");//ffdec_mpeg4
	autovideosink = gst_element_factory_make("autovideosink", "autovideosink");
	//autovideosink = gst_element_factory_make("filesink", "autovideosink");
	//autovideosink->setProperty("location", "test");

		// Set bus message handler
	bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline)); 
	g_assert(bus); //error occcurs here
	/* add watch for messages */ 
	gst_bus_add_watch (bus, (GstBusFunc) bus_message, app); 
	gst_object_unref (bus); 


	g_signal_connect(video_buffer->appsrc, "need-data", G_CALLBACK(start_feed), video_buffer);
	g_signal_connect(video_buffer->appsrc, "enough-data", G_CALLBACK(stop_feed), video_buffer);


	gst_bin_add_many((GstBin *) decode_pipeline, video_buffer->appsrc, mp4decoder, ffmpegcolorspace, autovideosink, NULL);
	if (! gst_element_link_many(video_buffer->appsrc, mp4decoder, ffmpegcolorspace, autovideosink, NULL)) {
		cout << "could not link decode elements" << endl;
		Sleep(1000);
		return 1;
	}
	
	gst_app_src_set_caps(GST_APP_SRC(video_buffer->appsrc), video_buffer->caps);
	cout<<"before calling main loop run"<< endl;
	
	//g_main_loop_run(app->loop);
	gst_element_set_state (decode_pipeline, GST_STATE_PLAYING);
	if (!decode_pipeline) {
		fprintf (stderr, "Parse error: %s\n", error->message);
		 exit(1);
	}
	//while(1){};
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
	gst_init(NULL, NULL);
	VideoBuffer *test = new VideoBuffer();
	VideoEncoder ve(test);

	HANDLE thread = (HANDLE)::_beginthreadex(NULL, 0, encode_task, test, 0, NULL);
	ve.decode();
	Sleep(1000);
	while(1) {
	::Sleep(10);
	};
	return 0;
}

/* Make it run in release mode.
*/