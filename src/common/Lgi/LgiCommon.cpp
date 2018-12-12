//
//	Cross platform LGI functions
//
#if COCOA
#import <Foundation/Foundation.h>
#endif

#define _WIN32_WINNT 0x501

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>

#ifdef WINDOWS
	#include <winsock2.h>
	#include <shlobj.h>
	#include "GRegKey.h"
	#include <sys/types.h>
	#include <sys/stat.h>
#else
	#include <unistd.h>
	#define _getcwd getcwd
#endif

#include "Lgi.h"
#include "GToken.h"
#include "GCapabilities.h"

#if defined(LINUX) && !defined(LGI_SDL)
	#include "LgiWinManGlue.h"
#elif defined(WINDOWS)
	#include "GRegKey.h"
#elif defined(BEOS)
	#include <FindDirectory.h>
#endif

#if defined POSIX
	#include <sys/time.h>
	#include <sys/types.h>
	#include <pwd.h>
	#include <sys/utsname.h>
	#include "GProcess.h"
#elif defined BEOS
	#include <Path.h>
#endif

#if defined(WIN32)
	#include "../win32/GSymLookup.h"
#elif defined(LINUX)
	#include "../linux/GSymLookup.h"
#else
	#include "GSymLookup.h"
#endif
#include "GLibrary.h"

#if defined(__GTK_H__)
namespace Gtk {
	#include "LgiWidget.h"
}
#endif

//////////////////////////////////////////////////////////////////////////
// Misc stuff
#if defined MAC
	#import <foundation/foundation.h>
	#if COCOA
		GString LgiArgsAppPath;
	#else
		bool _get_path_FSRef(FSRef &fs, GStringPipe &a)
		{
			HFSUniStr255 Name;
			ZeroObj(Name);
			FSRef Parent;
			FSCatalogInfo Cat;
			ZeroObj(Cat);
			OSErr e = FSGetCatalogInfo(&fs,
										kFSCatInfoVolume|kFSCatInfoNodeID,
										&Cat,
										&Name,
										NULL,
										&Parent);
			if (!e)
			{
				if (_get_path_FSRef(Parent, a))
				{
					GAutoString u((char*)LgiNewConvertCp("utf-8", Name.unicode, "utf-16", Name.length * sizeof(Name.unicode[0]) ));

					// printf("CatInfo = '%s' %x %x\n", u.Get(), Cat.nodeID, Cat.volume);
					if (u && Cat.nodeID > 2)
					{
						a.Print("%s%s", DIR_STR, u.Get());
					}
				}
				
				return true;
			}

			return false;
		}

		GAutoString FSRefPath(FSRef &fs)
		{
			GStringPipe a;
			if (_get_path_FSRef(fs, a))
			{
				return GAutoString(a.NewStr());
			}
			
			return GAutoString();
		}
	#endif
#endif

bool LgiPostEvent(OsView Wnd, int Event, GMessage::Param a, GMessage::Param b)
{
	#if LGI_SDL

	SDL_Event e;

	e.type = SDL_USEREVENT;
	e.user.code = Event;
	e.user.data1 = Wnd;
	e.user.data2 = a || b ? new GMessage::EventParams(a, b) : NULL;

	/*
	printf("LgiPostEvent Wnd=%p, Event=%i, a/b: %i/%i\n",
		Wnd, Event, (int)a, (int)b);
	*/

	return SDL_PushEvent(&e) == 0;

	#elif WINNATIVE

	return PostMessage(Wnd, Event, a, b) != 0;

	#elif defined(__GTK_H__)

	GViewI *View = g_object_get_data(GtkCast(Wnd, g_object, GObject), "GViewI");
	if (View)
	{
		GMessage m(0);
		m.Set(Event, a, b);
		return m.Send(View);
	}
	else printf("%s:%i - Error: LgiPostEvent can't cast OsView to GViewI\n", _FL);

	#elif defined(BEOS)
	
	if (Wnd)
	{
		BMessage Msg(Event);
		Msg.AddInt32("a", a);
		Msg.AddInt32("b", b);

		BMessenger m(Wnd);
		return m.SendMessage(&Msg) == B_OK;
	}

	#elif defined(MAC) && !defined COCOA
	
	#if 0
	int64 Now = LgiCurrentTime();
	static int64 Last = 0;
	static int Count = 0;
	
	Count++;
	if (Now > Last + 1000)
	{
		printf("Sent %i events in the last %ims\n", Count, (int)(Now-Last));
		Last = Now;
		Count = 0;
	}
	#endif
	
	EventRef Ev;
	OSStatus e = CreateEvent(NULL,
							kEventClassUser,
							kEventUser,
							0, // EventTime 
							kEventAttributeNone,
							&Ev);
	if (e)
	{
		printf("%s:%i - CreateEvent failed with %i\n", _FL, (int)e);
	}
	else
	{
		EventTargetRef t = GetControlEventTarget(Wnd);
		
		e = SetEventParameter(Ev, kEventParamLgiEvent, typeUInt32, sizeof(Event), &Event);
		if (e) printf("%s:%i - error %i\n", _FL, (int)e);
		e = SetEventParameter(Ev, kEventParamLgiA, typeUInt32, sizeof(a), &a);
		if (e) printf("%s:%i - error %i\n", _FL, (int)e);
		e = SetEventParameter(Ev, kEventParamLgiB, typeUInt32, sizeof(b), &b);
		if (e) printf("%s:%i - error %i\n", _FL, (int)e);
		
		bool Status = false;
		EventQueueRef q = GetMainEventQueue();

		e = SetEventParameter(Ev, kEventParamPostTarget, typeEventTargetRef, sizeof(t), &t);
		if (e) printf("%s:%i - error %i\n", _FL, (int)e);
		e = PostEventToQueue(q, Ev, kEventPriorityStandard);
		if (e) printf("%s:%i - error %i\n", _FL, (int)e);
		else Status = true;
		
		// printf("PostEventToQueue %i,%i,%i -> %p\n", Event, a, b, q);
		
		ReleaseEvent(Ev);
		
		return Status;
	}
	
	#endif

	return false;
}

void LgiExitApp()
{
	exit(0);
}

//////////////////////////////////////////////////////////////////////////
#ifdef WIN32
bool RegisterActiveXControl(char *Dll)
{
	GLibrary Lib(Dll);
	if (Lib.IsLoaded())
	{
		#ifdef _MSC_VER
		typedef HRESULT (STDAPICALLTYPE *p_DllRegisterServer)(void);

		p_DllRegisterServer DllRegisterServer = (p_DllRegisterServer)Lib.GetAddress("DllRegisterServer");
		if (DllRegisterServer)
		{
			return DllRegisterServer() == S_OK;
		}
		#else
		LgiAssert(!"Not impl.");
		#endif
	}

	return false;
}
#endif

//////////////////////////////////////////////////////////////////////////
#ifdef WINDOWS
#include <lm.h>
#pragma comment(lib, "netapi32.lib")
#endif

/// \brief Returns the operating system that Lgi is running on.
/// \sa Returns one of the defines starting with LGI_OS_UNKNOWN in LgiDefs.h
int LgiGetOs
(
	/// Returns the version of the OS or NULL if you don't care
	GArray<int> *Ver
)
{
	#if defined(WIN32) || defined(WIN64)

	static int Os = LGI_OS_UNKNOWN;
	static int Version = 0, Revision = 0;

	if (Os == LGI_OS_UNKNOWN)
	{
		#if defined(WIN64)
		BOOL IsWow64 = TRUE;
		#elif defined(WIN32)
		BOOL IsWow64 = FALSE;
		IsWow64Process(GetCurrentProcess(), &IsWow64);
		#endif

		#if 1
		SERVER_INFO_101 *v = NULL;
		auto r = NetServerGetInfo(NULL, 101, (LPBYTE*)&v);
		if (r == NERR_Success)
		{
			Version = v->sv101_version_major;
			Revision = v->sv101_version_minor;
			Os = (v->sv101_version_major >= 6)
				?
				#ifdef WIN32
				(IsWow64 ? LGI_OS_WIN64 : LGI_OS_WIN32)
				#else
				LGI_OS_WIN64
				#endif
				:
				LGI_OS_WIN9X;

			NetApiBufferFree(v);
		}
		else
		{
			LgiAssert(0);
		}
		#else
		OSVERSIONINFO v;
		v.dwOSVersionInfoSize = sizeof(v);
		GetVersionEx(&v);

		Version = v.dwMajorVersion;
		Revision = v.dwMinorVersion;
		#ifdef WIN32
		BOOL IsWow64 = FALSE;
		IsWow64Process(GetCurrentProcess(), &IsWow64);
		#endif
		Os = (v.dwPlatformId == VER_PLATFORM_WIN32_NT)
			?
			#ifdef WIN32
			(IsWow64 ? LGI_OS_WIN64 : LGI_OS_WIN32)
			#else
			LGI_OS_WIN64
			#endif
			:
			LGI_OS_WIN9X;
		#endif
	}

	if (Ver)
	{
		Ver->Add(Version);
		Ver->Add(Revision);
	}

	return Os;

	#elif defined BEOS

	if (Ver)
	{
		Ver->Add(5);
		Ver->Add(0);
	}
	
	return LGI_OS_HAIKU;

	#elif defined LINUX

	if (Ver)
	{
		utsname Buf;
		if (!uname(&Buf))
		{
			GToken t(Buf.release, ".");
			for (int i=0; i<t.Length(); i++)
			{
				Ver->Add(atoi(t[i]));
			}
		}
	}

	return LGI_OS_LINUX;

	#elif defined MAC

	if (Ver)
	{
		NSOperatingSystemVersion v = [[NSProcessInfo processInfo] operatingSystemVersion];
		Ver->Add((int)v.majorVersion);
		Ver->Add((int)v.minorVersion);
		Ver->Add((int)v.patchVersion);
	}
	
	return LGI_OS_MAC_OS_X;

	#else

	return LGI_OS_UNKNOWN;

	#endif
}

const char *LgiGetOsName()
{
	const char *Str[LGI_OS_MAX] =
	{
		"Unknown",
		"Win9x",
		"Win32",
		"Win64",
		"Haiku",
		"Linux",
		"MacOSX",
	};

	return Str[LgiGetOs()];
}

#ifdef WIN32
#define RecursiveFileSearch_Wildcard "*.*"
#else // unix'ish OS
#define RecursiveFileSearch_Wildcard "*"
#endif

bool LgiRecursiveFileSearch(const char *Root,
							GArray<const char*> *Ext,
							GArray<char*> *Files,
							uint64 *Size,
							uint64 *Count,
							RecursiveFileSearch_Callback Callback,
							void *UserData)
{
	bool Status = false;

	// validate args
	if (!Root) return 0;

	// get directory enumerator
	GDirectory Dir;
	Status = true;

	// enumerate the directory contents
	for (auto Found = Dir.First(Root); Found; Found = Dir.Next())
	{
		char Name[300];
		if (!Dir.Path(Name, sizeof(Name)))
			continue;

		if (Callback)
		{
			if (!Callback(UserData, Name, &Dir))
			{
				continue;
			}
		}

		if (Dir.IsDir())
		{
			// dir
			LgiRecursiveFileSearch(	Name,
									Ext,
									Files,
									Size,
									Count,
									Callback,
									UserData);

		}
		else
		{
			// process file
			bool Match = true; // if no Ext's then default to match
			if (Ext)
			{
				for (int i=0; i<Ext->Length(); i++)
				{
					const char *e = (*Ext)[i];
					char *RawFile = strrchr(Name, DIR_CHAR);
					if (RawFile &&
						(Match = MatchStr(e, RawFile+1)))
					{
						break;
					}
				}
			}

			if (Match)
			{
				// file matched... process:
				if (Files)
				{
					Files->Add(NewStr(Name));
				}

				if (Size)
				{
					*Size += Dir.GetSize();
				}

				if (Count)
				{
					Count++;
				}

				Status = true;
			}
		}
	}

	return Status;
}

#define LGI_TRACE_TO_FILE
// #include <malloc.h>

#ifndef WIN32
#define _vsnprintf vsnprintf
#endif

static GStreamI *_LgiTraceStream = NULL;

void LgiTraceSetStream(GStreamI *stream)
{
	_LgiTraceStream = stream;
}

bool LgiTraceGetFilePath(char *LogPath, int BufLen)
{
	if (!LogPath)
		return false;

	if (LgiGetExeFile(LogPath, BufLen))
	{
		#ifdef MAC
		char *Dir = strrchr(LogPath, DIR_CHAR);
		if (Dir)
		{
			char Part[256];
			strcpy_s(Part, sizeof(Part), Dir+1);
            LogPath[0] = 0;
			LgiMakePath(LogPath, BufLen, LogPath, "~/Library/Logs");
			LgiMakePath(LogPath, BufLen, LogPath, Dir+1);
			strcat_s(LogPath, BufLen, ".txt");
		}
		else
		#endif
		{
			char *Dot = strrchr(LogPath, '.');
			if (Dot && !strchr(Dot, DIR_CHAR))
				strcpy_s(Dot+1, BufLen - (Dot - LogPath) - 1, "txt");
			else
				strcat(LogPath, ".txt");
		}

		GFile f;		
		if (f.Open(LogPath, O_WRITE))
		{
			f.Close();
		}
		else
		{
			char Leaf[64];
			char *Dir = strrchr(LogPath, DIR_CHAR);
			if (Dir)
			{
				strcpy_s(Leaf, sizeof(Leaf), Dir + 1);
				LGetSystemPath(LSP_APP_ROOT, LogPath, BufLen);
				if (!DirExists(LogPath))
					FileDev->CreateFolder(LogPath);
				LgiMakePath(LogPath, BufLen, LogPath, Leaf);
			}
			else goto OnError;
		}
		
		#if 0
		GFile tmp;
		if (tmp.Open("c:\\temp\\log.txt", O_WRITE))
		{
			tmp.SetSize(0);
			tmp.Write(LogPath, strlen(LogPath));
		}
		#endif
	}
	else
	{
		// Well what to do now? I give up
		OnError:
		strcpy_s(LogPath, BufLen, "trace.txt");
		return false;
	}
	
	return true;
}

#define LGI_TRACE_TS	0


void LgiTrace(const char *Msg, ...)
{
#if LGI_TRACE_TS
GProfile Prof("LgiTrace");
#endif

	#if defined _INC_MALLOC && WINNATIVE
	if (_heapchk() != _HEAPOK)
	{
		return;
	}
	#endif

	if (!Msg)
		return;

	#ifdef WIN32
	static LMutex Sem;
	Sem.Lock(_FL, true);
	#endif

	char Buffer[2049] = "";
	#ifdef LGI_TRACE_TO_FILE
	static GFile f;
	static char LogPath[MAX_PATH] = "";
	
	if (!_LgiTraceStream && LogPath[0] == 0)
	{
		LgiTraceGetFilePath(LogPath, sizeof(LogPath));
	}
	#endif

#if LGI_TRACE_TS
Prof.Add("vprint");
#endif

	va_list Arg;
	va_start(Arg, Msg);
	int Ch = vsnprintf(Buffer, sizeof(Buffer)-1, Msg, Arg);
	va_end(Arg);

#if LGI_TRACE_TS
Prof.Add("open");
#endif

	#ifdef LGI_TRACE_TO_FILE
	GStreamI *Output = NULL;
	if (_LgiTraceStream)
		Output = _LgiTraceStream;
	else
	{
		if (!f.IsOpen() &&
			f.Open(LogPath, O_WRITE))
			f.Seek(0, SEEK_END);
		Output = &f;
	}

#if LGI_TRACE_TS
Prof.Add("write");
#endif
	if (Output && Ch > 0)
		Output->Write(Buffer, Ch);
#if LGI_TRACE_TS
Prof.Add("close");
#endif
	if (!_LgiTraceStream)
	{
		#ifdef WINDOWS
		// Windows can take AGES to close a file when there is anti-virus on, like 100ms.
		// We can't afford to wait here so just keep the file open but flush the
		// buffers if we can.
		FlushFileBuffers(f.Handle());
		#else
		f.Close();
		#endif
	}
	#endif


#if LGI_TRACE_TS
Prof.Add("OutputDebugStringA");
#endif
	#if defined WIN32
	OutputDebugStringA(Buffer);
#if LGI_TRACE_TS
Prof.Add("unlock");
#endif
	Sem.Unlock();
	#else
	printf("%s", Buffer);
	#endif
}

#ifndef LGI_STATIC
#define STACK_SIZE 12

void LgiStackTrace(const char *Msg, ...)
{
	GSymLookup::Addr Stack[STACK_SIZE];
	ZeroObj(Stack);
	GSymLookup *Lu = LgiApp ? LgiApp->GetSymLookup() : NULL;
	if (!Lu)
	{
		printf("%s:%i - Failed to get sym lookup object.\n", _FL);
		return;
	}
	
	int Frames = Lu ? Lu->BackTrace(0, 0, Stack, STACK_SIZE) : 0;
	if (Msg)
	{
		char Buffer[2049] = "";
		#ifdef LGI_TRACE_TO_FILE
		GFile f;
		if (LgiGetExeFile(Buffer, sizeof(Buffer)))
		{
			char *Dot = strrchr(Buffer, '.');
			if (Dot && !strchr(Dot, DIR_CHAR))
			{
				strcpy_s(Dot+1, sizeof(Buffer) - (Dot - Buffer) - 1, "txt");
			}
			else
			{
				strcat(Buffer, ".txt");
			}
			f.Open(Buffer, O_WRITE);
		}
		#endif

		va_list Arg;
		va_start(Arg, Msg);
		int Len = vsnprintf(Buffer, sizeof(Buffer)-1, Msg, Arg);
		va_end(Arg);

		Lu->Lookup(Buffer+Len, sizeof(Buffer)-Len-1, Stack, Frames);

		#ifdef LGI_TRACE_TO_FILE
		if (f.IsOpen())
		{
			f.Seek(0, SEEK_END);
			f.Write(Buffer, (int)strlen(Buffer));
			f.Close();
		}
		#endif

		#if defined WIN32
		OutputDebugStringA(Buffer);
		#else
		printf("Trace: %s", Buffer);
		#endif
	}
}
#endif

bool LgiTrimDir(char *Path)
{
	if (!Path)
		return false;

	char *p = strrchr(Path, DIR_CHAR);
	if (!p)
		return false;
	if (p[1] == 0) // Trailing DIR_CHAR doesn't count... do it again.
	{
		*p = 0;
		p = strrchr(Path, DIR_CHAR);
		if (!p)
			return false;
	}

	*p = 0;
	return true;
}

GAutoString LgiMakeRelativePath(const char *Base, const char *Path)
{
	GStringPipe Status;

	if (Base && Path)
	{
		#ifdef WIN32
		bool SameNs = strnicmp(Base, Path, 3) == 0;
		#else
		bool SameNs = true;
		#endif
		if (SameNs)
		{
			GToken b(Base + 1, ":" DIR_STR);
			GToken p(Path + 1, ":" DIR_STR);
			int Same = 0;
			while (b[Same] && p[Same] && stricmp(b[Same], p[Same]) == 0)
			{
				Same++;
			}

			for (int i = Same; i<b.Length(); i++)
			{
				Status.Print("..%s", DIR_STR);
			}
			if (Same >= b.Length())
			{
				Status.Print(".%s", DIR_STR);
			}

			for (int n = Same; n<p.Length(); n++)
			{
				if (n > Same)
				{
					Status.Push(DIR_STR);
				}
				Status.Push(p[n]);
			}
		}
	}

	return GAutoString(Status.NewStr());
}

bool LgiIsRelativePath(const char *Path)
{
	if (!Path)
		return false;

	if (*Path == '.')
		return true;

	#ifdef WIN32
	if (IsAlpha(Path[0]) && Path[1] == ':') // Drive letter path
		return false;
	#endif

	if (*Path == DIR_CHAR)
		return false;

	if (strstr(Path, "://")) // Protocol def
		return false;

	return true; // Correct or not???
}

bool LgiMakePath(char *Str, int StrSize, const char *Path, const char *File)
{
	LgiAssert(Str != 0 &&
			  StrSize > 0 &&
			  Path != 0 &&
			  File != 0);

	if (StrSize <= 4)
	{
		printf("%s:%i - LgiMakeFile buf size=%i?\n", _FL, StrSize);
	}

	if (Str && Path && File)
	{
		if (Str != Path)
		{
			strcpy_s(Str, StrSize, Path);
		}

		char Dir[] = { '/', '\\', 0 };
		#define EndStr() Str[strlen(Str)-1]
		#define EndDir() if (!strchr(Dir, EndStr())) strcat(Str, DIR_STR);

		size_t Len = strlen(Str);
		char *End = Str + (Len ? Len - 1 : 0);
		if (strchr(Dir, *End) && End > Str)
		{
			*End = 0;
		}

		GToken T(File, Dir);
		for (int i=0; i<T.Length(); i++)
		{
			if (!stricmp(T[i], "."))
				;
			else if (!stricmp(T[i], ".."))
				LgiTrimDir(Str);
			else if (!stricmp(T[i], "~"))
				LGetSystemPath(LSP_HOME, Str, StrSize);
			else
			{
				size_t Len = strlen(Str);
				if (!strchr(Dir, Str[Len-1]))
				{
					if (Len >= StrSize - 1)
						return false;
					Str[Len++] = DIR_CHAR;
					Str[Len] = 0;
				}
				size_t SegLen = strlen(T[i]);
				if (Len + SegLen + 1 > StrSize)
				{
					return false;
				}				
				strcpy_s(Str + Len, StrSize - Len, T[i]);
			}
		}
	}
	
	return true;
}

bool LgiGetTempPath(char *Dst, int DstSize)
{
	return LGetSystemPath(LSP_TEMP, Dst, DstSize);
}

bool LGetSystemPath(LgiSystemPath Which, char *Dst, int DstSize)
{
	if (!Dst || DstSize <= 0)
		return false;

	GFile::Path p;
	GString s = p.GetSystem(Which, 0);
	if (!s)
		return false;

	strcpy_s(Dst, DstSize, s);
	return true;
}

GString LGetSystemPath(LgiSystemPath Which, int WordSize)
{
	GFile::Path p;
	return p.GetSystem(Which, WordSize);
}

GFile::Path::State GFile::Path::Exists()
{
	if (Length() == 0)
		return TypeNone;

	#ifdef WINDOWS
	struct _stat64 s;
	int r = _stat64(GetFull(), &s);
	if (r)
		return TypeNone;
	
	if (s.st_mode & _S_IFDIR)
		return TypeFolder;

	if (s.st_mode & _S_IFREG)
		return TypeFile;
	#else
	struct stat s;
	int r = stat(GetFull(), &s);
	if (r)
		return TypeNone;
	
	if (S_ISDIR(s.st_mode))
		return TypeFolder;

	if (S_ISREG(s.st_mode))
		return TypeFile;
	#endif
		
	return TypeNone;
}

GString GFile::Path::GetSystem(LgiSystemPath Which, int WordSize = 0)
{
	GString Path;

	#if defined(WIN32)
		#ifndef CSIDL_PROFILE
			#define CSIDL_PROFILE						0x0028
		#endif 
		#if !defined(CSIDL_MYDOCUMENTS)
			#define CSIDL_MYDOCUMENTS					0x0005
		#endif
		#if !defined(CSIDL_MYMUSIC)
			#define CSIDL_MYMUSIC						0x000d
		#endif
		#if !defined(CSIDL_MYVIDEO)
			#define CSIDL_MYVIDEO						0x000e
		#endif
		#if !defined(CSIDL_LOCAL_APPDATA)
			#define CSIDL_LOCAL_APPDATA					0x001c
		#endif
		#if !defined(CSIDL_COMMON_APPDATA)
			#define CSIDL_COMMON_APPDATA				0x0023
		#endif
		#if !defined(CSIDL_APPDATA)
			#define CSIDL_APPDATA						0x001a
		#endif
	#endif

	#if defined(LINUX) && !defined(LGI_SDL)
	// Ask our window manager add-on if it knows the path
	GLibrary *WmLib = LgiApp ? LgiApp->GetWindowManagerLib() : NULL;
	if (WmLib)
	{
		Proc_LgiWmGetPath WmGetPath = (Proc_LgiWmGetPath) WmLib->GetAddress("LgiWmGetPath");
		char Buf[MAX_PATH];
		if (WmGetPath && WmGetPath(Which, Buf, sizeof(Buf)))
		{
			Path = Buf;
			return Path;
		}
	}
	#endif

	switch (Which)
	{
		default:
			break;
		case LSP_USER_DOWNLOADS:
		{
			#if defined(WIN32) && defined(_MSC_VER)

			// OMG!!!! Really?
			
			#ifndef REFKNOWNFOLDERID
			typedef GUID KNOWNFOLDERID;
			#define REFKNOWNFOLDERID const KNOWNFOLDERID &
			GUID FOLDERID_Downloads = {0x374DE290,0x123F,0x4565,{0x91,0x64,0x39,0xC4,0x92,0x5E,0x46,0x7B}};
			#endif
			
			GLibrary Shell("Shell32.dll");
			typedef HRESULT (STDAPICALLTYPE *pSHGetKnownFolderPath)(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPath);
			pSHGetKnownFolderPath SHGetKnownFolderPath = (pSHGetKnownFolderPath)Shell.GetAddress("SHGetKnownFolderPath");
			if (SHGetKnownFolderPath)
			{
				PWSTR ptr = NULL;
				HRESULT r = SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &ptr);
				if (SUCCEEDED(r))
				{
					GAutoString u8(WideToUtf8(ptr));
					if (u8)
						Path = u8;
					CoTaskMemFree(ptr);
				}
			}

			if (!Path.Get())
			{
				GRegKey k(false, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders");
				char *p = k.GetStr("{374DE290-123F-4565-9164-39C4925E467B}");
				if (DirExists(p))
					Path = p;
			}
			
			if (!Path.Get())
			{
				GString MyDoc = WinGetSpecialFolderPath(CSIDL_MYDOCUMENTS);
				if (MyDoc)
				{
					char Buf[MAX_PATH];
					LgiMakePath(Buf, sizeof(Buf), MyDoc, "Downloads");
					if (DirExists(Buf))
						Path = Buf;
				}
			}

			if (!Path.Get())
			{
				GString Profile = WinGetSpecialFolderPath(CSIDL_PROFILE);
				if (Profile)
				{
					char Buf[MAX_PATH];
					LgiMakePath(Buf, sizeof(Buf), Profile, "Downloads");
					if (DirExists(Buf))
						Path = Buf;
				}
			}

			#else

			LgiAssert(!"Not implemented");

			#endif
			break;
		}
		case LSP_USER_LINKS:
		{
			GString Home = LGetSystemPath(LSP_HOME);
			
			#if defined(WIN32)

			char p[MAX_PATH];
			if (LgiMakePath(p, sizeof(p), Home, "Links"))
				Path = p;

			#elif defined(LINUX)

			char p[MAX_PATH];
			if (LgiMakePath(p, sizeof(p), Home, ".config/gtk-3.0"))
				Path = p;
			
			#elif defined(BEOS)
			
			// No known user links path for Haiku			

			#else

			LgiAssert(!"Not implemented");

			#endif
			break;
		}
		case LSP_USER_PICTURES:
		{
			#if defined(WIN32)
			Path = WinGetSpecialFolderPath(CSIDL_MYPICTURES);
			if (Path)
				return Path;
			#endif

			// Default to ~/Pictures
			char p[MAX_PATH];
			GString Home = LGetSystemPath(LSP_HOME);
			if (LgiMakePath(p, sizeof(p), Home, "Pictures"))
				Path = p;
			break;
		}
		case LSP_USER_DOCUMENTS:
		{
			#if defined(WIN32) && defined(_MSC_VER)
			Path = WinGetSpecialFolderPath(CSIDL_MYDOCUMENTS);
			if (Path)
				return Path;
			#endif

			// Default to ~/Documents
			char p[MAX_PATH];
			GString Home = LGetSystemPath(LSP_HOME);
			if (LgiMakePath(p, sizeof(p), Home, "Documents"))
				Path = p;
			break;
		}
		case LSP_USER_MUSIC:
		{
			#if defined WIN32

			Path = WinGetSpecialFolderPath(CSIDL_MYMUSIC);
			
			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kUserDomain, kMusicDocumentsFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
			else
			{
				GAutoString a = FSRefPath(Ref);
				if (a)
					Path = a.Get();
			}				

			#endif
			
			if (!Path)
			{
				// Default to ~/Music
				char p[MAX_PATH];
				GString Home = LGetSystemPath(LSP_HOME);
				if (LgiMakePath(p, sizeof(p), Home, "Music"))
					Path = p;
			}
			break;
		}
		case LSP_USER_VIDEO:
		{
			#if defined WIN32

			Path = WinGetSpecialFolderPath(CSIDL_MYVIDEO);

			#elif defined MAC && !defined COCOA

			FSRef Ref;
			OSErr e = FSFindFolder(kUserDomain, kMovieDocumentsFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
			else
			{
				GAutoString a = FSRefPath(Ref);
				if (a)
					Path = a.Get();
			}

			#endif
			
			if (!Path)
			{
				// Default to ~/Video
				char p[MAX_PATH];
				GString Home = LGetSystemPath(LSP_HOME);
				if (LgiMakePath(p, sizeof(p), Home, "Video"))
					Path = p;
			}
			break;
		}
		case LSP_USER_APPS:
		{
			#if defined WIN32
			int Id = 
				#ifdef WIN64
				CSIDL_PROGRAM_FILES
				#else
				CSIDL_PROGRAM_FILESX86
				#endif
				;

			if (WordSize == 32)
				Id = CSIDL_PROGRAM_FILESX86;
			else if (WordSize == 64)
				Id = CSIDL_PROGRAM_FILES;

			Path = WinGetSpecialFolderPath(Id);
			#elif defined MAC
			Path = "/Applications";
			#elif defined LINUX
			Path = "/usr/bin";
			#elif defined BEOS
			Path = "/boot/system/apps";
			#else
			LgiAssert(!"Impl me.");
			#endif
			break;
		}
		case LSP_APP_INSTALL:
		{
			Path = LGetSystemPath(LSP_EXE);
			if (Path)
			{
				size_t Last = Path.RFind(DIR_STR);
				if (Last > 0)
				{
					GString s = Path(Last, -1);
					if
					(
						stristr(s,
								#ifdef _DEBUG
								"Debug"
								#else
								"Release"
								#endif
								)
					)
						Path = Path(0, Last);
				}					
			}
			break;
		}
		case LSP_APP_ROOT:
		{
			#ifndef LGI_STATIC
			char *Name = NULL, Exe[MAX_PATH];
			
			// Try and get the configured app name:
			if (LgiApp)
				Name = LgiApp->GBase::Name();
			
			if (!Name)
			{
				// Use the exe name?
				if (LgiGetExeFile(Exe, sizeof(Exe)))
				{
					char *l = LgiGetLeaf(Exe);
					if (l)
					{
						#ifdef WIN32
						char *d = strrchr(l, '.');
						*d = NULL;
						#endif
						Name = l;
						// printf("%s:%i - name '%s'\n", _FL, Name);
					}
				}
			}
			
			if (!Name)
			{
				LgiAssert(0);
				break;
			}

			#if defined MAC

				#if COCOA
				NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
				if (paths)
				{
					Path = [[paths objectAtIndex:0] UTF8String];
				}
				#else
				FSRef Ref;
				OSErr e = FSFindFolder(kUserDomain, kDomainLibraryFolderType, kDontCreateFolder, &Ref);
				if (e)
				{
					printf("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
					LgiAssert(0);
				}
				else
				{
					GAutoString Base = FSRefPath(Ref);
					Path = Base.Get();
				}
				#endif

			#elif defined WIN32

				Path = WinGetSpecialFolderPath(CSIDL_APPDATA);

			#elif defined LINUX

				char Dot[128];
				snprintf(Dot, sizeof(Dot), ".%s", Name);
				Name = Dot;
				struct passwd *pw = getpwuid(getuid());
				if (pw)
					Path = pw->pw_dir;
				else
					LgiAssert(0);

			#elif defined BEOS
				
				char p[MAX_PATH] = "";
				if (find_directory(B_USER_SETTINGS_DIRECTORY, 0, false, p, sizeof(p)))
					Path = p;
				else
					Path = "/boot/home/config/settings";
				
			#else

				LgiAssert(0);

			#endif

			if (Path)
			{
				Path += DIR_STR;
				Path += Name;
			}
			#endif
			break;
		}
		case LSP_OS:
		{
			#if defined WIN32

			char16 p[MAX_PATH];
			if (GetWindowsDirectoryW(p, CountOf(p)) > 0)
				Path = p;

			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kOnAppropriateDisk,  kSystemFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
					Path = u.Get();
			}
			
			#elif defined BEOS
			
			char p[MAX_PATH] = "";
			if (find_directory(B_SYSTEM_DIRECTORY, 0, false, p, sizeof(p)))
				Path = p;
			else
				LgiAssert(0);

			#else

			Path = "/boot"; // it'll do for now...

			#endif
			break;
		}
		case LSP_OS_LIB:
		{
			#if defined WIN32

			char16 p[MAX_PATH];
			if (GetSystemDirectoryW(p, CountOf(p)) > 0)
				Path = p;
			
			#elif defined MAC
			
			Path = "/Library";
			
			#elif defined BEOS

			char p[MAX_PATH] = "";
			if (find_directory(B_SYSTEM_LIB_DIRECTORY, 0, false, p, sizeof(p)))
				Path = p;
			else
				LgiAssert(0);

			#else

			Path = "/lib"; // it'll do for now...

			#endif
			break;
		}
		case LSP_TEMP:
		{
			#if defined WIN32

			char16 t[MAX_PATH];
			if (GetTempPathW(CountOf(t), t) > 0)
			{
				GAutoString utf(WideToUtf8(t));
				if (utf)
					Path = utf;
			}

			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kUserDomain, kTemporaryFolderType, kCreateFolder, &Ref);
			if (e) LgiTrace("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
				{
					Path = u.Get();
				}
				else LgiTrace("%s:%i - FSRefPath failed.\n", _FL);
			}
			
			#elif defined BEOS
			
			char p[MAX_PATH] = "";
			if (find_directory(B_SYSTEM_TEMP_DIRECTORY, 0, false, p, sizeof(p)))
				Path = p;
			else
				Path = "/tmp";

			#else

			Path = "/tmp"; // it'll do for now...

			#endif
			break;
		}
		case LSP_COMMON_APP_DATA:
		{
			#if defined WIN32

			Path = WinGetSpecialFolderPath(CSIDL_COMMON_APPDATA);
			
			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kOnSystemDisk, kDomainLibraryFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
					Path = u.Get();
			}

			#elif defined LINUX

			Path = "/usr";

			#elif defined BEOS

			Path = "/boot/apps";

			#endif
			break;
		}
		case LSP_USER_APP_DATA:
		{
			#if defined WIN32
			
			Path = WinGetSpecialFolderPath(CSIDL_APPDATA);

			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kUserDomain, kDomainLibraryFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
					Path = u.Get();
			}

			#elif defined LINUX

			Path = "/usr";

			#endif
			break;
		}
		case LSP_LOCAL_APP_DATA:
		{
			#if defined WIN32
			
			Path = WinGetSpecialFolderPath(CSIDL_LOCAL_APPDATA);
			
			#else
			
			LgiAssert(!"Impl me.");
			
			#endif
			break;
		}
		case LSP_DESKTOP:
		{
			#if defined(WINDOWS) && defined(_MSC_VER)

			Path = WinGetSpecialFolderPath(CSIDL_DESKTOPDIRECTORY);
			
			#elif defined(MAC)
			
				#if defined COCOA
				#else
				FSRef Ref;
				OSErr e = FSFindFolder(kOnAppropriateDisk, kDesktopFolderType, kDontCreateFolder, &Ref);
				if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
				else
				{
					GAutoString u = FSRefPath(Ref);
					if (u)
						Path = u.Get();
				}
				#endif

			#elif defined POSIX

			struct passwd *pw = getpwuid(getuid());
			if (pw)
			{
				#ifdef LINUX
				WindowManager wm = LgiGetWindowManager();
				if (wm == WM_Gnome)
				{
					Path.Printf("%s/.gnome-desktop", pw->pw_dir);
				}
				#endif

				if (!DirExists(Path))
				{
					Path.Printf("%s/Desktop", pw->pw_dir);
				}
			}

			#elif defined BEOS

			char p[MAX_PATH] = "";
			if (find_directory(B_DESKTOP_DIRECTORY, 0, false, p, sizeof(p))
				Path = p;
			else
				LgiAssert(0);
			
			#endif
			break;
		}
		case LSP_HOME:
		{
			#if defined WIN32

			Path = WinGetSpecialFolderPath(CSIDL_PROFILE);

			#elif defined POSIX

			struct passwd *pw = getpwuid(getuid());
			if (pw)
				Path = pw->pw_dir;

			#elif defined BEOS

			char p[MAX_PATH] = "";
			if (find_directory(B_USER_DIRECTORY, 0, false, p, sizeof(p))
				Path = p;
			else
				LgiAssert(0);

			#endif
			break;
		}
		case LSP_EXE:
		{
			char Buf[MAX_PATH];
			if (LgiGetExeFile(Buf, sizeof(Buf)))
			{
				LgiTrimDir(Buf);
				Path = Buf;
			}
			break;
		}
		case LSP_TRASH:
		{
			#if defined LINUX
			
			switch (LgiGetWindowManager())
			{
				case WM_Kde:
				{
					static char KdeTrash[256] = "";
					
					if (!ValidStr(KdeTrash))
					{
						// Ask KDE where the current trash is...
						GProcess p;
						GStringPipe o;
						if (p.Run("kde-config", "--userpath trash", 0, true, 0, &o))
						{
							char *s = o.NewStr();
							if (s)
							{
								// Store it..
								strcpy_s(KdeTrash, sizeof(KdeTrash), s);
								DeleteArray(s);

								// Clear out any crap at the end...
								char *e = KdeTrash + strlen(KdeTrash) - 1;
								while (e > KdeTrash && strchr(" \r\n\t/", *e))
								{
									*e-- = 0;
								}
							}
							else
							{
								printf("%s:%i - No output from 'kde-config'.\n", _FL);
							}
						}
						else
						{
							printf("%s:%i - Run 'kde-config' failed.\n", _FL);
						}
					}

					if (ValidStr(KdeTrash))
						Path = KdeTrash;
					break;
				}
				default:
				{
					GString Home = LGetSystemPath(LSP_HOME);
					if (!Home)
					{
						LgiTrace("%s:%i - Can't get LSP_HOME.\n", _FL);
						break;
					}
					
					char p[MAX_PATH];
					if (!LgiMakePath(p, sizeof(p), Home, ".local/share/Trash") ||
						!DirExists(p))
					{
						LgiTrace("%s:%i - '%s' doesn't exist.\n", _FL, p);
						break;
					}
					
					Path = p;
					break;
				}
			}
			
			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kUserDomain, kTrashFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
					Path = u.Get();
			}
			
			#elif defined BEOS

			char p[MAX_PATH] = "";
			if (find_directory(B_TRASH_DIRECTORY, 0, false, p, sizeof(p)))
				Path = p;
			else
				LgiAssert(0);

			#elif defined WIN32

			// This should work but doesn't... *shrug*
			// char *f = GetWin32Folder(CSIDL_BITBUCKET);

			#else

			#endif
			break;
		}
	}

	return Path;
}

bool LgiGetExeFile(char *Dst, int DstSize)
{
	if (Dst)
	{
		#if defined WIN32

		if (LgiGetOs() == LGI_OS_WIN9X)
		{
			char16 Exe[256];
			if (GetModuleFileNameW(NULL, Exe, CountOf(Exe)) > 0)
			{
				GString e = Exe;
				if (e)
				{
					strcpy_s(Dst, DstSize, e.Lower());
					return true;
				}
				else
				{
					LgiMsg(0, "LgiFromNativeCp returned 0, ANSI CodePage=%i (%s)", "LgiGetExeFile Error", MB_OK, GetACP(), LgiAnsiToLgiCp());
				}
			}

			GString m;
			m.Printf("GetModuleFileName failed err: %08.8X", GetLastError());
			MessageBoxA(0, m, "LgiGetExeFile Error", MB_OK);
			LgiExitApp();
		}
		else
		{
			char16 Exe[256];
			if (GetModuleFileNameW(NULL, Exe, sizeof(Exe)) > 0)
			{
				char *e = WideToUtf8(Exe);
				if (e)
				{
					strcpy_s(Dst, DstSize, e);
					DeleteArray(e);
					return true;
				}
			}
		}

		#elif defined BEOS

		app_info Info;
		if (LgiApp->GetAppInfo(&Info) == B_OK)
		{
			BEntry e(&Info.ref);
			BPath p;
			if (e.GetPath(&p) == B_OK)
			{
				strcpy_s(Dst, DstSize, p.Path());
				return true;
			}
		}

		#elif defined ATHEOS

		os::Directory AppPath;
		if (AppPath.SetTo("^/.") == 0)
		{
			std::string p;
			if (AppPath.GetPath(&p) == 0)
			{
				sprintf_s(Dst, DstSize, "%s%s%s", p.c_str(), DIR_STR, LgiApp->_AppFile);
				return true;
			}
		}

		#elif defined LINUX

		static char ExePathCache[256] = "";
		bool Status = false;

		// this is _REALLY_ lame way to do it... but hey there aren't any
		// other better ways to get the full path of the running executable
		if (!ExePathCache[0])
		{
			// First try the self method
			int Len = readlink("/proc/self/exe", ExePathCache, sizeof(ExePathCache));
			Status = FileExists(ExePathCache);
			// printf("readlink=%i Status=%i Exe='%s'\n", Len, Status, ExePathCache);
			if (!Status)
			{
				ExePathCache[0] = 0;
				
				// Next try the map file method
				char ProcFile[256];
				sprintf_s(ProcFile, sizeof(ProcFile), "/proc/%i/maps", getpid());
				int fd = open(ProcFile, O_RDONLY);

				if (fd >= 0)
				{
					int Len = 16 << 10; // is this enough?
										// no better way of determining the length of proc info?
					char *Buf = new char[Len+1];
					if (Buf)
					{
						int r = read(fd, Buf, Len);
						Buf[r] = 0;

						char *s = strchr(Buf, '/');
						if (s)
						{
							char *e = strchr(s, '\n');
							if (e)
							{
								*e = 0;
								strcpy_s(ExePathCache, sizeof(ExePathCache), s);
								Status = true;
							}
						}
						
						DeleteArray(Buf);
					}

					close(fd);
				}
				else
				{
					// Non proc system (like cygwin for example)
					// char Cmd[256];
					// sprintf_s(Cmd, sizeof(Cmd), "ps | grep \"%i\"", getpid());
					
					GStringPipe Ps;
					GProcess p;
					if (p.Run("ps", 0, 0, true, 0, &Ps))
					{
						char *PsOutput = Ps.NewStr();
						if (PsOutput)
						{
							GToken n(PsOutput, "\r\n");
							for (int i=0; !Status && i<n.Length(); i++)
							{
								GToken t(n[i], " \t\r\n");
								if (t.Length() > 7)
								{
									int LinePid = 0;
									for (int i=0; i<t.Length()-1; i++)
									{
										if (IsDigit(t[i][0]))
										{
											LinePid = atoi(t[i]);
											break;
										}
									}

									if (LinePid == getpid())
									{
										strcpy_s(ExePathCache, sizeof(ExePathCache), t[t.Length()-1]);
										Status = true;
									}
								}
							}
							
							DeleteArray(PsOutput);
						}					
					}
				}
			}
		}
		else Status = true;

		if (Status)
		{
			strcpy_s(Dst, DstSize, ExePathCache);
		}
		
		return Status;
		
		#elif defined MAC
		
			bool Status = false;
		
			#if COCOA
		
			if (FileExists(LgiArgsAppPath))
			{
				LgiMakePath(Dst, DstSize, LgiArgsAppPath, "../../..");
				Status = true;
			}
		
			#else
		
			ProcessSerialNumber ps;
			OSErr e = GetCurrentProcess(&ps);
			if (!e)
			{
				FSRef fs;
				OSStatus s = GetProcessBundleLocation(&ps, &fs);
				if (!s)
				{
					GAutoString u = FSRefPath(fs);
					if (!e)
					{
						strcpy_s(Dst, DstSize, u);
						Status = true;
					}
					else printf("%s:%i - FSGetCatalogInfo failed (e=%i).\n", _FL, e);
				}
				else printf("%s:%i - GetProcessBundleLocation failed (e=%i).\n", _FL, (int)s);
			}
			else printf("%s:%i - GetCurrentProcess failed (e=%i).\n", _FL, e);
		
			#endif
			
			return Status;
		
		#endif
	}

	return false;
}

bool LgiGetExePath(char *Dst, int DstSize)
{
	return LGetSystemPath(LSP_EXE, Dst, DstSize);
}

char *LgiGetExtension(const char *File)
{
	if (File)
	{
		char *Dot = strrchr((char*)File, '.');
		if (Dot && !strchr(Dot, DIR_CHAR))
		{
			return Dot + 1;
		}
	}

	return 0;
}

#define DEBUG_FIND_FILE		0

char *LgiFindFile(const char *Name)
{
	char *Result = 0;
	
	#if DEBUG_FIND_FILE
	printf("%s:%i - Name='%s'\n", __FILE__, __LINE__, Name);
	#endif
	
	if (Name)
	{
		char Exe[MAX_PATH];
		
		#ifndef MAC
		LgiGetExePath(Exe, sizeof(Exe));
		#else
		LgiGetExeFile(Exe, sizeof(Exe));
		// LgiTrimDir(Exe);
		#endif

		#if DEBUG_FIND_FILE
		printf("%s:%i - Exe='%s'\n", _FL, Exe);
		#endif

		char CurWorking[MAX_PATH];
		_getcwd(CurWorking, sizeof(CurWorking));
		const char *PrefPath[] =
		{
			".",
			"..",
			"Code",
			#ifdef MAC
			"./Contents/Resources",
			#else
			"./Resources",
			#endif
			#ifndef WIN32
			"./resources",
			#endif
			"../Code",
			"../Resources",
			#ifdef WIN32
			"../Debug",
			"../Release",
			"../../Debug",
			"../../Release",
			#endif
			CurWorking,
			0
		};

		// Look in prefered paths first...
		for (const char **Pref = PrefPath; *Pref; Pref++)
		{
			char Path[MAX_PATH];
			if (LgiIsRelativePath(*Pref))
			{
				LgiMakePath(Path, sizeof(Path), Exe, *Pref);
				LgiMakePath(Path, sizeof(Path), Path, Name);
			}
			else
			{
				LgiMakePath(Path, sizeof(Path), *Pref, Name);
			}
			size_t PathLen = strlen(Path);
			LgiAssert(PathLen < sizeof(Path));

			if (FileExists(Path))
				return NewStr(Path);

			#ifdef WIN32
			if (PathLen < sizeof(Path) - 4)
			{
				strcat(Path, ".lnk");
				if (ResolveShortcut(Path, Path, sizeof(Path)))
					return NewStr(Path);
			}
			#endif
		}

		// General search fall back...
		GArray<const char*> Ext;
		GArray<char*> Files;
		Ext.Add((char*)Name);

		#if DEBUG_FIND_FILE
		printf("%s:%i - Exe='%s'\n", _FL, Exe);
		#endif

		if (LgiRecursiveFileSearch(Exe, &Ext, &Files) &&
			Files.Length())
		{
			Result = Files[0];
			Files.DeleteAt(0);
		}
		Files.DeleteArrays();
	}

	return Result;
}

#if defined WIN32
static LARGE_INTEGER Freq = {0};
static bool CurTimeInit = false;
#endif

uint64 LgiCurrentTime()
{
	#if defined WIN32

	if (!CurTimeInit)
	{
		CurTimeInit = true;
		if (!QueryPerformanceFrequency(&Freq))
			Freq.QuadPart = 0;
	}

	if (Freq.QuadPart)
	{
		// Return performance counter in ms
		LARGE_INTEGER i;
		if (QueryPerformanceCounter(&i))
		{
			return i.QuadPart * 1000 / Freq.QuadPart;
		}

		// Now what?? Give up and go back to tick count I guess.
		Freq.QuadPart = 0;
	}

	// Fall back for systems without a performance counter.
	return GetTickCount();

	#elif defined BEOS

	return system_time() / 1000;

	#elif defined MAC && !defined COCOA
	
	UnsignedWide t;
	Microseconds(&t);
	uint64 i = ((uint64)t.hi << 32) | t.lo;
	return i / 1000;

	#else

	timeval tv;
	gettimeofday(&tv, 0);
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

	#endif
}

uint64 LgiMicroTime()
{
	#if defined WIN32

	if (!CurTimeInit)
	{
		CurTimeInit = true;
		if (!QueryPerformanceFrequency(&Freq))
			Freq.QuadPart = 0;
	}
	if (Freq.QuadPart)
	{
		// Return performance counter in ms
		LARGE_INTEGER i;
		if (QueryPerformanceCounter(&i))
		{
			return i.QuadPart * 1000000 / Freq.QuadPart;
		}
	}
	return 0;

	#elif defined BEOS

	LgiAssert(!"Not impl.");
	return 0;

	#elif defined MAC && !defined COCOA
	
	UnsignedWide t;
	Microseconds(&t);
	return ((uint64)t.hi << 32) | t.lo;

	#else

	timeval tv;
	gettimeofday(&tv, 0);
	return (tv.tv_sec * 1000000) + tv.tv_usec;

	#endif
}

int LgiIsReleaseBuild()
{
	#ifdef _DEBUG
	return 0;
	#else
	return 1;
 	#endif
}

bool LgiIsVolumeRoot(const char *Path)
{
	if (!Path)
		return false;
	
	#ifdef WIN32
		if
		(
			IsAlpha(Path[0])
			&&
			Path[1] == ':'
			&&
			(
				(Path[2] == 0)
				||
				(Path[2] == '\\' && Path[3] == 0)
			)
		)
		{
			return true;
		}
	#else
		GToken t(Path, DIR_STR);
		if (t.Length() == 0)
			return true;
		#ifdef MAC
			if (!stricmp(t[0], "Volumes") &&
				t.Length() == 2)
				return true;
		#else
			if (!stricmp(t[0], "mnt") &&
				t.Length() == 2)
				return true;
		#endif
	#endif
	
	return false;
}

void LgiFormatSize(char *Str, int SLen, uint64 Size)
{
	uint64 K = 1024;
	uint64 M = K * K;
	uint64 G = K * M;
	uint64 T = K * G;

	if (Size == 1)
	{
		strcpy_s(Str, SLen, "1 byte");
	}
	else if (Size < K)
	{
		sprintf_s(Str, SLen, "%u bytes", (int)Size);
	}
	else if (Size < 10 * K)
	{
		double d = (double)(int64)Size;
		sprintf_s(Str, SLen, "%.2f KiB", d / K);
	}
	else if (Size < M)
	{
		sprintf_s(Str, SLen, "%u KiB", (int) (Size / K));
	}
	else if (Size < G)
	{
		double d = (double)(int64)Size;
		sprintf_s(Str, SLen, "%.2f MiB", d / M);
	}
	else if (Size < T)
	{
		double d = (double)(int64)Size;
		sprintf_s(Str, SLen, "%.2f GiB", d / G);
	}
	else
	{
		double d = (double)(int64)Size;
		sprintf_s(Str, SLen, "%.2f TiB", d / T);
	}
}

char *LgiTokStr(const char *&s)
{
	char *Status = 0;

	if (s && *s)
	{
		// Skip whitespace
		static char Delim[] = ", \t\r\n";
		while (*s && strchr(Delim, *s)) s++;
		if (*s)
		{
			if (strchr("\'\"", *s))
			{
				char Delim = *s++;
				const char *e = strchr(s, Delim);
				if (!e)
				{
					// error, no end delimiter
					e = s;
					while (*e) e++;
				}
				Status = NewStr(s, e - s);
				s = *e ? e + 1 : e;
			}
			else
			{
				const char *e = s;
				while (*e && !strchr(Delim, *e)) e++;
				Status = NewStr(s, e - s);
				s = e;
			}
		}		
	}
	
	return Status;
}

GString LGetEnv(const char *Var)
{
#ifdef _MSC_VER
	char *s = NULL;
	size_t sz;
	errno_t err = _dupenv_s(&s, &sz, Var);
	if (err)
		return GString();

	GString ret(s);
	free(s);
	return ret;
#else
	return getenv("PATH");
#endif
}

GString::Array LGetPath()
{
	GString::Array Paths;

	#ifdef MAC
		// OMG, WHY?! Seriously?
		//
		// The GUI application path is NOT the same as what is configured for the terminal.
		// At least in 10.12. And I don't know how to make them the same. This works around
		// that for the time being.
		GFile::Path Home(LSP_HOME);
		Home += ".profile";
		auto Profile = GFile(Home, O_READ).Read().Split("\n");
		for (auto Ln : Profile)
		{
			auto p = Ln.SplitDelimit(" =", 2);
			if (p.Length() == 3 &&
				p[0].Equals("export") &&
				p[1].Equals("PATH"))
			{
				Paths = p[2].Strip("\"").SplitDelimit(LGI_PATH_SEPARATOR);
				Paths.SetFixedLength(false);
				for (auto &p : Paths)
				{
					if (p.Equals("$PATH"))
					{
						auto SysPath = LGetEnv("PATH").SplitDelimit(LGI_PATH_SEPARATOR);
						for (unsigned i=0; i<SysPath.Length(); i++)
						{
							if (i) Paths.New() = SysPath[i];
							else p = SysPath[i];
						}
					}
				}
				break;
			}
		}
	#else
		Paths = LGetEnv("PATH").SplitDelimit(LGI_PATH_SEPARATOR);
	#endif

	return Paths;
}

//////////////////////////////////////////////////////////////////////////////
DoEvery::DoEvery(int p) // p = timeout in ms
{
	Init(p);
}

void DoEvery::Init(int p) // p = timeout in ms
{
	LastTime = LgiCurrentTime();
	if (p > 0)
	{
		Period = p;
	}
}

bool DoEvery::DoNow()
{
	int64 Now = LgiCurrentTime();
	if (LastTime + Period < Now)
	{
		LastTime = Now;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////
bool GCapabilityClient::NeedsCapability(const char *Name, const char *Param)
{
    for (int i=0; i<Targets.Length(); i++)
        Targets[i]->NeedsCapability(Name, Param);
    return Targets.Length() > 0;
}

GCapabilityClient::~GCapabilityClient()
{
    for (int i=0; i<Targets.Length(); i++)
        Targets[i]->Clients.Delete(this);
}

void GCapabilityClient::Register(GCapabilityTarget *t)
{
    if (t && !Targets.HasItem(t))
    {
        Targets.Add(t);
        t->Clients.Add(this);
    }
}

GCapabilityTarget::~GCapabilityTarget()
{
    for (int i=0; i<Clients.Length(); i++)
        Clients[i]->Targets.Delete(this);
}

/////////////////////////////////////////////////////////////////////
#define BUF_SIZE (4 << 10)
#define PROFILE_MICRO	1

GProfile::GProfile(const char *Name, int HideMs)
{
	MinMs = HideMs;
	Used = 0;
	Buf = NULL;
	Add(Name);
}

GProfile::~GProfile()
{
	Add("End");
	
	if (MinMs > 0)
	{
		uint64 TotalMs = s.Last().Time - s[0].Time;
		if (TotalMs < MinMs
			#if PROFILE_MICRO
			* 1000
			#endif
			)
		{
			return;
		}
	}
	
	// char c[1024];
	// int ch = 0;

	for (int i=0; i<s.Length()-1; i++)
	{
		Sample &a = s[i];
		Sample &b = s[i+1];
		
		#if 1

			#if PROFILE_MICRO
			LgiTrace("%s%s = %.2f ms\n", i ? "    " : "", a.Name, (double)(b.Time - a.Time)/1000.0);
			#else
			LgiTrace("%s%s = %i ms\n", i ? "    " : "", a.Name, (int)(b.Time - a.Time));
			#endif
		
		#else

			#if PROFILE_MICRO
			ch += sprintf_s(c+ch, sizeof(c)-ch, "%s%s = %.2f ms\n", i ? "    " : "", a.Name, (double)(b.Time - a.Time)/1000.0);
			#else
			ch += sprintf_s(c+ch, sizeof(c)-ch, "%s%s = %i ms\n", i ? "    " : "", a.Name, (int)(b.Time - a.Time));
			#endif

		#endif
	}

	// OutputDebugStringA(c);
	DeleteArray(Buf);
}

void GProfile::HideResultsIfBelow(int Ms)
{
	MinMs = Ms;
}

void GProfile::Add(const char *Name)
{
	s.Add(Sample(
		#if PROFILE_MICRO
		LgiMicroTime(),
		#else
		LgiCurrentTime(),
		#endif
		Name));
}

void GProfile::Add(const char *File, int Line)
{
	if (!Buf)
		if (!(Buf = new char[BUF_SIZE]))
			return;
	if (Used > BUF_SIZE - 64)
	{
		LgiAssert(0);
		return;
	}
	char *Name = Buf + Used;
	Used += sprintf_s(Name, BUF_SIZE - Used, "%s:%i", File, Line) + 1;
	s.Add(Sample(
		#if PROFILE_MICRO
		LgiMicroTime(),
		#else
		LgiCurrentTime(),
		#endif
		Name));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool LIsValidEmail(GString Email)
{
	// Local part
	char buf[321];
	char *o = buf;
	char *e = Email;

	if (!Email || *e == '.')
		return false;

	#define OutputChar()	\
		if (o - buf >= sizeof(buf) - 1)	\
			return false;	\
		*o++ = *e++

	// Local part
	while (*e)
	{
		if (strchr("!#$%&\'*+-/=?^_`.{|}~", *e) ||
			IsAlpha((uchar)*e) ||
			IsDigit((uchar)*e))
		{
			OutputChar();
		}
		else if (*e == '\"')
		{
			// Quoted string
			OutputChar();

			bool quote = false;
			while (*e && !quote)
			{
				quote = *e == '\"';
				OutputChar();
			}
		}
		else if (*e == '\\')
		{
			// Quoted character
			e++;
			if (*e < ' ' || *e >= 0x7f)
				return false;
			OutputChar();
		}
		else if (*e == '@')
		{
			break;
		}
		else
		{
			// Illegal character
			return false;
		}
	}

	// Process the '@'
	if (*e != '@' || o - buf > 64)
		return false;
	OutputChar();
	
	// Domain part...
	if (*e == '[')
	{
		// IP addr
		OutputChar();

		// Initial char must by a number
		if (!IsDigit(*e))
			return false;
		
		// Check the rest...
		char *Start = e;
		while (*e)
		{
			if (IsDigit(*e) ||
				*e == '.')
			{
				OutputChar();
			}
			else
			{
				return false;
			}
		}

		// Not a valid IP
		if (e - Start > 15)
			return false;

		if (*e != ']')
			return false;

		OutputChar();
	}
	else
	{
		// Hostname, check initial char
		if (!IsAlpha(*e) && !IsDigit(*e))
			return false;
		
		// Check the rest.
		while (*e)
		{
			if (IsAlpha(*e) ||
				IsDigit(*e) ||
				strchr(".-", *e))
			{
				OutputChar();
			}
			else
			{
				return false;
			}
		}
	}

	// Remove any trailing dot/dash
	while (strchr(".-", o[-1]))
		o--;

	// Output
	*o = 0;
	LgiAssert(o - buf <= sizeof(buf));
	if (strcmp(Email, buf))
		Email.Set(buf, o - buf);
	return true;
}

//////////////////////////////////////////////////////////////////////////
GString LGetAppForProtocol(const char *Protocol)
{
	GString App;

	if (!Protocol)
		return App;

	#ifdef WINDOWS
		GRegKey k(false, "HKEY_CLASSES_ROOT\\%s\\shell\\open\\command", Protocol);
		if (k.IsOk())
		{
			const char *p = k.GetStr();
			if (p)
			{
				GAutoString a(LgiTokStr(p));
				App = a.Get();
			}
		}
	#elif defined(LINUX)
		const char *p = NULL;
		if (stricmp(Protocol, "mailto"))
			p = "xdg-email";
		else
			p = "xdg-open";
		GString Path = LGetEnv("PATH");
		GString::Array a = Path.SplitDelimit(LGI_PATH_SEPARATOR);
		for (auto i : a)
		{
			GFile::Path t(i);
			t += p;
			if (t.Exists())
			{
				App = t.GetFull();
				break;
			}
		}
	#elif defined(MAC)
		// Get the handler type
		CFStringRef Type = GString(Protocol).CreateStringRef();
		CFStringRef Handler = LSCopyDefaultHandlerForURLScheme(Type);
		CFRelease(Type);
		if (Handler)
		{
			// Convert to app path
			CFErrorRef Err;
			auto a = LSCopyApplicationURLsForBundleIdentifier(Handler, &Err);
			if (a)
			{
				if (CFArrayGetCount(a) > 0)
				{
					CFURLRef nsurl = (CFURLRef)CFArrayGetValueAtIndex(a, 0);
					App = CFURLGetString(nsurl);
					if (App.Find("file:///") == 0)
						App = App(7,-2);
				}
				CFRelease(a);
			}
		}
		CFRelease(Handler);
	#else
		#error "Impl me."
	#endif

	return App;
}

