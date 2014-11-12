/*
	BEHUGO.H
	Copyright (c) 1999-2006 by Kent Tessman
*/

#include <Alert.h>
#include <Application.h>
#include <Beep.h>
#include <Bitmap.h>
#include <File.h>
#include <FilePanel.h>
#include <Window.h>
#include <View.h>
#ifdef DEBUG
#include <Debug.h>
#endif

#define APP_SIG "application/x-vnd.hugo-engine"


//--------------------------------------------------------------------
// Class definitions
//--------------------------------------------------------------------

class HugoApplication : public BApplication 
{
public:
	HugoApplication();
	virtual void RefsReceived(BMessage *msg);
	virtual void ArgvReceived(int32 argc, char **argv);
	virtual void GetFilename();
	virtual void ReadyToRun(void);
	virtual bool QuitRequested();
};

class HugoWindow : public BWindow 
{
public:
	bool isactive;
	bool resizing;
	
	// current_rect is (0, 0) based, even though it physically appears
	// below the menubar
	BRect current_rect;

	HugoWindow(BRect frame);
	virtual void MessageReceived(BMessage *msg);
	virtual	bool QuitRequested();
	virtual void WindowActivated(bool active);
	virtual void FrameResized(float width, float height);
	virtual void Zoom(BPoint origin, float width, float height);
	void ResizeBitmap();
	void StoryMenu(int32 what);
};

class HugoBitmap : public BBitmap
{
public:
	HugoBitmap(BRect bounds, color_space space, bool accepts_views);
	bool Lock();
};

class HugoView : public BView 
{
public:
	char needs_updating;
	char override_updating;
	int scroll_offset;
	
	HugoView(BRect frame, const char *name);
	void Update(bool visible);
};

class HugoVisibleView : public BView 
{
public:
	char caret_drawn;
	int caret_x, caret_y;
	
	HugoVisibleView(BRect frame, const char *name);
	virtual void Draw(BRect frame);
	void DrawCaret();
	void DrawCaret(int x, int y);
	virtual void KeyDown(const char *bytes, int32 numBytes);
	virtual void MouseDown(BPoint point);
	virtual void MouseMoved(BPoint point, uint32 transit, const BMessage *message);
	virtual void HandleContextMenu(BPoint point);
};


//--------------------------------------------------------------------
// General definitions
//--------------------------------------------------------------------

// Messages
enum
{
	// File menu
	MSG_OPEN_NEW = 'OPEN',
	MSG_ABOUT,
	MSG_QUIT,
	
	// Story menu
	MSG_RESTART,
	MSG_RESTORE,
	MSG_SAVE,
	MSG_UNDO,
	
	// Options menu
	MSG_PROP_FAMILY, MSG_PROP_SIZE,
		MSG_FIXED_FAMILY, MSG_FIXED_SIZE,
		MSG_ENCODING,
	// The order of the MSG_ISO_* messages must match Font.h
	MSG_UNICODE_UTF8, MSG_ISO_8859_1, MSG_ISO_8859_2,
		MSG_ISO_8859_3, MSG_ISO_8859_4, MSG_ISO_8859_5,
		MSG_ISO_8859_6, MSG_ISO_8859_7, MSG_ISO_8859_8,
		MSG_ISO_8859_9, MSG_ISO_8859_10, MSG_MACINTOSH_ROMAN,
	MSG_SMART_FORMATTING,
	MSG_COLOR_FOREGROUND,
		MSG_COLOR_BACKGROUND,
		MSG_COLOR_SLFOREGROUND,
		MSG_COLOR_SLBACKGROUND,
		MSG_COLOR_SELECTOR,	// posted by ColorSelector
		MSG_COLOR_RESET,
	MSG_FULL_SCREEN,
	MSG_FAST_SCROLLING,
	MSG_TEXT_SELECT,
	MSG_DISPLAY_GRAPHICS,
	MSG_GRAPHICS_SMOOTHING,
	MSG_ENABLE_AUDIO,
	MSG_UNFREEZE_WINDOWS,
	MSG_RESET_DISPLAY,
	MSG_SHOW_COMPASS,
	MSG_SHOW_SCROLLBACK
};

// Faux-keypress codes
enum
{
	CTRL_LEFT_KEY = 1000,
	CTRL_RIGHT_KEY,
	BACKSPACE_KEY,
	OVERRIDE_UPDATING,
	RESTORE_UPDATING
};

// rgb_color struct manipulation
#define MAKE_RGB(color, r, g, b) \
	{color.red = r, color.green = g, color.blue = b;}
#define MAKE_RGB_INT32(color, i) \
	{color.red = (i&0xff0000)>>16, color.green = (i&0xff00)>>8, color.blue = i&0xff;}
#define MAKE_INT32_RGB(i, color) \
	{i = (int32)color.red<<16 | (int32)color.green<<8 | (int32)color.blue;}
	
// Used whenever the engine thread is waiting for something, in order
// to check for quit requests and ease up on CPU usage
extern "C" void process_he_thread_request(int32);
#define IDLE_ENGINE_THREAD(); \
{ \
	if (quit_he_thread) exit_thread(he_thread_running = 0); \
	if (he_thread_request) process_he_thread_request(he_thread_request); \
	snooze(50000); \
}

#define HUGO_BEEP(); { if (enable_audio) beep(); }

// Sub-directories under the main Hugo directory
#define GAMES_SUBDIR "games"
#define SAVE_SUBDIR "save"

//--------------------------------------------------------------------
// externs
//--------------------------------------------------------------------

// From behugo.cpp:
void TypeCommand(char *cmd, bool clear, bool linefeed);

extern HugoWindow *window;
extern HugoBitmap *bitmap;
extern HugoView *view;
extern HugoVisibleView *visible_view;
extern char app_directory[];
extern thread_id he_thread;
extern int he_thread_request;
extern bool he_thread_running, quit_he_thread;
extern uint32 font_encoding;
extern BFont prop_font, fixed_font, current_font;
extern rgb_color def_fcolor, def_bgcolor, def_slfcolor, def_slbgcolor;
extern rgb_color current_text_color, current_back_color;
extern rgb_color update_bgcolor;
extern bool processed_accelerator_key;
extern bool fast_scrolling, display_graphics, enable_audio, audio_grayed_out;
extern bool graphics_smoothing;
extern BFilePanel *file_panel;
extern char file_selected[];

// From hebe.cpp:
extern "C"
{
void PushKeypress(int k);
int PullKeypress(void);
void ConstrainCursor(void);
void FlushBuffer(void);
rgb_color hugo_color(int c);

extern bool override_client_updating;
extern bool getline_active;
extern char scrollback_buffer[];
extern int scrollback_pos;
extern char waiting_for_key;
extern char prop_font_precalc_done;
}

// From picture.cpp:
BFile *TrytoOpenBFile(char *name, char *where);

// From sound.cpp:
int InitPlayer(void);
void SuspendAudio(void);
void ResumeAudio(void);
extern "C"
{
void hugo_stopsample(void);
void hugo_stopmusic(void);
}

extern bool audio_factory_suspended;

// From video.cpp:
extern "C"
{
void hugo_stopvideo(void);
}
extern bool video_playing, video_background;
