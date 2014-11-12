/*
	video.cpp
*/

#ifndef COMPILE_V25

#include <File.h>

#include "behugo.h"
#include "MediaView.h"
#include "SubsetIO.h"

extern "C"
{
#include "heheader.h"

int hugo_hasvideo(void);
void hugo_stopvideo(void);
int hugo_playvideo(HUGO_FILE, long, char, char, int);
}

MediaView *videoview;
static thread_id video_thread;
static float video_volume = 100.0;
static bool video_loop = false;
static BRect video_rect;
bool video_playing = false, video_background = false;


/* hugo_hasvideo */

int hugo_hasvideo(void)
{
	if (!display_graphics) return false;
	return true;
}


/* hugo_stopvideo */

void hugo_stopvideo(void)
{
	if (video_playing)
	{
// This way of stopping the video seems to cause a race condition
// with window->Lock():
//		videoview->Control(MEDIA_STOP);
//		while (video_playing)
//		{
//			snooze(5000);
//		}
		video_playing = false;
		snooze(50000);
	}
}


/* VideoThread */

int32 VideoThread(void *data)
{
	bool is_suspended = false;
	float suspended_vol = 1.0;
		
	float w = 0.0;
	float h = 0.0;
	subset_io_data *sid = (subset_io_data *)data;
	
	videoview = new MediaView(BRect(0, 0, 0, 0), "videoview", B_FOLLOW_NONE);
	visible_view->AddChild(videoview);
	SubsetIO *sio = new SubsetIO(sid->file, sid->start, sid->start+sid->length);
	if (videoview->SetMediaSource(sio)!=B_NO_ERROR) goto Exit;

	if (window->Lock())
	{
		float window_width, window_height, ratio;
		window_width = video_rect.Width();
		window_height = video_rect.Height();

		videoview->GetPreferredSize(&w, &h);
		ratio = w/h;
		if (w > window_width) w = window_width;
		if (h > window_height) h = window_height;
		
		if (w/h > ratio) w = h*ratio;
		if (w/h < ratio) h = w/ratio;

		videoview->MoveTo(
			video_rect.left + window_width/2.0 - w/2.0,
			video_rect.top + window_height/2.0 - h/2.0);

		videoview->ResizeTo(w, h);

		window->Unlock();
	}

	videoview->loop = video_loop;
	videoview->SetVolume((float)video_volume/100.0);

	videoview->Control(MEDIA_PLAY);
	video_playing = true;
	
	while (videoview->IsPlaying() && video_playing)
	{
		// Suspend
		if ((!enable_audio || audio_factory_suspended) && !is_suspended)
		{
			suspended_vol = videoview->Volume();
			videoview->SetVolume(0);
			is_suspended = true;
		}
		// Resume
		else if ((enable_audio && !audio_factory_suspended) && is_suspended)
		{
			videoview->SetVolume(suspended_vol);
			is_suspended = false;
		}
		
		if (!display_graphics || quit_he_thread)
		{
			videoview->Control(MEDIA_STOP);
			break;
		}
		snooze(20000);
	}
	
	// In case we exit by clearing video_playing in hugo_stopvideo()
	if (videoview->IsPlaying()) videoview->Control(MEDIA_STOP);

Exit:
	if (window->Lock())
	{
		visible_view->RemoveChild(videoview);
		window->Unlock();
	}
	
	video_playing = false;
	// If we're quitting, destroying the parent will destroy the view
	if (!quit_he_thread) delete videoview;
	delete sid;
	delete sio;
	return 0;
}


/* hugo_playvideo */

int hugo_playvideo(HUGO_FILE infile, long reslength,
	char loop_flag, char background, int volume)
{
	long fpos = ftell(infile);
	fclose(infile);
	if (fpos==-1) return false;
	
	if (!display_graphics) return false;

	view->needs_updating = true;	// sometimes the screen isn't updated,
	view->Update(true);		// so force an update
	
	char *path;
	if (!strcmp(loaded_filename, ""))
		path = loaded_resname;
	else
		path = loaded_filename;

	// Create a BFile from the path and position it; VideoThread
	// will delete it when it deletes the subset_io_data
	BFile *file;
	if (!(file = TrytoOpenBFile(path, "games")))
	{
		if (!(file = TrytoOpenBFile(path, "object")))
		{
			return false;
		}
	}
	if (file->InitCheck()!=B_OK)
	{
		delete file;
		return false;
	}

	subset_io_data *sid = new subset_io_data(file, fpos, reslength);

	// Figure out the area the video will play back in
	video_rect = BRect(physical_windowleft, physical_windowtop,
		physical_windowright, physical_windowbottom);
	// Set up some playback params for VideoThread
	video_volume = volume;
	video_loop = loop_flag;
	video_background = background;
		
	// If we're blocking while playing...
	if (!background)
	{
		VideoThread(sid);
	}
	// ...otherwise launch a thread and get out of here
	else
	{
		video_thread = spawn_thread(VideoThread, "MediaView video thread", B_NORMAL_PRIORITY, sid);
		resume_thread(video_thread);
	}
	
	return true;
}

#endif	// !COMPILE_V25
