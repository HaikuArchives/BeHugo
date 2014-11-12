/*
	AboutBox.cpp

	Implementation of an About dialog
	by Kent Tessman (c) 1999-2002
*/

// Standard dialog
#include <Button.h>
#include <MessageFilter.h>
#include <Screen.h>
#include <TextView.h>
#include <Window.h>
#include <View.h>

// Needed for bitmap display
#include <Bitmap.h>
#include <TranslationUtils.h>

#include "AboutBox.h"

#include "behugo.h"
extern "C"
{
#include "heheader.h"
}

// Bad global mojo follows:

// Standard dialog setup
static AboutBox *_dlg;
static AboutBoxView *_dlg_view;

// Specific to AboutBox
#define BITMAP_NAME "hugologo.png"
#define BITMAP_WIDTH  200
#define BITMAP_HEIGHT 125
static BBitmap *aboutlogo_bitmap = NULL;
#define PADDING 20
static BTextView *textview;
static BButton *ok_button;
static float new_width;


//-----------------------------------------------------------------------------
// AboutBox
//-----------------------------------------------------------------------------

AboutBox::AboutBox(char *title)
	:BWindow(BRect(50, 50, 375, 375), title,
		B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_ASYNCHRONOUS_CONTROLS)
{
	// Add the view
	_dlg = this;
	_dlg_view = new AboutBoxView(Bounds(), "AboutBox View",
			B_FOLLOW_ALL, B_WILL_DRAW);
	AddChild(_dlg_view);
	_dlg_view->MakeFocus();
	
	// Set up the _dlg_view
	rgb_color clr = {216, 216, 216, 0};
	_dlg_view->SetViewColor(clr);
	
	// Get some metrics for be_plain_font
	font_height fh;
	be_plain_font->GetHeight(&fh);
	float lineheight = ceil(fh.ascent + fh.descent + fh.leading);

	// Create the BTextView that will hold the "About" text
	float tv_width = be_plain_font->StringWidth("M")*30;
	float tv_height = lineheight*9;
	float tv_x = PADDING, tv_y = BITMAP_HEIGHT+PADDING*2;
	BRect rect(tv_x, tv_y, tv_x+tv_width, tv_y+tv_height);
	BRect textrect = BRect(0, 0, tv_width, tv_height);
	textview = new BTextView(rect, "About textview", textrect,
		B_FOLLOW_NONE, B_WILL_DRAW);
	textview->SetViewColor(clr);
	textview->MakeEditable(false);
	textview->MakeSelectable(false);
	textview->SetWordWrap(true);
	char about_text[256];
	sprintf(about_text,
"Hugo for BeOS v%d.%d%s (%s)\n"
"Copyright © 2006 by Kent Tessman\n"
"The General Coffee Company Film Productions\n\n"
"MikMod sound system used under license."
"\n\nAll rights reserved.  Please see the Hugo License for details.",
		HEVERSION, HEREVISION, HEINTERIM, __DATE__);
	textview->SetText(about_text);	
	_dlg_view->AddChild(textview);

	// Figure out the new width of the About box proper
	new_width = tv_width+PADDING*1.5;
	float new_height = tv_y+tv_height+lineheight+PADDING*2;

	// Add the OK button
	float button_width = be_plain_font->StringWidth("OK")*5;
	float button_x = new_width/2 - button_width/2;
	
	BRect ok_rect(button_x, tv_y+tv_height+lineheight,
		button_x+button_width, tv_y+tv_height+lineheight*2);
	ok_button = new BButton(ok_rect, "OK Button", "OK",
		new BMessage(_ABOUTBOX_MSG_OK_BUTTON));
	ok_button->MakeDefault(true);
	_dlg_view->AddChild(ok_button);
	
	// Resize the About box window itself...
	ResizeTo(new_width, new_height);
	
	// ...and center it onscreen 
	BScreen screen(B_MAIN_SCREEN_ID);
	float new_x = screen.Frame().right/2 - new_width/2;
	float new_y = screen.Frame().bottom/2 - new_height/1.75;
	MoveTo(new_x, new_y);
}

AboutBox::~AboutBox()
{
	if (aboutlogo_bitmap)
	{
		delete aboutlogo_bitmap;
		aboutlogo_bitmap = NULL;
	}
}

void AboutBox::Go()
{
	Show();
	window->UpdateIfNeeded();	// fix menu damage
	
	// The bitmap logo is displayed here so we can animate it after
	// the dialog is displayed
	if (!aboutlogo_bitmap)
		aboutlogo_bitmap = BTranslationUtils::GetBitmap(B_RAW_TYPE, BITMAP_NAME);
	if (aboutlogo_bitmap)
	{
		for (float scale=0.05; scale<=1.0; scale+=0.025)
		{
			DrawLogo(scale);
			snooze(10000);
		}
	}
}

void AboutBox::DrawLogo(float scale)
{
	if (!aboutlogo_bitmap) return;
	
	BRect src = BRect(0, 0, BITMAP_WIDTH*scale, BITMAP_HEIGHT*scale);
	src.OffsetTo(50-50*scale, 50-50*scale);
	BRect dest = BRect(0, 0, BITMAP_WIDTH, BITMAP_HEIGHT);
	dest.OffsetTo(new_width/2-BITMAP_WIDTH/2, PADDING);
	if (Lock())
	{
		_dlg_view->DrawBitmap(aboutlogo_bitmap, src, dest);
		
		// Draw a rectangle around the logo
		BRect rect = BRect(0, 0, BITMAP_WIDTH+6, BITMAP_HEIGHT+6);
		rect.OffsetTo(new_width/2-BITMAP_WIDTH/2-3, PADDING-3);
		_dlg_view->StrokeRect(rect);
		
		Unlock();
	}
}

bool AboutBox::QuitRequested()
{
	return BWindow::QuitRequested();
}

void AboutBox::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case _ABOUTBOX_MSG_OK_BUTTON:
			Quit();
			break;
			
		default:
			BWindow::MessageReceived(msg);
	}
}


//-----------------------------------------------------------------------------
// AboutBoxView
//-----------------------------------------------------------------------------

AboutBoxView::AboutBoxView(BRect frame, const char *name,
		uint32 resizing_mode, uint32 flags)
	:BView(frame, name, resizing_mode, flags)
{
}

AboutBoxView::~AboutBoxView()
{
}

// On a MouseDown, redraw the bitmap-zooming business
void AboutBoxView::MouseDown(BPoint point)
{
	if ((point.x >= new_width/2-BITMAP_WIDTH/2 && point.x <= new_width/2+BITMAP_WIDTH/2)
		&& (point.y >= PADDING && point.y <= PADDING+BITMAP_HEIGHT))
	{
		for (float scale=0.05; scale<=1.0; scale+=0.025)
		{
			_dlg->DrawLogo(scale);
			snooze(10000);
		}
	}
}

void AboutBoxView::Draw(BRect rect)
{
	_dlg->DrawLogo(1.0);
}

