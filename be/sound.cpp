/*
	SOUND.CPP

	BeOS routines for music (MOD, S3M, XM format modules) using MikMod
	and sound (RIFF WAVE samples):
	
		hugo_playmusic
		hugo_stopmusic
		hugo_playsample
		hugo_stopsample

	for the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman

	Both hugo_playmusic() and hugo_playsample() are called with the
	file hot--i.e., opened and positioned to the start of the resource.
	It must be closed before returning.
	
	- A MediaView is used for playback of media-kit-supported formats
	  like MP3 (AudioView) and WAV (SampleView).
	
	- The MIDI Kit is used for playback of MIDI.

	- MikMod is used to playback MOD, S3M, and XM.
*/

extern "C"
{
#include "heheader.h"

int hugo_playmusic(FILE *infile, long reslength, char loop_flag);
void hugo_musicvolume(int vol);
void hugo_stopmusic(void);
int hugo_playsample(FILE *infile, long reslength, char loop_flag);
void hugo_samplevolume(int vol);
void hugo_stopsample(void);

// From heres.c:
long FindResource(char *filename, char *resname);
}

#if !defined (NO_SOUND)

#include <MidiSynthFile.h>
#include <SoundPlayer.h>

#include "behugo.h"
#include "MediaView.h"
#define MIKMODAPI
#include "mikmod.h"
#include "SubsetIO.h"

#undef DEBUGGER

// From temp.cpp:
int CreateResourceCache(char **tempfilename, FILE *file, long length);

int hugo_playmusic_MediaView(FILE *infile, long reslength, char loop_flag);
int hugo_playmusic_MIDI(FILE *infile, long reslength, char loop_flag);
int hugo_playmusic_MikMod(FILE *infile, long reslength, char loop_flag);
void hugo_stopmusic_MediaView(void);
void hugo_stopmusic_MIDI(void);
void hugo_stopmusic_MikMod(void);

char audio_init_failed = true;
MODULE *modfile;
char music_is_playing = false;
int music_volume = 100;
char sample_is_playing = false;
int sample_volume = 100;
bool audio_factory_suspended = false;

#define MUSIC_MEDIAVIEW 1
#define MUSIC_MIDI	2
#define MUSIC_MIKMOD    3

#define MAXCHANNELS 32			/* for MikMod */


//--------------------------------------------------------------------
// General interface:
//--------------------------------------------------------------------

void PrintAudioError(void)
{
	static char printed_audio_error = 0;

	if (!printed_audio_error && enable_audio)
	{
		sprintf(line, "Unable to play sound/music:\n%s",
			MikMod_strerror(MikMod_errno));
		BAlert *alert = new BAlert("Sound Error", line,
			"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->Go();
		printed_audio_error = 1;
	}
}


/* hugo_playmusic */

int hugo_playmusic(FILE *infile, long reslength, char loop_flag)
{
	// It's important to call hugo_stopmusic() first, since this
	// will take care of deleting any tempfiles
	if (music_is_playing) hugo_stopmusic();

#ifdef DEBUG_SOUND
fprintf(stderr, "hugo_playmusic(infile, %ld, %d)\n", reslength, loop_flag);
fprintf(stderr, "[%s: %s]\n", loaded_filename, loaded_resname);
#endif	
	switch (resource_type)
	{
		case MOD_R:
		case S3M_R:
		case XM_R:
			return hugo_playmusic_MikMod(infile, reslength, loop_flag);
#if !defined (COMPILE_V25)
		case MP3_R:
			return hugo_playmusic_MediaView(infile, reslength, loop_flag);
		case MIDI_R:
			return hugo_playmusic_MIDI(infile, reslength, loop_flag);
#endif
		default:
			fclose(infile);
			return false;	// error
	}
}


/* hugo_stopmusic */

void hugo_stopmusic(void)
{
#ifdef DEBUG_SOUND
fprintf(stderr, "hugo_stopmusic()\n");
#endif
	if (music_is_playing==MUSIC_MEDIAVIEW)
		hugo_stopmusic_MediaView();
	else if (music_is_playing==MUSIC_MIDI)
		hugo_stopmusic_MIDI();
	else if (music_is_playing==MUSIC_MIKMOD)
		hugo_stopmusic_MikMod();
}


/* hugo_musicvolume */

void hugo_musicvolume(int vol)
{
	music_volume = vol;
}


//--------------------------------------------------------------------
// MediaView playback:
//--------------------------------------------------------------------

static MediaView *audioview;
static thread_id audio_thread;
static bool audio_loop;

int32 AudioThread(void *data)
{
	bool is_suspended = false;
	float suspended_vol = 1.0;
		
	subset_io_data *sid = (subset_io_data *)data;
	
	audioview = new MediaView(BRect(0, 0, 0, 0), "audioview", B_FOLLOW_NONE);
	SubsetIO *sio = new SubsetIO(sid->file, sid->start, sid->start+sid->length);
	if (audioview->SetMediaSource(sio)!=B_NO_ERROR) goto Exit;

	audioview->loop = audio_loop;
	audioview->SetVolume((float)music_volume/100.0);
#ifdef DEBUG_SOUND
fprintf(stderr, "AudioThread: audioview->SetVolume(%f)\n", (float)music_volume/100.0);
#endif
	audioview->Control(MEDIA_PLAY);
	music_is_playing = MUSIC_MEDIAVIEW;
	
	while (audioview->IsPlaying() && music_is_playing)
	{
		// Suspend
		if ((!enable_audio || audio_factory_suspended) && !is_suspended)
		{
			suspended_vol = audioview->Volume();
#ifdef DEBUG_SOUND
fprintf(stderr, "AudioThread: suspended_vol = %f\n", suspended_vol);
#endif
			audioview->SetVolume(0.0);
#ifdef DEBUG_SOUND
fprintf(stderr, "AudioThread suspend: audioview->SetVolume(0.0)\n");
#endif
			is_suspended = true;
		}
		// Resume
		else if ((enable_audio && !audio_factory_suspended) && is_suspended)
		{
			audioview->SetVolume(suspended_vol);
#ifdef DEBUG_SOUND
fprintf(stderr, "AudioThread resume: audioview->SetVolume(%f)\n", suspended_vol);
#endif
			is_suspended = false;
		}
		
		if (quit_he_thread)
		{
			audioview->Control(MEDIA_STOP);
			break;
		}
		snooze(20000);
	}
	
	// In case we got here by clearing music_is_playing in hugo_stopmusic()
	if (audioview->IsPlaying()) audioview->Control(MEDIA_STOP);

Exit:
	music_is_playing = false;
	delete audioview;
	delete sid;
	delete sio;
	return 0;
}


/* hugo_playmusic_MediaView

	Returns false if it fails because of an ERROR.
*/

int hugo_playmusic_MediaView(FILE *infile, long reslength, char loop_flag)
{
	long fpos = ftell(infile);
	fclose(infile);
	if (fpos==-1) return false;
	
	char *path;
	if (!strcmp(loaded_filename, ""))
		path = loaded_resname;
	else
		path = loaded_filename;

	// Create a BFile from the path and position it; AudioThread
	// will delete it
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

	audio_loop = loop_flag;
		
	// Launch the AudioThread
	audio_thread = spawn_thread(AudioThread, "MediaView audio thread", B_NORMAL_PRIORITY, sid);
	resume_thread(audio_thread);
	
	return true;
}


void hugo_stopmusic_MediaView(void)
{
	music_is_playing = false;
}


//--------------------------------------------------------------------
// MIDI playback:
//--------------------------------------------------------------------

static char *midi_tempfile = NULL;
static thread_id midi_thread;

int32 MIDIThread(void *data)
{
	bool is_suspended = false;
		
	BEntry *entry = (BEntry *)data;
	entry_ref ref;
	entry->GetRef(&ref);
	
	BMidiSynthFile file;
	file.LoadFile(&ref);
	file.EnableLooping(audio_loop);
	be_synth->SetSynthVolume((double)music_volume/(double)100);
#ifdef DEBUG_SOUND
fprintf(stderr, "MIDIThread: be_synth->SetSynthVolume(%f)\n", (float)((double)music_volume/(double)100));
#endif

	file.Start();
	music_is_playing = MUSIC_MIDI;
	
	while (!file.IsFinished() && music_is_playing)
	{
		// Suspend
		if ((!enable_audio || audio_factory_suspended) && !is_suspended)
		{
			file.Pause();
#ifdef DEBUG_SOUND
fprintf(stderr, "MIDIThread suspend: file.Pause()\n");
#endif
			is_suspended = true;
		}
		// Resume
		else if ((enable_audio && !audio_factory_suspended) && is_suspended)
		{
			file.Resume();
#ifdef DEBUG_SOUND
fprintf(stderr, "MIDI resume: file.Resume()\n");
#endif
			is_suspended = false;
		}
		
		if (quit_he_thread)
		{
			music_is_playing = false;
			break;
		}
		
		snooze(20000);
	}

	delete entry;
exit_thread(0);
	return 0;
}


/* hugo_playmusic_MIDI

	Returns false if it fails because of an ERROR.
	
	Note that we have to use a tempfile to load the MIDI file, since
	Be's MIDI Kit expects these to be standalone files, not part of a
	concatenated resource file.
*/

int hugo_playmusic_MIDI(FILE *infile, long reslength, char loop_flag)
{
	if (!CreateResourceCache(&midi_tempfile, infile, reslength)) return false;

	// Create a BEntry from tempfile; MIDIThread will delete it
	BEntry *entry = new BEntry(midi_tempfile, false);
	if (entry->InitCheck()!=B_OK) return false;
	
	audio_loop = loop_flag;
		
	// Launch the MIDIThread
	midi_thread = spawn_thread(MIDIThread, "MediaView MIDI thread", B_NORMAL_PRIORITY, entry);
	resume_thread(midi_thread);
	
	return true;
}

void hugo_stopmusic_MIDI(void)
{
	music_is_playing = false;

	// If a midi_tempfile exists, delete it
	if (midi_tempfile)
	{
		entry_ref ref;
		if (get_ref_for_path(midi_tempfile, &ref)==B_OK)
		{
			BEntry del(&ref, false);
			del.Remove();
		}
		free(midi_tempfile);
		midi_tempfile = NULL;
	}
}

 
//--------------------------------------------------------------------
// MikMod playback:
//--------------------------------------------------------------------
 
// The timer that will run the MikMod player:

static thread_id mikmod_thread;
extern BSoundPlayer *soco_bsbp;  // from MikMod's drv_be.cpp

int32 MikModThread(void *data)
{
	bool is_suspended = false;
		
	while (true)
	{
		if (quit_he_thread)
		{
			exit_thread(0);
			break;		// unnecessary
		}
		
		// Suspend
		if ((!enable_audio || audio_factory_suspended) && !is_suspended)
		{
			if (!Player_Paused()) Player_TogglePause();
			soco_bsbp->SetVolume(0.0);
#ifdef DEBUG_SOUND
fprintf(stderr, "MikModThread suspend: soco_bsbp->SetVolume(0.0)\n");
#endif
			is_suspended = true;
		}
		// Resume
		else if ((enable_audio && !audio_factory_suspended) && is_suspended)
		{
			if (Player_Paused()) Player_TogglePause();
			soco_bsbp->SetVolume(1.0);
#ifdef DEBUG_SOUND
fprintf(stderr, "MikModThread resume: soco_bsbp->SetVolume(1.0)\n");
#endif
			is_suspended = false;
		}

		MikMod_Update();
		// 20 ms resolution
		snooze(20000);
	}
exit_thread(0);
	return 0;
}


/* InitPlayer */

extern MDRIVER drv_be;

int InitPlayer(void)
{
	MikMod_RegisterLoader(&load_mod);	/* module loaders */
	MikMod_RegisterLoader(&load_s3m);
	MikMod_RegisterLoader(&load_xm);

//	MikMod_RegisterAllDrivers();		/* valid audio drivers */
	MikMod_RegisterDriver(&drv_be);

	md_mixfreq = 44100;			/* standard mixing frequency */
	md_mode = DMODE_16BITS | DMODE_STEREO |
		// software mixing
		DMODE_SOFT_MUSIC | DMODE_SOFT_SNDFX;
	md_device = 0;				/* standard device: autodetect */

	// This is 6 by default, but that's too much
	md_reverb = 0;

	if (MikMod_Init(""))			/* initialize driver */
	{
		audio_init_failed = true;
		return false;
	}
	audio_init_failed = false;

	/* Start the player here and keep it running globally */
	MikMod_SetNumVoices(MAXCHANNELS, MAXCHANNELS);
	MikMod_EnableOutput();
	// Start the timer
	mikmod_thread = spawn_thread(MikModThread, "MikMod thread", B_NORMAL_PRIORITY, NULL);
	resume_thread(mikmod_thread);

	return true;
}


/* hugo_playmusic_MikMod

	Returns false if it fails because of an ERROR.
*/

int hugo_playmusic_MikMod(FILE *f, long reslength, char loop_flag)
{
	if (audio_init_failed)
	{
		PrintAudioError();
		fclose(f);
		return true;	/* not an error */
	}

//	MikMod_Reset(NULL);
Player_Stop();

	// The file is already positioned
	modfile = Player_LoadFP(f, MAXCHANNELS, 0);
	fclose(f);

	if (modfile)
	{
		if (loop_flag)
			modfile->wrap = 1;
		else
			modfile->wrap = 0;

		soco_bsbp->SetVolumeDB(1.0);

		Player_SetVolume((music_volume*128)/100);
		Player_Start(modfile);

		// This extra bit of encouragement to shut up is apparently needed,
		// not just in MikModThread
		if (!enable_audio || audio_factory_suspended)
		{
			soco_bsbp->SetVolume(0.0);
		}

		music_is_playing = MUSIC_MIKMOD;
		
		return true;
	}
#if defined (DEBUGGER)
	else
		DebugMessageBox("Sound Library Error", MikMod_strerror(MikMod_errno));
#endif
	return false;
}


void hugo_stopmusic_MikMod(void)
{
	if (audio_init_failed) return;

	if (music_is_playing)
	{
		music_is_playing = false;
		modfile->numpos = 0;
		
		Player_Free(modfile);
	}
}


//--------------------------------------------------------------------
// Sample playback:
//--------------------------------------------------------------------
 
static MediaView *sampleview;
static thread_id sample_thread;
static bool sample_loop;
static bool sample_deleted = true;

int32 SampleThread(void *data)
{
	bool is_suspended = false;
	float suspended_vol = 1.0;
	
sample_deleted = false;

	subset_io_data *sid = (subset_io_data *)data;
	
	sampleview = new MediaView(BRect(0, 0, 0, 0), "sampleview", B_FOLLOW_NONE);
	SubsetIO *sio = new SubsetIO(sid->file, sid->start, sid->start+sid->length);
	if (sampleview->SetMediaSource(sio)!=B_NO_ERROR) goto Exit;

	sampleview->loop = sample_loop;
	sampleview->SetVolume((float)sample_volume/100.0);
#ifdef DEBUG_SOUND
fprintf(stderr, "SampleThread: sampleview->SetVolume(%f)\n", (float)sample_volume/100.0);
#endif
	
	sampleview->Control(MEDIA_PLAY);
	sample_is_playing = true;

	while (sampleview->IsPlaying() && sample_is_playing)
	{
		
		// Suspend
		if ((!enable_audio || audio_factory_suspended) && !is_suspended)
		{
			suspended_vol = sampleview->Volume();
#ifdef DEBUG_SOUND
fprintf(stderr, "SampleThread: suspended_vol = %f\n", suspended_vol);
#endif
			sampleview->SetVolume(0.0);
#ifdef DEBUG_SOUND
fprintf(stderr, "SampleThread suspend: sampleview->SetVolume(0.0)\n");
#endif
			is_suspended = true;
		}
		// Resume
		else if ((enable_audio && !audio_factory_suspended) && is_suspended)
		{
			sampleview->SetVolume(suspended_vol);
#ifdef DEBUG_SOUND
fprintf(stderr, "SampleThread resume: sampleview->SetVolume(%f)\n", suspended_vol);
#endif
			is_suspended = false;
		}
		
		if (quit_he_thread)
		{
			sampleview->Control(MEDIA_STOP);
			break;
		}
		snooze(20000);
	}
	
	// In case we got here by clearing music_is_playing in hugo_stopsample()
	if (sampleview->IsPlaying()) sampleview->Control(MEDIA_STOP);

Exit:
	sample_is_playing = false;
	delete sampleview;
	delete sid;
	delete sio;
sample_deleted = true;
exit_thread(0);
	return 0;
}


/* hugo_playsample

	Returns false if it fails because of an ERROR.
*/

int hugo_playsample(FILE *infile, long reslength, char loop_flag)
{
	long fpos = ftell(infile);
	fclose(infile);
	if (fpos==-1) return false;
	
#ifdef DEBUG_SOUND
fprintf(stderr, "hugo_playmusic(infile, %ld, %d)\n", reslength, loop_flag);
fprintf(stderr, "[%s: %s]\n", loaded_filename, loaded_resname);
#endif
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

	sample_loop = loop_flag;
		
	// Launch the SampleThread
while (!sample_deleted) snooze(1000);
	sample_thread = spawn_thread(SampleThread, "MediaView sample thread", B_NORMAL_PRIORITY, sid);
	resume_thread(sample_thread);
	
	return true;
}


void hugo_stopsample(void)
{
#ifdef DEBUG_SOUND
fprintf(stderr, "hugo_stopsample()\n");
#endif
	sample_is_playing = false;
}


void hugo_samplevolume(int vol)
{
	sample_volume = vol;
}


#else	/* NO_SOUND */


//--------------------------------------------------------------------
// No-sound stubs:
//--------------------------------------------------------------------

extern "C"
{

int hugo_playmusic(FILE *f, long reslength, char loop_flag)
{
	fclose(f);
	return true;	/* not an error */
}

void hugo_musicvolume(int vol)
{}

void hugo_stopmusic(void)
{}

int hugo_playsample(FILE *f, long reslength, char loop_flag)
{
	fclose(f);
	return true;	/* not an error */
}

void hugo_samplevolume(int vol)
{}

void hugo_stopsample(void)
{}

}	// extern "C"

#endif 	/* NO_SOUND */
