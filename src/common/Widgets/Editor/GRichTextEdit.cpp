#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GInput.h"
#include "GScrollBar.h"
#ifdef WIN32
#include <imm.h>
#endif
#include "GClipBoard.h"
#include "GDisplayString.h"
#include "GViewPriv.h"
#include "GCssTools.h"
#include "GFontCache.h"
#include "GUnicode.h"
#include "GDropFiles.h"

#include "GHtmlCommon.h"
#include "GHtmlParser.h"
#include "LgiRes.h"

#define DefaultCharset              "utf-8"

#define GDCF_UTF8					-1
#define POUR_DEBUG					0
#define PROFILE_POUR				0

#define ALLOC_BLOCK					64
#define IDC_VS						1000

#define PAINT_BORDER				Back
#define PAINT_AFTER_LINE			Back

#if !defined(WIN32) && !defined(toupper)
#define toupper(c)					(((c)>='a'&&(c)<='z') ? (c)-'a'+'A' : (c))
#endif

// static char SelectWordDelim[] = " \t\n.,()[]<>=?/\\{}\"\';:+=-|!@#$%^&*";

#include "GRichTextEditPriv.h"


//////////////////////////////////////////////////////////////////////
GRichTextEdit::GRichTextEdit(	int Id,
								int x, int y, int cx, int cy,
								GFontType *FontType)
	: ResObject(Res_Custom)
{
	// init vars
	GView::d->Css.Reset(new GRichTextPriv(this, &d));

	// setup window
	SetId(Id);
	SetTabStop(true);

	// default options
	#if WINNATIVE
	CrLf = true;
	SetDlgCode(DLGC_WANTALLKEYS);
	#else
	CrLf = false;
	#endif
	d->Padding(GCss::Len(GCss::LenPx, 4));
	
	#if 0
	d->BackgroundColor(GCss::ColorDef(GColour::Green));
	#else
	d->BackgroundColor(GCss::ColorDef(GCss::ColorRgb, Rgb24To32(LC_WORKSPACE)));
	#endif
	
	SetFont(SysFont);

	#if 0 // def _DEBUG
	Name("<html>\n"
		"<body>\n"
		"	This is some <b style='font-size: 20pt; color: green;'>bold text</b> to test with.<br>\n"
		"   A second line of text for testing.\n"
		"</body>\n"
		"</html>\n");
	#endif
}

GRichTextEdit::~GRichTextEdit()
{
	// 'd' is owned by the GView CSS autoptr.
}

bool GRichTextEdit::SetSpellCheck(GSpellCheck *sp)
{
	if ((d->SpellCheck = sp))
	{
		if (IsAttached())
			d->SpellCheck->EnumLanguages(AddDispatch());
		// else call that OnCreate
		
	}

	return d->SpellCheck != NULL;
}

/*
bool GRichTextEdit::NeedsCapability(const char *Name, const char *Param)
{
	for (unsigned i=0; i<d->NeedsCap.Length(); i++)
	{
		if (d->NeedsCap[i].Name.Equals(Name))
			return true;
	}

	d->NeedsCap.New().Set(Name, Param);
	Invalidate();
	return true;
}

void GRichTextEdit::OnInstall(CapsHash *Caps, bool Status)
{
	OnCloseInstaller();
}

void GRichTextEdit::OnCloseInstaller()
{
	d->NeedsCap.Length(0);
	Invalidate();
}
*/

bool GRichTextEdit::IsDirty()
{
	return d->Dirty;
}

void GRichTextEdit::IsDirty(bool dirty)
{
	if (d->Dirty ^ dirty)
	{
		d->Dirty = dirty;
	}
}

void GRichTextEdit::SetFixedWidthFont(bool i)
{
	if (FixedWidthFont ^ i)
	{
		if (i)
		{
			GFontType Type;
			if (Type.GetSystemFont("Fixed"))
			{
				GDocView::SetFixedWidthFont(i);
			}
		}

		OnFontChange();
		Invalidate();
	}
}

void GRichTextEdit::SetReadOnly(bool i)
{
	GDocView::SetReadOnly(i);

	#if WINNATIVE
	SetDlgCode(i ? DLGC_WANTARROWS : DLGC_WANTALLKEYS);
	#endif
}

GRect GRichTextEdit::GetArea(RectType Type)
{
	return	Type >= ContentArea &&
			Type <= MaxArea
			?
			d->Areas[Type]
			:
			GRect(0, 0, -1, -1);
}

bool GRichTextEdit::ShowStyleTools()
{
	return d->ShowTools;
}

void GRichTextEdit::ShowStyleTools(bool b)
{
	if (d->ShowTools ^ b)
	{
		d->ShowTools = b;
		Invalidate();
	}
}

void GRichTextEdit::SetTabSize(uint8 i)
{
	TabSize = limit(i, 2, 32);
	OnFontChange();
	OnPosChange();
	Invalidate();
}

void GRichTextEdit::SetWrapType(LDocWrapType i)
{
	GDocView::SetWrapType(i);

	OnPosChange();
	Invalidate();
}

GFont *GRichTextEdit::GetFont()
{
	return d->Font;
}

void GRichTextEdit::SetFont(GFont *f, bool OwnIt)
{
	if (!f)
		return;
	
	if (OwnIt)
	{
		d->Font.Reset(f);
	}
	else if (d->Font.Reset(new GFont))
	{		
		*d->Font = *f;
		d->Font->Create(NULL, 0, 0);
	}
	
	OnFontChange();
}

void GRichTextEdit::OnFontChange()
{
}

void GRichTextEdit::PourText(ssize_t Start, ssize_t Length /* == 0 means it's a delete */)
{
}

void GRichTextEdit::PourStyle(ssize_t Start, ssize_t EditSize)
{
}

bool GRichTextEdit::Insert(int At, char16 *Data, int Len)
{
	return false;
}

bool GRichTextEdit::Delete(int At, int Len)
{
	return false;
}

bool GRichTextEdit::DeleteSelection(char16 **Cut)
{
	AutoTrans t(new GRichTextPriv::Transaction);
	if (!d->DeleteSelection(t, Cut))
		return false;

	return d->AddTrans(t);
}

int64 GRichTextEdit::Value()
{
	char *n = Name();
	#ifdef _MSC_VER
	return (n) ? _atoi64(n) : 0;
	#else
	return (n) ? atoll(n) : 0;
	#endif
}

void GRichTextEdit::Value(int64 i)
{
	char Str[32];
	sprintf_s(Str, sizeof(Str), LPrintfInt64, i);
	Name(Str);
}

bool GRichTextEdit::GetFormattedContent(const char *MimeType, GString &Out, GArray<ContentMedia> *Media)
{
	if (!MimeType || _stricmp(MimeType, "text/html"))
		return false;

	if (!d->ToHtml(Media))
		return false;
	
	Out = d->UtfNameCache;
	return true;
}

char *GRichTextEdit::Name()
{
	d->ToHtml();
	return d->UtfNameCache;
}

const char *GRichTextEdit::GetCharset()
{
	return d->Charset;
}

void GRichTextEdit::SetCharset(const char *s)
{
	d->Charset = s;
}

bool GRichTextEdit::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = LgiStringToDomProp(Name);
	switch (p)
	{
		case HtmlImagesLinkCid:
		{
			Value = d->HtmlLinkAsCid;
			break;
		}
		case SpellCheckLanguage:
		{
			Value = d->SpellLang.Get();
			break;
		}
		case SpellCheckDictionary:
		{
			Value = d->SpellDict.Get();
			break;
		}
		default:
			return false;
	}
	
	return true;
}

bool GRichTextEdit::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = LgiStringToDomProp(Name);
	switch (p)
	{
		case HtmlImagesLinkCid:
		{
			d->HtmlLinkAsCid = Value.CastInt32() != 0;
			break;
		}
		case SpellCheckLanguage:
		{
			d->SpellLang = Value.Str();
			break;
		}
		case SpellCheckDictionary:
		{
			d->SpellDict = Value.Str();
			break;
		}
		default:
			return false;
	}
	
	return true;
}

static GHtmlElement *FindElement(GHtmlElement *e, HtmlTag TagId)
{
	if (e->TagId == TagId)
		return e;
		
	for (unsigned i = 0; i < e->Children.Length(); i++)
	{
		GHtmlElement *c = FindElement(e->Children[i], TagId);
		if (c)
			return c;
	}
	
	return NULL;
}

void GRichTextEdit::OnAddStyle(const char *MimeType, const char *Styles)
{
	if (d->CreationCtx)
	{
		d->CreationCtx->StyleStore.Parse(Styles);
	}
}

bool GRichTextEdit::Name(const char *s)
{
	d->Empty();
	d->OriginalText = s;
	
	GHtmlElement Root(NULL);

	if (!d->CreationCtx.Reset(new GRichTextPriv::CreateContext(d)))
		return false;

	if (!d->GHtmlParser::Parse(&Root, s))
		return d->Error(_FL, "Failed to parse HTML.");
	
	GHtmlElement *Body = FindElement(&Root, TAG_BODY);
	if (!Body)
		Body = &Root;

	bool Status = d->FromHtml(Body, *d->CreationCtx);
	
	// d->DumpBlocks();
	
	if (!d->Blocks.Length())
	{
		d->EmptyDoc();
	}
	else
	{
		// Clear out any zero length blocks.
		for (unsigned i=0; i<d->Blocks.Length(); i++)
		{
			GRichTextPriv::Block *b = d->Blocks[i];
			if (b->Length() == 0)
			{
				d->Blocks.DeleteAt(i--, true);
				DeleteObj(b);
			}
		}
	}
	
	if (Status)
		SetCursor(0, false);
	
	Invalidate();
	
	return Status;
}

char16 *GRichTextEdit::NameW()
{
	d->WideNameCache.Reset(Utf8ToWide(Name()));
	return d->WideNameCache;
}

bool GRichTextEdit::NameW(const char16 *s)
{
	GAutoString a(WideToUtf8(s));
	return Name(a);
}

char *GRichTextEdit::GetSelection()
{
	if (!HasSelection())
		return NULL;

	GArray<char16> Text;
	if (!d->GetSelection(&Text, NULL))
		return NULL;
	
	return WideToUtf8(&Text[0]);
}

bool GRichTextEdit::HasSelection()
{
	return d->Selection.Get() != NULL;
}

void GRichTextEdit::SelectAll()
{
	AutoCursor Start(new BlkCursor(d->Blocks.First(), 0, 0));
	d->SetCursor(Start);

	GRichTextPriv::Block *Last = d->Blocks.Length() ? d->Blocks.Last() : NULL;
	if (Last)
	{
		AutoCursor End(new BlkCursor(Last, Last->Length(), Last->GetLines()-1));
		d->SetCursor(End, true);
	}
	else d->Selection.Reset();

	Invalidate();
}

void GRichTextEdit::UnSelectAll()
{
	bool Update = HasSelection();

	if (Update)
	{
		d->Selection.Reset();
		Invalidate();
	}
}

void GRichTextEdit::SetStylePrefix(GString s)
{
	d->SetPrefix(s);
}

size_t GRichTextEdit::GetLines()
{
	uint32 Count = 0;
	for (size_t i=0; i<d->Blocks.Length(); i++)
	{
		GRichTextPriv::Block *b = d->Blocks[i];
		Count += b->GetLines();
	}
	return Count;
}

int GRichTextEdit::GetLine()
{
	if (!d->Cursor)
		return -1;

	ssize_t Idx = d->Blocks.IndexOf(d->Cursor->Blk);
	if (Idx < 0)
	{
		LgiAssert(0);
		return -1;
	}

	int Count = 0;
	
	// Count lines in blocks before the cursor...
	for (int i=0; i<Idx; i++)
	{
		GRichTextPriv::Block *b = d->Blocks[i];
		Count += b->GetLines();
	}

	// Add the lines in the cursor's block...
	if (d->Cursor->LineHint)
	{
		Count += d->Cursor->LineHint;
	}
	else
	{
		GArray<int> BlockLine;
		if (d->Cursor->Blk->OffsetToLine(d->Cursor->Offset, NULL, &BlockLine))
			Count += BlockLine.First();
		else
		{
			// Hmmm...
			LgiAssert(!"Can't find block line.");
			return -1;
		}
	}


	return Count;
}

void GRichTextEdit::SetLine(int i)
{
	int Count = 0;
	
	// Count lines in blocks before the cursor...
	for (int i=0; i<(int)d->Blocks.Length(); i++)
	{
		GRichTextPriv::Block *b = d->Blocks[i];
		int Lines = b->GetLines();
		if (i >= Count && i < Count + Lines)
		{
			int BlockLine = i - Count;
			int Offset = b->LineToOffset(BlockLine);
			if (Offset >= 0)
			{
				AutoCursor c(new BlkCursor(b, Offset, BlockLine));
				d->SetCursor(c);
				break;
			}
		}		
		Count += Lines;
	}
}

void GRichTextEdit::GetTextExtent(int &x, int &y)
{
	x = d->DocumentExtent.x;
	y = d->DocumentExtent.y;
}

bool GRichTextEdit::GetLineColumnAtIndex(GdcPt2 &Pt, ssize_t Index)
{
	ssize_t Offset = -1;
	int BlockLines = -1;
	GRichTextPriv::Block *b = d->GetBlockByIndex(Index, &Offset, NULL, &BlockLines);
	if (!b)
		return false;

	int Cols;
	GArray<int> Lines;
	if (b->OffsetToLine(Offset, &Cols, &Lines))
		return false;
	
	Pt.x = Cols;
	Pt.y = BlockLines + Lines.First();
	return true;
}

ssize_t GRichTextEdit::GetCaret(bool Cur)
{
	if (!d->Cursor)
		return -1;
		
	int CharPos = 0;
	for (unsigned i=0; i<d->Blocks.Length(); i++)
	{
		GRichTextPriv::Block *b = d->Blocks[i];
		if (d->Cursor->Blk == b)
			return CharPos + d->Cursor->Offset;
		CharPos += b->Length();
	}
	
	LgiAssert(!"Cursor block not found.");
	return -1;
}

bool GRichTextEdit::IndexAt(int x, int y, ssize_t &Off, int &LineHint)
{
	GdcPt2 Doc = d->ScreenToDoc(x, y);
	Off = d->HitTest(Doc.x, Doc.y, LineHint);
	return Off >= 0;
}

ssize_t GRichTextEdit::IndexAt(int x, int y)
{
	ssize_t Idx;
	int Line;
	if (!IndexAt(x, y, Idx, Line))
		return -1;
	return Idx;
}

void GRichTextEdit::SetCursor(int i, bool Select, bool ForceFullUpdate)
{
	ssize_t Offset = -1;
	GRichTextPriv::Block *Blk = d->GetBlockByIndex(i, &Offset);
	if (Blk)
	{
		AutoCursor c(new BlkCursor(Blk, Offset, -1));
		if (c)
			d->SetCursor(c, Select);
	}
}

bool GRichTextEdit::Cut()
{
	if (!HasSelection())
		return false;

	char16 *Txt = NULL;
	if (!DeleteSelection(&Txt))
		return false;

	bool Status = true;
	if (Txt)
	{
		GClipBoard Cb(this);
		Status = Cb.TextW(Txt);
		DeleteArray(Txt);
	}

	SendNotify(GNotifyDocChanged);

	return Status;
}

bool GRichTextEdit::Copy()
{
	if (!HasSelection())
		return false;

	GArray<char16> PlainText;
	GAutoString Html;
	if (!d->GetSelection(&PlainText, &Html))
		return false;

	// Put on the clipboard
	GClipBoard Cb(this);
	bool Status = Cb.TextW(PlainText.AddressOf());
	Cb.Html(Html, false);
	return Status;
}

bool GRichTextEdit::Paste()
{
	GString Html;
	GAutoWString Text;
	GAutoPtr<GSurface> Img;

	{
		GClipBoard Cb(this);	
		Html = Cb.Html();
		if (!Html)
		{
			Text.Reset(NewStrW(Cb.TextW()));
			if (!Text)
				Img.Reset(Cb.Bitmap());
		}
	}

	if (!Html && !Text && !Img)
		return false;
	
	if (!d->Cursor ||
		!d->Cursor->Blk)
	{
		LgiAssert(0);
		return false;
	}

	AutoTrans Trans(new GRichTextPriv::Transaction);						

	if (HasSelection())
	{
		if (!d->DeleteSelection(Trans, NULL))
			return false;
	}

	if (Html)
	{
		GHtmlElement Root(NULL);

		if (!d->CreationCtx.Reset(new GRichTextPriv::CreateContext(d)))
			return false;

		if (!d->GHtmlParser::Parse(&Root, Html))
			return d->Error(_FL, "Failed to parse HTML.");
	
		GHtmlElement *Body = FindElement(&Root, TAG_BODY);
		if (!Body)
			Body = &Root;

		if (d->Cursor)
		{
			auto *b = d->Cursor->Blk;
			ssize_t BlkIdx = d->Blocks.IndexOf(b);
			GRichTextPriv::Block *After = NULL;
			ssize_t AddIndex = BlkIdx;;

			// Split 'b' to make room for pasted objects
			if (d->Cursor->Offset > 0)
			{
				After = b->Split(Trans, d->Cursor->Offset);
				AddIndex = BlkIdx+1;									
			}
			// else Insert before cursor block

			auto *PastePoint = new GRichTextPriv::TextBlock(d);
			if (PastePoint)
			{
				d->Blocks.AddAt(AddIndex++, PastePoint);
				if (After) d->Blocks.AddAt(AddIndex++, After);

				d->CreationCtx->Tb = PastePoint;
				d->FromHtml(Body, *d->CreationCtx);
			}
		}
	}
	else if (Text)
	{
		GAutoPtr<uint32,true> Utf32((uint32*)LgiNewConvertCp("utf-32", Text, LGI_WideCharset));
		ptrdiff_t Len = Strlen(Utf32.Get());
		if (!d->Cursor->Blk->AddText(Trans, d->Cursor->Offset, Utf32.Get(), (int)Len))
		{
			LgiAssert(0);
			return false;
		}

		d->Cursor->Offset += Len;
		d->Cursor->LineHint = -1;
	}
	else if (Img)
	{
		GRichTextPriv::Block *b = d->Cursor->Blk;
		ssize_t BlkIdx = d->Blocks.IndexOf(b);
		GRichTextPriv::Block *After = NULL;
		ssize_t AddIndex;
		
		LgiAssert(BlkIdx >= 0);

		// Split 'b' to make room for the image
		if (d->Cursor->Offset > 0)
		{
			After = b->Split(Trans, d->Cursor->Offset);
			AddIndex = BlkIdx+1;									
		}
		else
		{
			// Insert before..
			AddIndex = BlkIdx;
		}

		GRichTextPriv::ImageBlock *ImgBlk = new GRichTextPriv::ImageBlock(d);
		if (ImgBlk)
		{
			d->Blocks.AddAt(AddIndex++, ImgBlk);
			if (After)
				d->Blocks.AddAt(AddIndex++, After);

			Img->MakeOpaque();
			ImgBlk->SetImage(Img);
			
			AutoCursor c(new BlkCursor(ImgBlk, 1, -1));
			d->SetCursor(c);			
		}
	}

	Invalidate();
	SendNotify(GNotifyDocChanged);

	return d->AddTrans(Trans);
}

bool GRichTextEdit::ClearDirty(bool Ask, char *FileName)
{
	if (1 /*dirty*/)
	{
		int Answer = (Ask) ? LgiMsg(this,
									LgiLoadString(L_TEXTCTRL_ASK_SAVE, "Do you want to save your changes to this document?"),
									LgiLoadString(L_TEXTCTRL_SAVE, "Save"),
									MB_YESNOCANCEL) : IDYES;
		if (Answer == IDYES)
		{
			GFileSelect Select;
			Select.Parent(this);
			if (!FileName &&
				Select.Save())
			{
				FileName = Select.Name();
			}

			Save(FileName);
		}
		else if (Answer == IDCANCEL)
		{
			return false;
		}
	}

	return true;
}

bool GRichTextEdit::Open(const char *Name, const char *CharSet)
{
	bool Status = false;
	GFile f;

	if (f.Open(Name, O_READ|O_SHARE))
	{
		size_t Bytes = (size_t)f.GetSize();
		SetCursor(0, false);
		
		char *c8 = new char[Bytes + 4];
		if (c8)
		{
			if (f.Read(c8, (int)Bytes) == Bytes)
			{
				char *DataStart = c8;

				c8[Bytes] = 0;
				c8[Bytes+1] = 0;
				c8[Bytes+2] = 0;
				c8[Bytes+3] = 0;
				
				if ((uchar)c8[0] == 0xff && (uchar)c8[1] == 0xfe)
				{
					// utf-16
					if (!CharSet)
					{
						CharSet = "utf-16";
						DataStart += 2;
					}
				}
				
			}

			DeleteArray(c8);
		}
		else
		{
		}

		Invalidate();
	}

	return Status;
}

bool GRichTextEdit::Save(const char *FileName, const char *CharSet)
{
	GFile f;
	if (!FileName || !f.Open(FileName, O_WRITE))
		return false;

	f.SetSize(0);
	char *Nm = Name();
	if (!Nm)
		return false;

	size_t Len = strlen(Nm);
	return f.Write(Nm, (int)Len) == Len;
}

void GRichTextEdit::UpdateScrollBars(bool Reset)
{
	if (VScroll)
	{
		//GRect Before = GetClient();

	}
}

bool GRichTextEdit::DoCase(bool Upper)
{
	if (!HasSelection())
		return false;

	bool Cf = d->CursorFirst();
	GRichTextPriv::BlockCursor *Start = Cf ? d->Cursor : d->Selection;
	GRichTextPriv::BlockCursor *End = Cf ? d->Selection : d->Cursor;
	if (Start->Blk == End->Blk)
	{
		// In the same block...
		ssize_t Len = End->Offset - Start->Offset;
		Start->Blk->DoCase(NoTransaction, Start->Offset, Len, Upper);
	}
	else
	{
		// Multi-block delete...

		// 1) Delete all the content to the end of the first block
		ssize_t StartLen = Start->Blk->Length();
		if (Start->Offset < StartLen)
			Start->Blk->DoCase(NoTransaction, Start->Offset, StartLen - Start->Offset, Upper);

		// 2) Delete any blocks between 'Start' and 'End'
		ssize_t i = d->Blocks.IndexOf(Start->Blk);
		if (i >= 0)
		{
			for (++i; d->Blocks[i] != End->Blk && i < (int)d->Blocks.Length(); )
			{
				GRichTextPriv::Block *b = d->Blocks[i];
				b->DoCase(NoTransaction, 0, -1, Upper);
			}
		}
		else
		{
			LgiAssert(0);
			return false;
		}

		// 3) Delete any text up to the Cursor in the 'End' block
		End->Blk->DoCase(NoTransaction, 0, End->Offset, Upper);
	}

	// Update the screen
	d->Dirty = true;
	Invalidate();
	
	return true;
}

bool GRichTextEdit::DoGoto()
{
	GInput Dlg(this, "", LgiLoadString(L_TEXTCTRL_GOTO_LINE, "Goto line:"), "Text");
	if (Dlg.DoModal() == IDOK)
	{
		GString s = Dlg.GetStr();
		int64 i = s.Int();
		if (i >= 0)
			SetLine((int)i);
	}

	return true;
}

GDocFindReplaceParams *GRichTextEdit::CreateFindReplaceParams()
{
	return new GDocFindReplaceParams3;
}

void GRichTextEdit::SetFindReplaceParams(GDocFindReplaceParams *Params)
{
	if (Params)
	{
	}
}

bool GRichTextEdit::DoFindNext()
{
	return false;
}

bool
RichText_FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User)
{
	return ((GRichTextEdit*)User)->OnFind(Dlg);
}

////////////////////////////////////////////////////////////////////////////////// FIND
bool GRichTextEdit::DoFind()
{
	GArray<char16> Sel;
	if (HasSelection())
		d->GetSelection(&Sel, NULL);
	GAutoString u(Sel.Length() ? WideToUtf8(&Sel.First()) : NULL);
	GFindDlg Dlg(this, u, RichText_FindCallback, this);
	Dlg.DoModal();	
	Focus(true);
	return false;
}

bool GRichTextEdit::OnFind(GFindReplaceCommon *Params)
{
	if (!Params || !d->Cursor)
	{
		LgiAssert(0);
		return false;
	}
	
	GAutoPtr<uint32,true> w((uint32*)LgiNewConvertCp("utf-32", Params->Find, "utf-8", Params->Find.Length()));
	ssize_t Idx = d->Blocks.IndexOf(d->Cursor->Blk);
	if (Idx < 0)
	{
		LgiAssert(0);
		return false;
	}

	for (unsigned n = 0; n < d->Blocks.Length(); n++)
	{
		ssize_t i = Idx + n;
		GRichTextPriv::Block *b = d->Blocks[i % d->Blocks.Length()];
		ssize_t At = n ? 0 : d->Cursor->Offset;
		ssize_t Result = b->FindAt(At, w, Params);
		if (Result >= At)
		{
			ptrdiff_t Len = Strlen(w.Get());
			AutoCursor Sel(new BlkCursor(b, Result, -1));
			d->SetCursor(Sel, false);

			AutoCursor Cur(new BlkCursor(b, Result + Len, -1));
			return d->SetCursor(Cur, true);
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////// REPLACE
bool GRichTextEdit::DoReplace()
{
	return false;
}

bool GRichTextEdit::OnReplace(GFindReplaceCommon *Params)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////////////
void GRichTextEdit::SelectWord(size_t From)
{
	int BlockIdx;
	ssize_t Start, End;
	GRichTextPriv::Block *b = d->GetBlockByIndex(From, &Start, &BlockIdx);
	if (!b)
		return;

	GArray<uint32> Txt;
	if (!b->CopyAt(0, b->Length(), &Txt))
		return;

	End = Start;
	while (Start > 0 &&
			!IsWordBreakChar(Txt[Start-1]))
		Start--;
	
	while
	(
		End < b->Length()
		&&
		(
			End == Txt.Length()
			||
			!IsWordBreakChar(Txt[End])
		)
	)
		End++;

	AutoCursor c(new BlkCursor(b, Start, -1));
	d->SetCursor(c);
	c.Reset(new BlkCursor(b, End, -1));
	d->SetCursor(c, true);
}

bool GRichTextEdit::OnMultiLineTab(bool In)
{
	return false;
}

void GRichTextEdit::OnSetHidden(int Hidden)
{
}

void GRichTextEdit::OnPosChange()
{
	static bool Processing = false;

	if (!Processing)
	{
		Processing = true;
		GLayout::OnPosChange();

		// GRect c = GetClient();
		Processing = false;
	}
}

int GRichTextEdit::WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState)
{
	const char *Fd = LGI_FileDropFormat;

	for (char *s = Formats.First(); s; )
	{
		if (!_stricmp(s, Fd) ||
			!_stricmp(s, "UniformResourceLocatorW"))
		{
			s = Formats.Next();
		}
		else
		{
			// LgiTrace("Ignoring format '%s'\n", s);
			Formats.Delete(s);
			DeleteArray(s);
			s = Formats.Current();
		}
	}

	return Formats.Length() ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

int GRichTextEdit::OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState)
{
	int Effect = DROPEFFECT_NONE;

	for (unsigned i=0; i<Data.Length(); i++)
	{
		GDragData &dd = Data[i];
		if (dd.IsFileDrop() && d->Areas[ContentArea].Overlap(Pt.x, Pt.y))
		{
			int AddIndex = -1;
			GdcPt2 TestPt(	Pt.x - d->Areas[ContentArea].x1,
							Pt.y - d->Areas[ContentArea].y1);

			GDropFiles Df(dd);
			for (unsigned n=0; n<Df.Length(); n++)
			{
				const char *f = Df[n];
				char Mt[128];
				if (LGetFileMimeType(f, Mt, sizeof(Mt)) &&
					!_strnicmp(Mt, "image/", 6))
				{
					if (AddIndex < 0)
					{
						int LineHint = -1;
						ssize_t Idx = d->HitTest(TestPt.x, TestPt.y, LineHint);
						if (Idx >= 0)
						{
							ssize_t BlkOffset;
							int BlkIdx;
							GRichTextPriv::Block *b = d->GetBlockByIndex(Idx, &BlkOffset, &BlkIdx);
							if (b)
							{
								GRichTextPriv::Block *After = NULL;

								// Split 'b' to make room for the image
								if (BlkOffset > 0)
								{
									After = b->Split(NoTransaction, BlkOffset);
									AddIndex = BlkIdx+1;									
								}
								else
								{
									// Insert before..
									AddIndex = BlkIdx;
								}

								GRichTextPriv::ImageBlock *ImgBlk = new GRichTextPriv::ImageBlock(d);
								if (ImgBlk)
								{
									d->Blocks.AddAt(AddIndex++, ImgBlk);
									if (After)
										d->Blocks.AddAt(AddIndex++, After);

									ImgBlk->Load(f);
									Effect = DROPEFFECT_COPY;
								}
							}
						}
					}
				}
			}

			break;
		}
	}

	if (Effect != DROPEFFECT_NONE)
	{
		Invalidate();
		SendNotify(GNotifyDocChanged);
	}

	return Effect;
}

void GRichTextEdit::OnCreate()
{
	SetWindow(this);
	DropTarget(true);

	if (Focus())
		SetPulse(RTE_PULSE_RATE);

	if (d->SpellCheck)
		d->SpellCheck->EnumLanguages(AddDispatch());
}

void GRichTextEdit::OnEscape(GKey &K)
{
}

bool GRichTextEdit::OnMouseWheel(double l)
{
	if (VScroll)
	{
		VScroll->Value(VScroll->Value() + (int64)l);
		Invalidate();
	}
	
	return true;
}

void GRichTextEdit::OnFocus(bool f)
{
	Invalidate();
	SetPulse(f ? RTE_PULSE_RATE : -1);
}

ssize_t GRichTextEdit::HitTest(int x, int y)
{
	int Line = -1;
	return d->HitTest(x, y, Line);
}

void GRichTextEdit::Undo()
{
	if (d->UndoPos > 0)
		d->SetUndoPos(d->UndoPos - 1);
}

void GRichTextEdit::Redo()
{
	if (d->UndoPos < (int)d->UndoQue.Length())
		d->SetUndoPos(d->UndoPos + 1);
}

#ifdef _DEBUG
class NodeView : public GWindow
{
public:
	GTree *Tree;
	
	NodeView(GViewI *w)
	{
		GRect r(0, 0, 500, 600);
		SetPos(r);
		MoveSameScreen(w);
		Attach(0);

		if ((Tree = new GTree(100, 0, 0, 100, 100)))
		{
			Tree->SetPourLargest(true);
			Tree->Attach(this);
		}
	}
};
#endif

void GRichTextEdit::DoContextMenu(GMouse &m)
{
	GMenuItem *i;
	GSubMenu RClick;
	GAutoString ClipText;
	{
		GClipBoard Clip(this);
		ClipText.Reset(NewStr(Clip.Text()));
	}

	GRichTextPriv::Block *Over = NULL;
	GRect &Content = d->Areas[ContentArea];
	GdcPt2 Doc = d->ScreenToDoc(m.x, m.y);
	// int BlockIndex = -1;
	ssize_t Offset = -1;
	if (Content.Overlap(m.x, m.y))
	{
		int LineHint;
		Offset = d->HitTest(Doc.x, Doc.y, LineHint, &Over);
	}
	if (Over)
		Over->DoContext(RClick, Doc, Offset, true);

	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_CUT, "Cut"), IDM_RTE_CUT, HasSelection());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_COPY, "Copy"), IDM_RTE_COPY, HasSelection());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_PASTE, "Paste"), IDM_RTE_PASTE, ClipText != 0);
	RClick.AppendSeparator();

	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_UNDO, "Undo"), IDM_RTE_UNDO, false /* UndoQue.CanUndo() */);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_REDO, "Redo"), IDM_RTE_REDO, false /* UndoQue.CanRedo() */);
	RClick.AppendSeparator();

	#if 0
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_FIXED, "Fixed Width Font"), IDM_FIXED, true);
	if (i) i->Checked(GetFixedWidthFont());
	#endif

	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_AUTO_INDENT, "Auto Indent"), IDM_AUTO_INDENT, true);
	if (i) i->Checked(AutoIndent);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_SHOW_WHITESPACE, "Show Whitespace"), IDM_SHOW_WHITE, true);
	if (i) i->Checked(ShowWhiteSpace);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_HARD_TABS, "Hard Tabs"), IDM_HARD_TABS, true);
	if (i) i->Checked(HardTabs);
	
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_INDENT_SIZE, "Indent Size"), IDM_INDENT_SIZE, true);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_TAB_SIZE, "Tab Size"), IDM_TAB_SIZE, true);
	
	GSubMenu *Src = RClick.AppendSub("Source");
	if (Src)
	{
		Src->AppendItem("Copy Original", IDM_COPY_ORIGINAL, d->OriginalText.Get() != NULL);
		Src->AppendItem("Copy Current", IDM_COPY_CURRENT);
		#ifdef _DEBUG
		Src->AppendItem("Dump Nodes", IDM_DUMP_NODES);
		// Edit->DumpNodes(Tree);
		#endif
	}

	if (Over)
	{
		#ifdef _DEBUG
		// RClick.AppendItem(Over->GetClass(), -1, false);
		#endif
		Over->DoContext(RClick, Doc, Offset, false);
	}
	if (Environment)
		Environment->AppendItems(&RClick);

	int Id = 0;
	m.ToScreen();
	switch (Id = RClick.Float(this, m.x, m.y))
	{
		case IDM_FIXED:
		{
			SetFixedWidthFont(!GetFixedWidthFont());							
			SendNotify(GNotifyFixedWidthChanged);
			break;
		}
		case IDM_RTE_CUT:
		{
			Cut();
			break;
		}
		case IDM_RTE_COPY:
		{
			Copy();
			break;
		}
		case IDM_RTE_PASTE:
		{
			Paste();
			break;
		}
		case IDM_RTE_UNDO:
		{
			Undo();
			break;
		}
		case IDM_RTE_REDO:
		{
			Redo();
			break;
		}
		case IDM_AUTO_INDENT:
		{
			AutoIndent = !AutoIndent;
			break;
		}
		case IDM_SHOW_WHITE:
		{
			ShowWhiteSpace = !ShowWhiteSpace;
			Invalidate();
			break;
		}
		case IDM_HARD_TABS:
		{
			HardTabs = !HardTabs;
			break;
		}
		case IDM_INDENT_SIZE:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%i", IndentSize);
			GInput i(this, s, "Indent Size:", "Text");
			if (i.DoModal())
			{
				IndentSize = i.GetStr().Int();
			}
			break;
		}
		case IDM_TAB_SIZE:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%i", TabSize);
			GInput i(this, s, "Tab Size:", "Text");
			if (i.DoModal())
			{
				SetTabSize(i.GetStr().Int());
			}
			break;
		}
		case IDM_COPY_ORIGINAL:
		{
			GClipBoard c(this);
			c.Text(d->OriginalText);
			break;
		}
		case IDM_COPY_CURRENT:
		{
			GClipBoard c(this);
			c.Text(Name());
			break;
		}
		case IDM_DUMP_NODES:
		{
			#ifdef _DEBUG
			NodeView *nv = new NodeView(GetWindow());
			DumpNodes(nv->Tree);
			nv->Visible(true);
			#endif
			break;
		}
		default:
		{
			if (Over)
			{
				GMessage Cmd(M_COMMAND, Id);
				if (Over->OnEvent(&Cmd))
					break;
			}
			
			if (Environment)
			{
				Environment->OnMenu(this, Id, 0);
			}
			break;
		}
	}
}

void GRichTextEdit::OnMouseClick(GMouse &m)
{
	bool Processed = false;
	RectType Clicked = 	d->PosToButton(m);
	if (m.Down())
	{
		Focus(true);
		
		if (m.IsContextMenu())
		{
			DoContextMenu(m);
			return;
		}
		else
		{
			Focus(true);

			if (d->Areas[ToolsArea].Overlap(m.x, m.y)
				// || d->Areas[CapabilityArea].Overlap(m.x, m.y)
				)
			{
				if (Clicked != MaxArea)
				{
					if (d->BtnState[Clicked].IsPress)
					{
						d->BtnState[d->ClickedBtn = Clicked].Pressed = true;
						Invalidate(d->Areas + Clicked);
						Capture(true);
					}
					else
					{
						Processed |= d->ClickBtn(m, Clicked);
					}
				}
			}
			else
			{
				d->WordSelectMode = !Processed && m.Double();

				AutoCursor c(new BlkCursor(NULL, 0, 0));
				GdcPt2 Doc = d->ScreenToDoc(m.x, m.y);
				ssize_t Idx = -1;
				if (d->CursorFromPos(Doc.x, Doc.y, &c, &Idx))
				{
					d->ClickedBtn = ContentArea;
					d->SetCursor(c, m.Shift());
					if (d->WordSelectMode)
						SelectWord(Idx);
				}
			}
		}
	}
	else if (IsCapturing())
	{
		Capture(false);

		if (d->ClickedBtn != MaxArea)
		{
			d->BtnState[d->ClickedBtn].Pressed = false;
			Invalidate(d->Areas + d->ClickedBtn);
			Processed |= d->ClickBtn(m, Clicked);
		}

		d->ClickedBtn = MaxArea;
	}

	if (!Processed)
	{
		Capture(m.Down());
	}
}

int GRichTextEdit::OnHitTest(int x, int y)
{
	#ifdef WIN32
	if (GetClient().Overlap(x, y))
	{
		return HTCLIENT;
	}
	#endif
	return GView::OnHitTest(x, y);
}

void GRichTextEdit::OnMouseMove(GMouse &m)
{
	GRichTextEdit::RectType OverBtn = d->PosToButton(m);
	if (d->OverBtn != OverBtn)
	{
		if (d->OverBtn < MaxArea)
		{
			d->BtnState[d->OverBtn].MouseOver = false;
			Invalidate(&d->Areas[d->OverBtn]);
		}
		d->OverBtn = OverBtn;
		if (d->OverBtn < MaxArea)
		{
			d->BtnState[d->OverBtn].MouseOver = true;
			Invalidate(&d->Areas[d->OverBtn]);
		}
	}
	
	if (IsCapturing())
	{
		if (d->ClickedBtn == ContentArea)
		{
			AutoCursor c;
			GdcPt2 Doc = d->ScreenToDoc(m.x, m.y);
			ssize_t Idx = -1;
			if (d->CursorFromPos(Doc.x, Doc.y, &c, &Idx) && c)
			{
				if (d->WordSelectMode && d->Selection)
				{
					// Extend the selection to include the whole word
					if (!d->CursorFirst())
					{
						// Extend towards the end of the doc...
						GArray<uint32> Txt;
						if (c->Blk->CopyAt(0, c->Blk->Length(), &Txt))
						{
							while
							(
								c->Offset < (int)Txt.Length() &&
								!IsWordBreakChar(Txt[c->Offset])
							)
								c->Offset++;
						}
					}
					else
					{
						// Extend towards the start of the doc...
						GArray<uint32> Txt;
						if (c->Blk->CopyAt(0, c->Blk->Length(), &Txt))
						{
							while
							(
								c->Offset > 0 &&
								!IsWordBreakChar(Txt[c->Offset-1])
							)
								c->Offset--;
						}
					}
				}

				d->SetCursor(c, m.Left());
			}
		}
	}

	#ifdef WIN32
	GRect c = GetClient();
	c.Offset(-c.x1, -c.y1);
	if (c.Overlap(m.x, m.y))
	{
		/*
		GStyle *s = HitStyle(Hit);
		TCHAR *c = (s) ? s->GetCursor() : 0;
		if (!c) c = IDC_IBEAM;
		::SetCursor(LoadCursor(0, MAKEINTRESOURCE(c)));
		*/
	}
	#endif
}

bool GRichTextEdit::OnKey(GKey &k)
{
	if (k.Down() &&
		d->Cursor)
		d->Cursor->Blink = true;

	// k.Trace("GRichTextEdit::OnKey");
	if (k.IsContextMenu())
	{
		GMouse m;
		DoContextMenu(m);
	}
	else if (k.IsChar)
	{
		switch (k.c16)
		{
			default:
			{
				// process single char input
				if
				(
					!GetReadOnly()
					&&
					(
						(k.c16 >= ' ' || k.c16 == VK_TAB)
						&&
						k.c16 != 127
					)
				)
				{
					if (k.Down() &&
						d->Cursor &&
						d->Cursor->Blk)
					{
						// letter/number etc
						GRichTextPriv::Block *b = d->Cursor->Blk;

						GNamedStyle *AddStyle = NULL;
						if (d->StyleDirty.Length() > 0)
						{
							GAutoPtr<GCss> Mod(new GCss);
							if (Mod)
							{
								// Get base styles at the cursor..
								GNamedStyle *Base = b->GetStyle(d->Cursor->Offset);
								if (Base && Mod)
									*Mod = *Base;

								// Apply dirty toolbar styles...
								if (d->StyleDirty.HasItem(FontFamilyBtn))
									Mod->FontFamily(GCss::StringsDef(d->Values[FontFamilyBtn].Str()));
								if (d->StyleDirty.HasItem(FontSizeBtn))
									Mod->FontSize(GCss::Len(GCss::LenPt, (float) d->Values[FontSizeBtn].CastDouble()));
								if (d->StyleDirty.HasItem(BoldBtn))
									Mod->FontWeight(d->Values[BoldBtn].CastInt32() ? GCss::FontWeightBold : GCss::FontWeightNormal);
								if (d->StyleDirty.HasItem(ItalicBtn))
									Mod->FontStyle(d->Values[ItalicBtn].CastInt32() ? GCss::FontStyleItalic : GCss::FontStyleNormal);
								if (d->StyleDirty.HasItem(UnderlineBtn))
									Mod->TextDecoration(d->Values[UnderlineBtn].CastInt32() ? GCss::TextDecorUnderline : GCss::TextDecorNone);
								if (d->StyleDirty.HasItem(ForegroundColourBtn))
									Mod->Color(GCss::ColorDef(GCss::ColorRgb, (uint32)d->Values[ForegroundColourBtn].CastInt64()));
								if (d->StyleDirty.HasItem(BackgroundColourBtn))
									Mod->BackgroundColor(GCss::ColorDef(GCss::ColorRgb, (uint32)d->Values[BackgroundColourBtn].CastInt64()));
							
								AddStyle = d->AddStyleToCache(Mod);
							}
							
							d->StyleDirty.Length(0);
						}

						AutoTrans Trans(new GRichTextPriv::Transaction);						
						d->DeleteSelection(Trans, NULL);

						uint32 Ch = k.c16;
						if (b->AddText(Trans, d->Cursor->Offset, &Ch, 1, AddStyle))
						{
							d->Cursor->Set(d->Cursor->Offset + 1);
							Invalidate();
							SendNotify(GNotifyDocChanged);

							d->AddTrans(Trans);
						}
					}
					return true;
				}
				break;
			}
			case VK_RETURN:
			{
				if (GetReadOnly())
					break;

				if (k.Down() && k.IsChar)
					OnEnter(k);

				return true;
			}
			case VK_BACKSPACE:
			{
				if (GetReadOnly())
					break;

				bool Changed = false;
				AutoTrans Trans(new GRichTextPriv::Transaction);

				if (k.Ctrl())
				{
				    // Ctrl+H
				}
				else if (k.Down())
				{
					GRichTextPriv::Block *b;

					if (HasSelection())
					{
						Changed = d->DeleteSelection(Trans, NULL);
					}
					else if (d->Cursor &&
							 (b = d->Cursor->Blk))
					{
						if (d->Cursor->Offset > 0)
						{
							Changed = b->DeleteAt(Trans, d->Cursor->Offset-1, 1) > 0;
							if (Changed)
							{
								// Has block size reached 0?
								if (b->Length() == 0)
								{
									// Then delete it...
									GRichTextPriv::Block *n = d->Next(b);
									if (n)
									{
										d->Blocks.Delete(b, true);
										d->Cursor.Reset(new GRichTextPriv::BlockCursor(n, 0, 0));
									}
								}
								else
								{
									d->Cursor->Set(d->Cursor->Offset - 1);
								}
							}
						}
						else
						{
							// At the start of a block:
							GRichTextPriv::Block *Prev = d->Prev(d->Cursor->Blk);
							if (Prev)
							{
								// Try and merge the two blocks...
								ssize_t Len = Prev->Length();
								d->Merge(Trans, Prev, d->Cursor->Blk);
								
								AutoCursor c(new BlkCursor(Prev, Len, -1));
								d->SetCursor(c);
							}
							else // at the start of the doc...
							{
								// Don't send the doc changed...
								return true;
							}
						}
					}
				}

				if (Changed)
				{
					Invalidate();
					d->AddTrans(Trans);
					SendNotify(GNotifyDocChanged);
				}
				return true;
			}
		}
	}
	else // not a char
	{
		switch (k.vkey)
		{
			case VK_TAB:
				return true;
			case VK_RETURN:
				return !GetReadOnly();
			case VK_BACKSPACE:
			{
				if (!GetReadOnly())
				{
					if (k.Alt())
					{
						if (k.Down())
						{
							if (k.Ctrl())
								Redo();
							else
								Undo();
						}
					}
					else if (k.Ctrl())
					{
						if (k.Down())
						{
							// Implement delete by word
							LgiAssert(!"Impl backspace by word");
						}
					}

					return true;
				}
				break;
			}
			case VK_F3:
			{
				if (k.Down())
					DoFindNext();
				return true;
			}
			case VK_LEFT:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					if (HasSelection() && !k.Shift())
					{
						GRect r = d->SelectionRect();
						Invalidate(&r);
						
						AutoCursor c(new BlkCursor(d->CursorFirst() ? *d->Cursor : *d->Selection));
						d->SetCursor(c);
					}
					else
					{
						#ifdef MAC
						if (k.System())
							goto Jump_StartOfLine;
						else
						#endif

						d->Seek(d->Cursor,
								k.Ctrl() ? GRichTextPriv::SkLeftWord : GRichTextPriv::SkLeftChar,
								k.Shift());
					}
				}
				return true;
			}
			case VK_RIGHT:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					if (HasSelection() && !k.Shift())
					{
						GRect r = d->SelectionRect();
						Invalidate(&r);

						AutoCursor c(new BlkCursor(d->CursorFirst() ? *d->Selection : *d->Cursor));
						d->SetCursor(c);
					}
					else
					{
						#ifdef MAC
						if (k.System())
							goto Jump_EndOfLine;
						#endif

						d->Seek(d->Cursor,
								k.Ctrl() ? GRichTextPriv::SkRightWord : GRichTextPriv::SkRightChar,
								k.Shift());
					}
				}
				return true;
			}
			case VK_UP:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageUp;
					#endif

					d->Seek(d->Cursor,
							GRichTextPriv::SkUpLine,
							k.Shift());
				}
				return true;
			}
			case VK_DOWN:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageDown;
					#endif

					d->Seek(d->Cursor,
							GRichTextPriv::SkDownLine,
							k.Shift());
				}
				return true;
			}
			case VK_END:
			{
				if (k.Down())
				{
					#ifdef MAC
					if (!k.Ctrl())
						Jump_EndOfLine:
					#endif

					d->Seek(d->Cursor,
							k.Ctrl() ? GRichTextPriv::SkDocEnd : GRichTextPriv::SkLineEnd,
							k.Shift());
				}
				return true;
			}
			case VK_HOME:
			{
				if (k.Down())
				{
					#ifdef MAC
					if (!k.Ctrl())
						Jump_StartOfLine:
					#endif

					d->Seek(d->Cursor,
							k.Ctrl() ? GRichTextPriv::SkDocStart : GRichTextPriv::SkLineStart,
							k.Shift());
				}
				return true;
			}
			case VK_PAGEUP:
			{
				#ifdef MAC
				GTextView4_PageUp:
				#endif
				if (k.Down())
				{
					d->Seek(d->Cursor,
							GRichTextPriv::SkUpPage,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_PAGEDOWN:
			{
				#ifdef MAC
				GTextView4_PageDown:
				#endif
				if (k.Down())
				{
					d->Seek(d->Cursor,
							GRichTextPriv::SkDownPage,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_INSERT:
			{
				if (k.Down())
				{
					if (k.Ctrl())
					{
						Copy();
					}
					else if (k.Shift())
					{
						if (!GetReadOnly())
						{
							Paste();
						}
					}
				}
				return true;
				break;
			}
			case VK_DELETE:
			{
				if (GetReadOnly())
					break;

				if (!k.Down())
					return true;

				bool Changed = false;
				GRichTextPriv::Block *b;
				AutoTrans Trans(new GRichTextPriv::Transaction);

				if (HasSelection())
				{
					if (k.Shift())
						Changed |= Cut();
					else
						Changed |= d->DeleteSelection(Trans, NULL);
				}
				else if (d->Cursor &&
						(b = d->Cursor->Blk))
				{
					if (d->Cursor->Offset >= b->Length())
					{
						// Cursor is at the end of this block, pull the styles
						// from the next block into this one.
						GRichTextPriv::Block *next = d->Next(b);
						if (!next)
						{
							// No next block, therefor nothing to delete
							break;
						}

						// Try and merge the blocks
						if (d->Merge(Trans, b, next))
							Changed = true;
						else
						{
							// If the cursor is on the last empty line of a text block,
							// we should delete that '\n' first
							GRichTextPriv::TextBlock *tb = dynamic_cast<GRichTextPriv::TextBlock*>(b);
							if (tb && tb->IsEmptyLine(d->Cursor))
								Changed = tb->StripLast(Trans);

							// move the cursor to the next block
							d->Cursor.Reset(new GRichTextPriv::BlockCursor(b = next, 0, 0));
						}
					}

					if (!Changed && b->DeleteAt(Trans, d->Cursor->Offset, 1))
					{
						if (b->Length() == 0)
						{
							GRichTextPriv::Block *n = d->Next(b);
							if (n)
							{
								d->Blocks.Delete(b, true);
								d->Cursor.Reset(new GRichTextPriv::BlockCursor(n, 0, 0));
							}
						}

						Changed = true;
					}
				}
						
				if (Changed)
				{
					Invalidate();
					d->AddTrans(Trans);
					SendNotify(GNotifyDocChanged);
				}
				return true;
			}
			default:
			{
				if (k.c16 == 17)
					break;

				if (k.c16 == ' ' &&
					k.Ctrl() &&
					k.Alt() &&
					d->Cursor &&
					d->Cursor->Blk)
				{
					if (k.Down())
					{
						// letter/number etc
						GRichTextPriv::Block *b = d->Cursor->Blk;
						uint32 Nbsp[] = {0xa0};
						if (b->AddText(NoTransaction, d->Cursor->Offset, Nbsp, 1))
						{
							d->Cursor->Set(d->Cursor->Offset + 1);
							Invalidate();
							SendNotify(GNotifyDocChanged);
						}
					}
					break;
				}

				if (k.Modifier() &&
					!k.Alt())
				{
					switch (k.GetChar())
					{
						case 0xbd: // Ctrl+'-'
						{
							/*
							if (k.Down() &&
								Font->PointSize() > 1)
							{
								Font->PointSize(Font->PointSize() - 1);
								OnFontChange();
								Invalidate();
							}
							*/
							break;
						}
						case 0xbb: // Ctrl+'+'
						{
							/*
							if (k.Down() &&
								Font->PointSize() < 100)
							{
								Font->PointSize(Font->PointSize() + 1);
								OnFontChange();
								Invalidate();
							}
							*/
							break;
						}
						case 'a':
						case 'A':
						{
							if (k.Down())
							{
								// select all
								SelectAll();
							}
							return true;
							break;
						}
						case 'b':
						case 'B':
						{
							if (k.Down())
							{
								// Bold selection
								GMouse m;
								GetMouse(m);
								d->ClickBtn(m, BoldBtn);
							}
							return true;
							break;
						}
						case 'i':
						case 'I':
						{
							if (k.Down())
							{
								// Italic selection
								GMouse m;
								GetMouse(m);
								d->ClickBtn(m, ItalicBtn);
							}
							return true;
							break;
						}
						case 'y':
						case 'Y':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									Redo();
								}
								return true;
							}
							break;
						}
						case 'z':
						case 'Z':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									if (k.Shift())
									{
										Redo();
									}
									else
									{
										Undo();
									}
								}
								return true;
							}
							break;
						}
						case 'x':
						case 'X':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									Cut();
								}
								return true;
							}
							break;
						}
						case 'c':
						case 'C':
						{
							if (k.Shift())
								return false;
							
							if (k.Down())
								Copy();
							return true;
							break;
						}
						case 'v':
						case 'V':
						{
							if (!k.Shift() &&
								!GetReadOnly())
							{
								if (k.Down())
								{
									Paste();
								}
								return true;
							}
							break;
						}
						case 'f':
						{
							if (k.Down())
								DoFind();
							return true;
						}
						case 'g':
						case 'G':
						{
							if (k.Down())
							{
								DoGoto();
							}
							return true;
							break;
						}
						case 'h':
						case 'H':
						{
							if (k.Down())
							{
								DoReplace();
							}
							return true;
							break;
						}
						case 'u':
						case 'U':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									DoCase(k.Shift());
								}
								return true;
							}
							break;
						}
						case VK_RETURN:
						{
							if (!GetReadOnly() && !k.Shift())
							{
								if (k.Down())
								{
									OnEnter(k);
								}
								return true;
							}
							break;
						}
					}
				}
				break;
			}
		}
	}
	
	return false;
}

void GRichTextEdit::OnEnter(GKey &k)
{
	AutoTrans Trans(new GRichTextPriv::Transaction);						

	// Enter key handling
	bool Changed = false;

	if (HasSelection())
		Changed |= d->DeleteSelection(Trans, NULL);
	
	if (d->Cursor &&
		d->Cursor->Blk)
	{
		GRichTextPriv::Block *b = d->Cursor->Blk;
		const uint32 Nl[] = {'\n'};

		if (b->AddText(Trans, d->Cursor->Offset, Nl, 1))
		{
			d->Cursor->Set(d->Cursor->Offset + 1);
			Changed = true;
		}
		else
		{
			// Some blocks don't take text. However a new block can be created or
			// the text added to the start of the next block
			if (d->Cursor->Offset == 0)
			{
				GRichTextPriv::Block *Prev = d->Prev(b);
				if (Prev)
					Changed = Prev->AddText(Trans, Prev->Length(), Nl, 1);
				else // No previous... must by first block... create new block:
				{
					GRichTextPriv::TextBlock *tb = new GRichTextPriv::TextBlock(d);
					if (tb)
					{
						Changed = true; // tb->AddText(Trans, 0, Nl, 1);
						d->Blocks.AddAt(0, tb);
					}
				}
			}
			else if (d->Cursor->Offset == b->Length())
			{
				GRichTextPriv::Block *Next = d->Next(b);
				if (Next)
				{
					if ((Changed = Next->AddText(Trans, 0, Nl, 1)))
						d->Cursor->Set(Next, 0, -1);
				}
				else // No next block. Create one:
				{
					GRichTextPriv::TextBlock *tb = new GRichTextPriv::TextBlock(d);
					if (tb)
					{
						Changed = true; // tb->AddText(Trans, 0, Nl, 1);
						d->Blocks.Add(tb);
					}
				}
			}
		}
	}

	if (Changed)
	{
		Invalidate();
		d->AddTrans(Trans);
		SendNotify(GNotifyDocChanged);
	}
}

void GRichTextEdit::OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour)
{
	pDC->Colour(colour);
	pDC->Rectangle(&r);
}

void GRichTextEdit::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();
	if (!r.Valid())
		return;

	#if 0
	pDC->Colour(GColour(255, 0, 255));
	pDC->Rectangle();
	#endif

	int FontY = GetFont()->GetHeight();

	GCssTools ct(d, d->Font);
	r = ct.PaintBorder(pDC, r);

	bool HasSpace = r.Y() > (FontY * 3);
	/*
	if (d->NeedsCap.Length() > 0 && HasSpace)
	{
		d->Areas[CapabilityArea] = r;
		d->Areas[CapabilityArea].y2 = d->Areas[CapabilityArea].y1 + 4 + ((FontY + 4) * (int)d->NeedsCap.Length());
		r.y1 = d->Areas[CapabilityArea].y2 + 1;

		d->Areas[CapabilityBtn] = d->Areas[CapabilityArea];
		d->Areas[CapabilityBtn].Size(2, 2);
		d->Areas[CapabilityBtn].x1 = d->Areas[CapabilityBtn].x2 - 30;
	}
	else
	{
		d->Areas[CapabilityArea].ZOff(-1, -1);
		d->Areas[CapabilityBtn].ZOff(-1, -1);
	}
	*/

	if (d->ShowTools && HasSpace)
	{
		d->Areas[ToolsArea] = r;
		d->Areas[ToolsArea].y2 = d->Areas[ToolsArea].y1 + (FontY + 8) - 1;
		r.y1 = d->Areas[ToolsArea].y2 + 1;
	}
	else
	{
		d->Areas[ToolsArea].ZOff(-1, -1);
	}

	d->Areas[ContentArea] = r;

	#if 0
	CGAffineTransform t1 = CGContextGetCTM(pDC->Handle());
	CGRect rc = CGContextGetClipBoundingBox(pDC->Handle());
	LgiTrace("d->Areas[ContentArea]=%s  %f,%f,%f,%f\n",
		d->Areas[ContentArea].GetStr(),
		rc.origin.x, rc.origin.y,
		rc.size.width, rc.size.height);
	if (rc.size.width < 20)
	{
		int asd=0;
	}
	#endif

	if (d->Layout(VScroll))
		d->Paint(pDC, VScroll);
	// else the scroll bars changed, wait for re-paint
}

GMessage::Result GRichTextEdit::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_CUT:
		{
			Cut();
			break;
		}
		case M_COPY:
		{
			Copy();
			break;
		}
		case M_PASTE:
		{
			Paste();
			break;
		}
		case M_BLOCK_MSG:
		{
			GRichTextPriv::Block *b = (GRichTextPriv::Block*)Msg->A();
			GAutoPtr<GMessage> msg((GMessage*)Msg->B());
			if (d->Blocks.HasItem(b) && msg)
			{
				b->OnEvent(msg);
			}
			else printf("%s:%i - No block to receive M_BLOCK_MSG.\n", _FL);
			break;
		}
		case M_ENUMERATE_LANGUAGES:
		{
			GAutoPtr< GArray<GSpellCheck::LanguageId> > Languages((GArray<GSpellCheck::LanguageId>*)Msg->A());
			if (!Languages)
			{
				LgiTrace("%s:%i - M_ENUMERATE_LANGUAGES no param\n", _FL);
				break;
			}

			// LgiTrace("%s:%i - Got M_ENUMERATE_LANGUAGES %s\n", _FL, d->SpellLang.Get());
			bool Match = false;
			for (unsigned i=0; i<Languages->Length(); i++)
			{
				GSpellCheck::LanguageId &s = (*Languages)[i];
				if (s.LangCode.Equals(d->SpellLang) ||
					s.EnglishName.Equals(d->SpellLang))
				{
					// LgiTrace("%s:%i - EnumDict called %s\n", _FL, s.LangCode.Get());
					d->SpellCheck->EnumDictionaries(AddDispatch(), s.LangCode);
					Match = true;
					break;
				}
			}
			if (!Match)
				LgiTrace("%s:%i - EnumDict not called %s\n", _FL, d->SpellLang.Get());
			break;
		}
		case M_ENUMERATE_DICTIONARIES:
		{
			GAutoPtr< GArray<GSpellCheck::DictionaryId> > Dictionaries((GArray<GSpellCheck::DictionaryId>*)Msg->A());
			if (!Dictionaries)
				break;
	
			bool Match = false;		
			for (unsigned i=0; i<Dictionaries->Length(); i++)
			{
				GSpellCheck::DictionaryId &s = (*Dictionaries)[i];
				if (s.Dict.Equals(d->SpellDict))
				{
					// LgiTrace("%s:%i - M_ENUMERATE_DICTIONARIES: %s, %s\n", _FL, s.Dict.Get(), d->SpellDict.Get());
					d->SpellCheck->SetDictionary(AddDispatch(), s.Lang, s.Dict);
					Match = true;
					break;
				}
			}
			if (!Match)
				LgiTrace("%s:%i - No match in M_ENUMERATE_DICTIONARIES: %s\n", _FL, d->SpellDict.Get());
			break;
		}
		case M_SET_DICTIONARY:
		{
			d->SpellDictionaryLoaded = Msg->A() != 0;
			// LgiTrace("%s:%i - M_SET_DICTIONARY=%i\n", _FL, d->SpellDictionaryLoaded);
			if (d->SpellDictionaryLoaded)
			{
				AutoTrans Trans(new GRichTextPriv::Transaction);

				// Get any loaded text blocks to check their spelling
				bool Status = false;
				for (unsigned i=0; i<d->Blocks.Length(); i++)
				{
					Status |= d->Blocks[i]->OnDictionary(Trans);
				}

				if (Status)
					d->AddTrans(Trans);
			}
			break;
		}
		case M_CHECK_TEXT:
		{
			GAutoPtr<GSpellCheck::CheckText> Ct((GSpellCheck::CheckText*)Msg->A());
			if (!Ct || Ct->User.Length() > 1)
			{
				LgiAssert(0);
				break;
			}

			GRichTextPriv::Block *b = (GRichTextPriv::Block*)Ct->User[SpellBlockPtr].CastVoidPtr();
			if (!d->Blocks.HasItem(b))
				break;

			b->SetSpellingErrors(Ct->Errors, *Ct);
			Invalidate();
			break;
		}
		#if defined WIN32
		case WM_GETTEXTLENGTH:
		{
			return 0 /*Size*/;
		}
		case WM_GETTEXT:
		{
			int Chars = (int)MsgA(Msg);
			char *Out = (char*)MsgB(Msg);
			if (Out)
			{
				char *In = (char*)LgiNewConvertCp(LgiAnsiToLgiCp(), NameW(), LGI_WideCharset, Chars);
				if (In)
				{
					int Len = (int)strlen(In);
					memcpy(Out, In, Len);
					DeleteArray(In);
					return Len;
				}
			}
			return 0;
		}
		case M_COMPONENT_INSTALLED:
		{
			GAutoPtr<GString> Comp((GString*)Msg->A());
			if (Comp)
				d->OnComponentInstall(*Comp);
			break;
		}

		/* This is broken... the IME returns garbage in the buffer. :(
		case WM_IME_COMPOSITION:
		{
			if (Msg->b & GCS_RESULTSTR) 
			{
				HIMC hIMC = ImmGetContext(Handle());
				if (hIMC)
				{
					int Size = ImmGetCompositionString(hIMC, GCS_RESULTSTR, NULL, 0);
					char *Buf = new char[Size];
					if (Buf)
					{
						ImmGetCompositionString(hIMC, GCS_RESULTSTR, Buf, Size);

						char16 *Utf = (char16*)LgiNewConvertCp(LGI_WideCharset, Buf, LgiAnsiToLgiCp(), Size);
						if (Utf)
						{
							Insert(Cursor, Utf, StrlenW(Utf));
							DeleteArray(Utf);
						}

						DeleteArray(Buf);
					}

					ImmReleaseContext(Handle(), hIMC);
				}
				return 0;
			}
			break;
		}
		*/
		#endif
	}

	return GLayout::OnEvent(Msg);
}

int GRichTextEdit::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl->GetId() == IDC_VSCROLL && VScroll)
	{
		Invalidate(d->Areas + ContentArea);
	}

	return 0;
}

void GRichTextEdit::OnPulse()
{
	if (!ReadOnly && d->Cursor)
	{
		uint64 n = LgiCurrentTime();
		if (d->BlinkTs - n >= RTE_CURSOR_BLINK_RATE)
		{
			d->BlinkTs = n;
			d->Cursor->Blink = !d->Cursor->Blink;
			d->InvalidateDoc(&d->Cursor->Pos);
		}
		
		// Do autoscroll while the user has clicked and dragged off the control:
		if (VScroll && IsCapturing() && d->ClickedBtn == GRichTextEdit::ContentArea)
		{
			GMouse m;
			GetMouse(m);
			
			// Is the mouse outside the content window
			GRect &r = d->Areas[ContentArea];
			if (!r.Overlap(m.x, m.y))
			{
				AutoCursor c(new BlkCursor(NULL, 0, 0));
				GdcPt2 Doc = d->ScreenToDoc(m.x, m.y);
				ssize_t Idx = -1;
				if (d->CursorFromPos(Doc.x, Doc.y, &c, &Idx))
				{
					d->SetCursor(c, true);
					if (d->WordSelectMode)
						SelectWord(Idx);
				}
				
				// Update the screen.
				d->InvalidateDoc(NULL);
			}
		}
	}
}

void GRichTextEdit::OnUrl(char *Url)
{
	if (Environment)
	{
		Environment->OnNavigate(this, Url);
	}
}

bool GRichTextEdit::OnLayout(GViewLayoutInfo &Inf)
{
	Inf.Width.Min = 32;
	Inf.Width.Max = -1;

	// Inf.Height.Min = (Font ? Font->GetHeight() : 18) + 4;
	Inf.Height.Max = -1;

	return true;
}

#if _DEBUG
void GRichTextEdit::SelectNode(GString Param)
{
	GRichTextPriv::Block *b = (GRichTextPriv::Block*) Param.Int(16);
	bool Valid = false;
	for (auto i : d->Blocks)
	{
		if (i == b)
			Valid = true;
		i->DrawDebug = false;
	}
	if (Valid)
	{
		b->DrawDebug = true;
		Invalidate();
	}
}

void GRichTextEdit::DumpNodes(GTree *Root)
{
	d->DumpNodes(Root);
}
#endif


///////////////////////////////////////////////////////////////////////////////
SelectColour::SelectColour(GRichTextPriv *priv, GdcPt2 p, GRichTextEdit::RectType t) : GPopup(priv->View)
{
	d = priv;
	Type = t;

	int Px = 16;
	int PxSp = Px + 2;
	int x = 6;
	int y = 6;

	// Do grey ramp
	for (int i=0; i<8; i++)
	{
		Entry &en = e.New();
		int Grey = i * 255 / 7;
		en.r.ZOff(Px-1, Px-1);
		en.r.Offset(x + (i * PxSp), y);
		en.c.Rgb(Grey, Grey, Grey);
	}

	// Do colours
	y += PxSp + 4;
	int SatRange = 255 - 64;
	int SatStart = 255 - 32;
	int HueStep = 360 / 8;
	for (int sat=0; sat<8; sat++)
	{
		for (int hue=0; hue<8; hue++)
		{
			GColour c;
			c.SetHLS(hue * HueStep, SatStart - ((sat * SatRange) / 7), 255);
			c.ToRGB();

			Entry &en = e.New();
			en.r.ZOff(Px-1, Px-1);
			en.r.Offset(x + (hue * PxSp), y);
			en.c = c;
		}

		y += PxSp;
	}

	SetParent(d->View);

	GRect r(0, 0, 12 + (8 * PxSp) - 1, y + 6 - 1);
	r.Offset(p.x, p.y);
	SetPos(r);

	Visible(true);
}

void SelectColour::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
	for (unsigned i=0; i<e.Length(); i++)
	{
		pDC->Colour(e[i].c);
		pDC->Rectangle(&e[i].r);
	}
}

void SelectColour::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		for (unsigned i=0; i<e.Length(); i++)
		{
			if (e[i].r.Overlap(m.x, m.y))
			{
				d->Values[Type] = (int64)e[i].c.c32();
				d->View->Invalidate(d->Areas + Type);
				d->OnStyleChange(Type);
				Visible(false);
				break;
			}
		}
	}
}

void SelectColour::Visible(bool i)
{
	GPopup::Visible(i);
	if (!i)
	{
		d->View->Focus(true);
		delete this;
	}
}

///////////////////////////////////////////////////////////////////////////////
#define EMOJI_PAD	2
#include "Emoji.h"
int EmojiMenu::Cur = 0;

EmojiMenu::EmojiMenu(GRichTextPriv *priv, GdcPt2 p) : GPopup(priv->View)
{
	d = priv;

	d->GetEmojiImage();

	int MaxIdx = 0;
	GRange EmojiBlocks[2] = { GRange(0x203c, 0x3299 - 0x203c + 1), GRange(0x1f004, 0x1f6c5 - 0x1f004 + 1) };
	LHashTbl<IntKey<int>, int> Map;
	for (int b=0; b<CountOf(EmojiBlocks); b++)
	{
		GRange &r = EmojiBlocks[b];
		for (int i=0; i<r.Len; i++)
		{
			uint32 u = (int)r.Start + i;
			int Idx = EmojiToIconIndex(&u, 1);
			if (Idx >= 0)
			{
				Map.Add(Idx, u);
				MaxIdx = MAX(MaxIdx, Idx);
			}
		}
	}

	int Sz = EMOJI_CELL_SIZE - 1;
	int PaneCount = 5;
	int PaneSz = Map.Length() / PaneCount;
	int ImgIdx = 0;

	int PaneSelectSz = SysFont->GetHeight() * 2;
	int Rows = (PaneSz + EMOJI_GROUP_X - 1) / EMOJI_GROUP_X;
	GRect r(0, 0,
			(EMOJI_CELL_SIZE + EMOJI_PAD) * EMOJI_GROUP_X + EMOJI_PAD,
			(EMOJI_CELL_SIZE + EMOJI_PAD) * Rows + EMOJI_PAD + PaneSelectSz);
	r.Offset(p.x, p.y);
	SetPos(r);
	
	for (int pi = 0; pi < PaneCount; pi++)
	{
		Pane &p = Panes[pi];
		int Wid = X() - (EMOJI_PAD*2);
		p.Btn.x1 = EMOJI_PAD + (pi * Wid / PaneCount);
		p.Btn.y1 = EMOJI_PAD;
		p.Btn.x2 = EMOJI_PAD + ((pi + 1) * Wid / PaneCount) - 1;
		p.Btn.y2 = EMOJI_PAD + PaneSelectSz;
		int Dx = EMOJI_PAD;
		int Dy = p.Btn.y2 + 1;
		
		while ((int)p.e.Length() < PaneSz && ImgIdx <= MaxIdx)
		{
			uint32 u = Map.Find(ImgIdx);
			if (u)
			{
				Emoji &Ch = p.e.New();
				Ch.u = u;

				int Sx = ImgIdx % EMOJI_GROUP_X;
				int Sy = ImgIdx / EMOJI_GROUP_X;

				Ch.Src.ZOff(Sz, Sz);
				Ch.Src.Offset(Sx * EMOJI_CELL_SIZE, Sy * EMOJI_CELL_SIZE);

				Ch.Dst.ZOff(Sz, Sz);
				Ch.Dst.Offset(Dx, Dy);

				Dx += EMOJI_PAD + EMOJI_CELL_SIZE;
				if (Dx + EMOJI_PAD + EMOJI_CELL_SIZE >= r.X())
				{
					Dx = EMOJI_PAD;
					Dy += EMOJI_PAD + EMOJI_CELL_SIZE;
				}
			}
			ImgIdx++;
		}
	}

	SetParent(d->View);
	Visible(true);
}

void EmojiMenu::OnPaint(GSurface *pDC)
{
	GAutoPtr<GDoubleBuffer> DblBuf;
	if (!pDC->SupportsAlphaCompositing())
		DblBuf.Reset(new GDoubleBuffer(pDC));

	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();

	GSurface *EmojiImg = d->GetEmojiImage();
	if (EmojiImg)
	{
		pDC->Op(GDC_ALPHA);
		
		for (unsigned i=0; i<Panes.Length(); i++)
		{
			Pane &p = Panes[i];
			
			GString s;
			s.Printf("%i", i);
			GDisplayString Ds(SysFont, s);
			if (Cur == i)
			{
				pDC->Colour(LC_LIGHT, 24);
				pDC->Rectangle(&p.Btn);
			}
			SysFont->Fore(LC_TEXT);
			SysFont->Transparent(true);
			Ds.Draw(pDC, p.Btn.x1 + ((p.Btn.X()-Ds.X())>>1), p.Btn.y1 + ((p.Btn.Y()-Ds.Y())>>1));
		}
		
		Pane &p = Panes[Cur];
		for (unsigned i=0; i<p.e.Length(); i++)
		{
			Emoji &g = p.e[i];
			pDC->Blt(g.Dst.x1, g.Dst.y1, EmojiImg, &g.Src);
		}
	}
	else
	{
		GRect c = GetClient();
		GDisplayString Ds(SysFont, "Loading...");
		SysFont->Colour(LC_TEXT, LC_MED);
		SysFont->Transparent(true);
		Ds.Draw(pDC, (c.X()-Ds.X())>>1, (c.Y()-Ds.Y())>>1);
	}
}

bool EmojiMenu::InsertEmoji(uint32 Ch)
{
	if (!d->Cursor || !d->Cursor->Blk)
		return false;

	AutoTrans Trans(new GRichTextPriv::Transaction);						

	if (!d->Cursor->Blk->AddText(NoTransaction, d->Cursor->Offset, &Ch, 1, NULL))
		return false;

	AutoCursor c(new BlkCursor(*d->Cursor));
	c->Offset++;
	d->SetCursor(c);

	d->AddTrans(Trans);
						
	d->Dirty = true;
	d->InvalidateDoc(NULL);
	d->View->SendNotify(GNotifyDocChanged);

	return true;
}

void EmojiMenu::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		for (unsigned i=0; i<Panes.Length(); i++)
		{
			Pane &p = Panes[i];
			if (p.Btn.Overlap(m.x, m.y))
			{
				Cur = i;
				Invalidate();
				return;
			}
		}
		
		Pane &p = Panes[Cur];
		for (unsigned i=0; i<p.e.Length(); i++)
		{
			Emoji &Ch = p.e[i];
			if (Ch.Dst.Overlap(m.x, m.y))
			{
				InsertEmoji(Ch.u);
				Visible(false);
				break;
			}
		}
	}
}

void EmojiMenu::Visible(bool i)
{
	GPopup::Visible(i);
	if (!i)
	{
		d->View->Focus(true);
		delete this;
	}
}

///////////////////////////////////////////////////////////////////////////////
class GRichTextEdit_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (_stricmp(Class, "GRichTextEdit") == 0)
		{
			return new GRichTextEdit(-1, 0, 0, 2000, 2000);
		}

		return 0;
	}
} RichTextEdit_Factory;
