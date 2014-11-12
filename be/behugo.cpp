/*
 	BEHUGO.CPP
 	Non-portable user-interface/front-end functions for BeOS

	for the Hugo Engine

 	by Kent Tessman (c) 1999-2006

	Note that this file behugo.cpp contains user interface functions;
	hebe.cpp contains the standard engine interface routines.

	Quick structural overview:

	HugoWindow window - main visible window
	HugoVisibleView visible_view - child of window
	HugoBitmap bitmap
	    	- bitmap is twice the height of the visible view, so
	    	  that instead of scrolling every line, we just have to
	    	  move the "virtual window" down the bitmap; the visible
		  view is updated by drawing the virtual window on the 
		  bitmap to the visible view, then moving the virtual
		  window back to the top of the bitmap
	HugoView view - off-screen view for drawing bitmap
*/

#include <Directory.h>
#include <FindDirectory.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Mime.h>
#include <Path.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <TextView.h>
#include <TranslationUtils.h>

#if !defined (COMPILE_V25)
#include <PopUpMenu.h>
#include "MediaView.h"
#endif

#define TYPED_STORY_COMMANDS

#include "behugo.h"
#include "icons.h"
#include "AboutBox.h"
#include "ColorSelector.h"
#include "CompassRose.h"

extern "C"
{
#include "heheader.h"
}

//--------------------------------------------------------------------
// Global data
//--------------------------------------------------------------------

// Display, etc.
HugoWindow *window = NULL;
BRect default_rect(100, 100, 500, 400);	// 400 x 300
HugoView *view;
HugoVisibleView *visible_view;
HugoBitmap *bitmap;
BMenuBar *menubar;
CompassRose *compass;
BPoint compass_point;
bool processed_accelerator_key = false;
bool fast_scrolling = true, full_screen = true,
	display_graphics = true, show_compass = false;
bool graphics_smoothing = false;
bool enable_audio = true, audio_grayed_out = false;
BMenuItem *smartformatting_menu, *fast_scrolling_menu, *full_screen_menu,
	*display_graphics_menu, *graphics_smoothing_menu,
	*enable_audio_menu, *show_compass_menu;
#ifdef USE_TEXTBUFFER
BMenuItem *text_select_menu;
#endif
BFilePanel *file_panel;
char file_selected[MAXPATH];

// Application init
#define MAXARGS 8
int passed_argc = 1; char *passed_argv[MAXARGS];
char app_directory[MAXPATH];
char *supported_mime_types[] = {
	"application/x-hugo-executable", "hex", "Hugo executable file",
	"application/x-hugo-debuggable", "hdx", "Hugo debuggable executable",
	NULL};

// Engine thread
thread_id he_thread;
bool he_thread_running = 0, quit_he_thread = 0;

// Fonts
uint32 font_encoding = B_UNICODE_UTF8;
BFont prop_font, fixed_font, current_font;

// Colors
rgb_color def_fcolor, def_bgcolor, def_slfcolor, def_slbgcolor;
rgb_color current_text_color, current_back_color;
rgb_color update_bgcolor;

// Scrollback window
BMenuItem *show_scrollback_menu;
BTextView *textview;
BScrollView *scrollview;
bool scrollback_shown = false;

// From hemisc.c:
extern char gamepath[];

// From herun.c:
extern int physical_lowest_windowbottom, lowest_windowbottom;
extern rgb_color current_back_color;


//--------------------------------------------------------------------
// Entry point
//--------------------------------------------------------------------

int main()
{
#ifdef DEBUG
	SET_DEBUG_ENABLED(true);
	fprintf(stdout, "BeHugo: Compiled with DEBUG set\n");
#endif
	
	HugoApplication app;
	app.Run();

	return 0;
}

void TypeCommand(char *cmd, bool clear, bool enter)
{
	int i;
	
	PushKeypress(OVERRIDE_UPDATING);
	if (clear) PushKeypress(27);
	for (i=0; i<(int)strlen(cmd); i++)
		PushKeypress(cmd[i]);
	if (enter)
	{
		PushKeypress(RESTORE_UPDATING);
		PushKeypress(13);
	}
	else
	{
		PushKeypress(' ');
		PushKeypress(RESTORE_UPDATING);
	}
}


//--------------------------------------------------------------------
// Loading and saving settings
//--------------------------------------------------------------------

#define SETTINGS_SUBDIR "General Coffee Co."
#define SETTINGS_FILE "Hugo Engine settings"

void LoadSettings()
{
	BPath path;
	BDirectory dir;
	BEntry entry;
	
	find_directory(B_USER_SETTINGS_DIRECTORY, &path, true);
	dir.SetTo(path.Path());
	if (dir.FindEntry(SETTINGS_SUBDIR, &entry)!=B_OK) return;
	path.Append(SETTINGS_SUBDIR);
	dir.SetTo(path.Path());
	path.Append(SETTINGS_FILE);
	
	BFile file(path.Path(), B_READ_ONLY);
	if (file.InitCheck()!=B_OK) return;
	
	BMessage msg;
	// First of all, unflatten the message from the settings file
	msg.Unflatten(&file);

	// Default window rectangle
	msg.FindRect("default_rect", &default_rect);
	// Default colors
	const void *data;
	ssize_t size;
	if (msg.FindData("def_fcolor", B_RGB_COLOR_TYPE, &data, &size)==B_OK)
		memcpy(&def_fcolor, data, size);
	if (msg.FindData("def_bgcolor", B_RGB_COLOR_TYPE, &data, &size)==B_OK)
		memcpy(&def_bgcolor, data, size);
	if (msg.FindData("def_slfcolor", B_RGB_COLOR_TYPE, &data, &size)==B_OK)
		memcpy(&def_slfcolor, data, size);
	if (msg.FindData("def_slbgcolor", B_RGB_COLOR_TYPE, &data, &size)==B_OK)
		memcpy(&def_slbgcolor, data, size);
	// Currently selected fonts
	int32 fdata;
/* The post-R5 way:
	font_family family;
	if (msg.FindString("prop_family", (const char **)&family)==B_OK)
		prop_font.SetFamilyAndStyle(family, NULL);
	if (msg.FindString("fixed_family", (const char **)&family)==B_OK)
		fixed_font.SetFamilyAndStyle(family, NULL);
*/
	if (msg.FindInt32("prop_family", &fdata)==B_OK) prop_font.SetFamilyAndStyle(fdata);
	if (msg.FindInt32("prop_size", &fdata)==B_OK) prop_font.SetSize(fdata);
	if (msg.FindInt32("fixed_family", &fdata)==B_OK) fixed_font.SetFamilyAndStyle(fdata);
	if (msg.FindInt32("fixed_size", &fdata)==B_OK) fixed_font.SetSize(fdata);
	msg.FindInt32("font_encoding", (int32 *)&font_encoding);
	msg.FindInt32("smartformatting", (int32 *)&smartformatting);
	// Other settings
	msg.FindBool("full_screen", &full_screen);
	msg.FindBool("fast_scrolling", &fast_scrolling);
#ifdef USE_TEXTBUFFER
	bool b;
	msg.FindBool("allow_text_selection", &b);
	allow_text_selection = b;
#endif
	msg.FindBool("display_graphics", &display_graphics);
	msg.FindBool("graphics_smoothing", &graphics_smoothing);
	msg.FindBool("enable_audio", &enable_audio);
	msg.FindBool("show_compass", &show_compass);
	if (msg.FindPoint("compass_point", &compass_point)!=B_OK) compass_point = BPoint(0, 0);
}

void SaveSettings()
{
	BPath path;
	BDirectory dir;
	BEntry entry;
	
	find_directory(B_USER_SETTINGS_DIRECTORY, &path, true);
	dir.SetTo(path.Path());
	if (dir.FindEntry(SETTINGS_SUBDIR, &entry)!=B_NO_ERROR)
	{
		if (dir.CreateDirectory(SETTINGS_SUBDIR, &dir)!=B_NO_ERROR)
			return;
	}
	path.Append(SETTINGS_SUBDIR);
	dir.SetTo(path.Path());
	path.Append(SETTINGS_FILE);

	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck()!=B_OK) return;

	BMessage msg;
	// Default window rectangle
	msg.AddRect("default_rect", default_rect);
	// Default colors
	msg.AddData("def_fcolor", B_RGB_COLOR_TYPE, (void *)&def_fcolor, sizeof(def_fcolor));
	msg.AddData("def_bgcolor", B_RGB_COLOR_TYPE, (void *)&def_bgcolor, sizeof(def_bgcolor));
	msg.AddData("def_slfcolor", B_RGB_COLOR_TYPE, (void *)&def_slfcolor, sizeof(def_slfcolor));
	msg.AddData("def_slbgcolor", B_RGB_COLOR_TYPE, (void *)&def_slbgcolor, sizeof(def_slbgcolor));
	// Currently selected fonts
/* The post-R5 way:
	font_family family;
	font_style style;
	prop_font.GetFamilyAndStyle(&family, &style);
	msg.AddString("prop_family", family);
	fixed_font.GetFamilyAndStyle(&family, &style);
	msg.AddString("fixed_family", family);
*/
	msg.AddInt32("prop_family", prop_font.FamilyAndStyle());
	msg.AddInt32("prop_size", prop_font.Size());
	msg.AddInt32("fixed_family", fixed_font.FamilyAndStyle());
	msg.AddInt32("fixed_size", fixed_font.Size());
	msg.AddInt32("font_encoding", font_encoding);
	msg.AddInt32("smartformatting", smartformatting);
	// Other settings
	msg.AddBool("full_screen", full_screen);
	msg.AddBool("fast_scrolling", fast_scrolling);
#ifdef USE_TEXTBUFFER
	msg.AddBool("allow_text_selection", allow_text_selection);
#endif
	msg.AddBool("display_graphics", display_graphics);
	msg.AddBool("graphics_smoothing", graphics_smoothing);
	msg.AddBool("enable_audio", enable_audio);
	msg.AddBool("show_compass", show_compass);
	msg.AddPoint("compass_point", compass_point);
	
	// Finally, flatten the image, writing it to the settings file
	msg.Flatten(&file);
}

//--------------------------------------------------------------------
// Application class
//--------------------------------------------------------------------

HugoApplication::HugoApplication()
	: BApplication(APP_SIG)
{
	// Find out what directory we're based in for future reference
	app_info info;
	GetAppInfo(&info);
	BEntry entry = BEntry(&info.ref);
	BPath path = BPath(&entry);
	strcpy(app_directory, path.Path());
	char *leaf = strstr(app_directory, path.Leaf());
	if (leaf) *leaf = '\0';
	
	// Since he_main() won't get a program argument:
	strcpy(program_path, path.Path());
	
	// Set the MIME info for any supported types
	BMimeType type;

	for (int n=0; supported_mime_types[n]!=NULL; n+=3)
	{
		bool valid = false;
		BMessage msg;
		int32 index = 0;
		const char *str;
		
		type.SetTo(supported_mime_types[n]);	// this supported MIME type
		
		// Check the validity of this MIME type first
		if (type.IsInstalled())
		{
			if (type.GetAttrInfo(&msg)==B_NO_ERROR)
			{
				while (msg.FindString("attr:name", index++, &str)==B_NO_ERROR)
				{
					// If there's a "Hugo:title" attribute,
					// assume the type is properly set up
					if (!strcmp(str, "Hugo:title"))
					{
						valid = true;
						break;
					}
				}
				if (!valid) type.Delete();
			}
		}

		if (valid) continue;	// proper type is already installed, so skip
		
		type.Install();
		
		type.SetPreferredApp(APP_SIG);		// this app is preferred
		msg.MakeEmpty();
		msg.AddString("extensions", supported_mime_types[n+1]);
		type.SetFileExtensions(&msg);		// add file extension
		type.SetShortDescription(supported_mime_types[n+2]);
		type.SetLongDescription(supported_mime_types[n+2]);

		// Set up the icons for the supported type
		BBitmap large_icon(BRect(0, 0, B_LARGE_ICON - 1, B_LARGE_ICON - 1), B_COLOR_8_BIT);
		BBitmap mini_icon(BRect(0, 0, B_MINI_ICON - 1, B_MINI_ICON - 1), B_COLOR_8_BIT);
		if (n==0)
		{
			// Hugo executable
			large_icon.SetBits(kLargeHEIconBits, large_icon.BitsLength(), 0, B_COLOR_8_BIT);
			mini_icon.SetBits(kSmallHEIconBits, mini_icon.BitsLength(), 0, B_COLOR_8_BIT);
		}
		else if (n==3)
		{
			// Hugo debuggable executable
			large_icon.SetBits(kLargeHDIconBits, large_icon.BitsLength(), 0, B_COLOR_8_BIT);
			mini_icon.SetBits(kSmallHDIconBits, mini_icon.BitsLength(), 0, B_COLOR_8_BIT);
		}
		type.SetIcon(&large_icon, B_LARGE_ICON);
		type.SetIcon(&mini_icon, B_MINI_ICON);
		
		// Set up additional type-specific attributes
		msg.MakeEmpty();

		msg.AddString("attr:name", "Hugo:title");
		msg.AddString("attr:public_name", "Title");
		msg.AddInt32("attr:type", B_STRING_TYPE);
		msg.AddBool("attr:editable", true);
		msg.AddBool("attr:viewable", true);
		msg.AddInt32("attr:width", 120); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false); 

		msg.AddString("attr:name", "Hugo:author");
		msg.AddString("attr:public_name", "Author");
		msg.AddInt32("attr:type", B_STRING_TYPE);
		msg.AddBool("attr:editable", true);
		msg.AddBool("attr:viewable", true);
		msg.AddInt32("attr:width", 120); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false); 

		msg.AddString("attr:name", "Hugo:genre");
		msg.AddString("attr:public_name", "Genre");
		msg.AddInt32("attr:type", B_STRING_TYPE);
		msg.AddBool("attr:editable", true);
		msg.AddBool("attr:viewable", true);
		msg.AddInt32("attr:width", 90); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false); 

		type.SetAttrInfo(&msg);
	}
	
	// Do any default initialization before reading options
	def_fcolor = hugo_color(DEF_FCOLOR);
	def_bgcolor = hugo_color(DEF_BGCOLOR);
	def_slfcolor = hugo_color(DEF_SLFCOLOR);
	def_slbgcolor = hugo_color(DEF_SLBGCOLOR);
	update_bgcolor = def_bgcolor;
	prop_font = be_plain_font;
	fixed_font = be_fixed_font;
	
	// After defaults are initialized, we can try to read saved settings
	LoadSettings();
	
	// Set up a rectangle and instantiate the main window
	window = new HugoWindow(default_rect);
	window->Show();
	
	// Create the compass rose
	compass = new CompassRose(&compass_point);
	if (!show_compass) compass->Hide();
	compass->Show();

	// Start the sound factory
#ifndef NO_SOUND
	if ((!audio_grayed_out) && !InitPlayer()) audio_grayed_out = true;
#endif

	// Set up the updating defaults
	current_text_color = def_fcolor;
	current_back_color = def_bgcolor;

	// Save settings at exit
	atexit(SaveSettings);
}

void HugoApplication::RefsReceived(BMessage *msg)
{
	entry_ref ref;
	
	if (msg->FindRef("refs", &ref)==B_OK)
	{
		BEntry entry = BEntry(&ref);
		BPath path = BPath(&entry);
		passed_argc = 2;
		passed_argv[0] = "";
		passed_argv[1] = strdup(path.Path());
	}
}

void HugoApplication::ArgvReceived(int32 argc, char **argv)
{
	// Called if arguments are passed at initialization
	passed_argc = argc;
	for (int i=0; i<argc && i<MAXARGS; i++)
		// argv isn't static
		passed_argv[i] = strdup(argv[i]);
}

void HugoApplication::GetFilename()
{
	// Puts up a BFilePanel but doesn't wait for anything to be returned;
	// that's in a message sent to the main HugoWindow message handler
	
	sprintf(line, "%s/%s", app_directory, GAMES_SUBDIR);
	BEntry entry = BEntry(line, true);
	entry_ref panel_directory;
	entry.GetRef(&panel_directory);
	
	BMessenger target = BMessenger(window);

	file_panel = new BFilePanel(B_OPEN_PANEL, &target,
		&panel_directory, B_FILE_NODE, false,
		NULL, NULL, true, false);
	BWindow *panel_window = file_panel->Window();
	panel_window->SetTitle("Select a Hugo file to open");
	strcpy(file_selected, "");
	file_panel->Show();
}

int32 CallThread(void *data)
{
	BPath path(passed_argv[passed_argc-1]);
//	sprintf(line, "Hugo: %s", passed_argv[passed_argc-1]);
	sprintf(line, "Hugo: %s", path.Leaf());
	hugo_setgametitle(line);

	// Call the engine itself
	he_thread_running = 1;
	he_main(passed_argc, passed_argv);
	he_thread_running = 0;
	
	// Quit the app after the engine returns
	be_app->PostMessage(B_QUIT_REQUESTED);
	return 0;
}

extern "C" void exit_he_thread(int n)	// the C source's exit() maps to this
{
	he_thread_running = 0;
	// Quit the app after the engine calls exit()
	be_app->PostMessage(B_QUIT_REQUESTED);
	exit_thread((status_t)n);
}

void HugoApplication::ReadyToRun(void)
{
	if (passed_argc < 2)
		GetFilename();
	else
	{
		// Call the engine thread
		he_thread = spawn_thread(CallThread, "Hugo Engine thread", B_NORMAL_PRIORITY, NULL);
		resume_thread(he_thread);
	}
}

bool HugoApplication::QuitRequested()
{
	// Check to make sure the user actually wants to quit if the window
	// is still open (so check to see if it exists)
	if (window->Lock())
	{
		bool quit = window->QuitRequested();
		window->Unlock();
		if (!quit) return false;
	}
	
	hugo_stopmusic();
	hugo_stopsample();
	hugo_stopvideo();
	
	// Kill the engine thread if it's still running
	while (he_thread_running)
	{
		quit_he_thread = true;
		snooze(5000);
	}
	
	return BApplication::QuitRequested();
}

/* PrintFatalError

	Necessary because BeOS can't just dump an error message to
	stderr--there may be no standard error output.
*/

extern "C" void PrintFatalError(char *a)
{
	if (a[strlen(a)-1]=='\n') a[strlen(a)-1] = '\0';
	if (a[0]=='\n') a = &a[1];
	BAlert *alert = new BAlert("Fatal Error", a, "OK", NULL, NULL,
		B_WIDTH_AS_USUAL, B_STOP_ALERT);
	alert->Go();
	exit(255);
}


//--------------------------------------------------------------------
// Main window class
//--------------------------------------------------------------------

HugoWindow::HugoWindow(BRect frame)
	: BWindow(frame, "Hugo", B_TITLED_WINDOW, B_OUTLINE_RESIZE)
{
	isactive = false;
	resizing = 0;

	// Start with the menu
	menubar = new BMenuBar(Bounds(), "Hugo menubar");
	
	// File:
	BMenu *file_menu = new BMenu("File");
	file_menu->AddItem(new BMenuItem("Open New File...", new BMessage(MSG_OPEN_NEW), 'O'));
	file_menu->AddItem(new BSeparatorItem);
	file_menu->AddItem(new BMenuItem("About Hugo...", new BMessage(MSG_ABOUT)));
	file_menu->AddItem(new BSeparatorItem);
	file_menu->AddItem(new BMenuItem("Quit", new BMessage(MSG_QUIT), 'Q'));
	menubar->AddItem(file_menu);
	
	// Story:
	BMenu *story_menu = new BMenu("Story");
	story_menu->AddItem(new BMenuItem("Restart Story", new BMessage(MSG_RESTART)));
	story_menu->AddItem(new BSeparatorItem);
	story_menu->AddItem(new BMenuItem("Restore Story...", new BMessage(MSG_RESTORE), 'R'));
	story_menu->AddItem(new BMenuItem("Save Story...", new BMessage(MSG_SAVE), 'S'));
	story_menu->AddItem(new BSeparatorItem);
	story_menu->AddItem(new BMenuItem("Undo Last Turn", new BMessage(MSG_UNDO), 'Z'));
	menubar->AddItem(story_menu);	
	
	// Options:
	
	// Fonts:
	BMenu *options_menu = new BMenu("Options");
	BMenu *prop_menu = new BMenu("Proportional Font");
	BMenu *fixed_menu = new BMenu("Fixed Width Font");
	BMenu *prop_families = new BMenu("Family");
	prop_families->SetRadioMode(true);
	BMenu *fixed_families = new BMenu("Family");
	fixed_families->SetRadioMode(true);
	BMenu *prop_sizes = new BMenu("Size");
	prop_sizes->SetRadioMode(true);
	BMenu *fixed_sizes = new BMenu("Size");
	fixed_sizes->SetRadioMode(true);

	// Populate the font menus:
	int32 num_families = count_font_families();
	font_family prop_family;
	font_family fixed_family;
	prop_font.GetFamilyAndStyle(&prop_family, NULL);
	fixed_font.GetFamilyAndStyle(&fixed_family, NULL);
	// First with all the font family names...
	for (int32 i=0; i<num_families; i++)
	{
		font_family family;
		uint32 flags;
		if (get_font_family(i, &family, &flags)==B_OK)
		{
			BMessage *prop_msg = new BMessage(MSG_PROP_FAMILY);
			prop_msg->AddString("family", family);
			BMenuItem *prop_item = new BMenuItem(family, prop_msg);
			prop_families->AddItem(prop_item);
			if (!strcmp(family, prop_family))
				// Check selected prop size
				prop_item->SetMarked(true);
			// Add only fixed-width fonts to the Fixed menu
			if (flags & B_IS_FIXED)
			{
				BMessage *fixed_msg = new BMessage(MSG_FIXED_FAMILY);
				fixed_msg->AddString("family", family);
				BMenuItem *fixed_item = new BMenuItem(family, fixed_msg);
				fixed_families->AddItem(fixed_item);
				if (!strcmp(family, fixed_family))
					// Check selected fixed size
					fixed_item->SetMarked(true);
			}
		}
	}
	// ...then with a range of sizes
	for (int i=10; i<25; i++)
	{
		char buffer[20];
		sprintf(buffer, "%d", i);
		BMessage *prop_msg = new BMessage(MSG_PROP_SIZE);
		prop_msg->AddInt32("size", i);
		BMenuItem *prop_item = new BMenuItem(buffer, prop_msg);
		prop_sizes->AddItem(prop_item);
		if (i==prop_font.Size())
			// Check selected prop size
			prop_item->SetMarked(true);
		BMessage *fixed_msg = new BMessage(MSG_FIXED_SIZE);
		fixed_msg->AddInt32("size", i);
		BMenuItem *fixed_item = new BMenuItem(buffer, fixed_msg);
		fixed_sizes->AddItem(fixed_item);
		if (i==fixed_font.Size())
			// Check selected fixed size
			fixed_item->SetMarked(true);
	}
	prop_menu->AddItem(prop_families);
	prop_menu->AddItem(prop_sizes);
	fixed_menu->AddItem(fixed_families);
	fixed_menu->AddItem(fixed_sizes);
	options_menu->AddItem(fixed_menu);
	options_menu->AddItem(prop_menu);
	
	// Add the character encoding choices:
	BMenu *encoding_menu = new BMenu("Character Encoding");
	encoding_menu->AddItem(new BMenuItem("Arabic (ISO 8859-6)", new BMessage(MSG_ISO_8859_6)));
	encoding_menu->AddItem(new BMenuItem("Central European (ISO 8859-2)", new BMessage(MSG_ISO_8859_2)));
	encoding_menu->AddItem(new BMenuItem("Cyrillic (ISO 8859-5)", new BMessage(MSG_ISO_8859_5)));
	encoding_menu->AddItem(new BMenuItem("Greek (ISO 8859-7)", new BMessage(MSG_ISO_8859_7)));
	encoding_menu->AddItem(new BMenuItem("Hebrew (ISO 8859-8)", new BMessage(MSG_ISO_8859_8)));
	encoding_menu->AddItem(new BMenuItem("Latin (ISO 8859-3)", new BMessage(MSG_ISO_8859_3)));
	encoding_menu->AddItem(new BMenuItem("Latin (ISO 8859-4)", new BMessage(MSG_ISO_8859_4)));
	encoding_menu->AddItem(new BMenuItem("Latin (ISO 8859-9)", new BMessage(MSG_ISO_8859_9)));
	encoding_menu->AddItem(new BMenuItem("Latin (ISO 8859-10)", new BMessage(MSG_ISO_8859_10)));
	encoding_menu->AddItem(new BMenuItem("Unicode (UTF-8)", new BMessage(MSG_UNICODE_UTF8)));
	encoding_menu->AddItem(new BMenuItem("Western - Default (ISO 8859-1)", new BMessage(MSG_ISO_8859_1)));
	encoding_menu->AddItem(new BMenuItem("Western (Macintosh Roman)", new BMessage(MSG_MACINTOSH_ROMAN)));
	encoding_menu->SetRadioMode(true);
	encoding_menu->FindItem(font_encoding+MSG_UNICODE_UTF8)->SetMarked(true);
	options_menu->AddItem(encoding_menu);

	smartformatting_menu = new BMenuItem("Smart formatting", new BMessage(MSG_SMART_FORMATTING));
	smartformatting_menu->SetMarked(smartformatting!=0);
	options_menu->AddItem(smartformatting_menu);

	options_menu->AddItem(new BSeparatorItem);
	BMenu *color_menu = new BMenu("Set Default Colors");
	color_menu->AddItem(new BMenuItem("Foreground...", new BMessage(MSG_COLOR_FOREGROUND)));
	color_menu->AddItem(new BMenuItem("Background...", new BMessage(MSG_COLOR_BACKGROUND)));
	color_menu->AddItem(new BMenuItem("Statusline Foreground...", new BMessage(MSG_COLOR_SLFOREGROUND)));
	color_menu->AddItem(new BMenuItem("Statusline Background...", new BMessage(MSG_COLOR_SLBACKGROUND)));
	color_menu->AddItem(new BSeparatorItem);
	color_menu->AddItem(new BMenuItem("Reset Default Colors", new BMessage(MSG_COLOR_RESET)));
	options_menu->AddItem(color_menu);
	
	options_menu->AddItem(new BSeparatorItem);
	full_screen_menu = new BMenuItem("Full Screen", new BMessage(MSG_FULL_SCREEN), 'F');
	full_screen_menu->SetMarked(full_screen!=0);
	options_menu->AddItem(full_screen_menu);
	fast_scrolling_menu = new BMenuItem("Fast Scrolling", new BMessage(MSG_FAST_SCROLLING));
	fast_scrolling_menu->SetMarked(fast_scrolling!=0);
	options_menu->AddItem(fast_scrolling_menu);
#ifdef USE_TEXTBUFFER
	text_select_menu = new BMenuItem("Allow Text Selection", new BMessage(MSG_TEXT_SELECT));
	text_select_menu->SetMarked(allow_text_selection!=0);
	options_menu->AddItem(text_select_menu);
#endif
	display_graphics_menu = new BMenuItem("Display Graphics", new BMessage(MSG_DISPLAY_GRAPHICS));
	display_graphics_menu->SetMarked(display_graphics!=0);
	graphics_smoothing_menu = new BMenuItem("Graphics Smoothing", new BMessage(MSG_GRAPHICS_SMOOTHING));
	graphics_smoothing_menu->SetMarked(graphics_smoothing!=0);
#ifndef NO_GRAPHICS
	options_menu->AddItem(display_graphics_menu);
	options_menu->AddItem(graphics_smoothing_menu);
#endif
	enable_audio_menu = new BMenuItem("Play Sounds and Music", new BMessage(MSG_ENABLE_AUDIO), 'P');
	enable_audio_menu->SetMarked(enable_audio!=0);
	enable_audio_menu->SetEnabled(!audio_grayed_out);
#ifndef NO_SOUND
	options_menu->AddItem(enable_audio_menu);
#endif
	options_menu->AddItem(new BSeparatorItem);
	options_menu->AddItem(new BMenuItem("Unfreeze Windows", new BMessage(MSG_UNFREEZE_WINDOWS), 'U'));
	options_menu->AddItem(new BMenuItem("Reset Display", new BMessage(MSG_RESET_DISPLAY), 'D'));
	options_menu->AddItem(new BSeparatorItem);
	show_compass_menu = new BMenuItem("Show Compass Rose", new BMessage(MSG_SHOW_COMPASS), 'C');
	show_compass_menu->SetMarked(show_compass!=0);
	options_menu->AddItem(show_compass_menu);
	show_scrollback_menu = new BMenuItem("Show Scrollback", new BMessage(MSG_SHOW_SCROLLBACK), 'L');
	options_menu->AddItem(show_scrollback_menu);
	
	menubar->AddItem(options_menu);

	// Add the menubar to the window
	AddChild(menubar);

	// Set up a rectangle and instantiate a new view
	BRect rect(Bounds());
	current_rect = rect;	// (0, 0) based
	rect.top+=(menubar->Bounds().Height()+1);
	current_rect.bottom-=(menubar->Bounds().Height()+1);
	visible_view = new HugoVisibleView(rect, "Hugo visible view");
	// Add view to window
	AddChild(visible_view);
	visible_view->MakeFocus();

	// Create the off-screen bitmap and view, twice the height of the
	// visible view
	rect = current_rect;
	rect.bottom*=2;
	view = new HugoView(rect, "Hugo view");
	bitmap = new HugoBitmap(rect, B_RGB32, true);
	bitmap->AddChild(view);
	
	// Create the scrollback window
	rect = Bounds();
	rect.top = menubar->Bounds().bottom + 1.0;
	rect.right -= B_V_SCROLL_BAR_WIDTH;
	rect.bottom -= B_H_SCROLL_BAR_HEIGHT;
	BRect textrect = rect;
	textrect.OffsetTo(B_ORIGIN);
	textview = new BTextView(rect, "Scrollback textview", textrect,
		B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
	scrollview = new BScrollView("Scrollback scrollview", textview,
		B_FOLLOW_ALL_SIDES, 0, true, true, B_NO_BORDER);
	textview->MakeEditable(false);
	AddChild(scrollview);
	scrollview->Hide();
	
	// Make sure this window is sanely positioned
	// Position:
	BPoint point = BPoint(frame.left, frame.top);
	BScreen screen(B_MAIN_SCREEN_ID);
	if (point.x > screen.Frame().right-5-(frame.right-frame.left))
		point.x = screen.Frame().right-5-(frame.right-frame.left);
	if (point.y > screen.Frame().bottom-5-(frame.bottom-frame.top))
		point.y = screen.Frame().bottom-5-(frame.bottom-frame.top);
	if (point.x <= 5) point.x = 5;
	if (point.y <= 5) point.y = 5;
	MoveTo(point);
	// And size:
	if (frame.Width() > screen.Frame().Width()-10)
		ResizeBy(screen.Frame().Width()-10-frame.Width(), 0);
	if (frame.Height() > screen.Frame().Height()-10)
		ResizeBy(0, screen.Frame().Height()-10-frame.Height());
	
	// Switch to full-screen if requested
	if (full_screen)
	{
		full_screen = 0;
		PostMessage(MSG_FULL_SCREEN);
	}
	
	// Reset the font once we've got the view and set up metrics
 	hugo_font(0);
}

// Here's the main message handler for the window:

void HugoWindow::MessageReceived(BMessage *msg)
{
	static BMessage *color_msg;
	
	switch (msg->what)
	{
		// Posted by BFilePanel:
		case B_REFS_RECEIVED:		// Open dialog
		{
			entry_ref ref;
			if (msg->FindRef("refs", &ref)==B_OK)
			{
				BEntry entry = BEntry(&ref, true);
				BPath path = BPath(&entry);
				strcpy(file_selected, path.Path());
			}
			
			// If this is the initial file selection, call
			// ReadyToRun again, this time with a file argument
			if (!he_thread_running)
			{
				delete file_panel;
				passed_argc = 2;
				passed_argv[1] = file_selected;
				be_app->ReadyToRun();
				strcpy(gamepath, file_selected);
			}
			
			break;
		}
		case B_SAVE_REQUESTED:		// Save dialog
		{
			entry_ref dir;
			char dirname[MAXPATH];
			const char *filename;
			if (msg->FindRef("directory", &dir)==B_OK &&
				msg->FindString("name", &filename)==B_OK)
			{
				BEntry entry = BEntry(&dir, true);
				BPath path = BPath(&entry);
				strcpy(dirname, path.Path());
				sprintf(file_selected, "%s/%s", dirname, filename);
			}
			break;
		}
		case B_CANCEL:
		{
			if (!he_thread_running)
				// i.e., cancelling initial file selection
				be_app->PostMessage(B_QUIT_REQUESTED);
			else
				strcpy(file_selected, "");
			break;
		}
		
		// Passed when a file is dropped on the window
		case B_SIMPLE_DATA:
		{
			entry_ref ref;
			int32 index = 0;
	
			while (msg->FindRef("refs", index, &ref)==B_OK)
			{
				BMessage newmsg(MSG_OPEN_NEW);
				newmsg.AddRef("ref", &ref);
				PostMessage(&newmsg);
				index++;
			}
			break;
		}

		// File menu:
		case MSG_OPEN_NEW:
		{
			// This message is also passed by the application to itself
			// when a file is dropped
			thread_id new_thread;
			
			// Get the pathname of this application
			app_info info;
			be_app->GetAppInfo(&info);
			BEntry entry = BEntry(&info.ref);
			BPath path; 
			entry.GetPath(&path); 

			// Now figure out argc and argv to pass to load_image
			int argc;
			const char *argv[2];
			extern char **environ;
			argv[0] = path.Path();
			
			// If a ref is passed (i.e., by dropping a file), it will
			// be stored in "ref"
			entry_ref ref;
			if (msg->FindRef("ref", &ref)==B_OK)
			{
				BEntry entry = BEntry(&ref, true);
				BPath new_file;
				entry.GetPath(&new_file);
				argv[1] = strdup(new_file.Path());
				argc = 2;
			}
			else
			{
				argv[1] = NULL; 
				argc = 1;
			}
			
			SaveSettings();
			new_thread = load_image(argc, argv, (const char **)environ);
			resume_thread(new_thread);
			break;
		}
		case MSG_ABOUT:
		{
			AboutBox *about_box = new AboutBox("About Hugo for BeOS");
			about_box->Go();
			break;
		}
		case MSG_QUIT:
		{
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;
		}
		
		// Story menu:
		case MSG_RESTART:
		case MSG_RESTORE:
		case MSG_SAVE:
		case MSG_UNDO:
			StoryMenu(msg->what);
			break;
		
		// Option menu:
		
		// Font handling:
		case MSG_PROP_FAMILY:
		{
			const char *name;
			if (msg->FindString("family", &name)==B_OK)
			{
				font_family family;
				strcpy(family, name);
				if (visible_view->caret_drawn) visible_view->DrawCaret();
				prop_font.SetFamilyAndStyle(family, NULL);
				prop_font_precalc_done = false;
				if (currentfont & PROP_FONT) hugo_font(currentfont);
				SaveSettings();
				display_needs_repaint = true;
			}
			break;
		}
		case MSG_PROP_SIZE:
		{
			int32 size;

			if (msg->FindInt32("size", &size)==B_OK)
			{
				if (visible_view->caret_drawn) visible_view->DrawCaret();
				prop_font.SetSize((float)size);
				prop_font_precalc_done = false;
				if (currentfont & PROP_FONT) hugo_font(currentfont);
				SaveSettings();
				display_needs_repaint = true;
			}
			break;
		}
		case MSG_FIXED_FAMILY:
		{
			const char *name;
			if (msg->FindString("family", &name)==B_OK)
			{
				font_family family;
				strcpy(family, name);
				if (visible_view->caret_drawn) visible_view->DrawCaret();
				fixed_font.SetFamilyAndStyle(family, NULL);
				if (!(currentfont & PROP_FONT)) hugo_font(currentfont);
				SaveSettings();
				display_needs_repaint = true;
			}
			break;
		}
		case MSG_FIXED_SIZE:
		{
			int32 size;
			if (msg->FindInt32("size", &size)==B_OK)
			{
				if (visible_view->caret_drawn) visible_view->DrawCaret();
				fixed_font.SetSize((float)size);
				if (!(currentfont & PROP_FONT))	hugo_font(currentfont);
				SaveSettings();
				display_needs_repaint = true;
			}
			break;
		}
		
		// Font encoding:
		case MSG_UNICODE_UTF8:
		case MSG_ISO_8859_1:
		case MSG_ISO_8859_2:
		case MSG_ISO_8859_3:
		case MSG_ISO_8859_4:
		case MSG_ISO_8859_5:
		case MSG_ISO_8859_6:
		case MSG_ISO_8859_7:
		case MSG_ISO_8859_8:
		case MSG_ISO_8859_9:
		case MSG_ISO_8859_10:
		case MSG_MACINTOSH_ROMAN:
			font_encoding = msg->what - MSG_UNICODE_UTF8;
			break;
		
		case MSG_SMART_FORMATTING:
			smartformatting = !smartformatting;
			smartformatting_menu->SetMarked(smartformatting!=0);
			break;

		// Color selection handling:
		case MSG_COLOR_FOREGROUND:
		case MSG_COLOR_BACKGROUND:
		case MSG_COLOR_SLFOREGROUND:
		case MSG_COLOR_SLBACKGROUND:
		{
			rgb_color fore_color, back_color;
			int which_color = 0;
			strcpy(line, "Select the new default ");
			switch (msg->what)
			{
				case MSG_COLOR_FOREGROUND:
					strcat(line, "foreground");
					fore_color = def_fcolor;
					back_color = def_bgcolor;
					which_color = 0;
					break;
				case MSG_COLOR_BACKGROUND:
					strcat(line, "background");
					fore_color = def_fcolor;
					back_color = def_bgcolor;
					which_color = 1;
					break;
				case MSG_COLOR_SLFOREGROUND:
					strcat(line, "statusline foreground");
					fore_color = def_slfcolor;
					back_color = def_slbgcolor;
					which_color = 0;
					break;
				case MSG_COLOR_SLBACKGROUND:
					strcat(line, "statusline background");
					fore_color = def_slfcolor;
					back_color = def_slbgcolor;
					which_color = 1;
					break;
			}
			strcat(line, " color:");
			color_msg = new BMessage(MSG_COLOR_SELECTOR);
			color_msg->AddInt32("which", msg->what);
			ColorSelector *cs;
			cs = new ColorSelector((BWindow *)window, "Select Color", line,
				color_msg, &fore_color, &back_color, which_color);
			cs->Show();
			break;
		}
		case MSG_COLOR_SELECTOR:	// posted by ColorSelector
		{
			bool ok;
			if (msg->FindBool("OK", &ok)!=B_OK)
			{
				delete color_msg;
				break;
			}
			int32 which;
			const void *data;
			ssize_t size;
			if (msg->FindInt32("which", &which)!=B_OK) break;
			if (msg->FindData("color", B_RGB_COLOR_TYPE, &data, &size)==B_OK)
			{
				rgb_color *color = NULL;
				switch (which)
				{
					case MSG_COLOR_FOREGROUND:
						color = &def_fcolor; break;
					case MSG_COLOR_BACKGROUND:
						color = &def_bgcolor; break;
					case MSG_COLOR_SLFOREGROUND:
						color = &def_slfcolor; break;
					case MSG_COLOR_SLBACKGROUND:
						color = &def_slbgcolor; break;
				}
				memcpy(color, data, size);
				SaveSettings();
				display_needs_repaint = true;
			}	
			delete color_msg;
			break;
		}
		case MSG_COLOR_RESET:
		{		
			def_fcolor = hugo_color(DEF_FCOLOR);
			def_bgcolor = hugo_color(DEF_BGCOLOR);
			def_slfcolor = hugo_color(DEF_SLFCOLOR);
			def_slbgcolor = hugo_color(DEF_SLBGCOLOR);
			update_bgcolor = def_bgcolor;
			display_needs_repaint = true;
			break;
		}
		
		// Other Option menu handling:
		case MSG_FULL_SCREEN:
		{
			// From window to full-screen
			if (!full_screen)
			{
				int frame_size = 0;
				BScreen screen(window);
				BRect rect;
				
				// Save current window coords. and expand the
				// window to a tabless full-screen window
				default_rect = Frame();
				rect = screen.Frame();
				window->SetType(B_BORDERED_WINDOW);
				window->SetLook(B_MODAL_WINDOW_LOOK);
				window->SetFlags(window->Flags() | B_NOT_MOVABLE |
					B_NOT_RESIZABLE);
				window->MoveTo(frame_size, frame_size);
				window->ResizeTo(rect.right-frame_size*2,
					rect.bottom-frame_size*2);
			}
			
			// From full-screen to window
			else
			{
				// Restore the normal window look and size
				window->SetType(B_TITLED_WINDOW);
				window->SetFlags(window->Flags() & ~B_NOT_MOVABLE &
					~B_NOT_RESIZABLE);
				window->MoveTo(default_rect.left, default_rect.top);
				window->ResizeTo(default_rect.right-default_rect.left,
					default_rect.bottom-default_rect.top);
			}
			full_screen = !full_screen;
			full_screen_menu->SetMarked(full_screen!=0);
			display_needs_repaint = true;
			break;
		}
		case MSG_FAST_SCROLLING:
		{
			fast_scrolling = !fast_scrolling;
			fast_scrolling_menu->SetMarked(fast_scrolling!=0);
			break;
		}
#ifdef USE_TEXTBUFFER
		case MSG_TEXT_SELECT:
		{
			allow_text_selection = !allow_text_selection;
			text_select_menu->SetMarked(allow_text_selection!=0);
			break;
		}
#endif
		case MSG_DISPLAY_GRAPHICS:
		{
			display_graphics = !display_graphics;
			display_graphics_menu->SetMarked(display_graphics!=0);
			display_needs_repaint = true;
			break;
		}
		case MSG_GRAPHICS_SMOOTHING:
		{
			graphics_smoothing = !graphics_smoothing;
			graphics_smoothing_menu->SetMarked(graphics_smoothing!=0);
			break;
		}
		case MSG_ENABLE_AUDIO:
		{
			enable_audio = !enable_audio;
			enable_audio_menu->SetMarked(enable_audio!=0);
			break;
		}
		case MSG_UNFREEZE_WINDOWS:
		{
			hugo_settextwindow(1, 1,
				SCREENWIDTH/FIXEDCHARWIDTH, 
				SCREENHEIGHT/FIXEDLINEHEIGHT);
			physical_lowest_windowbottom = lowest_windowbottom = 0;
			ConstrainCursor();
			view->Update(true);
			break;
		}
		case MSG_RESET_DISPLAY:
		{
			BAlert *alert = new BAlert("Reset Display",
				"Erase existing display?",
				"Yes, Erase", "No, Don't Erase", NULL, B_WIDTH_FROM_LABEL);
			// 0 = Yes, 1 = No
			if (alert->Go()) break;

			hugo_clearfullscreen();
			visible_view->Draw(window->current_rect);
			view->needs_updating = true;
			view->Update(true);
			break;
		}
		case MSG_SHOW_COMPASS:
		{
			// In the scrollback window, use Alt+C to copy, not
			// toggle the compass
			if (scrollback_shown)
			{
				textview->Copy(be_clipboard);
				return;
			}
			
			show_compass = !show_compass;
			show_compass_menu->SetMarked(show_compass!=0);
			if (show_compass)
				compass->Show();
			else
				compass->Hide();
			break;
		}
		
		case MSG_SHOW_SCROLLBACK:
		{
			scrollback_shown = !scrollback_shown;
			show_scrollback_menu->SetMarked(scrollback_shown!=0);
			if (scrollback_shown)
			{
				// It would be nice here to be able to set
				// the encoding of textview's font to
				// font_encoding, but BTextViews are always UTF-8
				
				textview->SetText(scrollback_buffer);
				textview->ScrollToOffset(scrollback_pos);
				scrollview->Show();
				visible_view->Hide();
				textview->MakeFocus(true);
				window->SetType(B_DOCUMENT_WINDOW);	// resizing box
			}
			else
			{
				visible_view->MakeFocus(true);
				scrollview->Hide();
				visible_view->Show();
				window->SetType(B_TITLED_WINDOW);	// no resizing box
				if (full_screen)
				{
					// Make sure we reset the proper full-screen look
					window->SetType(B_BORDERED_WINDOW);
					window->SetLook(B_MODAL_WINDOW_LOOK);
				}
			}
			break;
		}
		
		// default BWindow message processing
		default:
			BWindow::MessageReceived(msg);
			break;
	}
}

bool HugoWindow::QuitRequested()
{
#ifndef DEBUG
	if (he_thread_running)
	{
		visible_view->Draw(visible_view->Bounds());
		
		BAlert *alert = new BAlert("Quit", "Abandon the current game?",
			"Yes, Quit", "No, Don't Quit", NULL, B_WIDTH_FROM_LABEL);
//		HUGO_BEEP();
		// 0 = Yes, 1 = No
		if (alert->Go()) return false;
	}
#endif

	if (!full_screen)
		default_rect = Frame();		// save current window coords.
//	RemoveChild(menubar);
	quit_he_thread = true;
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}

void HugoWindow::WindowActivated(bool active)
{
#ifndef NO_SOUND
	if (!active)
	{
		audio_factory_suspended = true;
	}
	else if (audio_factory_suspended)
	{
		audio_factory_suspended = false;
	}
#endif
	isactive = active;
}

void HugoWindow::FrameResized(float width, float height)
{
	// Let's use the client width/height instead of the frame
	width = Bounds().Width();
	height = Bounds().Height()-(menubar->Bounds().Height());

	// Shift the bitmap up if necessary
	if (current_text_y+lineheight > (int)height)
	{
		BRect dest_rect = Bounds();
		BRect source_rect = Bounds();
		source_rect.OffsetBy(0, current_text_y+lineheight-(int)height);
		
		if (bitmap->Lock())
		{
			view->CopyBits(source_rect, dest_rect);
			bitmap->Unlock();
		}
		
		view->needs_updating = true;
		view->Update(true);
	}
	
	ResizeBitmap();
	hugo_settextmode();

	// Do engine-internal post-resize metric tweaking
	if (!inwindow)
	{
		physical_windowright = (int)width;
		physical_windowwidth = (int)width - physical_windowleft + 1;
		physical_windowbottom = (int)height;
		physical_windowheight = (int)height - physical_windowtop + 1;

		if (currentline > physical_windowheight/lineheight)
			currentline = physical_windowheight / lineheight;
	}
}

void HugoWindow::Zoom(BPoint origin, float width, float height)
{
	BWindow::Zoom(origin, width, height);
	ResizeBitmap();
}

void HugoWindow::ResizeBitmap()
{
	// Every time we resize the window, this gets called in order to copy the
	// existing bitmap, destroy it, create a new resized bitmap, and copy
	// the old contents back to it
	
	HugoBitmap *bitmapCopy = NULL;
	
	if (!bitmap) return;
	if (!bitmap->Lock()) return;
	
	resizing = true;
	
	// Copy the existing bitmap before we kill it
	bitmapCopy = new HugoBitmap(bitmap->Bounds(), B_RGB32, true);
	HugoView viewCopy(bitmap->Bounds(), "Hugo view copy");
	if (bitmapCopy)
	{
		bitmapCopy->Lock();
		bitmapCopy->AddChild(&viewCopy);
		viewCopy.DrawBitmap(bitmap, BPoint(0, 0));
		bitmapCopy->RemoveChild(&viewCopy);
		bitmapCopy->Unlock();
	}

	// Get rid of the old bitmap and view and create new resized ones
	bitmap->RemoveChild(view);
	BRect rect = window->Bounds();
	rect.bottom*=2;
	delete bitmap;
	delete view;
	bitmap = new HugoBitmap(rect, B_RGB32, true);
	bitmap->Lock();
	view = new HugoView(rect, "Hugo view");
	bitmap->AddChild(view);

	// Clear the newly recreated view to the background color
	view->SetLowColor(update_bgcolor);
	view->FillRect(rect, B_SOLID_LOW);

	// Copy the contents of the old bitmap back to the (new) memory bitmap
	if (bitmapCopy)
	{
		BRect brect(bitmapCopy->Bounds());
		if (brect.bottom > rect.bottom)
			brect.bottom = rect.bottom/2;	// only copy the visible portion
		view->DrawBitmap(bitmapCopy, brect, brect);
		delete bitmapCopy;
	}

	// If the text display has already been initialized, then recalculate
	// for the new canvas dimensions
	if (FIXEDCHARWIDTH)
	{
		hugo_settextmode();
	}
	display_needs_repaint = true;

	view->SetHighColor(current_text_color);
	view->SetLowColor(current_back_color);
	view->SetFont(&current_font);

	bitmap->Unlock();
	
	// Resize the current_rect
	current_rect = Bounds();
	current_rect.bottom-=menubar->Bounds().Height();

	resizing = false;
}

void HugoWindow::StoryMenu(int32 what)
{
	if (!during_player_input)
	{
		BAlert *alert = new BAlert("Story Operation",
			"Only valid at player command input.",
			"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
//		HUGO_BEEP();
		alert->Go();
		return;
	}
/*
	if (scrollback)
	{
		BAlert *alert = new BAlert("Story Operation",
			"You must close the scrollback window first.",
			"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
//		HUGO_BEEP();
		alert->Go();
		return;
	}
*/

#ifdef TYPED_STORY_COMMANDS
	if (what==MSG_SAVE)
		TypeCommand("Save", true, true);
	else if (what==MSG_RESTORE)
		TypeCommand("Restore", true, true);
	else if (what==MSG_UNDO)
		TypeCommand("Undo", true, true);
	else	// MSG_RESTART
#endif
		he_thread_request = what;
}


//--------------------------------------------------------------------
// Off-screen bitmap
//--------------------------------------------------------------------

HugoBitmap::HugoBitmap(BRect bounds, color_space space, bool accepts_views)
	:BBitmap(bounds, space, accepts_views)
{
}

bool HugoBitmap::Lock()
{
	// The engine thread can't lock the bitmap when the main
	// thread is resizing the window
	if (find_thread(NULL)==he_thread)
	{
		if (window->resizing) return false;
	}

	return BBitmap::Lock();
}


//--------------------------------------------------------------------
// Non-visible view class (off-screen bitmap)
//--------------------------------------------------------------------

HugoView::HugoView(BRect frame, const char *name)
	:BView(frame, name, B_FOLLOW_ALL_SIDES, 0)
{
	scroll_offset = 0;
	needs_updating = 0;
}

void HugoView::Update(bool visible)
{
	if (override_client_updating || !needs_updating) return;
	
	needs_updating = 0;
	
	if (!bitmap->Lock()) goto UpdateVisible;

	// If the virtual window has been scrolled down the off-screen bitmap,
	// move its contents to the top of the bitmap
	if (scroll_offset)
	{
		// The use of plw_offset and all the +1, +plw_offset stuff
		// below _seems_ to be unnecessary now; it was there before
		// to deal with trailing lines left on [MORE] prompts, etc.,
		// but that must've been due to bad calculations elsewhere.
//		int plw_offset = physical_lowest_windowbottom?1:0;

		BRect rect(window->current_rect);

		CopyBits(
			// source
			BRect(0, physical_lowest_windowbottom+scroll_offset,//+1,
				rect.right, rect.bottom+scroll_offset),//+1),
			// dest
			BRect(0, physical_lowest_windowbottom,//+plw_offset,
				rect.right, rect.bottom));//+plw_offset));

		// Erase the unseen portion of the scroll-offset area
		SetLowColor(update_bgcolor);
		rect = window->current_rect;
		rect.top = rect.bottom+1;
		rect.bottom*=2;
		FillRect(rect, B_SOLID_LOW);
	}

	// Just for the record, we used to sync in HugoVisibleView::Draw, but
	// the additional bitmap->Lock() would lead to a race condition with
	// alerts, etc. displayed by he_thread.
	view->Sync();
	bitmap->Unlock();

UpdateVisible:

	scroll_offset = 0;
	
	// Update the visible view
	if (visible)
	{
		BRect rect(window->current_rect);
		visible_view->Draw(rect);
	}
}


//--------------------------------------------------------------------
// Visible view class
//--------------------------------------------------------------------

HugoVisibleView::HugoVisibleView(BRect frame, const char *name)
	:BView(frame, name, B_FOLLOW_ALL_SIDES, B_WILL_DRAW)
{
	caret_drawn = 0;
}

void HugoVisibleView::Draw(BRect rect)
{
	if (window->Lock())
	{
		// Update the visible view with the bitmap
		DrawBitmap(bitmap, rect, rect);
		window->Unlock();

		// Redraw the caret if it got erased
		if (caret_drawn)
		{
			caret_drawn = !caret_drawn;
			DrawCaret();
		}
	}
}

void HugoVisibleView::DrawCaret()
{
	// With no parameters, DrawCaret() draws at the last-drawn position
	DrawCaret(caret_x, caret_y);
}

void HugoVisibleView::DrawCaret(int x, int y)
{
	// Draw a caret (in inverse mode) at x, y
	if (window->Lock())
	{
		BFont temp_font;
		GetFont(&temp_font);
		temp_font.SetSize(current_font.Size());
		SetFont(&temp_font);
		SetDrawingMode(B_OP_INVERT);
		DrawChar('|', BPoint(x-StringWidth("|")/2, y));
		SetDrawingMode(B_OP_COPY);
		Sync();
		caret_drawn = !caret_drawn;
		caret_x = x, caret_y = y;
		window->Unlock();
	}
}

void HugoVisibleView::KeyDown(const char *bytes, int32 numBytes)
{
	char key = bytes[0];
	uint32 m = modifiers();
	
	if (video_playing && !video_background)
	{
		if (key==B_ESCAPE) hugo_stopvideo();
		return;
	}
	
	switch (key)
	{
		case B_BACKSPACE:
			PushKeypress(BACKSPACE_KEY);
			break;
		case B_ENTER:
			PushKeypress(13);
			break;
		case B_UP_ARROW:
			PushKeypress(11);
			break;
		case B_DOWN_ARROW:
			PushKeypress(10);
			break;
		case B_LEFT_ARROW:
			if (m & B_CONTROL_KEY)
				PushKeypress(CTRL_LEFT_KEY);
			else
				PushKeypress(8);
			break;
		case B_RIGHT_ARROW:
			if (m & B_CONTROL_KEY)
				PushKeypress(CTRL_RIGHT_KEY);
			else
				PushKeypress(21);
			break;
/*
		case B_PAGE_UP:
			if (!scrollback_shown)
				window->PostMessage(MSG_SHOW_SCROLLBACK);
			break;
*/
		default:
			if (!(m & B_CONTROL_KEY))
				PushKeypress(key);
	}
}

void HugoVisibleView::MouseDown(BPoint point)
{
// No need to go through the hassle of checking what mouse button it
// is if we're < v3.0 (since we don't support menus in v2.5.x anyway)
#if !defined (COMPILE_V25)
	BPoint skip, initial_point;//, new_point;
	uint32 buttons;
	static bigtime_t last_mouse = 0;
	
	initial_point = point;	
	GetMouse(&skip, &buttons);
	
	if (buttons & B_PRIMARY_MOUSE_BUTTON && modifiers() & B_CONTROL_KEY)
		buttons = B_SECONDARY_MOUSE_BUTTON;
		
#ifdef USE_TEXTBUFFER
	if (getline_active)
	{
		if (allow_text_selection && buttons==B_PRIMARY_MOUSE_BUTTON)
		{
			bigtime_t interval, time = real_time_clock_usecs();
			get_click_speed(&interval);
			
			// Double-click
			if (time-last_mouse < interval)
			{
				char *w = TB_FindWord((int)point.x, (int)point.y);
				if (w)
				{
					if (current_text_x + hugo_textwidth(w) < physical_windowwidth)
					{
						TypeCommand(w, false, false);
					}
				}
				last_mouse = 0;
				return;
			}
			
			last_mouse = time;
			
			while (real_time_clock_usecs()-time < interval) 
			{
				GetMouse(&skip, &buttons);
				if (buttons!=B_PRIMARY_MOUSE_BUTTON)
					return;
				snooze(5000);
			}
		}
				
		buttons = B_SECONDARY_MOUSE_BUTTON;
	}
#else
	if (getline_active)
	{
		buttons = B_SECONDARY_MOUSE_BUTTON;
	}
#endif

	if (buttons & B_PRIMARY_MOUSE_BUTTON)
	{
/*
		bigtime_t t1, t2;
		
		// Make a held primary button act like a secondary (i.e.,
		// right) button.  But don't circumvent the primary button
		// if we're potentially going to use it (i.e., in a pause
		// where we're waiting_for_key)
		t1 = real_time_clock_usecs();
		t2 = t1;
		while (t2-t1 < 1000000 || waiting_for_key)
		{
			GetMouse(&skip, &buttons);
			if (!(buttons & B_PRIMARY_MOUSE_BUTTON))
				break;
			t2 = real_time_clock_usecs();
			snooze(5000);
		}
		if (t2-t1 >= 1000000 && !waiting_for_key)
		{
			GetMouse(&new_point, &buttons);
			if (new_point==initial_point)
				goto SecondaryButton;
		}
*/
#endif
		// i.e., hugo_waitforkey()
//		if (waiting_for_key)
		{
			int mouse_x, mouse_y;
			mouse_x = (int)(point.x - physical_windowleft)/FIXEDCHARWIDTH + 1;
			mouse_y = (int)(point.y - physical_windowtop)/FIXEDLINEHEIGHT + 1;
			PushKeypress(1);
			PushKeypress(mouse_x);
			PushKeypress(mouse_y);
			return;
		}
#if !defined (COMPILE_V25)
	}
		
	if (buttons & B_SECONDARY_MOUSE_BUTTON)
	{
//SecondaryButton:
		if (getline_active)
			HandleContextMenu(point);
	}
#endif
}

void HugoVisibleView::MouseMoved(BPoint point, uint32 transit, const BMessage *msg)
{
	// Whenever the mouse moves over the lower-right corner of the window,
	// display a sizing box (if the scrollback window isn't shown, since it
	// already has a sizing box).  Also, this doesn't apply if we're full-screen.
	
	if (scrollback_shown || full_screen) return;

	if (window->Lock())
	{
		BRect rect(Bounds());
		if (transit==B_INSIDE_VIEW && window->isactive &&
			point.x > rect.right-B_V_SCROLL_BAR_WIDTH &&
			point.y > rect.bottom-B_H_SCROLL_BAR_HEIGHT)
		{
			SetEventMask(B_POINTER_EVENTS);
			window->SetType(B_DOCUMENT_WINDOW);
		}
		else if (window->Type()!=B_TITLED_WINDOW)
		{
			SetEventMask(0);
			window->SetType(B_TITLED_WINDOW);
		}
		window->Unlock();
	}
}

void HugoVisibleView::HandleContextMenu(BPoint point)
{
#if !defined (COMPILE_V25)
	char *cc;
	int i, noreturn = 0;

	if (!context_commands) return;

	// Erase the caret from the visible view
	if (visible_view->caret_drawn)
	{
		if (window->Lock())
		{
			visible_view->DrawCaret();
			window->Unlock();
		}
	}
	
	BPopUpMenu *menu = new BPopUpMenu("Context menu", false, false);
	BMenuItem *item;

	for (i=0; i<context_commands; i++)
	{
		// separator
		if (context_command[i][0]=='-')
		{
			menu->AddItem(new BSeparatorItem());
		}

		// normal menu item
		else
		{
			context_command[i][0] = toupper(context_command[i][0]);
			menu->AddItem(new BMenuItem(context_command[i], NULL));
		}
	}

	ConvertToScreen(&point);
	// Use the BRect version to keep the menu onscreen (since it seems
	// to disappear when the mouse is released, otherwise)
	item = menu->Go(point, true, false,
		BRect(point.x-5, point.y-5, point.x+5, point.y+5), false);

	// If the menu returns a selected item, send it to the player input line
	if (item)
	{
		i = menu->IndexOf(item);
		cc = context_command[i];
					
		// If command ends with "...", don't push 
		// Enter at the end
		if ((strlen(cc)>=4) && !strcmp(cc+strlen(cc)-3, "..."))
		{
			noreturn = 1;
		}

		// Esc to clear input
		PushKeypress(27);
		// Each letter of the command
		PushKeypress(OVERRIDE_UPDATING);
		for (i=0; i<(int)strlen(cc)-(noreturn?4:0); i++)
			PushKeypress(cc[i]);
		if (!noreturn)
		{
			// Enter
			PushKeypress(RESTORE_UPDATING);
			PushKeypress(13);
		}
		else
		{
			PushKeypress(cc[i]);
			PushKeypress(' ');
			PushKeypress(RESTORE_UPDATING);
		}
	}
	
	delete menu;

#endif	// ifdef COMPILE_V25
}
