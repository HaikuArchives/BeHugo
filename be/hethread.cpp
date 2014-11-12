/*
 	HETHREAD.CPP
 	Thread management for the engine thread
	for the Hugo Engine

 	by Kent Tessman (c) 1999-2006

	Setting he_thread_request to a MSG_* value causes this function to be
	called from the engine thread.
*/

#include "behugo.h"

extern "C"
{
#include "heheader.h"
}

int he_thread_request = 0;

// From hemisc.c:
extern char during_player_input;

// From heparse.c:
extern char full_buffer;

void process_he_thread_request(int32 what)
{
	int r = 0;
	
	he_thread_request = 0;
	
	// From here down we deal with the following story-menu requests:
	//	MSG_RESTART
	//	MSG_RESTORE
	//	MSG_SAVE
	//	MSG_UNDO

	// The printing and new-prompt simulation are because the normal 
	// flow of a Hugo program expects "save", "restore", etc. to be 
	// entered as typed commands.
	//
	Printout("");
	
	getline_active = false;
	
	if (visible_view->caret_drawn) visible_view->DrawCaret();

	switch (what)
	{
		case MSG_RESTART:
		{
			BAlert *alert = new BAlert("Restart Current Story",
				"Are you sure you wish to abandon the current story?",
				"Yes, Restart", "No, Don't Restart", NULL, B_WIDTH_FROM_LABEL);
//			HUGO_BEEP();
			// 0 = Yes, 1 = No
			if (!alert->Go())
			{
				// Font may change
				int tf = currentfont &= ~ITALIC_FONT;
				// So restarts aren't nested
				during_player_input = false;
				r = -1;
				r = RunRestart();
				during_player_input = true;
				hugo_font(tf);
			}
			else
			{
				Printout("[Restart cancelled]");
				goto NewPrompt;
			}
			break;
		}
		
		// We won't get here if we're using typed commands for these:
		
		case MSG_RESTORE:
			Printout("[RESTORE]");
			r = RunRestore();
			break;
		case MSG_SAVE:
			Printout("[SAVE]");
			r = RunSave();
			break;
		case MSG_UNDO:
			Printout("[UNDO]");
			r = Undo();
			break;
	}

	if (!r && what!=MSG_UNDO)
	{
		BAlert *alert = new BAlert("File Error", 
			"Unable to perform file operation.",
			"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
//		HUGO_BEEP();
		alert->Go();
	}
	else if (!r)
	{
		BAlert *alert = new BAlert("Undo Error", "Unable to undo.", 
			"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->Go();
	}
	
NewPrompt:
	getline_active = true;

	// Simulate a new prompt a la GetCommand()
	full = 0;
	hugo_settextpos(1, physical_windowheight/lineheight+1);
	hugo_print("\n");
	hugo_print(GetWord(var[prompt]));
	FlushBuffer();
	ConstrainCursor();
	processed_accelerator_key = true;
	view->needs_updating = true;
	view->Update(true);

	full_buffer = false;
}
