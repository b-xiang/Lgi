#ifndef _GBOX_H_
#define _GBOX_H_

/// This is a vertical or horizontal layout box, similar to the
/// old GSplitter control except it can handle any number of children
class LgiClass GBox : public GView
{
	struct GBoxPriv *d;

protected:

public:
	struct Spacer
	{
		GRect Pos; // Position on screen in view coords
		GColour Colour; // Colour of the spacer
		uint32 SizePx; // Size in pixels
	};

	GBox(int Id = -1);
	~GBox();

	const char *GetClass() { return "GBox"; }

	bool IsVertical();
	void SetVertical(bool v);
	Spacer *GetSpacer(int i);
	
	void OnCreate();
	void OnPaint(GSurface *pDC);
	void OnPosChange();
	void OnMouseClick(GMouse &m);
	bool OnViewMouse(GView *v, GMouse &m);
	void OnMouseMove(GMouse &m);
	bool Pour(GRegion &r);
	LgiCursor GetCursor(int x, int y);

	bool Serialize(GDom *Dom, const char *OptName, bool Write);	
	bool SetSize(int ViewIndex, GCss::Len Size);
};

#endif