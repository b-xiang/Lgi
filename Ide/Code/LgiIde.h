#ifndef _LGI_IDE_H_
#define _LGI_IDE_H_

//
// Compiler specific macros:
//
//		Microsoft Visual C++: _MSC_VER
//
//		GCC: __GNUC__
//

#ifdef WIN32
#include "resource.h"
#endif
#include "resdefs.h"
#include "GDocView.h"
#include "GOptionsFile.h"
#include "FindSymbol.h"
#include "GStringClass.h"
#include "GDebugger.h"
#include "GTextView3.h"
#include "LList.h"

#define APP_VER					"1.0"
#define APP_URL					"http://www.memecode.com/lgi/ide"

#define DEBUG_FIND_DEFN			0

enum IdeMessages
{
	M_APPEND_TEXT = M_USER+200,
	M_APPEND_STR,
	M_START_BUILD,
	M_BUILD_ERR,
	M_BUILD_DONE,
	M_DEBUG_ON_STATE,
	
	/// Find symbol results message:
	/// GAutoPtr<FindSymRequest> Req((FindSymRequest*)Msg->A());
	M_FIND_SYM_REQUEST,
	
	/// Send a file to the worker thread...
	/// FindSymbolSystem::SymFileParams *Params = (FindSymbolSystem::SymFileParams*)Msg->A();
	M_FIND_SYM_FILE,

	/// Send a file to the worker thread...
	/// GAutoPtr<GString::Array> Paths((GString::Array*)Msg->A());
	M_FIND_SYM_INC_PATHS,

	/// Styling is finished
	M_STYLING_DONE,
};

#define ICON_PROJECT			0
#define ICON_DEPENDANCY			1
#define ICON_FOLDER				2
#define ICON_SOURCE				3
#define ICON_HEADER				4
#define ICON_RESOURCE			5
#define ICON_GRAPHIC			6
#define ICON_WEB				7

enum IdeIcon
{
	CMD_NEW,
	CMD_OPEN,
	CMD_SAVE,
	CMD_CUT,
	CMD_COPY,
	CMD_PASTE,
	CMD_COMPILE,
	CMD_BUILD,
	CMD_STOP_BUILD,
	CMD_EXECUTE,
	CMD_DEBUG,
	CMD_PAUSE,
	CMD_KILL,
	CMD_RESTART,
	CMD_RUN_TO,
	CMD_STEP_INTO,
	CMD_STEP_OVER,
	CMD_STEP_OUT,
	CMD_FIND_IN_FILES,
	CMD_SEARCH,
	CMD_SAVE_ALL,
};

enum IdeControls
{
	IDC_STATIC = -1,
	IDC_WATCH_LIST = 700,
	IDC_LOCALS_LIST,
	IDC_DEBUG_EDIT,
	IDC_DEBUGGER_LOG,
	IDC_BUILD_LOG,
	IDC_OUTPUT_LOG,
	IDC_FIND_LOG,
	IDC_CALL_STACK,
	IDC_DEBUG_TAB,
	IDC_OBJECT_DUMP,
	IDC_MEMORY_DUMP,
	IDC_MEMORY_TABLE,
	IDC_MEM_ADDR,
	IDC_MEM_SIZE,
	IDC_MEM_ROW_LEN,
	IDC_MEM_HEX,
	IDC_REGISTERS,
	IDC_THREADS,
	IDC_EDIT,
	IDC_FILE_SEARCH,
	IDC_METHOD_SEARCH,
	IDC_SYMBOL_SEARCH,
	IDC_PROJECT_TREE,
	IDC_ALL_PLATFORMS,
};

enum IdeMenuCmds
{
	IDM_CONTINUE = 900
};

#define BUILD_TYPE_DEBUG		0
#define BUILD_TYPE_RELEASE		1

#define OPT_EditorFont			"EdFont"
#define OPT_Jobs				"Jobs"

//////////////////////////////////////////////////////////////////////
// Platform stuff
enum IdePlatform
{
	PlatformCurrent = -1,
	PlatformWin32 = 0,
	PlatformLinux,
	PlatformMac,
	PlatformHaiku,
	PlatformMax,
};
#define PLATFORM_WIN32			(1 << PlatformWin32)
#define PLATFORM_LINUX			(1 << PlatformLinux)
#define PLATFORM_MAC			(1 << PlatformMac)
#define PLATFORM_HAIKU			(1 << PlatformHaiku)
#define PLATFORM_ALL			(PLATFORM_WIN32|PLATFORM_LINUX|PLATFORM_MAC|PLATFORM_HAIKU)

#if defined(_WIN32)
#define PLATFORM_CURRENT		PLATFORM_WIN32
#elif defined(MAC)
#define PLATFORM_CURRENT		PLATFORM_MAC
#elif defined(LINUX)
#define PLATFORM_CURRENT		PLATFORM_LINUX
#elif defined(BEOS)
#define PLATFORM_CURRENT		PLATFORM_HAIKU
#endif

extern const char *PlatformNames[];
extern const char sCurrentPlatform[];
extern const char *Untitled;
extern const char SourcePatterns[];

//////////////////////////////////////////////////////////////////////
class IdeDoc;
class IdeProject;

extern char AppName[];
extern char *FindHeader(char *Short, GArray<GString> &Paths);
extern bool BuildHeaderList(char *Cpp, GArray<char*> &Headers, GArray<GString> &IncPaths, bool Recurse);

class NodeView;
class NodeSource
{
	friend class NodeView;

protected:
	NodeView *nView;

public:
	NodeSource()
	{
		nView = 0;
	}
	virtual ~NodeSource();

	virtual GString GetFullPath() = 0;
	virtual bool IsWeb() = 0;
	virtual char *GetFileName() = 0;
	virtual char *GetLocalCache() = 0;
	virtual bool Load(GDocView *Edit, NodeView *Callback) = 0;
	virtual bool Save(GDocView *Edit, NodeView *Callback) = 0;
	virtual IdeProject *GetProject() = 0;
};

class NodeView
{
	friend class NodeSource;

protected:
	NodeSource *nSrc;

public:
	NodeView(NodeSource *s)
	{
		if ((nSrc = s))
		{
			nSrc->nView = this;
		}
	}

	virtual ~NodeView();

	virtual void OnDelete() = 0;
	virtual void OnSaveComplete(bool Status) = 0;

};

class AppWnd : public GWindow
{
	class AppWndPrivate *d;
	friend class AppWndPrivate;

	void UpdateMemoryDump();
	void DumpHistory();

public:
	enum Channels
	{
		BuildTab,
		OutputTab,
		FindTab,
		FtpTab,
		DebugTab,
	};

	enum DebugTabs
	{
		LocalsTab,
		ObjectTab,
		WatchTab,
		MemoryTab,
		ThreadsTab,
		CallStackTab,
		RegistersTab
	};

	AppWnd();
	~AppWnd();

	void SaveAll();
	void CloseAll();
	IdeDoc *OpenFile(const char *FileName, NodeSource *Src = 0);
	IdeDoc *NewDocWnd(const char *FileName, NodeSource *Src);
	IdeDoc *GetCurrentDoc();
	IdeProject *OpenProject(char *FileName, IdeProject *ParentProj, bool Create = false, bool Dep = false);
	IdeProject *RootProject();
	IdeDoc *TopDoc();
	IdeDoc *FocusDoc();
	GTextView3 *FocusEdit();
	void AppendOutput(char *Txt, Channels Channel);
	void UpdateState(int Debugging = -1, int Building = -1);
	void OnReceiveFiles(GArray<char*> &Files) override;
	int GetBuildMode();
	GTree *GetTree();
	GOptionsFile *GetOptions();
	LList *GetFtpLog();
	GStream *GetBuildLog();
	IdeDoc *FindOpenFile(char *FileName);
	IdeDoc *GotoReference(const char *File, int Line, bool CurIp, bool WithHistory = true);
	void FindSymbol(int ResultsSinkHnd, const char *Sym, bool AllPlatforms);
	bool GetSystemIncludePaths(GArray<GString> &Paths);
	bool IsReleaseMode();
	bool ShowInProject(const char *Fn);
	bool Build();
	
	// Events
	void OnLocationChange(const char *File, int Line);
	int OnCommand(int Cmd, int Event, OsView Wnd) override;
	void OnDocDestroy(IdeDoc *Doc);
	void OnProjectDestroy(IdeProject *Proj);
	void OnProjectChange();
	void OnFile(char *File, bool IsProject = false);
	bool OnRequestClose(bool IsClose) override;
	int OnNotify(GViewI *Ctrl, int Flags) override;
	GMessage::Result OnEvent(GMessage *m) override;
	bool OnNode(const char *Path, class ProjectNode *Node, FindSymbolSystem::SymAction Action);
	void OnPulse() override;

	// Debugging support
	class GDebugContext *GetDebugContext();
	bool ToggleBreakpoint(const char *File, ssize_t Line);
	bool OnBreakPoint(GDebugger::BreakPoint &b, bool Add);
	bool LoadBreakPoints(IdeDoc *doc);
	bool LoadBreakPoints(GDebugger *db);
	void OnDebugState(bool Debugging, bool Running);
};

#include "IdeDoc.h"
#include "IdeProject.h"
#include "FindInFiles.h"

extern void NewMemDumpViewer(AppWnd *App, char *file = 0);

class SysCharSupport : public GWindow
{
	class SysCharSupportPriv *d;

public:
	SysCharSupport(AppWnd *app);
	~SysCharSupport();

	int OnNotify(GViewI *v, int f);
	void OnPosChange();
};

#endif
