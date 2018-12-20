#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <ctype.h>

#include "Lgi.h"
#include "GProcess.h"
#include "GSkinEngine.h"
#include "GArray.h"
#include "LgiWinManGlue.h"
#include "GVariant.h"
#if defined(WIN32) && defined(__GTK_H__)
#include "../win32/GSymLookup.h"
#else
#include "GSymLookup.h"
#endif
#include "GToken.h"
#include "GFontCache.h"

#if HAS_LIB_MAGIC
// sudo apt-get install libmagic-dev
#include <magic.h>
#endif

#define DEBUG_MSG_TYPES				0
#define DEBUG_HND_WARNINGS			0

#ifndef WIN32
namespace Gtk {
#include <gdk/gdkx.h>
#undef Status
}
#endif

typedef GArray<GAppInfo*> AppArray;
using namespace Gtk;

__thread int GtkLockCount = 0;
bool GlibWidgetSearch(GtkWidget *p, GtkWidget *w, bool Debug, int depth = 0);

////////////////////////////////////////////////////////////////
struct OsAppArgumentsPriv
{
	::GArray<char*> Ptr;
	
	~OsAppArgumentsPriv()
	{
		Ptr.DeleteArrays();
	}
};

OsAppArguments::OsAppArguments(int args, const char **arg)
{
	d = new OsAppArgumentsPriv;

	for (int i=0; i<args; i++)
	{
		d->Ptr.Add(NewStr(arg[i]));
	}
	
	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];
}

OsAppArguments::~OsAppArguments()
{
	DeleteObj(d);
}

void OsAppArguments::Set(char *CmdLine)
{
	d->Ptr.DeleteArrays();
	
	if (!CmdLine)
		return;
	
	for (char *s = CmdLine; *s; )
	{
		while (*s && strchr(WhiteSpace, *s)) s++;
		if (*s == '\'' || *s == '\"')
		{
			char delim = *s++;
			char *e = strchr(s, delim);
			if (e)
				d->Ptr.Add(NewStr(s, e - s));
			else
				break;
			
			s = e + 1;
		}
		else
		{
			char *e = s;
			while (*e && !strchr(WhiteSpace, *e))
				e++;
			d->Ptr.Add(NewStr(s, e-s));
			s = e;
		}
	}	

	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];
}

OsAppArguments &OsAppArguments::operator =(OsAppArguments &a)
{
	d->Ptr.DeleteArrays();

	for (int i=0; i<a.Args; i++)
	{
		if (a.Arg[i])
			d->Ptr.Add(NewStr(a.Arg[i]));
	}
	
	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];

	return *this;
}

////////////////////////////////////////////////////////////////
#if HAS_SHARED_MIME

#include "GFilterUtils.h"
#include "mime-types.h"

class GSharedMime : public GLibrary
{
public:
	GSharedMime() :
		#ifdef _DEBUG
		GLibrary("libsharedmime1d")
		#else
		GLibrary("libsharedmime1")
		#endif
	{
	}
	
	DynFunc0(int, mimetypes_init);
	DynFunc1(const char*, mimetypes_set_default_type, const char *, default_type);	
	DynFunc2(const char*, mimetypes_get_file_type, const char*, pathname, mimetypes_flags, flags);
	DynFunc2(const char*, mimetypes_get_data_type, const void*, data, int, length);
	DynFunc3(bool, mimetypes_decode, const char *, type, char **, media_type, char **, sub_type);
	DynFunc2(char *, mimetypes_convert_filename, const char *, pathname, const char *, mime_type);
	DynFunc3(bool, mimetypes_add_mime_dir, const char *, path, bool, high_priority, bool, rescan);
	DynFunc2(const char *, mimetypes_get_type_info, const char *, mime_type, const char *, lang);
};

#endif

#if 1
/////////////////////////////////////////////////////////////////////////////
//
// Attempts to cleanup and call drkonqi to process the crash
//
void LgiCrashHandler(int Sig)
{
	// Don't get into an infinite loop
   	signal(SIGSEGV, SIG_DFL);

	#ifndef _MSC_VER
	// Our pid
	int MyPid = getpid();	
	printf("LgiCrashHandler trigger MyPid=%i\n", MyPid);
	#endif

	exit(-1);
}
#endif

/////////////////////////////////////////////////////////////////////////////
#ifndef XK_Num_Lock
#define XK_Num_Lock                      0xff7f
#endif
#ifndef XK_Shift_Lock
#define XK_Shift_Lock                    0xffe6
#endif
#ifndef XK_Caps_Lock
#define XK_Caps_Lock                     0xffe5
#endif

struct Msg
{
	GViewI *v;
	int m;
	GMessage::Param a, b;
	
	void Set(GViewI *V, int M, GMessage::Param A, GMessage::Param B)
	{
		v = V;
		m = M;
		a = A;
		b = B;
	}
};

// Out of thread messages... must lock before access.
class LMessageQue : public LMutex
{
public:
	typedef ::GArray<Msg> MsgArray;

	LMessageQue() : LMutex("LMessageQue")
	{
	}

	MsgArray *Lock(const char *file, int line)
	{
		if (!LMutex::Lock(file, line))
			return NULL;
		return &q;
	}
	
	operator bool()
	{
		return q.Length() > 0;
	}

private:
	MsgArray q;

}	MsgQue;

class GAppPrivate : public GSymLookup
{
public:
	// Common
	GXmlTag *Config;
	GFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	GLibrary *SkinLib;
	LHashTbl<StrKey<char,false>,AppArray*> MimeToApp;
	#if HAS_SHARED_MIME
	GSharedMime *Sm;
	#endif
	GLibrary *WmLib;
	LHashTbl<IntKey<int>, GView*> Handles;
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	int MessageLoopDepth;
	int CurEvent;
	#if DEBUG_MSG_TYPES
	GArray<int> Types;
	#endif
	::GArray<GViewI*> DeleteLater;
	GMouse LastMove;
	GAutoString Name;
	
	/// Desktop info
	GAutoPtr<GApp::DesktopInfo> DesktopInfo;

	#if HAS_LIB_MAGIC
	LMutex MagicLock;
	magic_t hMagic;
	#endif

	/// Any fonts needed for styling the elements
	GAutoPtr<GFontCache> FontCache;
	
	// Clipboard handling
	int Clipboard, Utf8, Utf8String;
	::GVariant ClipData;
	LHashTbl<IntKey<int>, ::GVariant*> ClipNotify;

	// Mouse click info
	uint64 LastClickTime;
	OsView LastClickWnd;
	int LastClickX;
	int LastClickY;

	GAppPrivate() : Args(0, 0), MagicLock("MagicLock")
	{
		CurEvent = 0;
		GuiThread = LgiGetCurrentThread();
		GuiThreadId = GetCurrentThreadId();
		WmLib = 0;
		FileSystem = 0;
		GdcSystem = 0;
		Config = 0;
		SkinLib = 0;
		MessageLoopDepth = 0;
		#if HAS_SHARED_MIME
		Sm = 0;
		#endif
		#if HAS_LIB_MAGIC
		hMagic = NULL;
		#endif

		LastClickTime = 0;
		LastClickWnd = 0;
		LastClickX = 0;
		LastClickY = 0;
	}

	~GAppPrivate()
	{
		#if HAS_LIB_MAGIC
		if (MagicLock.Lock(_FL))
		{
			if (hMagic != NULL)
			{
				magic_close(hMagic);
				hMagic = NULL;
			}
			MagicLock.Unlock();
		}
		#endif

		if (WmLib)
		{
			Proc_LgiWmExit WmExit = (Proc_LgiWmExit) WmLib->GetAddress("LgiWmExit");
			if (!WmExit || WmExit())
			{
				DeleteObj(WmLib);
			}
			else
			{
				// Do not unload the library is the exit function fails...
				// It could still be doing something or have AtExit handlers
				// in place.
				WmLib = 0;
				// Do just leak the memory. We're shutting down anyway.
			}
		}
		DeleteObj(SkinLib);
		
		#if HAS_SHARED_MIME
		DeleteObj(Sm);
		#endif
		
		for (auto p : MimeToApp)
		{
			p.value->DeleteObjects();
			DeleteObj(p.value);
		}
	}
};

/////////////////////////////////////////////////////////////////////////////
GSkinEngine *GApp::SkinEngine = 0;
GApp *TheApp = 0;
GMouseHook *GApp::MouseHook = 0;

GApp::GApp(OsAppArguments &AppArgs, const char *name, GAppArguments *Args) :
	OsApplication(AppArgs.Args, AppArgs.Arg)
{
	TheApp = this;
	SystemNormal = 0;
	SystemBold = 0;
	d = new GAppPrivate;
	Name(name);
	
	int WCharSz = sizeof(wchar_t);
	#if defined(_MSC_VER)
	LgiAssert(WCharSz == 2);
	#else
	LgiAssert(WCharSz == 4);
	#endif

  	Gtk::gdk_threads_init();	
	GtkLock _Lock;
  	
	// We want our printf's NOW!
	setvbuf(stdout,(char *)NULL,_IONBF,0); // print mesgs immediately.

	// Setup the file and graphics sub-systems
	d->FileSystem = new GFileSystem;
	d->GdcSystem = new GdcDevice;

	SetAppArgs(AppArgs);

	srand(LgiCurrentTime());
	LgiInitColours();
	AppWnd = 0;

	MouseHook = new GMouseHook;

	#if 0
	// Setup the SIGSEGV signal to call the KDE crash handler
	if (!GetOption("nch") &&
		LgiGetWindowManager() == WM_Kde)
	{
		signal(SIGSEGV, LgiCrashHandler);
	}
	#endif

	// System font setup
	SystemNormal = 0;
	GFontType SysFontType;

	if (SysFontType.GetSystemFont("System"))
	{
		SystemNormal = SysFontType.Create();
		if (SystemNormal)
			SystemNormal->Transparent(true);
		
		SystemBold = SysFontType.Create();
		if (SystemBold)
		{
			SystemBold->Bold(true);
			SystemBold->Transparent(true);
			SystemBold->Create();
		}
	}
	else
	{
		printf("%s:%i - Couldn't get system font setting.\n", __FILE__, __LINE__);
	}

	if (!SystemNormal)
	{
		LgiMsg(0, "Error: Couldn't create system font.", "Lgi Error: GApp::GApp", MB_OK);
		LgiExitApp();
	}
	
	if (!GetOption("noskin"))
	{
		extern GSkinEngine *CreateSkinEngine(GApp *App);
		SkinEngine = CreateSkinEngine(this);
	}
}

GApp::~GApp()
{
	GtkLock _Lock;

	DeleteObj(AppWnd);
	DeleteObj(SystemNormal);
	DeleteObj(SystemBold);
	
	DeleteObj(MouseHook);
	DeleteObj(d->FileSystem);
	DeleteObj(d->GdcSystem);
	DeleteObj(d->Config);
	DeleteObj(GFontSystem::Me);
	DeleteObj(d);
	TheApp = 0;
}

GApp *GApp::ObjInstance()
{
	return TheApp;
}

bool GApp::IsOk()
{
	bool Status = 	(this != 0) &&
					(d != 0);
					
	LgiAssert(Status);
	return Status;
}

GMouseHook *GApp::GetMouseHook()
{
	return MouseHook;
}

int GApp::GetMetric(LgiSystemMetric Metric)
{
	switch (Metric)
	{
		case LGI_MET_DECOR_X:
		{
			return 8;
		}
		case LGI_MET_DECOR_Y:
		{
			return 8 + 19;
		}
		case LGI_MET_DECOR_CAPTION:
		{
			return 19;
		}
	}

	return 0;
}

GViewI *GApp::GetFocus()
{
	// GtkWidget *w = gtk_window_get_focus(GtkWindow *window);
	return NULL;
}

OsThread GApp::_GetGuiThread()
{
	return d->GuiThread;
}

OsThreadId GApp::GetGuiThreadId()
{
	return d->GuiThreadId;
}

OsProcessId GApp::GetProcessId()
{
    #ifdef WIN32
	return GetCurrentProcessId();
    #else
	return getpid();
	#endif
}

OsAppArguments *GApp::GetAppArgs()
{
	return IsOk() ? &d->Args : 0;
}

void GApp::SetAppArgs(OsAppArguments &AppArgs)
{
	if (IsOk())
	{
		d->Args = AppArgs;
	}
}

bool GApp::InThread()
{
	OsThreadId Me = GetCurrentThreadId();
	OsThreadId Gui = GetGuiThreadId();
	// printf("Me=%i Gui=%i\n", Me, Gui);
	return Gui == Me;
}

#ifndef WIN32
void GApp::OnEvents()
{
}
#endif

struct GtkIdle
{
	GAppPrivate *d;
	GAppI::OnIdleProc cb;
	void *param;
};

Gtk::gboolean IdleWrapper(Gtk::gpointer data)
{
	#if 0
	
		static int64 ts = LgiCurrentTime();
		static int count = 0;
		int64 now = LgiCurrentTime();
		if (now - ts > 300)
		{
			printf("IdleWrapper = %i\n", count);
			count = 0;
			ts = now;
		}
		else count++;
		
	#endif

	GtkIdle *i = (GtkIdle*) data;
	if (i->cb)
		i->cb(i->param);
	
	LMessageQue::MsgArray *Msgs;
	if (MsgQue &&
		(Msgs = MsgQue.Lock(_FL)))
	{
		// Copy the messages out of the locked structure..
		// This allows new messages to arrive independant
		// of us processing them here...
		LMessageQue::MsgArray q = *Msgs;
		Msgs->Empty();
		MsgQue.Unlock();
		
		for (auto m : q)
		{
			// printf("Process %p,%i,%i,%i\n", m.v, m.m, m.a, m.b);
			if (!GView::LockHandler(m.v, GView::OpExists))
			{
				// printf("%s:%i - Invalid view to post event to.\n", _FL);
			}
			else
			{
				#if VIEW_REF_MODE
				
				GdkEvent *e = gdk_event_new(GDK_CLIENT_EVENT);
				if (e)
				{
					e->client.data.l[0] = m.m;
					e->client.data.l[1] = m.a;
					e->client.data.l[2] = m.b;			
					
					auto Widget = m.v->Handle();
					if (Widget)
					{
						gtk_propagate_event(Widget, e);
						gdk_event_free(e);
						
						// printf("Unref %p %s.%p\n", Widget, m.v->GetClass(), m.v);
						g_object_unref(Widget);
					}
				}
				else printf("%s:%i - gdk_event_new failed.\n", _FL);
				
				#else

				GMessage Msg(m.m, m.a, m.b);
				m.v->OnEvent(&Msg);
				
				#endif
			}
		}
	}
	
	LgiSleep(1);
	
	return TRUE;
}

bool GApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
	if (!InThread())
	{
		printf("%s:%i - Error: Out of thread.\n", _FL);
		return false;
	}

	if (Loop)
	{
		static GtkIdle idle = {0};
		if (!idle.d)
		{
			idle.d = d;
			idle.cb = IdleCallback;
			idle.param = IdleParam;
		}

		{			
			GtkLock _Lock;
			Gtk::guint Id = Gtk::gdk_threads_add_idle_full(	G_PRIORITY_DEFAULT_IDLE,
															IdleWrapper,
															&idle,
															NULL);
		}

		static bool CmdLineDone = false;
		if (!CmdLineDone)
		{
			CmdLineDone = true;
			OnCommandLine();
		}
		
		GtkLock _Lock;
	    Gtk::gtk_main();
	}
	else
	{
		GtkLock _Lock;
	    Gtk::gtk_main_iteration_do(false);
	}

	return true;
}

void GApp::Exit(int Code)
{
	if (Code)
	{
		// hard exit
		::exit(Code);
	}
	else
	{
		// soft exit
		GtkLock _Lock;
		Gtk::gtk_main_quit();
	}
}

void GApp::OnUrl(const char *Url)
{
	if (AppWnd)
		AppWnd->OnUrl(Url);
}

void GApp::OnReceiveFiles(::GArray<char*> &Files)
{
	if (AppWnd)
		AppWnd->OnReceiveFiles(Files);
}

void GApp::SetConfig(GXmlTag *Tag)
{
	if (IsOk() && Tag)
	{
		GXmlTag *Old = GetConfig(Tag->GetTag());
		if (Old)
		{
			Old->RemoveTag();
			DeleteObj(Old);
		}

		if (!d->Config)
		{
			d->Config = new GXmlTag("Config");
		}
		if (d->Config)
		{
			d->Config->InsertTag(Tag);
		}
	}
}

GXmlTag *GApp::GetConfig(const char *Tag)
{
	if (IsOk() && !d->Config)
	{
		char File[] = "lgi.conf";
		char Path[256];
		if (LgiGetExePath(Path, sizeof(Path)))
		{
			if (Path[strlen(Path)-1] != DIR_CHAR) strcat(Path, DIR_STR);
			strcat(Path, File);

			if (!FileExists(Path))
			{
				char *f = LgiFindFile(File);
				if (f)
				{
					strcpy(Path, f);
					DeleteArray(f);
				}
			}

			if (FileExists(Path))
			{
				d->Config = new GXmlTag("Config");
				if (d->Config)
				{
					::GFile f;
					if (f.Open(Path, O_READ))
					{
						GXmlTree t;
						t.Read(d->Config, &f, 0);
					}
				}
			}
		}
	}

	if (Tag && d->Config)
	{
		return d->Config->GetChildTag(Tag);
	}

	return 0;
}

const char *GApp::GetArgumentAt(int n)
{
	return n >= 0 && n < d->Args.Args ? NewStr(d->Args.Arg[n]) : 0;
}

bool GApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	::GString Buf;
	if (GetOption(Option, Buf))
	{
		if (Dest)
		{
			if (DestLen > 0)
			{
				strcpy_s(Dest, DestLen, Buf);
			}
			else return false;
		}
		return true;
	}
	return false;
}

bool GApp::GetOption(const char *Option, ::GString &Buf)
{
	if (IsOk() && Option)
	{
		int OptLen = strlen(Option);
		for (int i=1; i<d->Args.Args; i++)
		{
			char *a = d->Args.Arg[i];
			if (!a)
				continue;

			if (strchr("-/\\", a[0]))
			{
				if (strnicmp(a+1, Option, OptLen) == 0)
				{
					char *Arg = 0;
					if (strlen(a+1+OptLen) > 0)
					{
						Arg = a + 1 + OptLen;
					}
					else if (i < d->Args.Args - 1)
					{
						Arg = d->Args.Arg[i + 1];
					}
					
					if (Arg)
					{
						if (strchr("\'\"", *Arg))
						{
							char Delim = *Arg++;
							char *End = strchr(Arg, Delim);
							if (End)
							{
								int Len = (int)End-(int)Arg;
								if (Len > 0)
								    Buf.Set(Arg, Len);
								else
								    return false;
							}
							else return false;
						}
						else
						{
						    Buf = Arg;
						}
					}

					return true;
				}
			}
		}
	}

	return false;
}

void GApp::OnCommandLine()
{
	char WhiteSpace[] = " \r\n\t";

	::GArray<char*> Files;

	for (int i=1; i<GetAppArgs()->Args; i++)
	{
		char *a = GetAppArgs()->Arg[i];
		if (FileExists(a))
		{
			Files.Add(NewStr(a));
		}
	}

	// call app
	if (Files.Length() > 0)
	{
		OnReceiveFiles(Files);
	}

	// clear up
	Files.DeleteArrays();
}

GAutoString GApp::GetFileMimeType(const char *File)
{
	GAutoString Status;
	char Full[300] = "";

	if (!FileExists(File))
	{
		// Look in the path
		GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
		for (int i=0; i<p.Length(); i++)
		{
			LgiMakePath(Full, sizeof(Full), p[i], File);
			if (FileExists(Full))
			{
				File = Full;
				break;
			}
		}		
	}

	#if HAS_LIB_MAGIC
	
	static bool MagicError = false;
	if (d->MagicLock.Lock(_FL))
	{
		if (!d->hMagic && !MagicError)
		{
			d->hMagic = magic_open(MAGIC_MIME_TYPE);
			if (d->hMagic)
			{
				if (magic_load(d->hMagic, NULL) != 0)
				{
		        	printf("%s:%i - magic_load failed - %s\n", _FL, magic_error(d->hMagic));
		        	magic_close(d->hMagic);
		        	d->hMagic = NULL;
		        	MagicError = true;
				}
			}
			else
			{
				printf("%s:%i - magic_open failed.\n", _FL);
				MagicError = true;
			}
		}
		
		if (d->hMagic && !MagicError)
		{
			const char *mt = magic_file(d->hMagic, File);
			if (mt)
			{
				Status.Reset(NewStr(mt));
			}
		}
		
		d->MagicLock.Unlock();

		if (Status)
			return Status;
	}

	#endif

	#if HAS_SHARED_MIME
	// freedesktop.org rocks...
	if (!d->Sm)
	{
		// Not loaded, go and try to load it...
		d->Sm = new GSharedMime;
		if (d->Sm && d->Sm->IsLoaded())
		{
			d->Sm->mimetypes_init();
		}
	}
	if (d->Sm && d->Sm->IsLoaded())
	{
		// Loaded...
		char *m = (char*)d->Sm->mimetypes_get_file_type(File, MIMETYPES_CHECK_ALL);
		if (m)
		{
			#if HAS_FILE_CMD
			if (stricmp(m, "application/x-not-recognised") != 0)
			#endif
			{
				strcpy(Mime, m);
				return true;
			}
		}
		else
		{
			printf("%s:%i - mimetypes_get_file_type failed for '%s'\n", __FILE__, __LINE__, File);
		}
	}
	else
	{
		printf("%s:%i - Shared Mime not loaded!!!\n", __FILE__, __LINE__);
	}
	#endif

	#if HAS_FILE_CMD
	// doh! not installed... :(
	GStringPipe Output;
	char Args[256];
	sprintf(Args, "-i \"%s\"", File);
	GProcess p;
	if (p.Run("file", Args, 0, true, 0, &Output))
	{
		char *Out = Output.NewStr();
		if (Out)
		{
			char *s = strchr(Out, ':');
			if (s && strchr(Out, '/'))
			{
				s += 2;
				
				char *e = s;
				while
				(
					*e &&
					(
						IsAlpha(*e) ||
						IsDigit(*e) ||
						strchr(".-_/", *e)
					)
				)
					e++;
				*e = 0;
				
				Status.Reset(NewStr(s));
			}
			DeleteArray(Out);
		}
	}
	#endif
	
	return Status;
}

bool GApp::GetAppsForMimeType(char *Mime, ::GArray<::GAppInfo*> &Apps)
{
	// Find alternative version of the MIME type (e.g. x-type and type).
	char AltMime[256];
	strcpy(AltMime, Mime);
	char *s = strchr(AltMime, '/');
	if (s)
	{
		s++;
		
		int Len = strlen(s) + 1;
		if (strnicmp(s, "x-", 2) == 0)
		{
			memmove(s, s+2, Len - 2);
		}
		else
		{
			memmove(s+2, s, Len);
			s[0] = 'x';
			s[1] = '-';
		}
	}

	LgiGetAppsForMimeType(Mime, Apps);
	LgiGetAppsForMimeType(AltMime, Apps);
	
	return Apps.Length() > 0;
}

#ifndef WIN32
GLibrary *GApp::GetWindowManagerLib()
{
	if (this != NULL && !d->WmLib)
	{
		char Lib[32];
		WindowManager Wm = LgiGetWindowManager();
		switch (Wm)
		{
			case WM_Kde:
				strcpy(Lib, "liblgikde3");
				break;
			case WM_Gnome:
				strcpy(Lib, "liblgignome2");
				break;
			default:
				strcpy(Lib, "liblgiother");
				break;
		}
		#ifdef _DEBUG
		strcat(Lib, "d");
		#endif
		
		d->WmLib = new GLibrary(Lib, true);
		if (d->WmLib)
		{
			if (d->WmLib->IsLoaded())
			{
				Proc_LgiWmInit WmInit = (Proc_LgiWmInit) d->WmLib->GetAddress("LgiWmInit");
				if (WmInit)
				{
					WmInitParams Params;
					// Params.Dsp = XObject::XDisplay();
					Params.Args = d->Args.Args;
					Params.Arg = d->Args.Arg;
					
					WmInit(&Params);
				}
				// else printf("%s:%i - Failed to find method 'LgiWmInit' in WmLib.\n", __FILE__, __LINE__);
			}
			// else printf("%s:%i - couldn't load '%s.so'\n", __FILE__, __LINE__, Lib);
		}
		// else printf("%s:%i - alloc error\n", __FILE__, __LINE__);
	}
	
	return d->WmLib && d->WmLib->IsLoaded() ? d->WmLib : 0;
}

void GApp::DeleteMeLater(GViewI *v)
{
	d->DeleteLater.Add(v);
}

void GApp::SetClipBoardContent(OsView Hnd, ::GVariant &v)
{
	// Store the clipboard data we will serve
	d->ClipData = v;
}

bool GApp::GetClipBoardContent(OsView Hnd, ::GVariant &v, ::GArray<char*> &Types)
{
	return false;
}
#endif

GSymLookup *GApp::GetSymLookup()
{
	return d;
}

bool GApp::IsElevated()
{
	#ifdef WIN32
	LgiAssert(!"What API works here?");
	return false;
	#else
	return geteuid() == 0;
	#endif
}

int GApp::GetCpuCount()
{
	return 1;
}

GFontCache *GApp::GetFontCache()
{
	if (!d->FontCache)
		d->FontCache.Reset(new GFontCache(SystemNormal));
	return d->FontCache;
}

#ifdef LINUX
GApp::DesktopInfo::DesktopInfo(const char *file)
{
	File = file;
	Dirty = false;
	if (File)
		Serialize(false);
}

bool GApp::DesktopInfo::Serialize(bool Write)
{
	::GFile f;
	if (!f.Open(File, Write?O_WRITE:O_READ))
	{
		LgiTrace("%s:%i - Failed to open '%s'\n", _FL, File.Get());
		return false;
	}

	if (Write)
	{
		f.SetSize(0);
		for (unsigned i=0; i<Data.Length(); i++)
		{
			Section &s = Data[i];
			if (s.Name)
				f.Print("[%s]\n", s.Name.Get());
			for (unsigned n=0; n<s.Values.Length(); n++)
			{
				KeyPair &kp = s.Values[n];
				f.Print("%s=%s\n", kp.Key.Get(), kp.Value.Get());
			}
			f.Print("\n");
		}
		Dirty = false;
	}
	else
	{
		::GString::Array Lines = f.Read().Split("\n");
		Section *Cur = NULL;
		for (unsigned i=0; i<Lines.Length(); i++)
		{
			::GString l = Lines[i].Strip();
			if (l.Length() < 1)
				continue;
			int s = l.Find("[");
			if (s >= 0)
			{
				int e = l.Find("]", ++s);
				if (e >= 0)
				{
					Cur = &Data.New();
					Cur->Name = l(s, e - s + 1);
				}
			}
			else if ((s = l.Find("=")) >= 0)
			{
				if (!Cur)
					Cur = &Data.New();
				KeyPair &kp = Cur->Values.New();
				kp.Key = l(0, s).Strip();
				kp.Value = l(++s, -1).Strip();
				
				// printf("Read '%s': '%s'='%s'\n", Cur->Name.Get(), kp.Key.Get(), kp.Value.Get());
			}
		}
	}

	return true;
}

GApp::DesktopInfo::Section *GApp::DesktopInfo::GetSection(const char *Name, bool Create)
{
	for (unsigned i=0; i<Data.Length(); i++)
	{
		Section &s = Data[i];
		if (s.Name && s.Name == Name)
			return &s;
		if (!s.Name && !Name)
			return &s;
	}
	if (Create)
	{
		Section &s = Data.New();
		s.Name = Name;
		Dirty = true;
		return &s;
	}
	
	return NULL;
}

static const char *DefaultSection = "Desktop Entry";

::GString GApp::DesktopInfo::Get(const char *Field, const char *Sect)
{
	if (Field)
	{
		Section *s = GetSection(Sect ? Sect : DefaultSection, false);
		if (s)
		{
			KeyPair *kp = s->Get(Field, false, Dirty);
			if (kp)
			{
				return kp->Value;
			}
		}
	}
	
	return ::GString();
}

bool GApp::DesktopInfo::Set(const char *Field, const char *Value, const char *Sect)
{
	if (!Field)
		return false;

	Section *s = GetSection(Sect ? Sect : DefaultSection, true);
	if (!s)
		return false;

	KeyPair *kp = s->Get(Field, true, Dirty);
	if (!kp)
		return false;

	if (kp->Value != Value)
	{
		kp->Value = Value;
		Dirty = true;
	}
	return true;
}

GApp::DesktopInfo *GApp::GetDesktopInfo()
{
	char sExe[MAX_PATH] = "";
	LgiGetExeFile(sExe, sizeof(sExe));
	::GFile::Path Exe(sExe);
	::GFile::Path Desktop(LSP_HOME);
	::GString Leaf;
	Leaf.Printf("%s.desktop", Exe.Last().Get());
	
	Desktop += ".local/share/applications";
	Desktop += Leaf;
	
	const char *Ex = Exe;
	const char *Fn = Desktop;

	if (d->DesktopInfo.Reset(new DesktopInfo(Desktop)))
	{
		// Do a sanity check...
		::GString s = d->DesktopInfo->Get("Name");
		if (!s && Name())
			d->DesktopInfo->Set("Name", Name());
		
		char sExe[MAX_PATH] = "";
		if (LgiGetExeFile(sExe, sizeof(sExe)))
		{
			s = d->DesktopInfo->Get("Exec");
			if (!s || s != (const char*)sExe)
				d->DesktopInfo->Set("Exec", sExe);
		}
		
		s = d->DesktopInfo->Get("Type");
		if (!s) d->DesktopInfo->Set("Type", "Application");

		s = d->DesktopInfo->Get("Categories");
		if (!s) d->DesktopInfo->Set("Categories", "Application;");

		s = d->DesktopInfo->Get("Terminal");
		if (!s) d->DesktopInfo->Set("Terminal", "false");
		
		d->DesktopInfo->Update();
	}
	
	return d->DesktopInfo;
}

bool GApp::SetApplicationIcon(const char *FileName)
{
	DesktopInfo *di = GetDesktopInfo();
	if (!di)
		return false;
	
	::GString IcoPath = di->Get("Icon");
	if (IcoPath == FileName)
		return true;
	
	di->Set("Icon", FileName);
	return di->Update();
}
#endif

////////////////////////////////////////////////////////////////
OsApplication *OsApplication::Inst = 0;

class OsApplicationPriv
{
public:
	OsApplicationPriv()
	{
	}
};

OsApplication::OsApplication(int Args, char **Arg)
{
    Inst = this;
    d = new OsApplicationPriv;	
	
	#ifdef __GNUC__
	XInitThreads();
	#endif
    gtk_init(&Args, &Arg);
}

OsApplication::~OsApplication()
{
	DeleteObj(d);
	Inst = 0;
}

////////////////////////////////////////////////////////////////
void GMessage::Set(int Msg, Param ParamA, Param ParamB)
{
	if (!Event)
	{
		OwnEvent = true;
		if (LgiApp->InThread())
		{
			Event = gdk_event_new(GDK_CLIENT_EVENT);
		}
	}
	
	m = Msg;
	a = ParamA;
	b = ParamB;
	
	if (Event)
	{
		Event->client.data.l[0] = m;
		Event->client.data.l[1] = a;
		Event->client.data.l[2] = b;
	}
}

struct GlibEventParams
{
    GtkWidget *w;
    GdkEvent *e;
};

bool GlibWidgetSearch(GtkWidget *p, GtkWidget *w, bool Debug, int depth)
{
	char indent[256] = "";

	if (Debug)
	{	
		int ch = depth * 2;
		memset(indent, ' ', ch);
		indent[ch] = 0;

		printf("%sGlibWidgetSearch: %p, %p\n", indent, p, w);
	}
	
	if (p == w)
		return true;

	if (GTK_IS_CONTAINER(p))
	{
		int n = 0;
		Gtk::GList *top = gtk_container_get_children(GTK_CONTAINER(p));
		Gtk::GList *i = top;
		while (i)
		{
			if (Debug)
				printf("%s[%i]=%s\n", indent, n, gtk_widget_get_name((GtkWidget*)i->data));

			if (i->data == w)
				return true;
			else if (GlibWidgetSearch((GtkWidget*)i->data, w, Debug, depth + 1))
				return true;

			i = i->next;
			n++;
		}
		g_list_free(top);
	}
	else if (GTK_IS_BIN(p))
	{
	    GtkWidget *child = gtk_bin_get_child(GTK_BIN(p));
	    if (child)
	    {
	    	if (Debug)
				printf("%schild=%s\n", indent, gtk_widget_get_name(child));
			if (child == w)
				return true;
			else if (GlibWidgetSearch(child, w, Debug, depth + 1))
				return true;
		}
    }
    else if (Debug)
    {
		printf("%sUnknown=%s\n", indent, gtk_widget_get_name(p));
	}

	return false;
}

static gboolean 
GlibPostMessage(GlibEventParams *p)
{
	#if 1
	GtkWindow *w = NULL;
	if (p->e->client.window)
		gdk_window_get_user_data(p->e->client.window, (gpointer*)&w);
	
	if (!w)
	{
		// Window must of been destroyed...
	}
	else if (GlibWidgetSearch(GTK_WIDGET(w), p->w, false))
	{
	    gtk_propagate_event(p->w, p->e);
	}
	else
	{
		printf("%s:%i - Failed to find widget(%p) for PostMessage.\n", _FL, w);
		
		#if 0
		static int Count = 0;
		if (Count++ < 5)
		{
			GlibWidgetSearch(GTK_WIDGET(w), p->w, true);
		}
		if (Count > 20)
		{
			printf("%s:%i - Too many widget not found errors.\n", _FL);
			LgiExitApp();
		}
		#endif
	}
	#else
    gtk_propagate_event(p->w, p->e);
	#endif
	
    gdk_event_free(p->e);
    DeleteObj(p);

    return FALSE;
}

void GApp::OnDetach(GViewI *View)
{
	LMessageQue::MsgArray *q = MsgQue.Lock(_FL);
	if (!q)
	{
		printf("%s:%i - Couldn't lock app.\n", _FL);
		return;
	}

	#if VIEW_REF_MODE
	for (unsigned i=0; i<q->Length(); i++)
	{
		if ((*q)[i].v == View)
		{
			printf("Clearing detaching view.\n");
			q->DeleteAt(i--, true);
		}
	}
	#endif

	MsgQue.Unlock();
}

bool GApp::PostEvent(GViewI *View, int Msg, GMessage::Param a, GMessage::Param b)
{
	LMessageQue::MsgArray *q = MsgQue.Lock(_FL);
	if (!q)
	{
		printf("%s:%i - Couldn't lock app.\n", _FL);
		return false;
	}
	
	auto Widget = View->Handle();

	#if VIEW_REF_MODE
	// printf("Ref %p %s.%p (len=%i)\n", Widget, View->GetClass(), View, (int)q->Length());
	g_object_ref(Widget); // ref widget till we try and propagate the message to it...
	#endif
	
	q->New().Set(View, Msg, a, b);

	// printf("Insert %p,%i,%i,%i\n", View, Msg, a, b);
	MsgQue.Unlock();
	
	return true;
}

bool GMessage::Send(GViewI *View)
{
	if (!View)
	{
		LgiAssert(!"No view");
		return false;
	}
	
	return LgiApp->PostEvent(View, m, a, b);
}

