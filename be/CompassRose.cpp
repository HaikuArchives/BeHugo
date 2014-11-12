/*
	CompassRose.cpp

	The compass rose for the Hugo Engine
	by Kent Tessman (c) 1999-2006
*/

#include <Bitmap.h>
#include <Screen.h>
#include <TranslationUtils.h>
#include <Window.h>
#include <View.h>

#include "CompassRose.h"
#include "behugo.h"

extern "C"
{
#include "heheader.h"
}

#define COMPASS_WIDTH 252
#define COMPASS_HEIGHT 135
// Bad global mojo
CompassRose *compass_window;
CompassRoseView *compass_view;
BBitmap *compass_inactive, *compass_highlight;
BRect highlight_rect;

// From hemisc.c:
extern char during_player_input;

//-----------------------------------------------------------------------------
// CompassRose
//-----------------------------------------------------------------------------

CompassRose::CompassRose(BPoint *point)
	:BWindow(BRect(0, 0, COMPASS_WIDTH-1, COMPASS_HEIGHT-1), "CompassRose",
		B_BORDERED_WINDOW_LOOK, B_FLOATING_SUBSET_WINDOW_FEEL,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_NOT_CLOSABLE |
		B_AVOID_FOCUS)
{
	// Make this a floating window for the main window
	AddToSubset(window);
	
	// Add the view
	compass_window = this;
	BRect view_rect = Bounds();
	compass_view = new CompassRoseView(view_rect, "CompassRose View",
			B_FOLLOW_ALL, B_WILL_DRAW);
	AddChild(compass_view);
	
	// Load the inactive and highlight compass bitmaps
	compass_inactive = BTranslationUtils::GetBitmap(B_RAW_TYPE, "compass1.png");
	compass_highlight = BTranslationUtils::GetBitmap(B_RAW_TYPE, "compass2.png");
	highlight_rect = BRect(0, 0, 0, 0);
	
	// Make sure the compass window is sanely positioned
	BScreen screen(B_MAIN_SCREEN_ID);
	if (point->x > screen.Frame().right-COMPASS_WIDTH)
		point->x = screen.Frame().right-COMPASS_WIDTH;
	if (point->y > screen.Frame().bottom-COMPASS_HEIGHT)
		point->y = screen.Frame().bottom-COMPASS_HEIGHT;
	if (point->x < 0) point->x = 0;
	if (point->y < 0) point->y = 0;
	MoveTo(*point);
	
	// IMPORTANT:  This assumes that the BPoint supplied to the constructor
	// will be around for the life of the compass; it's so the app can keep
	// track of the window location
	_point = point;
}

CompassRose::~CompassRose()
{
	delete compass_inactive;
	delete compass_highlight;
}

void CompassRose::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		default:
			BWindow::MessageReceived(msg);
	}
}


//-----------------------------------------------------------------------------
// CompassRoseView
//-----------------------------------------------------------------------------

CompassRoseView::CompassRoseView(BRect frame, const char *name,
		uint32 resizing_mode, uint32 flags)
	:BView(frame, name, resizing_mode, flags)
{
	selected = -1;
	dragging = 0;
}

CompassRoseView::~CompassRoseView()
{
}

void CompassRoseView::Draw(BRect rect)
{
	if (compass_window->Lock())
	{
		DrawBitmap(compass_inactive);
		DrawBitmap(compass_highlight, highlight_rect, highlight_rect);
		compass_window->Unlock();
	}
}

// A grid of the boundaries for every direction hotspot on the compass
// rose:  12 directions (8 compass, up/down, int/out) * 4 coordinates in
// left, top, right, bottom order

static int grid[12][4] =
{
	// left, top, bottom, right
	{108, 5, 147, 45},	// N
	{148, 18, 182, 49},	// NE
	{150, 50, 210, 84},	// E
	{148, 85, 182, 115},	// SE
	{108, 85, 147, 126},	// S
	{65, 85, 107, 115},	// SW
	{40, 50, 105, 84},	// W
	{65, 18, 107, 49},	// NW
	{205, 15, 230, 55},	// in
	{205, 80, 232, 118},	// out
	{18, 15, 55, 40},	// up
	{18, 90, 55, 119},	// down
};

static char *compass_point[12] =
{
	"North", "Northeast", "East", "Southeast",
	"South", "Southwest", "West", "Northwest",
	"In", "Out", "Up", "Down"
};

int CompassRoseView::GetGridSelection(int x, int y)
{
	// Returns the number (0-12) of the selected area on the compass map
	// (as per the grid[] array).  Returns -1 if the point (x, y) is
	// not in a specific area.

	int i;

	for (i=0; i<12; i++)
	{
		if (x>=grid[i][0] && y>=grid[i][1] &&
			x<=grid[i][2] && y<=grid[i][3])
		{
			return i;
		}
	}

	return -1;
}

void CompassRoseView::KeyDown(const char *bytes, int32 numBytes)
{
	// Pass keystrokes on to the main window
	visible_view->KeyDown(bytes, numBytes);
}

// This little hack is to prevent MouseUp() from feeding a command when we're
// clicking on the Hugo window to activate it and get the compass by chance
bool just_got_focus = true;

void CompassRoseView::MouseDown(BPoint point)
{
	just_got_focus = false;
	
	// Keep tracking mouse events as long as the button is depressed,
	// even if we move out of the view
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	if (!dragging) dragpoint = point;
	if (GetGridSelection(point.x, point.y)==-1 || !during_player_input)
		dragging = true;
}

void CompassRoseView::MouseUp(BPoint point)
{
	if (just_got_focus==true) return;
	just_got_focus = true;
	
	if (during_player_input && selected>=0)
	{
		TypeCommand(compass_point[selected], true, true);
	}

	dragging = false;
}

void CompassRoseView::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
	selected = GetGridSelection(point.x, point.y);
	if (dragging || FindWord("north")==UNKNOWN_WORD) selected = -1;
	
	// If in a highlight region
	if (selected!=-1 && during_player_input)
	{
		if (selected==last_selected) return;
		
		highlight_rect = BRect(grid[selected][0], grid[selected][1],
			grid[selected][2], grid[selected][3]);
		Draw(highlight_rect);
	}
	// Or not in a highlight region (so redraw the inactive compass)
	else if (selected==-1)
	{
		highlight_rect = BRect(0, 0, 0, 0);
		Draw(Bounds());
	}
		
	if (dragging)
	{
		uint32 buttons;
		GetMouse(&point, &buttons, false);
		ConvertToScreen(&point);
		point-=dragpoint;
		compass_window->MoveTo(point);
		*compass_window->_point = point;
	}

	last_selected = selected;
}

