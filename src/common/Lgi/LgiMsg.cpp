#include <stdio.h>
#include "Lgi.h"

#if defined(__GTK_H__) || defined(MAC) || defined(LGI_SDL)
#include "GTextLabel.h"
#include "GButton.h"

#if COCOA
#import <Cocoa/Cocoa.h>
#endif

class GMsgDlg : public GDialog
{
public:
	GMsgDlg()
	{
		RegisterHook(this, GKeyEvents);
	}

	bool OnViewKey(GView *v, GKey &k)
	{
		if (k.Down())
		{
			int Id = -1;
			switch (k.c16)
			{
				case 'y':
				case 'Y':
				{
					Id = IDYES;
					break;
				}
				case 'n':
				case 'N':
				{
					Id = IDNO;
					break;
				}
				case 'c':
				case 'C':
				{
					Id = IDCANCEL;
					break;
				}
				case 'o':
				case 'O':
				{
					Id = IDOK;
					break;
				}
			}
			
			if (Id >= 0)
			{
				GViewI *c = FindControl(Id);
				if (c)
				{
					EndModal(c->GetId());
				}
			}
		}
		
		return GDialog::OnViewKey(v, k);
	}
	
	int OnNotify(GViewI *Ctrl, int f)
	{
		switch (Ctrl->GetId())
		{
			case IDOK:
			case IDCANCEL:
			case IDYES:
			case IDNO:
			{
				EndModal(Ctrl->GetId());
				break;
			}
		}
		return 0;
	}
};
#endif

int LgiMsg(GViewI *Parent, const char *Str, const char *Title, int Type, ...)
{
	int Res = 0;
	va_list Arg;
	va_start(Arg, Type);
	GString Msg;
	Msg.Printf(Arg, Str);
	va_end(Arg);

	#if defined BEOS

	if (Str)
	{
		BAlert *Dlg = 0;
		
		switch (Type)
		{
			case MB_OK:
			{
				Dlg = new BAlert((Title) ? Title : "Message", Msg, "Ok");
				break;
			}
			case MB_YESNO:
			{
				Dlg = new BAlert((Title) ? Title : "Message", Msg, "Yes", "No", NULL, B_WIDTH_AS_USUAL, B_IDEA_ALERT);
				break;
			}
			case MB_YESNOCANCEL:
			{
				Dlg = new BAlert((Title) ? Title : "Message", Msg, "Yes", "No", "Cancel", B_WIDTH_AS_USUAL, B_IDEA_ALERT);
				break;
			}
			default:
			{
				LgiAssert(0);
				break;
			}
		}
		
		if (Dlg)
		{
			Res = Dlg->Go();
		}
		
		if (Res >= 0 &&
			(Type == MB_YESNO || Type == MB_YESNOCANCEL))
		{
			if (Res == 0)
				return IDYES;
			if (Res == 1)
				return IDNO;
			if (Res == 2)
				return IDCANCEL;
		}
	}
	
	#elif WINNATIVE

	if (Str)
	{
		if (LgiGetOs() == LGI_OS_WIN9X)
		{
			char *t = LgiToNativeCp(Title ? Title : (char*)"Message");
			char *m = LgiToNativeCp(Msg);
			Res = MessageBoxA(Parent ? Parent->Handle() : 0, m?m:"", t?t:"", Type);
			if (Res == 0)
			{
				auto Err = GetLastError();
				LgiAssert(!"MessageBoxA failed.");
			}
			DeleteArray(t);
			DeleteArray(m);
		}
		else
		{
			char16 *t = Utf8ToWide(Title ? Title : (char*)"Message");
			char16 *m = Utf8ToWide(Msg);
			Res = MessageBoxW(Parent ? Parent->Handle() : 0, m?m:L"", t?t:L"", Type);
			if (Res == 0)
			{
				auto Err = GetLastError();
				LgiAssert(!"MessageBoxW failed.");
			}
			DeleteArray(t);
			DeleteArray(m);
		}
	}
	
	#elif COCOA
	
	NSAlert *alert = [[NSAlert alloc] init];
	auto msg = Msg.NsStr();
	auto title = GString(Title).NsStr();
	[alert setMessageText:msg];
	[alert setInformativeText:title];
	switch (Type & ~MB_SYSTEMMODAL)
	{
		default:
		case MB_OK:
		{
			[alert addButtonWithTitle:@"Ok"];
			break;
		}
		case MB_OKCANCEL:
		{
			[alert addButtonWithTitle:@"Cancel"];
			[alert addButtonWithTitle:@"Ok"];
			break;
		}
		case MB_YESNO:
		{
			[alert addButtonWithTitle:@"No"];
			[alert addButtonWithTitle:@"Yes"];
			break;
		}
		case MB_YESNOCANCEL:
		{
			[alert addButtonWithTitle:@"Cancel"];
			[alert addButtonWithTitle:@"No"];
			[alert addButtonWithTitle:@"Yes"];
			break;
		}
	}
	[alert runModal];
	[msg release];
	[title release];
	
	#else // Lgi only controls (used for Linux + Mac)

	if (Str && LgiApp)
	{
		GMsgDlg Dlg;
		Dlg.SetParent(Parent);
		Dlg.Name((char*)(Title ? Title : "Message"));

		GTextLabel *Text = new GTextLabel(-1, 10, 10, -1, -1, Msg);
		Dlg.AddView(Text);

		List<GButton> Btns;
		#ifdef LGI_TOUCHSCREEN
		float Scale = 1.6f;
		#else
		float Scale = 1.0f;
		#endif		
		int BtnY = (int) (Scale * 20.0f);
		int BtnX = (int) (Scale * 70.0f);

		switch (Type & ~MB_SYSTEMMODAL)
		{
			default:
			case MB_OK:
			{
				Btns.Insert(new GButton(IDOK, 10, 40, BtnX, BtnY, "Ok"));
				break;
			}
			case MB_OKCANCEL:
			{
				Btns.Insert(new GButton(IDOK, 10, 40, BtnX, BtnY, "Ok"));
				Btns.Insert(new GButton(IDCANCEL, 10, 40, BtnX, BtnY, "Cancel"));
				break;
			}
			case MB_YESNO:
			{
				Btns.Insert(new GButton(IDYES, 10, 40, BtnX, BtnY, "Yes"));
				Btns.Insert(new GButton(IDNO, 10, 40, BtnX, BtnY, "No"));
				break;
			}
			case MB_YESNOCANCEL:
			{
				Btns.Insert(new GButton(IDYES, 10, 40, BtnX, BtnY, "Yes"));
				Btns.Insert(new GButton(IDNO, 10, 40, BtnX, BtnY, "No"));
				Btns.Insert(new GButton(IDCANCEL, 10, 40, BtnX, BtnY, "Cancel"));
				break;
			}
		}

		int BtnsX = (int) ((Btns.Length() * BtnX) + ((Btns.Length()-1) * 10));
		int MaxX = MAX(BtnsX, Text->X());
		GRect p(0, 0, MaxX + 30, Text->Y() + 30 + BtnY + LgiApp->GetMetric(LGI_MET_DECOR_Y) );
		Dlg.SetPos(p);
		Dlg.MoveToCenter();

		int x = (p.X() - BtnsX) / 2;
		int y = Text->Y() + 20;
		for (GButton *b=Btns.First(); b; b=Btns.Next())
		{
			GRect r(x, y, x+BtnX-1, y+BtnY-1);
			b->SetPos(r);
			Dlg.AddView(b);
			x += r.X() + 10;
		}
		
		return Dlg.DoModal();
	}
	
	#endif

	#if 0 // defined MAC (doesn't work reliably)

	if (Str)
	{
		SInt16 r;
		Str255 t;
		Str255 s;
		Str63 sYes = {3, 'Y', 'e', 's'}, sNo = {2, 'N', 'o'};
		AlertStdAlertParamRec p;

		c2p255(t, Title);
		c2p255(s, Buffer);
		p.movable = true;
		p.helpButton = false;
		p.filterProc = 0;
		p.defaultText = (ConstStringPtr)kAlertDefaultOtherText;
		p.cancelText = 0;
		p.otherText = 0;
		p.position = kWindowDefaultPosition;

		switch (Type)
		{
			default:
				printf("%s:%i - unhandled LgiMsg type %i\n", __FILE__, __LINE__, Type);
			case MB_OK:
				p.defaultButton = kAlertStdAlertOKButton;
				p.cancelButton = kAlertStdAlertOtherButton;
				break;
			case MB_OKCANCEL:
				p.defaultButton = kAlertStdAlertOKButton;
				p.cancelButton = kAlertStdAlertCancelButton;
				p.cancelText = (ConstStringPtr)kAlertDefaultCancelText;
				break;
			case MB_YESNO:
				p.defaultButton = kAlertStdAlertOKButton;
				p.cancelButton = kAlertStdAlertOtherButton;
				/*
				p.defaultButton = kAlertStdAlertOKButton;
				p.cancelButton = kAlertStdAlertCancelButton;
				p.defaultText = sYes;
				p.cancelText = sNo;
				*/
				break;
			case MB_YESNOCANCEL:
				p.defaultButton = kAlertStdAlertOKButton;
				p.defaultText = sYes;
				p.cancelButton = kAlertStdAlertCancelButton;
				p.cancelText = (ConstStringPtr)kAlertDefaultCancelText;
				p.otherText = sNo;
				break;
		}
		
		OSErr e = StandardAlert(kAlertPlainAlert, t, s, &p, &r);
		
		switch (Type)
		{
			default:
			case MB_OK:
				Res = IDOK;
				break;
			case MB_OKCANCEL:
				Res = (r == kAlertStdAlertOKButton) ? IDOK : IDCANCEL;
				break;
			case MB_YESNO:
				Res = (r == kAlertStdAlertOKButton) ? IDYES : IDNO;
				break;
			case MB_YESNOCANCEL:
				if (r == kAlertStdAlertOKButton)
					Res = IDYES;
				else if (r == kAlertStdAlertOtherButton)
					Res = IDNO;
				else
					Res = IDCANCEL;					
				break;
		}
	}

	#endif

	return Res;
}

