#include "Lgi.h"
#include "GBitmap.h"
#include "GTableLayout.h"
#include "LgiRes.h"

//////////////////////////////////////////////////////////////////////////////////
#ifdef _MT
class GBitmapThread : public LThread
{
	GBitmap *Bmp;
	char *File;
	LThread **Owner;

public:
	GBitmapThread(GBitmap *bmp, char *file, LThread **owner) : LThread("GBitmapThread")
	{
		Bmp = bmp;
		File = NewStr(file);
		Owner = owner;
		if (Owner)
		{
			*Owner = this;
		}
		Run();
	}

	~GBitmapThread()
	{
		if (Owner)
		{
			*Owner = 0;
		}
		DeleteArray(File);
	}

	int Main()
	{
		if (Bmp)
		{
			GSurface *pDC = LoadDC(File);
			if (pDC)
			{
				Bmp->SetDC(pDC);

				while (!Bmp->Handle())
				{
					LgiSleep(10);
				}

				GRect r = Bmp->GetPos();
				r.Dimension(pDC->X()+4, pDC->Y()+4);
				Bmp->SetPos(r, true);
                Bmp->SendNotify();

				DeleteObj(pDC);
			}
		}

		return 0;
	}
};
#endif

GBitmap::GBitmap(int id, int x, int y, char *FileName, bool Async)
	: ResObject(Res_Bitmap)
{
	pDC = 0;
	pThread = 0;

	#if WINNATIVE
	SetStyle(WS_CHILD | WS_VISIBLE);
	#endif

	SetId(id);
	GRect r(0, 0, 4, 4);
	r.Offset(x, y);

	if (ValidStr(FileName))
	{
		#ifdef _MT
		if (!Async)
		{
		#endif
			pDC = LoadDC(FileName);
			if (pDC)
			{
				r.Dimension(pDC->X()+4, pDC->Y()+4);
			}
		#ifdef _MT
		}
		else
		{
			new GBitmapThread(this, FileName, &pThread);
		}
		#endif

	}

	SetPos(r);
	LgiResources::StyleElement(this);
}

GBitmap::~GBitmap()
{
	#ifdef _MT
	if (pThread)
	{
		pThread->Terminate();
		DeleteObj(pThread);
	}
	#endif
	DeleteObj(pDC);
}

void GBitmap::SetDC(GSurface *pNewDC)
{
	DeleteObj(pDC);
	if (pNewDC)
	{
		pDC = new GMemDC;
		if (pDC && pDC->Create(pNewDC->X(), pNewDC->Y(), GdcD->GetColourSpace()))
		{
			GColour Bk(LC_WORKSPACE, 24);
			if (GetCss())
			{
				GCss::ColorDef b = GetCss()->BackgroundColor();
				if (b.IsValid())
					Bk = b;
			}

			pDC->Colour(Bk);
			pDC->Rectangle();

			pDC->Op(GDC_ALPHA);
			pDC->Blt(0, 0, pNewDC);

			GRect r = GetPos();
			r.Dimension(pDC->X() + 4, pDC->Y() + 4);
			SetPos(r);
			Invalidate();

			for (GViewI *p = GetParent(); p; p = p->GetParent())
			{
				GTableLayout *Tl = dynamic_cast<GTableLayout*>(p);
				if (Tl)
				{
					Tl->InvalidateLayout();
					break;
				}
			}
			
			SendNotify(GNotifyTableLayout_Refresh);
		}
	}
}

GSurface *GBitmap::GetSurface()
{
	return pDC;
}

GMessage::Result GBitmap::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}

void GBitmap::OnPaint(GSurface *pScreen)
{
	GColour cBack = StyleColour(GCss::PropBackgroundColor, GColour(LC_MED, 24));

	GRect a = GetClient();
	pScreen->Colour(cBack);
	pScreen->Rectangle(&a);

	if (pDC)
	{
		pScreen->Op(GDC_ALPHA);
		pScreen->Blt(0, 0, pDC);
	}
}

void GBitmap::OnMouseClick(GMouse &m)
{
	if (!m.Down() && GetParent())
	{
		GDialog *Dlg = dynamic_cast<GDialog*>(GetParent());
		if (Dlg) Dlg->OnNotify(this, 0);
	}
}
