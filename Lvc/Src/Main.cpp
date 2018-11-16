#include "Lgi.h"
#include "../Resources/resdefs.h"
#include "Lvc.h"
#include "GTableLayout.h"
#ifdef WINDOWS
#include "../Resources/resource.h"
#endif
#include "GTextLog.h"
#include "GButton.h"
#include "GXmlTreeUi.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "Lvc";

VersionCtrl DetectVcs(const char *Path)
{
	char p[MAX_PATH];

	if (!Path)
		return VcNone;

	if (LgiMakePath(p, sizeof(p), Path, ".git") &&
		DirExists(p))
		return VcGit;

	if (LgiMakePath(p, sizeof(p), Path, ".svn") &&
		DirExists(p))
		return VcSvn;

	if (LgiMakePath(p, sizeof(p), Path, ".hg") &&
		DirExists(p))
		return VcHg;

	if (LgiMakePath(p, sizeof(p), Path, "CVS") &&
		DirExists(p))
		return VcCvs;

	return VcNone;
}


class DiffView : public GTextLog
{
public:
	DiffView(int id) : GTextLog(id)
	{
	}

	void PourStyle(size_t Start, ssize_t Length)
	{
		for (auto ln : GTextView3::Line)
		{
			if (!ln->c.IsValid())
			{
				char16 *t = Text + ln->Start;
				
				if (*t == '+')
				{
					ln->c = GColour::Green;
					ln->Back.Rgb(245, 255, 245);
				}
				else if (*t == '-')
				{
					ln->c = GColour::Red;
					ln->Back.Rgb(255, 245, 245);
				}
				else if (*t == '@')
				{
					ln->c.Rgb(128, 128, 128);
					ln->Back.Rgb(235, 235, 235);
				}
				else
					ln->c.Set(LC_TEXT, 24);
			}
		}
	}
};

class ToolBar : public GLayout, public GLgiRes
{
public:
	ToolBar()
	{
		GAutoString Name;
		GRect Pos;
		if (LoadFromResource(IDD_TOOLBAR, this, &Pos, &Name))
		{
			GTableLayout *v;
			if (GetViewById(IDC_TABLE, v))
			{
				GRect r = v->GetPos();
				r.Offset(-r.x1, -r.y1);
				r.x2++;
				v->SetPos(r);
				
				v->OnPosChange();
				r = v->GetUsedArea();
				if (r.Y() <= 1)
					r.Set(0, 0, 30, 30);
				GetCss(true)->Height(GCss::Len(GCss::LenPx, (float)r.Y()+3));
			}
			else LgiAssert(!"Missing table ctrl");
		}
		else LgiAssert(!"Missing toolbar resource");
	}

	void OnCreate()
	{
		AttachChildren();
	}

	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();
	}
};

class CommitCtrls : public GLayout, public GLgiRes
{
public:
	CommitCtrls()
	{
		GAutoString Name;
		GRect Pos;
		if (LoadFromResource(IDD_COMMIT, this, &Pos, &Name))
		{
			GTableLayout *v;
			if (GetViewById(IDC_COMMIT_TABLE, v))
			{
				v->GetCss(true)->PaddingRight("8px");
				GRect r = v->GetPos();
				r.Offset(-r.x1, -r.y1);
				r.x2++;
				v->SetPos(r);
				
				v->OnPosChange();
				r = v->GetUsedArea();
				if (r.Y() <= 1)
					r.Set(0, 0, 30, 30);
				GetCss(true)->Height(GCss::Len(GCss::LenPx, (float)r.Y()));
			}
			else LgiAssert(!"Missing table ctrl");
		}
		else LgiAssert(!"Missing toolbar resource");
	}

	void OnPosChange()
	{
		GTableLayout *v;
		if (GetViewById(IDC_COMMIT_TABLE, v))
			v->SetPos(GetClient());
	}

	void OnCreate()
	{
		AttachChildren();
	}
};

GString::Array GetProgramsInPath(const char *Program)
{
	GString::Array Bin;
	GString Prog = Program;
	#ifdef WINDOWS
	Prog += LGI_EXECUTABLE_EXT;
	#endif

	GString::Array a = LGetPath();
	for (auto p : a)
	{
		GFile::Path c(p, Prog);
		if (c.Exists())
			Bin.New() = c.GetFull();
	}

	return Bin;
}

class OptionsDlg : public GDialog, public GXmlTreeUi
{
	GOptionsFile &Opts;

public:
	OptionsDlg(GViewI *Parent, GOptionsFile &opts) : Opts(opts)
	{
		SetParent(Parent);
		Map("svn-path", IDC_SVN, GV_STRING);
		Map("git-path", IDC_GIT, GV_STRING);
		Map("hg-path", IDC_HG, GV_STRING);
		Map("cvs-path", IDC_CVS, GV_STRING);

		if (LoadFromResource(IDD_OPTIONS))
		{
			MoveSameScreen(Parent);
			Convert(&Opts, this, true);
		}
	}

	void Browse(int EditId)
	{
		GFileSelect s;
		s.Parent(this);
		if (s.Open())
		{
			SetCtrlName(EditId, s.Name());
		}
	}

	void BrowseFiles(GViewI *Ctrl, const char *Bin, int EditId)
	{
		GRect Pos = Ctrl->GetPos();
		GdcPt2 Pt(Pos.x1, Pos.y2 + 1);
		PointToScreen(Pt);
		
		GSubMenu s;

		GString::Array Bins = GetProgramsInPath(Bin);
		for (unsigned i=0; i<Bins.Length(); i++)
		{
			s.AppendItem(Bins[i], 1000+i);
		}

		if (Bins.Length() == 0)
		{
			Browse(EditId);
		}
		else
		{
			s.AppendSeparator();
			s.AppendItem("Browse...", 1);
			int Cmd = s.Float(this, Pt.x, Pt.y, GSubMenu::BtnLeft);
			switch (Cmd)
			{
				case 1:
					Browse(EditId);
					break;
				default:
					if (Cmd >= 1000)
					{
						GString Bin = Bins[Cmd - 1000];
						if (Bin)
							SetCtrlName(EditId, Bin);
					}
					break;
			}
		}
	}

	int OnNotify(GViewI *Ctrl, int Flags)
	{
		switch (Ctrl->GetId())
		{
			case IDC_SVN_BROWSE:
				BrowseFiles(Ctrl, "svn", IDC_SVN);
				break;
			case IDC_GIT_BROWSE:
				BrowseFiles(Ctrl, "git", IDC_GIT);
				break;
			case IDC_HG_BROWSE:
				BrowseFiles(Ctrl, "hg", IDC_HG);
				break;
			case IDC_CVS_BROWSE:
				BrowseFiles(Ctrl, "cvs", IDC_CVS);
				break;
			case IDOK:
				Convert(&Opts, this, false);
				// fall
			case IDCANCEL:
			{
				EndModal(Ctrl->GetId() == IDOK);
				break;
			}
		}

		return GDialog::OnNotify(Ctrl, Flags);
	}
};

class App : public GWindow, public AppPriv
{
	GAutoPtr<GImageList> ImgLst;

public:
	App()
	{
		GString AppRev;
		AppRev.Printf("%s v%s", AppName, APP_VERSION);
		Name(AppRev);

		GRect r(0, 0, 1400, 800);
		SetPos(r);
		MoveToCenter();
		SetQuitOnClose(true);

		#ifdef WINDOWS
		SetIcon(MAKEINTRESOURCEA(IDI_ICON1));
		#else
		SetIcon("icon32.png");
		#endif

		ImgLst.Reset(LgiLoadImageList("image-list.png", 16, 16));

		if (Attach(0))
		{
			if ((Menu = new GMenu))
			{
				Menu->SetPrefAndAboutItems(IDM_OPTIONS, IDM_ABOUT);
				Menu->Attach(this);
				Menu->Load(this, "IDM_MENU");
			}

			GBox *ToolsBox = new GBox(IDC_TOOLS_BOX, true, "ToolsBox");
			GBox *FoldersBox = new GBox(IDC_FOLDERS_BOX, false, "FoldersBox");
			GBox *CommitsBox = new GBox(IDC_COMMITS_BOX, true, "CommitsBox");

			ToolBar *Tools = new ToolBar;

			ToolsBox->Attach(this);
			Tools->Attach(ToolsBox);
			FoldersBox->Attach(ToolsBox);

			Tree = new GTree(IDC_TREE, 0, 0, 200, 200);
			Tree->GetCss(true)->Width(GCss::Len("320px"));
			Tree->ShowColumnHeader(true);
			Tree->AddColumn("Folder", 250);
			Tree->AddColumn("Counts", 50);
			Tree->Attach(FoldersBox);
			Tree->SetImageList(ImgLst, false);
			CommitsBox->Attach(FoldersBox);

			Lst = new LList(IDC_LIST, 0, 0, 200, 200);
			Lst->Attach(CommitsBox);
			Lst->GetCss(true)->Height("40%");
			Lst->AddColumn("---", 40);
			Lst->AddColumn("Commit", 270);
			Lst->AddColumn("Author", 240);
			Lst->AddColumn("Date", 130);
			Lst->AddColumn("Message", 400);

			GBox *FilesBox = new GBox(IDC_FILES_BOX, false);
			FilesBox->Attach(CommitsBox);

			Files = new LList(IDC_FILES, 0, 0, 200, 200);
			Files->GetCss(true)->Width("35%");
			Files->Attach(FilesBox);
			Files->AddColumn("[ ]", 30);
			Files->AddColumn("State", 100);
			Files->AddColumn("Name", 400);

			GBox *MsgBox = new GBox(IDC_MSG_BOX, true);
			MsgBox->Attach(FilesBox);
			
			CommitCtrls *Commit = new CommitCtrls;
			Commit->Attach(MsgBox);
			Commit->GetCss(true)->Height("25%");
			if (Commit->GetViewById(IDC_MSG, Msg))
			{
				GTextView3 *Tv = dynamic_cast<GTextView3*>(Msg);
				if (Tv)
				{
					Tv->Sunken(true);
					Tv->SetWrapType(TEXTED_WRAP_NONE);
				}
			}
			else LgiAssert(!"No ctrl?");

			Tabs = new GTabView(IDC_TAB_VIEW);
			Tabs->Attach(MsgBox);
			const char *Style = "Padding: 0px 8px 8px 0px";
			Tabs->GetCss(true)->Parse(Style);

			GTabPage *p = Tabs->Append("Diff");
			p->Append(Diff = new DiffView(IDC_TXT));
			// Diff->Sunken(true);
			Diff->SetWrapType(TEXTED_WRAP_NONE);

			p = Tabs->Append("Log");
			p->Append(Log = new GTextLog(IDC_LOG));
			// Log->Sunken(true);
			Log->SetWrapType(TEXTED_WRAP_NONE);

			AttachChildren();
			Visible(true);
		}

		Opts.SerializeFile(false);
		GXmlTag *f = Opts.LockTag(OPT_Folders, _FL);
		if (!f)
		{
			Opts.CreateTag(OPT_Folders);
			f = Opts.LockTag(OPT_Folders, _FL);
		}
		if (f)
		{
			bool Req[VcMax] = {0};
			
			for (GXmlTag *c = f->Children.First(); c; c = f->Children.Next())
			{
				if (c->IsTag(OPT_Folder))
				{
					auto f = new VcFolder(this, c);
				
					Tree->Insert(f);
					
					if (!Req[f->GetType()])
					{
						Req[f->GetType()] = true;
						f->GetVersion();
					}
				}
			}
			Opts.Unlock();
		}

		SetPulse(200);
	}

	~App()
	{
		GXmlTag *f = Opts.LockTag(OPT_Folders, _FL);
		if (f)
		{
			f->EmptyChildren();

			VcFolder *vcf = NULL; bool b;
			while ((b = Tree->Iterate(vcf)))
				f->InsertTag(vcf->Save());
			Opts.Unlock();
		}
		Opts.SerializeFile(true);
	}

	int OnCommand(int Cmd, int Event, OsView Wnd)
	{
		switch (Cmd)
		{
			case IDM_OPTIONS:
			{
				OptionsDlg Dlg(this, Opts);
				Dlg.DoModal();
				break;
			}
		}

		return 0;
	}

	void OnPulse()
	{
		VcFolder *vcf = NULL; bool b;
		while ((b = Tree->Iterate(vcf)))
			vcf->OnPulse();
	}

	void OpenFolder()
	{
		GFileSelect s;
		s.Parent(this);
		if (s.OpenFolder())
		{
			if (DetectVcs(s.Name()))
				Tree->Insert(new VcFolder(this, s.Name()));
			else
				LgiMsg(this, "Folder not under version control.", AppName);
		}
	}

	int OnNotify(GViewI *c, int flag)
	{
		switch (c->GetId())
		{
			case IDC_FILES:
			{
				switch (flag)
				{
					case GNotifyItem_ColumnClicked:
					{
						int Col = -1;
						GMouse m;
						if (Files->GetColumnClickInfo(Col, m))
						{
							if (Col == 0)
							{
								// Select / deselect all checkboxes..
								List<VcFile> n;
								if (Files->GetAll(n))
								{
									bool Checked = false;
									for (VcFile *f = n.First(); f; f = n.Next())
										Checked |= f->Checked() > 0;
									for (VcFile *f = n.First(); f; f = n.Next())
										f->Checked(Checked ? 0 : 1);
								}
							}
						}
						break;
					}
				}
				break;
			}
			case IDC_OPEN:
			{
				OpenFolder();
				break;
			}
			case IDC_TREE:
			{
				switch (flag)
				{
					case GNotifyContainer_Click:
					{
						GMouse m;
						c->GetMouse(m);
						if (m.Right())
						{
							GSubMenu s;
							s.AppendItem("Add", IDM_ADD);
							int Cmd = s.Float(c->GetGView(), m);
							switch (Cmd)
							{
								case IDM_ADD:
								{
									OpenFolder();
									break;
								}
							}
						}
						break;
					}
					case LvcCommandStart:
					{
						SetCtrlEnabled(IDC_PUSH, false);
						SetCtrlEnabled(IDC_PUSH_ALL, false);
						SetCtrlEnabled(IDC_PULL, false);
						SetCtrlEnabled(IDC_PULL_ALL, false);
						break;
					}
					case LvcCommandEnd:
					{
						SetCtrlEnabled(IDC_PUSH, true);
						SetCtrlEnabled(IDC_PUSH_ALL, true);
						SetCtrlEnabled(IDC_PULL, true);
						SetCtrlEnabled(IDC_PULL_ALL, true);
						break;
					}
				}
				break;
			}
			case IDC_CLEAR_FILTER:
			{
				SetCtrlName(IDC_FILTER, NULL);
				// Fall through
			}
			case IDC_FILTER:
			{
				VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
				if (f)
					f->Select(true);
				break;
			}
			case IDC_COMMIT_AND_PUSH:
			case IDC_COMMIT:
			{
				const char *Msg = GetCtrlName(IDC_MSG);
				if (ValidStr(Msg))
				{
					VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
					if (f)
					{
						char *Branch = GetCtrlName(IDC_BRANCH);
						bool AndPush = c->GetId() == IDC_COMMIT_AND_PUSH;
						f->Commit(Msg, ValidStr(Branch) ? Branch : NULL, AndPush);
					}
				}
				else LgiMsg(this, "No message for commit.", AppName);
				break;
			}
			case IDC_PUSH:
			{
				VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
				if (f)
					f->Push();
				break;
			}
			case IDC_PULL:
			{
				VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
				if (f)
					f->Pull();
				break;
			}
			case IDC_PULL_ALL:
			{
				GArray<VcFolder*> Folders;
				Tree->GetAll(Folders);
				for (auto f : Folders)
				{
					f->Pull(LogSilo);
				}
				break;
			}
			case IDC_STATUS:
			{
				GArray<VcFolder*> Folders;
				Tree->GetAll(Folders);
				for (auto f : Folders)
				{
					f->FolderStatus();
				}
				break;
			}
		}

		return 0;
	}
};

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

