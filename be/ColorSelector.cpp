/*
	ColorSelector.cpp

	Implementation of a modal color selector dialog
	by Kent Tessman (c) 1999-2006
*/

// Standard dialog
#include <Button.h>
#include <MessageFilter.h>
#include <Screen.h>
#include <Window.h>
#include <View.h>

// Specific to ColorControl
#include <ColorControl.h>

#include "ColorSelector.h"

// Bad global mojo follows:

// Standard dialog setup
static ColorSelector *_dlg;
static ColorSelectorView *_dlg_view;

// Specific to ColorControl
void DrawSampleText();
#define COLORCONTROL_SCALE 5
#define PADDING 10
static BColorControl *color_control;
static int color_control_y;
static BButton *ok_button;
static BButton *cancel_button;
static BRect sample_rect;
static rgb_color fcolor, bgcolor;
static int which;


//-----------------------------------------------------------------------------
// ColorSelector
//-----------------------------------------------------------------------------

ColorSelector::ColorSelector(BWindow *window,
		const char *title, const char *cap, BMessage *msg,
		rgb_color *fore_color, rgb_color *back_color, int which_color)
	:BWindow(BRect(50, 50, 375, 150), title,
		B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_ASYNCHRONOUS_CONTROLS)
{
	// _ok_msg is the value supplied by the application to be
	// passed back to the window processing messages on an "OK"
	_ok_msg = msg;
	strcpy(_caption, cap);
	_target = window;
	
	// Add the view
	_dlg = this;
	_dlg_view = new ColorSelectorView(Bounds(), "ColorSelector View",
			B_FOLLOW_NONE, B_WILL_DRAW);
	AddChild(_dlg_view);
	_dlg_view->MakeFocus();
	
	// Set up the _dlg_view
	rgb_color clr = {216, 216, 216, 0};
	_dlg_view->SetViewColor(clr);
	
	// Get some metrics for be_plain_font
	font_height fh;
	be_plain_font->GetHeight(&fh);
	float height = ceil(fh.ascent + fh.descent + fh.leading);

	// Calculate the placement of the color control below the caption
	color_control_y = PADDING*2 + (int)height;
	
	// Set up the color control
	color_control = new BColorControl(BPoint(PADDING, color_control_y),
		B_CELLS_32x8, COLORCONTROL_SCALE, "ColorControl",
		new BMessage(_COLORSELECTOR_MSG_COLOR_CHANGED));
	fcolor = *fore_color;
	bgcolor = *back_color;
	which = which_color;
	color_control->SetValue(which ? (bgcolor):(fcolor));
	_dlg_view->AddChild(color_control);
	
	// Start figuring out the window resizing
	float new_x, new_y, new_width, new_height;
	color_control->GetPreferredSize(&new_width, &new_height);
	if (new_width < be_plain_font->StringWidth(_caption))
		new_width = be_plain_font->StringWidth(_caption);
	new_width += PADDING*2;
	
	// Set up the OK button
	float bottomrow_top = new_height + color_control_y + PADDING*2;
	float bottomrow_bottom = bottomrow_top + height + PADDING;
	float button_width = be_plain_font->StringWidth("Cancel") + PADDING*4;
	BRect ok_rect(new_width-PADDING-button_width, bottomrow_top,
		new_width-PADDING, bottomrow_bottom);
	ok_button = new BButton(ok_rect, "OK Button", "OK",
		new BMessage(_COLORSELECTOR_MSG_OK_BUTTON));
	ok_button->MakeDefault(true);
	_dlg_view->AddChild(ok_button);
	
	// Set up the Cancel button
	BRect cancel_rect(new_width-button_width*2-PADDING*2, bottomrow_top,
		new_width-button_width-PADDING*2, bottomrow_bottom);
	cancel_button = new BButton(cancel_rect, "Cancel Button", "Cancel",
		new BMessage(_COLORSELECTOR_MSG_CANCEL_BUTTON));
	_dlg_view->AddChild(cancel_button);
	
	// Set up the text color sample rectangle
	sample_rect = BRect(PADDING, bottomrow_top,
		be_plain_font->StringWidth("Sample Text")+PADDING*2, bottomrow_bottom);

	// Now resize the dialog as calculated...
	new_height = bottomrow_bottom + PADDING;
	ResizeTo(new_width, new_height);
	_dlg_view->ResizeTo(new_width, new_height);
	
	// ...and center it onscreen 
	BScreen screen(window);
	new_x = screen.Frame().right/2 - new_width/2;
	new_y = screen.Frame().bottom/2 - new_height;
	MoveTo(new_x, new_y);
}

ColorSelector::~ColorSelector()
{
}

bool ColorSelector::QuitRequested()
{
	return BWindow::QuitRequested();
}

void ColorSelector::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case _COLORSELECTOR_MSG_COLOR_CHANGED:
			if (which==0)
				fcolor = color_control->ValueAsColor();
			else
				bgcolor = color_control->ValueAsColor();
			DrawSampleText();
			break;
			
		case _COLORSELECTOR_MSG_OK_BUTTON:
		{
			rgb_color color = color_control->ValueAsColor();
			_ok_msg->AddData("color", B_RGB_COLOR_TYPE, &color, sizeof(rgb_color));
			_ok_msg->AddBool("OK", true);
			_target->PostMessage(_ok_msg);
			// fall through
		}
			
		case _COLORSELECTOR_MSG_CANCEL_BUTTON:
			Quit();
			break;
			
		default:
			BWindow::MessageReceived(msg);
	}
}


//-----------------------------------------------------------------------------
// ColorSelectorView
//-----------------------------------------------------------------------------

ColorSelectorView::ColorSelectorView(BRect frame, const char *name,
		uint32 resizing_mode, uint32 flags)
	:BView(frame, name, resizing_mode, flags)
{
}

ColorSelectorView::~ColorSelectorView()
{
}

void ColorSelectorView::Draw(BRect rect)
{
	// Draw the caption
	font_height fh;
	be_plain_font->GetHeight(&fh);
	DrawString(_dlg->_caption, BPoint(PADDING, PADDING + (int)ceil(fh.ascent + fh.leading)));
	
	// Draw the sample text in the currently selected color (or background)
	DrawSampleText();
}

void ColorSelectorView::KeyDown(const char *bytes, int32 numbytes)
{
	char key = bytes[0];
	
	if (key==B_ESCAPE)
		_dlg->PostMessage(_COLORSELECTOR_MSG_CANCEL_BUTTON);
	else
		BView::KeyDown(bytes, numbytes);
}

void DrawSampleText()
{
	if (_dlg->Lock())
	{
		// Save the default view colors
		rgb_color temp_fcolor = _dlg_view->HighColor();
		rgb_color temp_bgcolor = _dlg_view->LowColor();

		font_height fh;
		be_plain_font->GetHeight(&fh);
		float height = fh.ascent + fh.leading;

		_dlg_view->SetHighColor(fcolor);
		_dlg_view->SetLowColor(bgcolor);
		_dlg_view->FillRect(sample_rect, B_SOLID_LOW);
		_dlg_view->DrawString("Sample Text",
			BPoint(sample_rect.left + PADDING/2, sample_rect.top+PADDING/2+height));
		
		// Restore view original colors
		_dlg_view->SetHighColor(temp_fcolor);
		_dlg_view->SetLowColor(temp_bgcolor);
		
		_dlg->Unlock();
	}
}

