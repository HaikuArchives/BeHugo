/*
	AboutBox.h
*/

class AboutBox : public BWindow
{
public:
	AboutBox(char *title);
	~AboutBox();
	virtual void Go();
	virtual bool QuitRequested();
	virtual void MessageReceived(BMessage *msg);
	virtual void DrawLogo(float scale);
};

class AboutBoxView : public BView
{
public:
	AboutBoxView(BRect frame, const char *name,
		uint32 resizing_mode, uint32 flags);
	~AboutBoxView();
	virtual void MouseDown(BPoint point);
	virtual void Draw(BRect rect);
};

// Internal messages
enum
{
	_ABOUTBOX_MSG_OK_BUTTON		= 'AMOB'
};
