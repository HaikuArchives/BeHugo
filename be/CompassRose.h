/*
	CompassRose.h
*/

class CompassRose : public BWindow
{
public:
	BPoint *_point;
	
	CompassRose(BPoint *point);
	~CompassRose();
	virtual void MessageReceived(BMessage *msg);
private:
};

class CompassRoseView : public BView
{
public:
	bool dragging;
	int selected, last_selected;
	BPoint dragpoint;

	CompassRoseView(BRect frame, const char *name,
		uint32 resizing_mode, uint32 flags);
	~CompassRoseView();
	virtual void Draw(BRect rect);
	virtual void KeyDown(const char *bytes, int32 numBytes);
	virtual void MouseDown(BPoint point);
	virtual void MouseUp(BPoint point);
	virtual void MouseMoved(BPoint point, uint32 transit, const BMessage *message);
	int GetGridSelection(int x, int y);
};
