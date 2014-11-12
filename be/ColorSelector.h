/*
	ColorSelector.h
*/

class ColorSelector : public BWindow
{
public:
	char _caption[256];

	ColorSelector(BWindow *window,
		const char *title, const char *caption, BMessage *msg,
		rgb_color *fore_color, rgb_color *back_color, int which_color);
	~ColorSelector();
	virtual bool QuitRequested();
	virtual void MessageReceived(BMessage *msg);

private:
	BMessage *_ok_msg;
	BWindow *_target;
};

class ColorSelectorView : public BView
{
public:
	ColorSelectorView(BRect frame, const char *name,
		uint32 resizing_mode, uint32 flags);
	~ColorSelectorView();
	virtual void Draw(BRect rect);
	virtual void KeyDown(const char *bytes, int32 numbytes);
};

// Internal messages
enum
{
	_COLORSELECTOR_MSG_COLOR_CHANGED	= 'CMCC',
	_COLORSELECTOR_MSG_OK_BUTTON		= 'CMOB',
	_COLORSELECTOR_MSG_CANCEL_BUTTON	= 'CMCB'
};
