/*hdr
**      FILE:           LgiRes.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           12/3/2000
**      DESCRIPTION:    Xp resource interfaces
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "Lgi.h"
#include "GToken.h"
#include "GList.h"
#include "GTableLayout.h"
#ifdef LINUX
#include "LgiWinManGlue.h"
#endif
#include "GVariant.h"

// If it is defined it will use the cross platform 
// "res" library distributed with the LGI library.

// #include "GXml.h"

#define DEBUG_RES_FILE						0
#define CastToGWnd(RObj)					((RObj != 0) ? dynamic_cast<GView*>(RObj) : 0)

char *LgiStringRes::CodePage = 0;
GLanguage *LgiStringRes::CurLang = 0;

LgiStringRes::LgiStringRes(LgiResources *res)
{
	Res = res;
	Ref = 0;
	Id = 0;
	Str = 0;
	Tag = 0;
	IsString = false;
}

LgiStringRes::~LgiStringRes()
{
	DeleteArray(Str);
	DeleteArray(Tag);
}

bool LgiStringRes::Read(GXmlTag *t, ResFileFormat Format)
{
	if (LgiStringRes::CurLang AND
		t AND
		stricmp(t->Tag, "string") == 0)
	{
		char *n = 0;
		if ((n = t->GetAttr("Cid")) OR
			(n = t->GetAttr("Id")) )
		{
			Id = atoi(n);
		}
		if (n = t->GetAttr("Ref"))
		{
			Ref = atoi(n);
		}
		if (n = t->GetAttr("Define"))
		{
			if (strcmp(n, "IDOK") == 0)
			{
				Id = IDOK;
			}
			else if (strcmp(n, "IDCANCEL") == 0)
			{
				Id = IDCANCEL;
			}
			else if (strcmp(n, "IDC_STATIC") == 0)
			{
				Id = -1;
			}
		}
		if (n = t->GetAttr("Tag"))
		{
			Tag = NewStr(n);
		}

		char *Cp = LgiStringRes::CodePage;
		char Name[256];
		strcpy(Name, LgiStringRes::CurLang->Id);
		n = 0;
		
		if ((n = t->GetAttr(Name)) AND
			strlen(n) > 0)
		{
			// Language string ok
			// Lang = LangFind(0, CurLang, 0);
		}
		else if (LgiStringRes::CurLang->OldId AND
				sprintf(Name, "Text(%i)", LgiStringRes::CurLang->OldId) AND
				(n = t->GetAttr(Name)) AND
				strlen(n) > 0)
		{
			// Old style language string ok
		}
		else if (!LgiStringRes::CurLang->IsEnglish())
		{
			// no string, try english
			n = t->GetAttr("en");
			GLanguage *Lang = GFindLang("en");
			if (Lang)
			{
				Cp = Lang->CodePage;
			}
		}

		if (n)
		{
			DeleteArray(Str);

			if (Cp)
			{
				Str = (char*)LgiNewConvertCp("utf-8", n, Cp);
			}
			else
			{
				Str = NewStr(n);
			}

			if (Str)
			{
				char *d = Str;
				for (char *s = Str; s AND *s;)
				{
					if (*s == '\\')
					{
						if (*(s+1) == 'n')
						{
							*d++ = '\n';
						}
						else if (*(s+1) == 't')
						{
							*d++ = '\t';
						}
						s += 2;
					}
					else 
					{
						*d++ = *s++;
					}
				}
				*d++ = 0;
			}
		}

		if (Res)
		{
			for (int a=0; a<t->Attr.Length(); a++)
			{
				GXmlAttr *v = &t->Attr[a];
				char *Name = v->GetName();

				if (GFindLang(Name))
				{
					Res->AddLang(Name);
				}
				else if (	Name[0] == 'T' AND
							Name[1] == 'e' AND 
							Name[2] == 'x' AND 
							Name[3] == 't' AND
							Name[4] == '(')
				{
					int Old = atoi(Name + 5);
					if (Old)
					{
						GLanguage *OldLang = GFindOldLang(Old);
						if (OldLang)
						{
							LgiAssert(OldLang->OldId == Old);
							Res->AddLang(OldLang->Id);
						}
					}
				}
			}
		}

		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////
// Lgi resources
class LgiResourcesPrivate
{
public:
	bool Ok;
	ResFileFormat Format;
	char *File;

	LgiResourcesPrivate()
	{
		Ok = false;
		Format = Lr8File;
		File = 0;
	}

	~LgiResourcesPrivate()
	{
		DeleteArray(File);
	}
};

List<LgiResources> LgiResources::ResourceContainers;

LgiResources::LgiResources(char *FileName, bool Warn)
{
	d = new LgiResourcesPrivate;
	ScriptEngine = 0;
	Languages = new GLanguageId[80];
	if (Languages) *Languages = 0;

	// global pointer list
	ResourceContainers.Insert(this);

	char File[256] = "";
	char *FullPath = 0;

#if DEBUG_RES_FILE
printf("%s:%i - Filename='%s'\n", _FL, FileName);
#endif
	
	if (FileExists(FileName))
	{
		FullPath = NewStr(FileName);
	}
	else
	{
		if (FileName)
		{
			// We're given the file name, and we should find the path.
			char *f = strrchr(FileName, DIR_CHAR);

#if DEBUG_RES_FILE
printf("%s:%i - f='%s'\n", _FL, f);
#endif

			if (f)
			{
				strcpy(File, f + 1);
			}
			else
			{
				strcpy(File, FileName);
			}

#if DEBUG_RES_FILE
printf("%s:%i - File='%s'\n", _FL, File);
#endif
		}
		else
		{
			// Need to look up the file associated by name with the current exe
			char Str[256];
			if (LgiGetExeFile(Str, sizeof(Str)))
			{
#if DEBUG_RES_FILE
printf("%s:%i - Str='%s'\n", _FL, Str);
#endif

				char *f = strrchr(Str, DIR_CHAR);
				if (f)
				{
					f++;
					
					#if WIN32
					char *Period = strrchr(f, '.');
					if (Period)
					{
						*Period = 0;
					}
					#endif

					strcpy(File, f);
				}

#if DEBUG_RES_FILE
printf("%s:%i - File='%s'\n", _FL, File);
#endif
			}
			else
			{
				LgiMsg(0, LgiLoadString(L_ERROR_RES_NO_EXE_PATH,
										"Fatal error: Couldn't get the path of the running\nexecutable. Can't find resource file."),
										"LgiResources::LgiResources");
				LgiExitApp();
			}
		}

		// Find the file..
		char *End = File + strlen(File);
		#ifdef MAC
		char *DotApp = stristr(File, ".app");
		if (DotApp) End = DotApp;
		#endif

#if DEBUG_RES_FILE
printf("%s:%i - File='%s'\n", _FL, File);
#endif

		strcpy(End, ".lr8");

#if DEBUG_RES_FILE
printf("%s:%i - File='%s'\n", _FL, File);
#endif

		FullPath = LgiFindFile(File);

#if DEBUG_RES_FILE
printf("%s:%i - FullPath='%s'\n", _FL, FullPath);
#endif

		if (!FullPath)
		{
			strcpy(End, ".lr");
			FullPath = LgiFindFile(File);
			if (!FullPath)
			{
				strcpy(End, ".lr8");
			}
		}
	}

	if (FullPath)
	{
		Load(FullPath);
		DeleteArray(FullPath);
	}
	else
	{
		char Msg[256];

		char Exe[256] = "(couldn't get exe path)";
		LgiGetExeFile(Exe, sizeof(Exe));

		// Prepare data
		sprintf(Msg,
				LgiLoadString(L_ERROR_RES_NO_LR8_FILE,
								"Couldn't find the file '%s' required to run this application\n(Exe='%s')"),
				File,
				Exe);

		// Dialog
		printf("%s", Msg);
		if (Warn)
		{
			LgiMsg(0, Msg, "LgiResources::LgiResources");

			// Exit
			LgiExitApp();
		}
	}
}

LgiResources::~LgiResources()
{
	ResourceContainers.Delete(this);
	LanguageNames.DeleteArrays();
	
	Dialogs.DeleteObjects();
	Strings.DeleteObjects();
	Menus.DeleteObjects();
	DeleteArray(Languages);
	DeleteObj(d);
}

ResFileFormat LgiResources::GetFormat()
{
	return d->Format;
}

bool LgiResources::IsOk()
{
	return d->Ok;
}

void LgiResources::AddLang(GLanguageId id)
{
	if (Languages)
	{
		// search through id's...
		GLanguageId *i;
		for (i=Languages; *i; i++)
		{
			if (stricmp(*i, id) == 0) return;
		}

		// add id..
		*i++ = id;
		*i++ = 0;
	}
}

char *LgiResources::GetFileName()
{
	return d->File;
}

bool LgiResources::Load(char *FileName)
{
	bool Status = false;

	DeleteArray(d->File);
	if (FileName)
	{
		GFile F;
		if (F.Open(FileName, O_READ))
		{
			d->File = NewStr(FileName);
			d->Format = Lr8File;
			char *Ext = LgiGetExtension(FileName);
			if (Ext AND stricmp(Ext, "lr") == 0)
			{
				d->Format = CodepageFile;
			}
			else if (Ext AND stricmp(Ext, "xml") == 0)
			{
				d->Format = XmlFile;
			}

			LgiStringRes::CurLang = LgiGetLanguageId();
			if (d->Format != CodepageFile)
			{
				LgiStringRes::CodePage = 0;
			}
			else
			{
				if (LgiStringRes::CurLang)
				{
					LgiStringRes::CodePage = LgiStringRes::CurLang->CodePage;
				}
			}

			GXmlTree x(0);
			GAutoPtr<GXmlTag> Root(new GXmlTag);
			if (x.Read(Root, &F, 0))
			{
				Status = true;

				for (GXmlTag *t = Root->Children.First(); t; )
				{
					if (t->Tag)
					{
						if (stricmp(t->Tag, "string-group") == 0)
						{
							bool IsString = true;
							char *Name = 0;
							if (Name = t->GetAttr("Name"))
							{
								IsString = stricmp("_Dialog Symbols_", Name) != 0;
							}

							for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
							{
								LgiStringRes *s = new LgiStringRes(this);
								if (s AND s->Read(c, d->Format))
								{
									// This code saves the names of the languages if specified in the LR8 files.
									char *Def = c->GetAttr("define");
									if (Def && !stricmp(Def, "IDS_LANGUAGE"))
									{
										for (int i=0; i<c->Attr.Length(); i++)
										{
											GXmlAttr &a = c->Attr[i];
											LanguageNames.Add(a.GetName(), NewStr(a.GetValue()));
										}
									}

									// Save the string for later.
									Strings.Insert(s);
									s->IsString = IsString;
									d->Ok = true;
								}
								else
								{
									LgiTrace("%s:%i - string read failed\n", _FL);
									DeleteObj(s);
								}
							}
						}
						else if (stricmp(t->Tag, "dialog") == 0)
						{
							LgiDialogRes *n = new LgiDialogRes(this);
							if (n AND n->Read(t, d->Format))
							{
								Dialogs.Insert(n);
								d->Ok = true;
								t->RemoveTag();
								t = 0;
							}
							else
							{
								LgiTrace("%s:%i - dialog read failed\n", _FL);
								DeleteObj(n);
							}
						}
						else if (stricmp(t->Tag, "menu") == 0)
						{
							LgiMenuRes *m = new LgiMenuRes(this);
							if (m AND m->Read(t, d->Format))
							{
								Menus.Insert(m);
								d->Ok = true;
								t->RemoveTag();
								t = 0;
							}
							else
							{
								LgiTrace("%s:%i - menu read failed\n", _FL);
								DeleteObj(m);
							}
						}
					}

					if (t)
						t = Root->Children.Next();
					else
						t = Root->Children.Current();
				}
			}
			else
			{
				LgiTrace("%s:%i - ParseXmlFile failed\n", _FL);
				LgiAssert(0);
			}
		}
		else
		{
			LgiTrace("%s:%i - Couldn't open '%s'.\n", _FL, FileName);
			LgiAssert(0);
		}
	}
	else
	{
		LgiTrace("%s:%i - No filename.\n", _FL);
		LgiAssert(0);
	}

	return Status;
}

LgiStringRes *LgiResources::StrFromRef(int Ref)
{
	for (LgiStringRes *s = Strings.First(); s; s = Strings.Next())
	{
		if (s->Ref == Ref)
		{
			return s;
		}
	}

	return 0;
}

char *LgiResources::StringFromId(int Id)
{
	LgiStringRes *NotStr = 0;

	List<LgiStringRes>::I it = Strings.Start();
	while (it.Each())
	{
		LgiStringRes *s = *it;
		if (s->Id == Id)
		{
			if (s->IsString)
			{
				return s->Str;
			}
			else
			{
				NotStr = s;
			}
		}
	}

	return NotStr ? NotStr->Str : 0;
}

char *LgiResources::StringFromRef(int Ref)
{
	for (LgiStringRes *s = Strings.First(); s; s = Strings.Next())
	{
		if (s->Ref == Ref)
		{
			return s->Str;
		}
	}

	return 0;
}

#include "GTextLabel.h"
#include "GEdit.h"
#include "GCheckBox.h"
#include "GButton.h"
#include "GRadioGroup.h"
#include "GTabView.h"
#include "GCombo.h"
#include "GBitmap.h"
#include "GSlider.h"
#include "GScrollBar.h"
#include "GTree.h"

ResObject *LgiResources::CreateObject(GXmlTag *t, ResObject *Parent)
{
	ResObject *Wnd = 0;
	if (t AND t->Tag)
	{
		char *Control = 0;
		
		if (stricmp(t->Tag, Res_StaticText) == 0)
		{
			Wnd = new GText(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->Tag, Res_EditBox) == 0)
		{
			Wnd = new GEdit(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->Tag, Res_CheckBox) == 0)
		{
			Wnd = new GCheckBox(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->Tag, Res_Button) == 0)
		{
			Wnd = new GButton(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->Tag, Res_Group) == 0)
		{
			Wnd = new GRadioGroup(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->Tag, Res_RadioBox) == 0)
		{
			Wnd = new GRadioButton(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->Tag, Res_TabView) == 0)
		{
			GTabView *Tv;
			if (Wnd = Tv = new GTabView(0, 10, 10, 100, 100, "GTabView"))
			{
				Tv->SetPourLargest(false);
				Tv->SetPourChildren(false);
			}
		}
		else if (stricmp(t->Tag, Res_Tab) == 0)
		{
			if (Parent)
			{
				// GView *p = (GView*) Parent;
				GTabView *Ctrl = dynamic_cast<GTabView*>(Parent);
				if (Ctrl)
				{
					Wnd = Ctrl->Append("<error>");
				}
			}
		}
		else if (stricmp(t->Tag, Res_ListView) == 0)
		{
			GList *w;
			Wnd = w = new GList(0, 0, 0, -1, -1, "");
			if (w)
			{
				w->Sunken(true);
			}
		}
		else if (stricmp(t->Tag, Res_Column) == 0)
		{
			if (Parent)
			{
				GList *Lst = dynamic_cast<GList*>(Parent);

				LgiAssert(Lst);

				if (Lst)
				{
					Wnd = Lst->AddColumn("");
				}
			}
		}
		else if (stricmp(t->Tag, Res_ComboBox) == 0)
		{
			Wnd = new GCombo(0, 0, 0, 100, 20, "");
		}
		else if (stricmp(t->Tag, Res_Bitmap) == 0)
		{
			Wnd = new GBitmap(0, 0, 0, 0);
		}
		else if (stricmp(t->Tag, Res_Progress) == 0)
		{
			Wnd = new GProgress(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->Tag, Res_Slider) == 0)
		{
			Wnd = new GSlider(0, 0, 0, -1, -1, "", false);
		}
		else if (stricmp(t->Tag, Res_ScrollBar) == 0)
		{
			Wnd = new GScrollBar(0, 0, 0, 20, 100, "");
		}
		else if (stricmp(t->Tag, Res_Progress) == 0)
		{
			Wnd = new GProgress(0, 0, 0, 1, 1, "");
		}
		else if (stricmp(t->Tag, Res_TreeView) == 0)
		{
			Wnd = new GTree(0, 0, 0, 1, 1, "");
		}
		else if (stricmp(t->Tag, Res_ControlTree) == 0)
		{
			GView *v = GViewFactory::Create("GControlTree");
			if (!(Wnd = dynamic_cast<ResObject*>(v)))
			{
				DeleteObj(v);
			}
		}
		else if (stricmp(t->Tag, Res_Custom) == 0)
		{
			Control = t->GetAttr("ctrl");
			GView *v = GViewFactory::Create(Control);
			if (v)
			{
				Wnd = dynamic_cast<ResObject*>(v);
				if (!Wnd)
				{
					// Not a "ResObject"
					LgiAssert(0);
					DeleteObj(v);
				}
			}
			else LgiAssert(!"No factory for control.");
		}
		else if (stricmp(t->Tag, Res_Table) == 0)
		{
			Wnd = new GTableLayout;
		}
	
		if (!Wnd)
		{
			printf(LgiLoadString(L_ERROR_RES_CREATE_OBJECT_FAILED,
								"LgiResources::CreateObject(%s) failed. (Ctrl=%s)\n"),
					t->Tag,
					Control);
		}
	}
	
	LgiAssert(Wnd);

	if (Wnd)
	{
		GView *v = dynamic_cast<GView*>(Wnd);
		if (v)
		{
			v->Script = ScriptEngine;
		}
	}

	return Wnd;
}

void LgiResources::Res_SetPos(ResObject *Obj, int x1, int y1, int x2, int y2)
{
	GListColumn *Col = dynamic_cast<GListColumn*>(Obj);
	if (Col)
	{
		Col->Width(x2-x1);
	}
	else
	{
		GView *w = CastToGWnd(Obj);
		if (w)
		{
			//#ifdef WIN32
			//GCombo *Cbo = dynamic_cast<GCombo*>(w);
			//if (Cbo)
			//{
			//	int y = y2-y1+1;
			//	y = max(y, 100);
			//	w->SetPos(GRect(x1, y1, x2, y1+y));
			//}
			//else
			//#endif
			{
				GRect n(x1, y1, x2, y2);
				w->SetPos(n);
			}
		}
	}
}

void LgiResources::Res_SetPos(ResObject *Obj, char *s)
{
	if (Obj AND s)
	{
		GToken T(s, ",");
		if (T.Length() == 4)
		{
			Res_SetPos(Obj, atoi(T[0]), atoi(T[1]), atoi(T[2]), atoi(T[3]));
		}
	}
}

bool LgiResources::Res_GetProperties(ResObject *Obj, GDom *Props)
{
	// this is a read-only system...
	return false;
}

bool LgiResources::Res_SetProperties(ResObject *Obj, GDom *Props)
{
	GView *v = dynamic_cast<GView*>(Obj);
	if (v AND Props)
	{
		GVariant i;
		if (Props->GetValue("enabled", i))
		{
			v->Enabled(i.CastInt32());
		}
		if (Props->GetValue("visible", i))
		{
			v->Visible(i.CastInt32());
		}

		GEdit *e = dynamic_cast<GEdit*>(v);
		if (e)
		{
			if (Props->GetValue("pw", i))
			{
				e->Password(i.CastInt32());
			}
			if (Props->GetValue("multiline", i))
			{
				e->MultiLine(i.CastInt32());
			}
		}

		return true;
	}
	return false;
}

GRect LgiResources::Res_GetPos(ResObject *Obj)
{
	GView *w = CastToGWnd(Obj);
	if (w)
	{
		return w->GetPos();
	}

	return GRect(0, 0, 0, 0);
}

int LgiResources::Res_GetStrRef(ResObject *Obj)
{
	return 0;
}

void LgiResources::Res_SetStrRef(ResObject *Obj, int Ref)
{
	for (LgiStringRes *s = Strings.First(); s; s = Strings.Next())
	{
		if (s->Ref == Ref)
		{
			GView *w = CastToGWnd(Obj);
			if (w)
			{
				w->Name(s->Str);
				w->SetId(s->Id);
			}
			else if (Obj)
			{
				GListColumn *Col = dynamic_cast<GListColumn*>(Obj);
				if (Col)
				{
					Col->Name(s->Str);
					return;
				}

				GTabPage *Page = dynamic_cast<GTabPage*>(Obj);
				if (Page)
				{
					Page->Name(s->Str);
				}
			}
			break;
		}
	}
}

void LgiResources::Res_Attach(ResObject *Obj, ResObject *Parent)
{
	GView *o = CastToGWnd(Obj);
	GView *p = CastToGWnd(Parent);
	GTabPage *Tab = dynamic_cast<GTabPage*>(Parent);
	if (o)
	{
		if (Tab)
		{
			if (Tab)
			{
				GRect r = o->GetPos();
				r.Offset(-4, -24);
				o->SetPos(r);

				Tab->Append(o);
			}
		}
		else if (p)
		{
			if (!p->IsAttached() OR
				!o->Attach(p))
			{
				p->AddView(o);
				o->SetParent(p);
			}
		}
		else
		{
			LgiAssert(p);
		}
	}
}

bool LgiResources::Res_GetChildren(ResObject *Obj, List<ResObject> *l, bool Deep)
{
	GView *o = CastToGWnd(Obj);
	if (o AND l)
	{
		GAutoPtr<GViewIterator> It(o->IterateViews());
		for (GViewI *w = It->First(); w; w = It->Next())
		{
			ResObject *n = dynamic_cast<ResObject*>(w);
			if (n)
			{
				l->Insert(n);
			}
		}
		return true;
	}
	return false;
}

void LgiResources::Res_Append(ResObject *Obj, ResObject *Parent)
{
	if (Obj AND Parent)
	{
		GListColumn *Col = dynamic_cast<GListColumn*>(Obj);
		GList *Lst = dynamic_cast<GList*>(Parent);
		if (Lst AND Col)
		{
			Lst->AddColumn(Col);
		}

		GTabPage *Tab = dynamic_cast<GTabPage*>(Obj);
		GTabView *Tabs = dynamic_cast<GTabView*>(Parent);
		if (Tab AND Tabs)
		{
			Tabs->Append(Tab);
		}
	}
}

bool LgiResources::Res_GetItems(ResObject *Obj, List<ResObject> *l)
{
	if (Obj AND l)
	{
		GList *Lst = dynamic_cast<GList*>(Obj);
		if (Lst)
		{
			for (int i=0; i<Lst->GetColumns(); i++)
			{
				l->Insert(Lst->ColumnAt(i));
			}
			return true;
		}

		GTabView *Tabs = dynamic_cast<GTabView*>(Obj);
		if (Tabs)
		{
			for (int i=0; i<Tabs->GetTabs(); i++)
			{
				l->Insert(Tabs->TabAt(i));
			}
			return true;
		}
	}
	return false;
}

GDom *LgiResources::Res_GetDom(ResObject *Obj)
{
	return dynamic_cast<GDom*>(Obj);
}

///////////////////////////////////////////////////////
LgiDialogRes::LgiDialogRes(LgiResources *res)
{
	Res = res;
	Dialog = 0;
	Str = 0;
}

LgiDialogRes::~LgiDialogRes()
{
	DeleteObj(Dialog);
}

bool LgiDialogRes::Read(GXmlTag *t, ResFileFormat Format)
{
	if (Dialog = t)
	{
		char *n = 0;
		if (n = Dialog->GetAttr("ref"))
		{
			int Ref = atoi(n);
			Str = Res->StrFromRef(Ref);
		}
		if (n = Dialog->GetAttr("pos"))
		{
			GToken T(n, ",");
			if (T.Length() == 4)
			{
				Pos.x1 = atoi(T[0]);
				Pos.y1 = atoi(T[1]);
				Pos.x2 = atoi(T[2]);
				Pos.y2 = atoi(T[3]);
			}
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
LgiMenuRes::LgiMenuRes(LgiResources *res)
{
	Res = res;
	Tag = 0;
}

LgiMenuRes::~LgiMenuRes()
{
	Strings.DeleteObjects();
	DeleteObj(Tag);
}

bool LgiMenuRes::Read(GXmlTag *t, ResFileFormat Format)
{
	Tag = t;
	if (t AND stricmp(t->Tag, "menu") == 0)
	{
		char *n;
		if (n = t->GetAttr("name"))
		{
			GObject::Name(n);
		}

		for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
		{
			if (stricmp(c->Tag, "string-group") == 0)
			{
				for (GXmlTag *i = c->Children.First(); i; i = c->Children.Next())
				{
					LgiStringRes *s = new LgiStringRes(Res);
					if (s AND s->Read(i, Format))
					{
						Strings.Insert(s);
					}
					else
					{
						DeleteObj(s);
					}
				}
				break;
			}
		}

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Dialog
bool GLgiRes::LoadFromResource(int Resource, GViewI *Parent, GRect *Pos, char *Name)
{
	LgiGetResObj();

	for (LgiResources  *r=LgiResources::ResourceContainers.First(); r;
						r=LgiResources::ResourceContainers.Next())
	{
		if (r->LoadDialog(Resource, Parent, Pos, Name))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////
// Get language
GLanguage *LgiGetLanguageId()
{
	// Check for command line override
	char Buf[64];
	if (LgiApp->GetOption("i", Buf))
	{
		GLanguage *i = GFindLang(Buf);
		if (i)
		{
			return i;
		}
	}

	// Check for config setting
	GXmlTag *LangConfig = LgiApp->GetConfig("Language");
	if (LangConfig)
	{
		char *Id;
		if ((Id = LangConfig->GetAttr("Id")) AND
			ValidStr(Id))
		{
			GLanguage *l = GFindLang(Id);
			if (l)
			{
				return l;
			}
		}
	}

	// Use a system setting
	#if defined WIN32

	int Lang = GetSystemDefaultLCID();

	TCHAR b[256];
	if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTLANGUAGE, b, sizeof(b)) > 0)
	{
		Lang = htoi(b);
	}

	int Primary = PRIMARYLANGID(Lang);
	int Sub = SUBLANGID(Lang);
	GLanguage *i, *English = 0;

	// Search for exact match
	for (i = LgiLanguageTable; i->Id; i++)
	{
		if (i->Win32Id == Lang)
		{
			return i;
		}
	}

	// Search for PRIMARYLANGID match
	for (i = LgiLanguageTable; i->Id; i++)
	{
		if (PRIMARYLANGID(i->Win32Id) == PRIMARYLANGID(Lang))
		{
			return i;
		}
	}

	#elif defined LINUX

	GLibrary *WmLib = LgiApp->GetWindowManagerLib();
	if (WmLib)
	{
		Proc_LgiWmGetLanguage GetLanguage = (Proc_LgiWmGetLanguage) WmLib->GetAddress("LgiWmGetLanguage");
		
		char Lang[256];
		if (GetLanguage AND
			GetLanguage(Lang))
		{
			GLanguage *l = GFindLang(Lang);
			if (l)
			{
				return l;
			}
		}
	}

	return GFindLang("en");

	/*
	// Read in KDE language setting
	char Path[256];
	if (LgiGetSystemPath(LSP_HOME, Path, sizeof(Path)))
	{
		LgiMakePath(Path, sizeof(Path), Path, ".kde/share/config/kdeglobals");
		char *Txt = ReadTextFile(Path);

		if (Txt)
		{
			extern bool _GetIniField(char *Grp, char *Field, char *In, char *Out, int OutSize);
			char Lang[256] = "";
			GLanguage *Ret = 0;
			if (_GetIniField("Locale", "Language", Txt, Lang, sizeof(Lang)))
			{
				GToken Langs(Lang, ":,; \t");
				for (int i=0; i<Langs.Length(); i++)
				{
					if (Ret = GFindLang(Langs[i]))
					{
						break;
					}
				}
			}
			DeleteArray(Txt);
			if (Ret)
			{
				return Ret;
			}
		}
	}
	*/
	
	#endif

	// Return a default of English
	return GFindLang("en");
}

///////////////////////////////////////////////////////////////////////////////////////
// Load string
char *LgiLoadString(int Res, char *Default)
{
	char *s = 0;
	LgiResources *r = LgiGetResObj();
	if (r)
	{
		// use to test for non-resource strings
		// return "@@@@";
		s = r->StringFromId(Res);
	}

	return s ? s : Default;
}

bool LgiResources::LoadDialog(int Resource, GViewI *Parent, GRect *Pos, char *Name, GEventsI *Engine)
{
	bool Status = false;

	if (Resource)
	{
		ScriptEngine = Engine;

		for (LgiDialogRes *Dlg = Dialogs.First(); Dlg; Dlg = Dialogs.Next())
		{
			if (Dlg->Id() == ((int) Resource))
			{
				// found the dialog to load, set properties
				if (Name AND Dlg->Name())
				{
					strcpy(Name, Dlg->Name());
				}
				
				if (Pos)
				{
					int x = Dlg->X();
					int y = Dlg->Y();
					#ifdef LINUX
					x -= 6;
					y -= 20;
					#else
					x += LgiApp->GetMetric(LGI_MET_DECOR_X) - 4;
					y += LgiApp->GetMetric(LGI_MET_DECOR_Y) - 18;
					#endif
					Pos->ZOff(x, y);
				}

				// instantiate control list
				for (GXmlTag *t = Dlg->Dialog->Children.First(); t; t = Dlg->Dialog->Children.Next())
				{
					ResObject *Obj = CreateObject(t, 0);
					if (Obj)
					{
						if (Res_Read(Obj, t))
						{
							GView *w = dynamic_cast<GView*>(Obj);
							if (w)
							{
								/*
								GRect r = w->GetPos();
								#ifdef WIN32
								r.Offset(-2, -16);
								#else
								r.Offset(-3, -17);
								#endif
								w->SetPos(r);
								*/

								Parent->AddView(w);
							}
						}
						else
						{
							LgiMsg(	NULL,
									LgiLoadString(	L_ERROR_RES_RESOURCE_READ_FAILED,
													"Resource read error, tag: %s"),
									"LgiResources::LoadDialog",
									MB_OK,
									t->Tag);
							break;
						}
					}
					else
					{
						LgiAssert(0);
					}
				}

				Status = true;
			}
		}
	}

	return Status;	
}

//////////////////////////////////////////////////////////////////////
// Menus
LgiStringRes *LgiMenuRes::GetString(GXmlTag *Tag)
{
	if (Tag)
	{
		char *n = Tag->GetAttr("ref");
		if (n)
		{
			int Ref = atoi(n);
			for (LgiStringRes *s = Strings.First(); s; s = Strings.Next())
			{
				if (s->Ref == Ref)
				{
					return s;
				}
			}
		}
	}

	return 0;
}

bool TagAdd(char *Tag, GHashTable &TagList)
{
	bool Add = true;
	if (Tag)
	{
		GToken t(Tag);
		for (int i=0; i<t.Length(); i++)
		{
			if (!TagList.Find(t[i]))
			{
				Add = false;
				break;
			}
		}
	}
	return Add;
}

bool GMenuLoader::Load(LgiMenuRes *MenuRes, GXmlTag *Tag, ResFileFormat Format, GHashTable &TagList)
{
	bool Status = false;

	if (Tag AND Tag->Tag)
	{
		if (stricmp(Tag->Tag, "menu") == 0)
		{
			#if WIN32NATIVE
			if (!Info)
			{
				Info = ::CreateMenu();
			}
			#endif
		}

		#if WIN32NATIVE
		if (Info)
		#endif
		{
			Status = true;
			for (GXmlTag *t = Tag->Children.First(); t AND Status; t = Tag->Children.Next())
			{
				if (stricmp(t->Tag, "submenu") == 0)
				{
					LgiStringRes *Str = MenuRes->GetString(t);
					if (Str AND Str->Str)
					{
						bool Add = TagAdd(Str->Tag, TagList);
						GSubMenu *Sub = AppendSub(Str->Str);
						if (Sub)
						{
							GMenuItem *p = Sub->GetParent();
							if (p)
							{
								p->Id(Str->Id);
							}
							
							Status = Sub->Load(MenuRes, t, Format, TagList);

							if (!Add)
							{
								Sub->GetParent()->Remove();
								delete Sub->GetParent();
							}
						}
					}
					else
					{
						LgiAssert(0);
					}
				}
				else if (stricmp(t->Tag, "menuitem") == 0)
				{
					char *n = 0;
					if ((n = t->GetAttr("sep")) AND atoi(n) != 0)
					{
						AppendSeparator();
					}
					else
					{
						LgiStringRes *Str = MenuRes->GetString(t);
						if (Str AND Str->Str)
						{
							if (TagAdd(Str->Tag, TagList))
							{
								int Enabled = (n = t->GetAttr("enabled")) ? atoi(n) : true;
								char *Shortcut = t->GetAttr("shortcut");
								Status = AppendItem(Str->Str, Str->Id, Enabled, -1, Shortcut) != 0;
							}
							else Status = true;
						}
						else
						{
							LgiAssert(0);
						}
					}
				}
			}
		}
	}
	return Status;
}

bool GMenu::Load(GView *w, char *Res, char *TagList)
{
	bool Status = false;

	LgiResources *r = LgiGetResObj();
	if (r)
	{
		GHashTable Tags;
		GToken Toks(TagList);
		for (int i=0; i<Toks.Length(); i++)
		{
			Tags.Add(Toks[i]);
		}

		for (LgiMenuRes *m = r->Menus.First(); m; m = r->Menus.Next())
		{
			if (stricmp(m->Name(), Res) == 0)
			{
				#if WIN32NATIVE
				Status = GSubMenu::Load(m, m->Tag, r->GetFormat(), Tags);
				#else
				Status = GMenuLoader::Load(m, m->Tag, r->GetFormat(), Tags);
				#endif
				break;
			}
		}
	}

	return Status;
}

LgiResources *LgiGetResObj(bool Warn, char *filename)
{
	if (LgiResources::ResourceContainers.Length() == 0)
	{
		new LgiResources(filename, Warn);
	}

	return LgiResources::ResourceContainers.First();
}
