/*
	MediaView.cpp
*/

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <Autolock.h>
#include <Bitmap.h>
#include <Entry.h>
#include <ScrollBar.h>
#include <Screen.h>
#include <Path.h>
#include <Window.h>

#include "MediaView.h"

#include "MediaFile.h"
#include "MediaTrack.h"
#include "AudioOutput.h"


MediaView::MediaView(BRect frame, const char *name, uint32 resizeMask, uint32 flags)
	: BView(frame, name, resizeMask, flags)
{
	InitObject();
}

MediaView::~MediaView()
{
	Stop();
	Reset();
}

//status_t MediaView::SetMediaSource(const char *path)
status_t MediaView::SetMediaSource(BDataIO *source)
{
	BAutolock autolock(Window());

//	status_t err = B_ERROR;
//	entry_ref ref;
//	err = get_ref_for_path(path, &ref);
//	if (err != B_NO_ERROR)
//		return (err);

//	fMediaFile = new BMediaFile(&ref);
	fMediaFile = new BMediaFile(source);
	
	bool foundTrack = false;
	int32 numTracks = fMediaFile->CountTracks();

	for (int32 i = 0; i < numTracks; i++)
	{
		BMediaTrack *track = fMediaFile->TrackAt(i);
		
		if (track == NULL)
		{
			Reset();
			return B_ERROR;
		}
		else
		{
			bool trackUsed = false;
			media_format mf;

			if (track->EncodedFormat(&mf) == B_NO_ERROR)
			{
				switch (mf.type)
				{
					case B_MEDIA_ENCODED_VIDEO:
						trackUsed = SetVideoTrack(track, &mf) == B_NO_ERROR;
						break;
	
					case B_MEDIA_RAW_AUDIO:
						trackUsed = SetAudioTrack(track, &mf) == B_NO_ERROR;
						break;
						
					case B_MEDIA_ENCODED_AUDIO:
						if (track->DecodedFormat(&mf) == B_NO_ERROR)			
							trackUsed = SetAudioTrack(track, &mf) == B_NO_ERROR;
						break;

					default:
						break;
				}
			}
	
			if (trackUsed)
				foundTrack = true;
			else
			{
				fMediaFile->ReleaseTrack(track);
			}
		}
	}

	if (foundTrack)
	{
		status_t err = B_ERROR;
	
		fPlayerThread = spawn_thread(MediaView::MediaPlayer,
			 "MediaView::MediaPlayer", B_NORMAL_PRIORITY, this);
	
		if (fPlayerThread < B_NO_ERROR)
		{
			err = fPlayerThread;
			fPlayerThread = B_ERROR;
			Reset();
			return err;
		}
	
		fPlaySem = create_sem(0, "MediaView::fPlaySem");
		if (fPlaySem < B_NO_ERROR)
		{
			err = fPlaySem;
			fPlaySem = B_ERROR;
			Reset();
			return err;
		}

		err = resume_thread(fPlayerThread);
	
		if (err!=B_NO_ERROR)
		{
			kill_thread(fPlayerThread);
			fPlayerThread = B_ERROR;
			Reset();
			return err;
		}
	}
	
	// If no track is found
	else
	{
		return B_MEDIA_NO_HANDLER;
	}
	
	return B_NO_ERROR;
}

status_t MediaView::SetColorSpace(color_space depth)
{
	BAutolock autolock(Window());
	fBitmapDepth = depth;
	return B_NO_ERROR;
}

color_space MediaView::ColorSpace() const
{
	return fBitmapDepth;
}

status_t MediaView::Control(media_action action)
{
	BAutolock autolock(Window());
	status_t err = B_NO_ERROR;
	switch (action)
	{
		case MEDIA_PLAY:
			err = Play();
			break;

		case MEDIA_STOP:
			err = Stop();
			break;

		default:
			err = B_ERROR;
			break;
	}
	return err;
}

bool MediaView::IsPlaying() const
{
	return fPlaying;
}

bool MediaView::HasVideoTrack() const
{
	return (fVideoTrack != NULL);
}

bool MediaView::HasAudioTrack() const
{
	return (fAudioTrack != NULL);
}

void MediaView::GetPreferredSize(float *width, float *height)
{
	if (fBitmap==NULL)
	{
 		BView::GetPreferredSize(width, height);
	}
	else
	{
		BRect bitmapBounds = fBitmap->Bounds();
		*width = bitmapBounds.Width();
		*height = bitmapBounds.Height();
	}
}

void MediaView::Draw(BRect updateRect)
{
	if ((fBitmap != NULL) && !fUsingOverlay)
		DrawBitmap(fBitmap, VideoBounds());
}

void MediaView::DetachedFromWindow()
{
	Stop();
}

void MediaView::FrameResized(float width, float height)
{
	Draw(Bounds());
}

void MediaView::InitObject()
{
	BScreen screen;

	fMediaFile = NULL;
	fVideoTrack = NULL;
	fAudioTrack = NULL;
	fAudioOutput = NULL;
	fBitmap = NULL;
	fBitmapDepth = screen.ColorSpace();
	fCurTime = 0;
	fPlayerThread = B_ERROR;
	fPlaySem = B_ERROR;
	fScrubSem = B_ERROR;
	fPlaying = false;
	fSnoozing = false;
	fAudioDumpingBuffer = NULL;

	SetViewColor(B_TRANSPARENT_32_BIT);
	
	loop = false;
	paused = false;
	volume = 1.0;
}

status_t MediaView::SetVideoTrack(BMediaTrack *track, media_format *format)
{
	if (fVideoTrack != NULL)
		// is it possible to have multiple video tracks?
		return (B_ERROR);

	fVideoTrack = track;

	BRect bitmapBounds(0.0, 0.0,
		format->u.encoded_video.output.display.line_width - 1.0,
		format->u.encoded_video.output.display.line_count - 1.0);

	fBitmap = new BBitmap(bitmapBounds, B_BITMAP_WILL_OVERLAY | B_BITMAP_RESERVE_OVERLAY_CHANNEL, B_YCbCr422);
	fUsingOverlay = true;
	if (fBitmap->InitCheck() != B_OK)
	{
		media_format mf;
		fVideoTrack->DecodedFormat(&mf);
		int bpr = mf.u.raw_video.display.bytes_per_row;
		if (bpr==(int)media_raw_video_format::wildcard.display.bytes_per_row)
		{
			bpr = B_ANY_BYTES_PER_ROW;
		}

		delete fBitmap;
		fBitmap = new BBitmap(bitmapBounds, 0, fBitmapDepth, bpr);
		fUsingOverlay = false;
	};

	/* loop, asking the track for a format we can deal with */
	for(;;)
	{
		media_format mf, old_mf;

		BuildMediaFormat(fBitmap, &mf);

		old_mf = mf;
		fVideoTrack->DecodedFormat(&mf);
		if (old_mf.u.raw_video.display.format == mf.u.raw_video.display.format)
		{
			break;
		}

		int bpr = mf.u.raw_video.display.bytes_per_row;
		if (bpr==(int)media_raw_video_format::wildcard.display.bytes_per_row)
		{
			bpr = B_ANY_BYTES_PER_ROW;
		}
	
		fBitmapDepth = mf.u.raw_video.display.format;
		delete fBitmap;
		fUsingOverlay = false;
		fBitmap = new BBitmap(bitmapBounds, 0, fBitmapDepth, bpr);
	}

	media_header mh;
	bigtime_t time = fCurTime;	
	fVideoTrack->SeekToTime(&time);

	int64 dummyNumFrames = 0;
	fVideoTrack->ReadFrames((char *)fBitmap->Bits(), &dummyNumFrames, &mh);

	time = fCurTime;
	fVideoTrack->SeekToTime(&time);	

	if (fUsingOverlay)
	{
		overlay_restrictions r;
		fBitmap->GetOverlayRestrictions(&r);

		rgb_color key;
		SetViewOverlay(fBitmap, bitmapBounds, VideoBounds(), &key,B_FOLLOW_ALL,
			B_OVERLAY_FILTER_HORIZONTAL | B_OVERLAY_FILTER_VERTICAL);
		SetViewColor(key);
	};

	return B_NO_ERROR;
}


status_t MediaView::SetAudioTrack(BMediaTrack *track, media_format *format)
{
	if (fAudioTrack != NULL)
		// is it possible to have multiple tracks?
		return (B_ERROR);

	fAudioTrack = track;

	fAudioOutput = new AudioOutput(fAudioTrack, "AudioTrack");;
	status_t err = fAudioOutput->InitCheck();
	if (err != B_NO_ERROR)
	{
		delete (fAudioOutput);
		fAudioOutput = NULL;
		fAudioTrack = NULL;
		return err;
	}

	fAudioDumpingBuffer = malloc(format->u.raw_audio.buffer_size);

	return B_NO_ERROR;
}

status_t MediaView::Play()
{
	if (fPlaying) return B_NO_ERROR;

	fPlaying = true;
	release_sem(fPlaySem);

	return B_NO_ERROR;
}

status_t MediaView::Stop()
{
	if (!fPlaying) return B_NO_ERROR;

	acquire_sem(fPlaySem);
	fPlaying = false;

	return B_NO_ERROR;
}

void MediaView::Reset()
{
	delete_sem(fPlaySem);
	fPlaySem = B_ERROR;

	delete_sem(fScrubSem);
	fScrubSem = B_ERROR;

	status_t result = B_NO_ERROR;
	wait_for_thread(fPlayerThread, &result);
	fPlayerThread = B_ERROR;

	fVideoTrack = NULL;

	fAudioTrack = NULL;

	delete (fAudioOutput);
	fAudioOutput = NULL;

	delete (fBitmap);
	fBitmap = NULL;

	delete (fMediaFile);
	fMediaFile = NULL;

	free(fAudioDumpingBuffer);
	fAudioDumpingBuffer = NULL;
}

BRect MediaView::VideoBounds() const
{
	BRect videoBounds = Bounds();
	return (videoBounds);
}

void MediaView::BuildMediaFormat(BBitmap *bitmap, media_format *format)
{
	media_raw_video_format *rvf = &format->u.raw_video;

	memset(format, 0, sizeof(*format));

	BRect bitmapBounds = bitmap->Bounds();

	rvf->last_active = (uint32)(bitmapBounds.Height() - 1.0);
	rvf->orientation = B_VIDEO_TOP_LEFT_RIGHT;
	rvf->pixel_width_aspect = 1;
	rvf->pixel_height_aspect = 3;
	rvf->display.format = bitmap->ColorSpace();
	rvf->display.line_width = (int32)bitmapBounds.Width();
	rvf->display.line_count = (int32)bitmapBounds.Height();
	rvf->display.bytes_per_row = bitmap->BytesPerRow();
}

int32 MediaView::MediaPlayer(void *arg)
{
	MediaView* view = (MediaView *)arg;
	BWindow* window = view->Window();
	BBitmap* bitmap = view->fBitmap;
	BMediaTrack* videoTrack = view->fVideoTrack;
	BMediaTrack* audioTrack = view->fAudioTrack;
	BMediaTrack* counterTrack = (videoTrack != NULL) ? videoTrack : audioTrack;
	AudioOutput* audioOutput = view->fAudioOutput;
	void *adBuffer = view->fAudioDumpingBuffer;
	int64 numFrames = counterTrack->CountFrames();
	int64 numFramesToSkip = 0;
	int64 numSkippedFrames = 0;
	bool scrubbing = false;
	bool seekNeeded = false;
	int64 dummy;
	media_header mh;
	bigtime_t vStartTime, aStartTime, seekTime, snoozeTime, startTime;
	bigtime_t curScrubbing, lastScrubbing, lastTime;
bigtime_t watchdog = 0;
int watchdog_count = 0;

	curScrubbing = lastScrubbing = system_time();
	seekTime = 0LL;

	// Main processing loop (handle stop->start->stop)
	while (acquire_sem(view->fPlaySem) == B_OK)
	{
		release_sem(view->fPlaySem);
		
		// as we are doing stop->start, restart audio if needed.
		if (audioTrack != NULL)
			audioOutput->Play();
		startTime = system_time()-counterTrack->CurrentTime();		

		// This will loop until the end of the stream
		while ((counterTrack->CurrentFrame() < numFrames) || scrubbing)
		{
			if (view->paused)
			{
				snooze(20000);
				continue;
			}
// There's some trouble with playing (MP3s), where counterTrack->CurrentFrame()
// never gets to equal numFrames, so look out for that if we're not paused
else
{
	if (watchdog==counterTrack->CurrentFrame())
		watchdog_count++;
	else
		watchdog_count = 0;
	watchdog = counterTrack->CurrentFrame();
	if (watchdog_count>=10)
	{
		numFrames = counterTrack->CurrentFrame();
		watchdog_count = 0;
	}
}
			
			// We are in scrub mode
			if (acquire_sem(view->fScrubSem) == B_OK)
			{
				curScrubbing = system_time();

				// We are entering scrub mode
				if (!scrubbing)
				{
					if (audioTrack != NULL)
						audioOutput->Stop();
					scrubbing = true;
				}
				// Do a seek.
				seekNeeded = true;
				seekTime = view->fScrubTime;
			}
			// We are not scrubbing
			else if (scrubbing)
			{
				if (audioTrack != NULL)
					audioOutput->Play();
				scrubbing = false;
			}
			
			// Handle seeking
			if (seekNeeded)
			{
				if (videoTrack)
				{
					// Seek the seekTime as close as possible
					vStartTime = seekTime;
					videoTrack->SeekToTime(&vStartTime);
					
					// Read frames until we get less than 50ms ahead.
					lastTime = vStartTime;
					do
					{
						bitmap->LockBits();
						status_t err = videoTrack->ReadFrames((char*)bitmap->Bits(), &dummy, &mh);
						bitmap->UnlockBits();
						if (err != B_OK) break;
						vStartTime = mh.start_time;
						if ((dummy == 0) || (vStartTime <= lastTime))
							break;
						lastTime = vStartTime;
					} while (seekTime - vStartTime > 50000);
				}
				
				if (audioTrack)
				{
					// Seek the extractor as close as possible
					aStartTime = seekTime;
					audioOutput->SeekToTime(&aStartTime);
					
					// Read frames until we get less than 50ms ahead.
					lastTime = aStartTime;
					while (seekTime - aStartTime > 50000) {
						if (audioTrack->ReadFrames((char *)adBuffer, &dummy, &mh) != B_OK)
							break;
						aStartTime = mh.start_time;
						if ((dummy == 0) || (aStartTime <= lastTime))
							break;
						lastTime = aStartTime;
					}
				}
				else startTime = system_time() - vStartTime;
				
				// Set the current time
				view->fCurTime = seekTime;	
			
				seekNeeded = false;
			}		
			// Handle normal playing mode
			else
			{
				// Get the next video frame, if any
				if (videoTrack != NULL)
				{
					bitmap->LockBits();
					status_t err = videoTrack->ReadFrames((char*)bitmap->Bits(), &dummy, &mh);
					bitmap->UnlockBits();
					if (err != B_OK) goto do_reset;
					if (dummy == 0)
						goto do_reset;
					vStartTime = mh.start_time;
				}

				// Estimated snoozeTime
				if (audioTrack != NULL)
					startTime = audioOutput->TrackTimebase();
				if (videoTrack != NULL)
					snoozeTime = vStartTime - (system_time() - startTime);
				else
					snoozeTime = 25000;

				// Handle timing issues
				if (snoozeTime > 5000LL)
				{
					view->fSnoozing = true;
					snooze(snoozeTime-1000);
					view->fSnoozing = false;
				}
				else if (snoozeTime < -5000)
				{
					numSkippedFrames++;
					numFramesToSkip++;
				}
				
				// Set the current time
				if (!scrubbing)
				{
					view->fCurTime = system_time() - startTime;
					if (view->fCurTime < seekTime)
						view->fCurTime = seekTime;
				}				
			}
				
			// Handle the drawing; no drawing if we need to skip a frame
			if (numSkippedFrames > 0)
				numSkippedFrames--;
			// If we can't lock the window after 50ms, better to give up for
			// that frame
			else if (window->LockWithTimeout(50000) == B_OK)
			{
				if ((videoTrack != NULL) && !view->fUsingOverlay)
					view->DrawBitmap(bitmap, view->VideoBounds());
				window->Unlock();
				// In scrub mode, don't scrub more than 10 times a second
				if (scrubbing)
				{
					snoozeTime = (100000LL+lastScrubbing) - system_time();
					if (snoozeTime > 4000LL)
					{
						view->fSnoozing = true;
						snooze(snoozeTime-1000LL);
						view->fSnoozing = false;
					}
					lastScrubbing = curScrubbing;
				}
			}				
			
			// Check if we are required to stop.
			if (acquire_sem_etc(view->fPlaySem, 1, B_TIMEOUT, 0) == B_OK)
				release_sem(view->fPlaySem);
			// The MediaView asked us to stop.
			else
			{
				if (audioTrack != NULL)
					audioOutput->Stop();
				goto do_restart;
			}
		}		

		// If we exited the main streaming loop because we are at the end,
		// then we need to loop.
		if (counterTrack->CurrentFrame() >= numFrames)
		{
do_reset:
			if (audioTrack != NULL)
				audioOutput->Stop();
				
			if (view->loop)
			{
				seekNeeded = true;
				seekTime = 0LL;
				scrubbing = true;
			}
			else
			{
				view->fPlaying = false;
				break;
			}
		}
do_restart:;
	}

	return B_NO_ERROR;
}

status_t MediaView::SetVolume(float vol)
{
	if (fAudioOutput) fAudioOutput->SetVolume(vol);
	return B_NO_ERROR;
}

float MediaView::Volume()
{
	if (fAudioOutput) return fAudioOutput->Volume();
	return 1.0;
}
