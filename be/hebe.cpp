/*
 	HEBE.CPP
 	Non-portable functions for BeOS

	for the Hugo Engine

 	by Kent Tessman (c) 1995-2006

 	Note that this file hebe.cpp contains the standard engine interface
	routines; behugo.cpp contains BeOS user interface functions.
*/	

#include "behugo.h"

#include <Path.h>

extern "C"
{
#include "heheader.h"
#include <malloc.h>

/* Function prototypes: */
void hugo_addcommand(void);
void hugo_restorecommand(int);

/* Specific to hebe.cpp: */
void RedrawInputLine(int index);
void ConstrainCursor(void);
void FlushBuffer(void);


#define SCROLLBACK_SIZE 32768	// for scrollback window
char scrollback_buffer[SCROLLBACK_SIZE];
int scrollback_pos = 0;

#define HISTORY_SIZE    16      // for command-line editing
int hcount = 0;
char *history[HISTORY_SIZE];

int current_text_col, current_text_row;
int text_descent = 0;
int prop_lineheight = 0;
int insert_mode = true;
char waiting_for_key = false;
int text_windowleft, text_windowtop, text_windowright, text_windowbottom,
	text_windowwidth;
bool override_client_updating = false;

#if defined (DEBUGGER)
void *AllocMemory(size_t size);
#endif


/*
    FILENAME MANAGEMENT:

    Different operating systems will have their own ways of naming
    files.  The following routines are simply required to know and
    be able to dissect/build the components of a particular filename,
    storing/restoring the compenents via the specified char arrays.

    For example, in MS-DOS:

        hugo_splitpath("C:\HUGO\FILES\HUGOLIB.H", ...)
                becomes:  C:, HUGO\FILES, HUGOLIB, H

    and

        hugo_makepath(..., "C:", "HUGO\FILES", "HUGOLIB", "H")
                becomes:  C:\HUGO\FILES\HUGOLIB.H

    The appropriate equivalent nomenclature should be used for the
    operating system in question.
*/

void hugo_splitpath(char *path, char *drive, char *dir, char *fname, char *ext)
{
	char *file;
	char *extension;

        strcpy(drive,"");
        strcpy(dir,"");
        strcpy(fname,"");
        strcpy(ext,"");

        if ((file = strrchr(path,'/')) == 0)
        {
                if ((file = strrchr(path,':')) == 0) file = path;
        }
        strncpy(dir,path,strlen(path)-strlen(file));
        *(dir+strlen(path)-strlen(file)) = 0;
        extension = strrchr(file,'.');
        if ((extension != 0) && strlen(extension) < strlen(file))
        {
                strncpy(fname,file,strlen(file)-strlen(extension));
                *(fname+strlen(file)-strlen(extension)) = 0;
                strcpy(ext,extension+1);
        }
        else strcpy(fname,file);

        if (strcmp(dir, "") && fname[0]=='/') strcpy(fname, fname+1);
}

void hugo_makepath(char *path, char *drive, char *dir, char *fname, char *ext)
{
        if (*ext == '.') ext++;
        strcpy(path,drive);
        strcat(path,dir);
        switch (*(path+strlen(path)))
        {
        case '/':
        case ':':
/*        case 0: */
                break;
        default:
                if (strcmp(path, "")) strcat(path,"/");
                break;
        }
        strcat(path,fname);
        if (strcmp(ext, "")) strcat(path,".");
        strcat(path,strlwr(ext));
}


/*
    MEMORY ALLOCATION:

    hugo_blockalloc(), and hugo_blockfree() are necessary because of
    the way MS-DOS handles memory allocation across more than 64K.
    For most systems, these will simply be normal ANSI function calls.
*/

void *hugo_blockalloc(long num)
{
        return malloc(num * sizeof(char));
}

void hugo_blockfree(void *block)
{
        free(block);
}


/*
    OVERWRITE:

    Checks to see if the given filename already exists, and prompts to
    replace it.  Returns true if file may be overwritten.
*/

int hugo_overwrite(char *f)
{
// BFilePanel looks after overwrite confirmation for us
return true;
	bool verify = false;
	FILE *tempfile;

	if (!(tempfile = fopen(f, "rb")))	/* if file doesn't exist */
		return true;
	fclose(tempfile);

	sprintf(buffer, "Overwrite existing \"%s\"?", f);

	BAlert *verify_box = new BAlert("File Exists", buffer,
		"Yes", "No", NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	verify_box->SetShortcut(1, B_ESCAPE);
//	HUGO_BEEP();
	verify = !verify_box->Go();

	return verify;
}


/*
    CLOSEFILES:

    Closes all open files.  NOTE:  If the operating system automatically
    closes any open streams upon exit from the program, this function may
    be left empty.
*/

void hugo_closefiles()
{
/*
        fclose(game);
  	if (script) fclose(script);
  	if (io) fclose(io);
  	if (record) fclose(record);
*/
}


/*
    GETFILENAME:

    Loads the name of the filename to save or restore (as specified by
    the argument <a>) into the line[] array.
*/

void hugo_getfilename(char *usage, char *def_filename)
{
	static char initial_dir[MAXPATH] = "";
	char drive[MAXDRIVE], dir[MAXDIR], fname[MAXFILENAME], ext[MAXEXT];
	file_panel_mode mode = B_SAVE_PANEL;
	
	sprintf(line, "Select the file %s", usage);

	if (def_filename==scriptfile)
	{
		hugo_splitpath(def_filename, drive, dir, fname, ext);
		if (!strcmp(initial_dir, ""))
			hugo_makepath(initial_dir, drive, dir, "", "");
	}
	else if (def_filename==recordfile)
	{
		hugo_splitpath(def_filename, drive, dir, fname, ext);
		if (!strcmp(initial_dir, ""))
			hugo_makepath(initial_dir, drive, dir, "", "");
		if (!strcmp(usage, "for command playback")) mode = B_OPEN_PANEL;
	}
	else	// save/restore
	{
		if (!strcmp(initial_dir, ""))
			// Hard-coded save subdirectory for the time being
			sprintf(initial_dir, "%s/%s", app_directory, SAVE_SUBDIR);
		if (!strcmp(usage, "to restore")) mode = B_OPEN_PANEL;
	}
	
	BEntry entry = BEntry(initial_dir, true);
	entry_ref panel_directory;
	entry.GetRef(&panel_directory);
	
	BMessenger target = BMessenger(window);

	file_panel = new BFilePanel(mode, &target,
		&panel_directory, B_FILE_NODE, false,
		NULL, NULL, true, false);
	BWindow *panel_window = file_panel->Window();
	panel_window->SetTitle(line);
	strcpy(file_selected, "//");
	file_panel->Show();
	
	while (!strcmp(file_selected, "//"))
	{
		if (quit_he_thread)
		{
			delete file_panel;
			file_panel = NULL;
		}
		IDLE_ENGINE_THREAD();
	}
	
	if (file_panel)
	{
		file_panel->GetPanelDirectory(&panel_directory);
		BPath path(&panel_directory);
		strcpy(initial_dir, path.Path());
		delete file_panel;
	}
	strcpy(line, file_selected);
}


/*
    GETKEY:

    Returns the next key waiting in the keyboard buffer.  It is
    expected that hugo_getkey() will return the following modified
    keystrokes:

	up-arrow        11 (CTRL-K)
	down-arrow      10 (CTRL-J)
	left-arrow       8 (CTRL-H)
	right-arrow     21 (CTRL-U)
*/

#define MAX_KEYPRESSES 128
int keypress_count = 0;
int keypress_queue[MAX_KEYPRESSES];

void PushKeypress(int k)
{
	if (keypress_count==MAX_KEYPRESSES) return;
	keypress_queue[keypress_count++] = k;
}

int PullKeypress(void)
{
	int i, k;

	if (keypress_count==0)
	{
		return 0;
	}

	k = keypress_queue[0];
	for (i=0; i<keypress_count-1; i++)
		keypress_queue[i] = keypress_queue[i+1];
	keypress_count--;
	keypress_queue[keypress_count] = 0;
	
	if (k==1 && keypress_count>=2)
	{
		display_pointer_x = keypress_queue[0];
		display_pointer_y = keypress_queue[1];
		for (i=0; i<keypress_count-2; i++)
			keypress_queue[i] = keypress_queue[i+2];
		keypress_queue[keypress_count-2] = 0;
		keypress_queue[keypress_count-1] = 0;
		keypress_count-=2;
	}

	if (k < 0) k = (unsigned char)k;

	return k;
}

#define BLINK_PERIOD 500000	// usecs

int hugo_getkey(void)
{
	int k;
	bigtime_t last_caret = real_time_clock_usecs()-BLINK_PERIOD;

	view->Update(true);

	while (!(k = PullKeypress()))
	{
		/* If an accelerator key (like Ctrl+Z for Undo) is
		   pressed, it is not treated as a normal keypress
		*/
		if (processed_accelerator_key)
		{
			k = 0;
			break;
		}

		IDLE_ENGINE_THREAD();
		
		// Blink the caret; erasing it if the window has lost focus
		if ((real_time_clock_usecs()-last_caret > BLINK_PERIOD && window->isactive)
			|| (!window->isactive && visible_view->caret_drawn))
		{
			if (window->Lock())
			{
				visible_view->DrawCaret(current_text_x,
					current_text_y+lineheight-text_descent);
				window->Unlock();
			}
			last_caret = real_time_clock_usecs();
		}
	}

	// Erase the caret from the visible view
	if (visible_view->caret_drawn)
	{
		if (window->Lock())
		{
			visible_view->DrawCaret();
			window->Unlock();
		}
	}
	
	return k;
}

/*
    GETLINE

    Gets a line of input from the keyboard, storing it in <buffer>.
*/

bool getline_active = false;

void hugo_getline(char *prmpt)
{
	int a, b, thiscommand;
	int c;                          /* c is the character being added */
	int oldx, oldy;
	int tempfont = currentfont;
	
	override_client_updating = false;
	getline_active = true;

	hugo_settextcolor(fcolor);
	hugo_setbackcolor(bgcolor);

	strcpy(buffer, "");
	c = 0;
	thiscommand = hcount;

	hugo_print(prmpt);
	FlushBuffer();
	
	/* An italic input font won't space/overwrite properly */
	hugo_font(currentfont &= ~ITALIC_FONT);

	/* i.e., the finishing position afer printing the prompt */
	oldx = current_text_x;
	oldy = current_text_y;

GetKey:

	b = hugo_getkey();

	/* If an accelerator key (like Ctrl+Z for Undo) is pressed, it
	   voids the current input
	*/
	if (processed_accelerator_key) goto BlankLine;

	hugo_settextcolor(icolor);
	hugo_setbackcolor(bgcolor);

	/* Now, start key checking */
	switch (b)
	{
		case (RESTORE_UPDATING):
			override_client_updating = false;
			if (window->Lock())
			{
				BRect rect(window->current_rect);
				visible_view->DrawBitmap(bitmap,
					BRect(0, current_text_y, rect.right, current_text_y+lineheight),
					BRect(0, current_text_y, rect.right, current_text_y+lineheight));
				window->Unlock();
			}
			break;
		case (OVERRIDE_UPDATING):
			override_client_updating = true;
			break;
		case (13):                      /* Enter */
		{
			full = 0;

			/* Copy the input to the script file (if open) */
			if (script) fprintf(script, "%s%s\n", prmpt, buffer);

			/* Copy the input to the scrollback buffer */
			hugo_sendtoscrollback(prmpt);
			hugo_sendtoscrollback(buffer);
			hugo_sendtoscrollback("\n");
			
			hugo_print("\r\n");
			
			strcpy(buffer, Rtrim(buffer));
			hugo_addcommand();

			/* Restore original font */
			hugo_font(currentfont = tempfont);
			
			getline_active = false;

			return;
		}
	        case B_INSERT:		/* Insert */
		{
			insert_mode = !insert_mode;
			goto GetKey;
		}
	        case BACKSPACE_KEY:
	        case B_DELETE:
		{
			if (strlen(buffer)>0)
			{
				if (b==BACKSPACE_KEY)
				{
					if (c==0) goto GetKey;
					c--;

					/* Move backward the width of the
					   deleted character
					*/
					current_text_x-=hugo_charwidth(buffer[c]);
					if (current_text_x < oldx)
						current_text_x = oldx;
				}

				/* Shift the buffer to account for the
				   deleted character
				*/
				for (a=c; a<=(int)strlen(buffer); a++)
					buffer[a] = buffer[a+1];

				RedrawInputLine(c);
			}
			goto GetKey;
		}
		case (8):                       /* left-arrow */
		{
			if (c > 0)
				current_text_x -= hugo_charwidth(buffer[--c]);
			goto GetKey;
		}
		case (21):                      /* right-arrow */
		{
			if (c<(int)strlen(buffer))
				current_text_x += hugo_charwidth(buffer[c++]);
			goto GetKey;
		}
		case CTRL_LEFT_KEY:
		{
			if (c)
			{
				do
				{
					do
					{
						current_text_x -= hugo_charwidth(buffer[--c]);
					}
					while (c && buffer[c-1]!=' ');
				}
				while (c && buffer[c]==' ');
			}
			goto GetKey;
		}
		case CTRL_RIGHT_KEY:
		{
			if (c<(int)strlen(buffer))
			{
				do
				{
					do
					{
						current_text_x += hugo_charwidth(buffer[c++]);
					}
					while (c<(int)strlen(buffer) &&
						buffer[c-1]!=' ');
				}
				while (c<(int)strlen(buffer) && buffer[c]==' ');
			}
			goto GetKey;
		}
		case (27):                      /* Escape */
		case (24):                      /* CTRL-X */
		{
BlankLine:
			strcpy(buffer, "");
			c = 0;
			current_text_x = oldx;
			RedrawInputLine(0);
			processed_accelerator_key = 0;
			goto GetKey;
		}
		case B_HOME:
		{
			c = 0;
			current_text_x = oldx;
			goto GetKey;
		}
		case B_END:
		{
			c = strlen(buffer);
			current_text_x = oldx + hugo_textwidth(buffer);
			goto GetKey;
		}
		case (11):                      /* up-arrow */
		{
			if (--thiscommand<0)
			{
				thiscommand = 0;
				goto GetKey;
			}
			a = strlen(buffer);
RestoreCommand:
			hugo_restorecommand(thiscommand);
			current_text_x = oldx;
			c = 0;
			RedrawInputLine(0);
			current_text_x = oldx+hugo_textwidth(buffer);
			c = strlen(buffer);
			goto GetKey;
		}
		case (10):                      /* down-arrow */
		{
			a = strlen(buffer);
			if (++thiscommand>=hcount)
			{
				thiscommand = hcount;
				goto BlankLine;
			}
			goto RestoreCommand;
		}
	}
	
	/* Disallow invalid keystrokes */
	if (b < 32 || b>255) goto GetKey;

	/* Hugo circa v2.2 allowed '^' and '~' for '\n' and '\"',
	   respectively
	*/
	if (game_version<=22 && (b=='^' || b=='~')) goto GetKey;

	/* (In Windows, we have to check both the window width AND
	   if we're going to overrun the buffer, just because a
	   ridiculously small-font/large-screen combination may be
	   in use.)
	*/
	if (current_text_x >= physical_windowright-charwidth ||
		c >= MAXBUFFER*2)
	{
		goto GetKey;
	}


	/* Add the new character: */

	/* If inserting, shift the post-insertion-point part of the buffer */
	buffer[strlen(buffer)+1] = '\0';
	if (c<(int)strlen(buffer) && insert_mode)
	{
		for (a=strlen(buffer); a>c; a--)
			buffer[a] = buffer[a-1];
	}
	buffer[c] = (char)b;

	/* Actually display the new character */
	RedrawInputLine(c);

	c++;
// For proper Be character/string formatting:
	current_text_x = oldx + (int)current_font.StringWidth(buffer, c);

	goto GetKey;
}


/* RedrawInputLine

	Redraws only the changed portion of the input line, i.e., from the
	current text position to the right edge of the window.  <index>
	gives the current position in the buffer (<c> from hugo_getline()).
*/

void RedrawInputLine(int index)
{
	if (!bitmap->Lock()) return;
	
	// Erase the rectangle of the input line
	view->SetLowColor(current_back_color);
	view->FillRect(BRect(current_text_x, current_text_y-1,
		physical_windowright, current_text_y+lineheight),
		B_SOLID_LOW);

	// Remember to add lineheight-1 to y-position
	view->DrawString(buffer+index,
		BPoint(current_text_x, current_text_y+lineheight-text_descent));

	view->Sync();
	
	// Update only only the dirty rectangle of the visible view
	if ((!override_client_updating) && window->Lock())
	{
		BRect rect(window->current_rect);
		visible_view->DrawBitmap(bitmap,
			BRect(0, current_text_y, rect.right, current_text_y+lineheight),
			BRect(0, current_text_y, rect.right, current_text_y+lineheight));
		window->Unlock();
	}
	
	bitmap->Unlock();
}


/* hugo_iskeywaiting

    Returns true if a keypress is waiting to be retrieved.
*/

int hugo_iskeywaiting(void)
{
	FlushBuffer();
	return (keypress_count > 0)?1:0;
}

/*
    WAITFORKEY:

    Provided to be replaced by multitasking systems where cycling while
    waiting for a keystroke may not be such a hot idea.
*/

int hugo_waitforkey(void)
{
	int key;
	
	FlushBuffer();
	view->needs_updating = true;	// sometimes the screen isn't updated,
	view->Update(true);		// so force an update

	waiting_for_key = true;
	key = hugo_getkey();
	waiting_for_key = false;
	
	return key;
}


/* hugo_timewait

    Waits for 1/n seconds.  Returns false if waiting is unsupported.
*/

int hugo_timewait(int n)
{
	static volatile int repaint_interval = 0;

	// Cycle for 1/n seconds
	bigtime_t t1, t2, diff;
	t1 = real_time_clock_usecs();
	do
	{
		// Check for a quit request
		if (quit_he_thread) exit_thread(he_thread_running = 0);
		
		t2 = real_time_clock_usecs();
		diff = (t2-t1)/1000;
		snooze(5000);
	} while (diff < 1000/n);

	// So that we don't bog things down repainting the
	// screen repeatedly, only do it every 1/10th second
	if ((repaint_interval+=1000/n) > 100)
	{
		view->needs_updating = true;
		view->Update(true);
		repaint_interval = 0;
	}	

	return true;
}


/*
    COMMAND HISTORY:

    To store/retrieve player inputs for editing.
*/

void hugo_addcommand(void)
{
	int i;

	if (!strcmp(buffer, "")) return;

        if (hcount>=HISTORY_SIZE)
	{
		hugo_blockfree(history[0]);
		for (i=0; i<HISTORY_SIZE-1; i++)
			history[i] = history[i+1];
		hcount = HISTORY_SIZE-1;
	}

	/* Because the debugger might use (up to) all available memory for
	   code line storage, a different means of memory allocation is
	   needed (at least in MS-DOS due to limited available memory to
	   begin with).
	*/
#if !defined (DEBUGGER)
	if ((history[hcount] = (char *)hugo_blockalloc((long)((strlen(buffer)+1)*sizeof(char))))==NULL)
#else
	if ((history[hcount] = (char *)AllocMemory((size_t)((strlen(buffer)+1)*sizeof(char))))==NULL)
#endif
	{
		hugo_blockfree(history[0]);
		if (hcount)
		{
			for (i=0; i<hcount; i++)
				history[i] = history[i+1];
			hcount--;
		}
		return;
	}

	for (i=0; i<=(int)strlen(buffer); i++)
		history[hcount][i] = buffer[i];
	hcount++;
}

void hugo_restorecommand(int n)
{
	int i;

	if (n < 0 || (n>=hcount && hcount!=HISTORY_SIZE-1)) return;

	i = 0;
	do
		buffer[i] = history[n][i];
	while (history[n][i++]!='\0');
}


/*
    DISPLAY CONTROL:

    These functions are specific library calls to QuickC/MS-DOS
    for screen control.  Each _function() should be replaced by
    the equivalent call for the operating system in question.  If
    a particular operation is unavailable, in some cases it may be
    left empty.  In other cases, it may be necessary to write a
    brief function to achieve the desired effect.
*/

void hugo_init_screen(void)
{}

void hugo_cleanup_screen(void)
{
// Not necessary, since we're destroying the window
}

void hugo_setgametitle(char *t)
{
	if (window->Lock())
	{
#if defined (DEBUGGER)
		/* The array must be larger than MAX_GAME_TITLE in heset.c */
		static char debug_game_title[96];
		sprintf(debug_game_title, "Hugo Debugger - %s", t);
		window->SetTitle(debug_game_title);
#else
		window->SetTitle(t);
#endif
		window->Unlock();
	}
}

void hugo_clearfullscreen(void)
{
/* Clears everything on the screen, moving the cursor to the top-left
   corner of the screen */

	FlushBuffer();

	BRect rect(window->current_rect);
	rect.bottom*=2;
	
	if (bitmap->Lock())
	{
		view->SetLowColor(current_back_color);
		view->FillRect(rect, B_SOLID_LOW);
		bitmap->Unlock();
	}
	
	view->needs_updating = true;
	
#ifdef USE_TEXTBUFFER
	TB_Clear(0, 0, (int)rect.Width(), (int)rect.Height());
#endif
	/* Must be set: */
	currentpos = 0;
	currentline = 1;
}


void hugo_clearwindow(void)
{
/* Clears the currently defined window, moving the cursor to the top-left
   corner of the window */

	FlushBuffer();

	BRect rect(physical_windowleft, physical_windowtop,
		physical_windowright, physical_windowbottom);

	if (!inwindow)
	{
		// adjustment for full-screen window?
		rect.bottom+=lineheight*2;
		
		// Also, so that when we change the window size, the
		// background is properly filled in:
		if (window->Lock())
		{
			visible_view->SetViewColor(current_back_color);
			window->Unlock();
		}
	}

	if (bitmap->Lock())
	{
		view->SetLowColor(current_back_color);
		view->FillRect(rect, B_SOLID_LOW);
		bitmap->Unlock();
	}

	/* Only forcing updating when not in a window will make for less
	   flickering
	*/
	if (!inwindow) view->needs_updating = true;

	/* Send a solid line to the scrollback buffer (unless the buffer is empty)... */
	if (!inwindow && scrollback_pos!=0 &&
		// ...preventing duplicate linebreaks
		((scrollback_pos>54) && scrollback_buffer[scrollback_pos-5]!='_'))
	{
		if (scrollback_buffer[scrollback_pos-1]!='\n')
			hugo_sendtoscrollback("\n");
		memset(line, '_', 20);
		sprintf(line+20, "\n\n");
		hugo_sendtoscrollback(line);
	}
	
#ifdef USE_TEXTBUFFER
	TB_Clear(physical_windowleft, physical_windowtop,
		physical_windowright, physical_windowbottom);
#endif
	/* Must be set: */
	currentpos = 0;
	currentline = 1;
}


void hugo_settextmode(void)
{
/* This function does whatever is necessary to set the system up for
   a standard text display */
	/* charwidth and lineheight are set up by hugo_font();
	   otherwise they would be set up here
	*/

	/* Must be set (as character or pixel coordinates, as
	   applicable--pixels, in this case):
	*/
	BRect rect(window->current_rect);
	SCREENWIDTH = (int)rect.right;
	SCREENHEIGHT = (int)rect.bottom;
	
	// The engine thread only calls this once, and it will have
	// to set up some default font metrics
	if (find_thread(NULL)==he_thread) hugo_font(0);
	
	/* Must be set: */
	hugo_settextwindow(1, 1,
		SCREENWIDTH/FIXEDCHARWIDTH, SCREENHEIGHT/FIXEDLINEHEIGHT);
}

void hugo_settextwindow(int left, int top, int right, int bottom)
{
/* Again, coords. are passed as text coordinates with the top corner (1, 1) */
	
	view->needs_updating = true;
	if (bitmap->Lock())
	{
		view->Update(false);
		bitmap->Unlock();
	}
	
	FlushBuffer();

	/* Must be set (as pixel coordinates): */
	physical_windowleft = (left-1)*FIXEDCHARWIDTH;
	physical_windowtop = (top-1)*FIXEDLINEHEIGHT;
	physical_windowright = right*FIXEDCHARWIDTH-1;
	physical_windowbottom = bottom*FIXEDLINEHEIGHT-1;

	/* Correct for full-width windows where the right border would
	   otherwise be clipped to a multiple of charwidth, leaving a sliver
	   of the former window at the righthand side.
	*/
	BRect rect(window->current_rect);
	if (right>=SCREENWIDTH/FIXEDCHARWIDTH)
		physical_windowright = (int)rect.right;
	if (bottom>=SCREENHEIGHT/FIXEDLINEHEIGHT)
		physical_windowbottom = (int)rect.bottom;

	physical_windowwidth = physical_windowright-physical_windowleft+1;
	physical_windowheight = physical_windowbottom-physical_windowtop+1;

	ConstrainCursor();
}

void hugo_settextpos(int x, int y)
{
/* The top-left corner of the current active window is (1, 1).

   (In other words, if the screen is being windowed so that the top row
   of the window is row 4 on the screen, the (1, 1) refers to the 4th
   row on the screen, and (1, 2) refers to the 5th.)

   This function must also properly set currentline and currentpos (where
   currentline is a the current character line, and currentpos may be
   either in pixels or characters, depending on the measure being used).
*/
	FlushBuffer();

	/* Must be set: */
	currentline = y;
	currentpos = (x-1)*FIXEDCHARWIDTH;   /* Note:  zero-based */

	/* current_text_x/row are calculated assuming that the
	   character position (1, 1) is the pixel position (0, 0)
	*/
	current_text_x = physical_windowleft + currentpos;
	current_text_y = physical_windowtop + (y-1)*lineheight;
	ConstrainCursor();
}

void ConstrainCursor(void)
{
	if (current_text_x < physical_windowleft)
		current_text_x = physical_windowleft;
//	if (current_text_x > physical_windowright-charwidth)
//		current_text_x = physical_windowright-charwidth;
	if (current_text_x > physical_windowright)
		current_text_x = physical_windowright;
	if (current_text_y > physical_windowbottom-lineheight)
		current_text_y = physical_windowbottom-lineheight;
	if (current_text_y < physical_windowtop)
		current_text_y = physical_windowtop;
}

// We use FlushBuffer() so that we can buffer multiple calls to hugo_print()
// in order to save on more expensive DrawString() GUI calls.  Note that we
// shift the drawing position down by (lineheight-1) because the DrawString()
// uses the given point as the baseline (+1).

#define MAX_FLUSH_BUFFER 512
static char flush_buffer[MAX_FLUSH_BUFFER] = "";
static int flush_x, flush_y, flush_len;

static char supposed_to_be_underlining = false;
static char last_was_italic = false;

void FlushBuffer(void)
{
	// Check for a quit request
	if (quit_he_thread) exit_thread(he_thread_running = 0);
	
	if (flush_len)
	{
		flush_buffer[flush_len] = '\0';

		if (bitmap->Lock())
		{
			// With Be, we don't get an opaque background rectangle,
			// so draw it
			view->SetLowColor(current_back_color);
			BRect rect(flush_x, flush_y,
				flush_x+current_font.StringWidth(flush_buffer),
				flush_y+lineheight-1);
				
			// In theory, we'd like to make sure we don't clip any
			// just-printed italic character, but it doesn't seem
			// to work properly (at least with fixed fonts)
			if (!last_was_italic)
				view->FillRect(rect, B_SOLID_LOW);
			else
				view->SetDrawingMode(B_OP_OVER);

			// Now draw the string itself
			view->DrawString(flush_buffer, BPoint(flush_x,
				flush_y+lineheight-text_descent));
				
			view->SetDrawingMode(B_OP_COPY);
			last_was_italic = false;
				
			// Draw any underline--why doesn't the font do this?
			if (supposed_to_be_underlining)
			{
				font_height fh;
				current_font.GetHeight(&fh);
				view->StrokeLine(
					BPoint(flush_x,
						flush_y+lineheight-fh.descent),
					BPoint(flush_x+current_font.StringWidth(flush_buffer)-1,
						flush_y+lineheight-fh.descent));
				supposed_to_be_underlining = false;
			}
		
			bitmap->Unlock();
		}
		
		view->needs_updating = true;
		flush_len = 0;
	}
}

void hugo_print(char *a)
{
       char last_was_newline = false;
	int i, len;

	len = strlen(a);

	for (i=0; i<len; i++)
	{
                last_was_newline = false;

		switch (a[i])
		{
			case '\n':
				FlushBuffer();
				current_text_y += lineheight;
				last_was_italic = false;
				break;
			case '\r':
				FlushBuffer();
				current_text_x = physical_windowleft;
				last_was_italic = false;
				break;
			default:
			{
				if (currentfont & ITALIC_FONT) last_was_italic = true;
				
				if (flush_len==0)
				{
					flush_x = current_text_x;
					if (inwindow)
						flush_y = current_text_y;
					else
						flush_y = current_text_y+view->scroll_offset;
				}
				
				if ((smartformatting) &&
					(((unsigned char)a[i]>=145 && (unsigned char)a[i]<=148) ||
					(unsigned char)a[i]==151))
				{
					FlushBuffer();
					flush_x = current_text_x;
					current_font.SetEncoding(B_UNICODE_UTF8);
					if (bitmap->Lock())
					{
						view->SetFont(&current_font);
						bitmap->Unlock();
					}	
					
					switch ((unsigned char)a[i])
					{
						case 145:	// left single quote
							strcpy(flush_buffer+flush_len, "‘");
							flush_len+=3;
							break;
						case 146:	// right single quote
							strcpy(flush_buffer+flush_len, "’");
							flush_len+=3;
							break;
						case 147:	// left quote
							strcpy(flush_buffer+flush_len, "“");
							flush_len+=3;
							break;
						case 148:	// right quote
							strcpy(flush_buffer+flush_len, "”");
							flush_len+=3;
							break;
						case 151:	// em-dash
							strcpy(flush_buffer+flush_len, "—");
							flush_len+=3;
							break;
					}
					
					FlushBuffer();
					current_font.SetEncoding(font_encoding);
					if (bitmap->Lock())
					{
						view->SetFont(&current_font);
						bitmap->Unlock();
					}	
				}
				else
				{
					flush_buffer[flush_len++] = a[i];
				}
				if (flush_len>=MAX_FLUSH_BUFFER-2) FlushBuffer();

				/* Increment the horizontal screen position by
				   the character width
				*/
				current_text_x += hugo_charwidth(a[i]);
			}
		}
		
		/* If we've passed the bottom of the window, align to the bottom edge */
		if (current_text_y > physical_windowbottom-lineheight)
		{
			int temp_lh = lineheight;
			FlushBuffer();
			lineheight = current_text_y - physical_windowbottom+lineheight;
			current_text_y -= lineheight;
			override_client_updating = true;
			hugo_scrollwindowup();
			override_client_updating = false;
			lineheight = temp_lh;
		}
	}
}


/* hugo_sendtoscrollback

	For copying printed text to the scrollback window buffer.
*/

void hugo_sendtoscrollback(char *a)
{
	int i;

	/* If the scrollback buffer is getting close to running out of room,
	   shift it backward, truncating at the head of the buffer
	*/
	if (scrollback_pos >= SCROLLBACK_SIZE-(int)strlen(a))
	{
		strcpy(scrollback_buffer, scrollback_buffer+2048);
		scrollback_pos -= 2048;
	}

	for (i=0; i<(int)strlen(a); i++)
	{
		if (a[i]=='\n')
			scrollback_buffer[scrollback_pos++] = '\n';
		else if ((unsigned char)a[i]>=' ')
			scrollback_buffer[scrollback_pos++] = a[i];
	}
	scrollback_buffer[scrollback_pos] = '\0';
}


void hugo_scrollwindowup()	/* one "text" line */
{
	int source_x, source_y, dest_x, dest_y, width, height;

#ifdef USE_TEXTBUFFER
	if (lineheight >= ((currentfont&PROP_FONT)?prop_lineheight:FIXEDLINEHEIGHT)/2)
		TB_Scroll();
#endif

	if (!bitmap->Lock()) return;
	
	/* If not in a window, just move the "virtual window" on the bitmap;
	 * we'll actually physically scroll it later with view->Update()
	 */
	if (!inwindow)
	{
		update_bgcolor = hugo_color(bgcolor);
		view->scroll_offset+=lineheight;

		/* Erase the leading/next line-plus in the scroll area */
		view->SetLowColor(current_back_color);
		BRect rect(window->current_rect);
		view->FillRect(BRect(0, rect.bottom+view->scroll_offset-lineheight+1,
			rect.right, rect.bottom+view->scroll_offset+lineheight+2),
			B_SOLID_LOW);

		goto FinishedScrolling;
	}

	/* Basically just copies a hunk of screen to an upward-
	   shifted position and fills in the screen behind/below it
	*/
	source_x = physical_windowleft;
	source_y = physical_windowtop+lineheight;
	dest_x = physical_windowleft;
	dest_y = physical_windowtop;
	width = physical_windowwidth;
	height = physical_windowheight;

	view->CopyBits(BRect(source_x, source_y, source_x+width-1, source_y+height-1),
		BRect(dest_x, dest_y, dest_x+width-1, dest_y+height-1));

	view->SetLowColor(current_back_color);
	view->FillRect(BRect(physical_windowleft, physical_windowbottom-lineheight,
		physical_windowright, physical_windowbottom), B_SOLID_LOW);
		
FinishedScrolling:
	if (!fast_scrolling)
	{
		view->Update(true);
	}
	
	bitmap->Unlock();
}


// We use precalc_charwidth because BFont::StringWidth() as used in
// hugo_charwidth() is the tightest bottleneck in the text-rendering
// process.
#define PRECALC_CHARS 52
char *precalc_charset[PRECALC_CHARS] = {
"A","B","C","D","E","F","G","H","I","J","K","L","M",
"N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
"a","b","c","d","e","f","g","h","i","j","k","l","m",
"n","o","p","q","r","s","t","u","v","w","x","y","z"
};
long int precalc_charset_lengths[PRECALC_CHARS] = {
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};
float precalc_charwidth[PRECALC_CHARS];
char prop_font_precalc_done = false;

void hugo_font(int f)
{
	static int last_font = -1;
	uint16 face = 0;
	font_height h;
	
	if (f==last_font) return;
	
	FlushBuffer();

	if (f & BOLD_FONT) face |= B_BOLD_FACE;
	if (f & ITALIC_FONT) face |= B_ITALIC_FACE;
	if (f & UNDERLINE_FONT)
	{
//		face |= B_UNDERSCORE_FACE;
		supposed_to_be_underlining = true;
	}
	else
		supposed_to_be_underlining = false;

	if (f & PROP_FONT)
	{
		current_font = prop_font;

		// Create the precalc_charwidth array for plain proportional font
		if (f!=last_font && f==PROP_FONT && !prop_font_precalc_done)
		{
			prop_font.GetStringWidths((const char **)precalc_charset, precalc_charset_lengths,
				PRECALC_CHARS, precalc_charwidth);
			prop_font_precalc_done = true;
		}
	}
	else
		current_font = fixed_font;

	current_font.SetFace(face);
	current_font.SetSpacing(B_BITMAP_SPACING);
//	current_font.SetEncoding(font_encoding);
	
	if (bitmap->Lock())
	{
		view->SetFont(&current_font);
		bitmap->Unlock();
	}	
	
	current_font.GetHeight(&h);
	lineheight = (int)ceil(h.ascent + h.descent + h.leading);
	text_descent = (int)ceil(h.descent);
	current_font.SetEncoding(MSG_UNICODE_UTF8);
	charwidth = (int)current_font.StringWidth("–");
	current_font.SetEncoding(font_encoding);
	if (f & PROP_FONT)
	{
		prop_lineheight = lineheight;
	}
	else
	{
		FIXEDCHARWIDTH = charwidth;
		FIXEDLINEHEIGHT = lineheight;
	}

	// May cause display problems with [MORE] prompt if the proportional
	// font is shorter than the fixed font
	if (lineheight < FIXEDLINEHEIGHT)
		prop_lineheight = lineheight = FIXEDLINEHEIGHT;
	
	last_font = f;
}

void hugo_settextcolor(int c)   /* foreground (print) color */
{
	static int last_text_color = -1;
	
	if (last_text_color==c) return;
	
	FlushBuffer();
	current_text_color = hugo_color(c);

	if (bitmap->Lock())
	{
		view->SetHighColor(current_text_color);
		bitmap->Unlock();
	}
	
	last_text_color = c;
}

void hugo_setbackcolor(int c)   /* background color */
{
	static int last_back_color = -1;
	
	if (last_back_color==c) return;
	
	FlushBuffer();
	current_back_color = hugo_color(c);

	if (bitmap->Lock())
	{
		view->SetLowColor(current_back_color);
		bitmap->Unlock();
	}
	
	last_back_color = c;
}

rgb_color hugo_color(int col)
{
	/* Color-setting functions should always pass the color through
	   hugo_color() in order to properly set default fore/background
	   colors
	*/

	static rgb_color c;

	if (col==16)      return def_fcolor;		// default foreground
	else if (col==17) return def_bgcolor;		// default background
	else if (col==18) return def_slfcolor;		// statusline foreground
	else if (col==19) return def_slbgcolor;		// statusline background
	else if (col==20) return hugo_color(fcolor);

	switch (col)	// red, green, blue
	{
		case 0:  MAKE_RGB(c, 0x00, 0x00, 0x00); break;	// black
		case 1:  MAKE_RGB(c, 0x00, 0x00, 0x7f); break;	// blue
		case 2:  MAKE_RGB(c, 0x00, 0x7f, 0x00); break;	// green
		case 3:  MAKE_RGB(c, 0x00, 0x7f, 0x7f); break;  // cyan
		case 4:  MAKE_RGB(c, 0x7f, 0x00, 0x00); break;	// red
		case 5:  MAKE_RGB(c, 0x7f, 0x00, 0x7f); break;	// magenta
		case 6:  MAKE_RGB(c, 0x7f, 0x5f, 0x00); break;  // brown
		case 7:  MAKE_RGB(c, 0xcf, 0xcf, 0xcf); break;	// white
		case 8:  MAKE_RGB(c, 0x3f, 0x3f, 0x3f); break;	// dark gray
		case 9:  MAKE_RGB(c, 0x00, 0x00, 0xff); break;	// light blue
		case 10: MAKE_RGB(c, 0x00, 0xff, 0x00); break;	// light green
		case 11: MAKE_RGB(c, 0x00, 0xff, 0xff); break;	// light cyan
		case 12: MAKE_RGB(c, 0xff, 0x00, 0x00); break;	// light red
		case 13: MAKE_RGB(c, 0xff, 0x00, 0xff); break;  // light magenta
		case 14: MAKE_RGB(c, 0xff, 0xff, 0x00); break;	// light yellow
		case 15: MAKE_RGB(c, 0xff, 0xff, 0xff); break;	// bright white
		default: MAKE_RGB(c, 0x00, 0x00, 0x00);
	}
	
	return c;
}


/* CHARACTER AND TEXT MEASUREMENT */

int hugo_charwidth(char a)
{
	if (a==FORCED_SPACE) a = ' ';
	
	if ((unsigned char)a >= ' ') /* alphanumeric characters */
	{
		/* proportional */
		if (currentfont & PROP_FONT)
		{
			char b[2];
			b[0] = a;
			b[1] = '\0';
			
			if ((smartformatting) &&
				(((unsigned char)a>=145 && (unsigned char)a<=148) ||
				(unsigned char)a==151))
			{
				int w = 0;
				current_font.SetEncoding(B_UNICODE_UTF8);
				
				switch ((unsigned char)a)
				{
					case 145:
						w = (int)current_font.StringWidth("‘", 3);
						break;
					case 146:
						w = (int)current_font.StringWidth("’", 3);
						break;
					case 147:
						w = (int)current_font.StringWidth("“", 3);
						break;
					case 148:
						w = (int)current_font.StringWidth("”", 3);
						break;
					case 151:
						w = (int)current_font.StringWidth("—", 3);
						break;
				}
				
				current_font.SetEncoding(font_encoding);
				return w;
			}
			
			// Some of the plain prop font has been precalculated
			if (currentfont==PROP_FONT)
			{
				if (a>='A' && a<='Z')
					return (int)precalc_charwidth[a-'A'];
				if (a>='a' && a<='z')
					return (int)precalc_charwidth[26+a-'a'];
			}
			
			return (int)current_font.StringWidth(b, 1);
		}

		/* fixed-width */
		else
			return FIXEDCHARWIDTH;
	}
	
	return 0;
}

int hugo_textwidth(char *a)
{
	int i, slen, len = 0;

	slen = strlen(a);

	for (i=0; i<slen; i++)
	{
		if (a[i]==COLOR_CHANGE) i+=2;
		else if (a[i]==FONT_CHANGE) i++;
		else
			len += hugo_charwidth(a[i]);
	}

	return len;
}

int hugo_strlen(char *a)
{
	int i, slen, len = 0;

	slen = strlen(a);

	for (i=0; i<slen; i++)
	{
		if (a[i]==COLOR_CHANGE) i+=2;
		else if (a[i]==FONT_CHANGE) i++;
		else len++;
	}

	return len;
}

}	// extern "C"
