#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "GProcess.h"
#include "GDocView.h"
#include "GToken.h"

/////////////////////////////////////////////////////////////////////////////////////
FindInFiles::FindInFiles(AppWnd *app)
{
	TypeHistory = 0;
	FolderHistory = 0;

	SetParent(App = app);
	Params = new FindParams;
	if (LoadFromResource(IDD_FIND_IN_FILES))
	{
		MoveToCenter();

		if (GetViewById(IDC_TYPE_HISTORY, TypeHistory))
			TypeHistory->SetTargetId(IDC_FILE_TYPES);

		if (GetViewById(IDC_FOLDER_HISTORY, FolderHistory))
			FolderHistory->SetTargetId(IDC_DIR);
	}
}

FindInFiles::~FindInFiles()
{
	DeleteObj(Params);
}

void SerializeHistory(GHistory *h, char *opt, GOptionsFile *p, bool Write)
{
	if (h AND p)
	{
		GVariant v;
		if (Write)
		{
			GStringPipe b;
			char *s;
			for (s=h->First(); s; s=h->Next())
			{
				if (b.GetSize()) b.Push("|");
				b.Push(s);
			}
			if (s = b.NewStr())
			{
				p->SetValue(opt, v = s);
				DeleteArray(s);
			}
		}
		else
		{
			if (p->GetValue(opt, v))
			{
				GToken t(v.Str(), "|");
				h->DeleteArrays();
				for (int i=0; i<t.Length(); i++)
				{
					h->Insert(NewStr(t[i]));
				}
				h->Update();
			}
		}
	}
}

void FindInFiles::OnCreate()
{
	if (Params)
	{
		SetCtrlName(IDC_LOOK_FOR, Params->Text);
		SetCtrlName(IDC_FILE_TYPES, Params->Ext);
		SetCtrlName(IDC_DIR, Params->Dir);
		
		SetCtrlValue(IDC_WHOLE_WORD, Params->MatchWord);
		SetCtrlValue(IDC_CASE, Params->MatchCase);
		SetCtrlValue(IDC_SUB_DIRS, Params->SubDirs);
		
		SerializeHistory(TypeHistory, "TypeHist", App->GetOptions(), false);
		SerializeHistory(FolderHistory, "FolderHist", App->GetOptions(), false);
	}
}

int FindInFiles::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDC_SET_DIR:
		{
			GFileSelect s;
			s.Parent(this);
			s.InitialDir(GetCtrlName(IDC_DIR));
			if (s.OpenFolder())
			{
				SetCtrlName(IDC_DIR, s.Name());
			}
			break;
		}
		case IDOK:
		{
			Params->Text = NewStr(GetCtrlName(IDC_LOOK_FOR));
			Params->Ext = NewStr(GetCtrlName(IDC_FILE_TYPES));
			Params->Dir = NewStr(GetCtrlName(IDC_DIR));
			
			Params->MatchWord = GetCtrlValue(IDC_WHOLE_WORD);
			Params->MatchCase = GetCtrlValue(IDC_CASE);
			Params->SubDirs = GetCtrlValue(IDC_SUB_DIRS);
		
			if (TypeHistory) TypeHistory->Add(Params->Ext);
			SerializeHistory(TypeHistory, "TypeHist", App->GetOptions(), true);
			if (FolderHistory) FolderHistory->Add(Params->Dir);
			SerializeHistory(FolderHistory, "FolderHist", App->GetOptions(), true);
			
			EndModal(1);
			break;
		}
		case IDCANCEL:
		{
			EndModal(0);
			break;
		}
	}
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
class FindInFilesThreadPrivate
{
public:
	AppWnd *App;
	FindParams *Params;
	bool Loop;
	GStringPipe Pipe;
	int64 Last;
};

FindInFilesThread::FindInFilesThread(AppWnd *App, FindParams *Params)
{
	d = new FindInFilesThreadPrivate;
	d->App = App;
	d->Params = Params;
	d->Loop = true;
	d->Last = 0;
	
	DeleteOnExit = true;	
	Run();
}

FindInFilesThread::~FindInFilesThread()
{
	DeleteObj(d->Params);
	DeleteObj(d);
}

void FindInFilesThread::Stop()
{
	d->Loop = false;
}

void FindInFilesThread::SearchFile(char *File)
{
	if (File AND ValidStr(d->Params->Text))
	{
		char *Doc = ReadTextFile(File);
		if (Doc)
		{
			int Len = strlen(d->Params->Text);
			
			char *LineStart = 0;
			int Line = 0;
			for (char *s = Doc; *s AND d->Loop; s++)
			{
				if (*s == '\n')
				{
					Line++;
					LineStart = 0;
				}
				else
				{
					if (!LineStart)
						LineStart = s;
						
					bool Match = false;
					if (d->Params->MatchCase)
					{
						if (d->Params->Text[0] == *s)
						{
							Match = strncmp(s, d->Params->Text, Len) == 0;
						}
					}
					else
					{
						if (toupper(d->Params->Text[0]) == toupper(*s))
						{
							Match = strnicmp(s, d->Params->Text, Len) == 0;
						}
					}
					
					if (Match)
					{
						char *Eol = s + Len;
						while (*Eol AND *Eol != '\n') Eol++;
						int LineLen = (int)Eol - (int)LineStart;						
						
						bool StartOk = true;
						bool EndOk = true;
						if (d->Params->MatchWord)
						{
							if (s > Doc)
							{
								StartOk = IsWordBoundry(s[-1]);
							}
							
							EndOk = IsWordBoundry(s[Len]);
						}
						
						if (StartOk AND EndOk)
						{
							static char Buf[1024];
							int Chars = snprintf(Buf, sizeof(Buf), "%s:%i:%.*s\n", File, Line + 1, LineLen, LineStart);
							d->Pipe.Push(Buf, Chars);
							
							int64 Now = LgiCurrentTime();
							if (Now > d->Last  + 500)
							{
								d->App->PostEvent(M_APPEND_TEXT, (int)d->Pipe.NewStr(), 2);
							}
						}
						s = Eol - 1;
					}
				}
			}

			DeleteArray(Doc);
		}
		else
		{
			printf("Couldn't Read file.\n");
		}
	}
}

bool FindInFilesCallback(char *Path, GDirectory *Dir)
{
	if (Dir->IsDir())
	{
		char *p = Dir->GetName();
		if (p AND stricmp(p, ".svn") == 0) return false;
	}
	
	return true;
}

int FindInFilesThread::Main()
{
	if (d->App AND
		d->Params AND
		ValidStr(d->Params->Text))
	{
		char Msg[256];

		snprintf(Msg, sizeof(Msg), "Searching for '%s'...\n", d->Params->Text);
		d->App->PostEvent(M_APPEND_TEXT, 0, 2);
		d->App->PostEvent(M_APPEND_TEXT, (int)NewStr(Msg), 2);

		GArray<char*> Ext;
		GToken e(d->Params->Ext, ";, ");
		for (int i=0; i<e.Length(); i++)
		{
			Ext.Add(e[i]);
		}
		
		GArray<char*> Files;
		if (LgiRecursiveFileSearch(d->Params->Dir, &Ext, &Files, 0, 0, FindInFilesCallback))
		{
			sprintf(Msg, "in %i files...\n", Files.Length());
			d->App->PostEvent(M_APPEND_TEXT, (int)NewStr(Msg), 2);
			
			for (int i=0; i<Files.Length(); i++)
			{
				char *f = Files[i];
				char *Dir = strrchr(f, DIR_CHAR);
				if (!Dir OR Dir[1] != '.')
				{
					SearchFile(f);
				}
			}
			
			char *Str = d->Pipe.NewStr();
			if (Str)
			{
				d->App->PostEvent(M_APPEND_TEXT, (int)Str, 2);
			}
			
			Files.DeleteArrays();

			d->App->PostEvent(M_APPEND_TEXT, (int)NewStr("Done.\n"), 2);
		}
		else
		{
			d->App->PostEvent(M_APPEND_TEXT, (int)NewStr("No files matched.\n"), 2);
		}
		
		d->App->OnFindFinished();
	}

	return 0;
}
