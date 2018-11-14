#ifndef _Lvc_h_
#define _Lvc_h_

#include "Lgi.h"
#include "LList.h"
#include "GBox.h"
#include "GTree.h"
#include "GOptionsFile.h"
#include "GTextView3.h"
#include "GTextLog.h"
#include "GTabView.h"
#include "GEdit.h"

#define OPT_Folders		"Folders"
#define OPT_Folder		"Folder"

#define APP_VERSION		"0.4"
extern const char *AppName;

enum LvcIcon
{
	IcoFolder,
	IcoCleanFolder,
	IcoDirtyFolder,
	IcoFile,
	IcoCleanFile,
	IcoDirtyFile,
};

enum LvcNotify
{
	 LvcBase = GNotifyUserApp,
	 LvcCommandStart,
	 LvcCommandEnd
};

enum FileColumns
{
	COL_CHECKBOX,
	COL_STATE,
	COL_FILENAME,
};

enum AppIds
{
	IDC_LIST = 100,
	IDC_TOOLS_BOX,
	IDC_FOLDERS_BOX,
	IDC_COMMITS_BOX,
	IDC_TREE,
	IDC_FILES,
	IDC_FILES_BOX,
	IDC_TXT,
	IDC_LOG,
	IDC_MSG_BOX,
	IDC_TAB_VIEW,

	IDM_ADD = 200,
	IDM_UPDATE,
	IDM_COPY_REV,
	IDM_REMOVE,
	IDM_CLEAN,
	IDM_REVERT,
	IDM_REVERT_TO_REV,
	IDM_BLAME,
	IDM_SAVE_AS,
	IDM_BROWSE,
	IDM_LOG,
	IDM_EOL_LF,
	IDM_EOL_CRLF,
	IDM_EOL_AUTO,
	IDM_ADD_FILE,
	IDM_ADD_BINARY_FILE,
	IDM_BROWSE_FOLDER,
	IDM_TERMINAL,
	IDM_RESOLVE,
};

enum VersionCtrl
{
	VcNone,
	
	VcCvs,
	VcSvn,
	VcGit,
	VcHg,
	
	VcMax,
};

class VcFolder;

struct AppPriv
{
	GTree *Tree;
	VcFolder *CurFolder;
	LList *Lst;
	LList *Files;
	GOptionsFile Opts;
	GEdit *Msg;
	GTextLog *Diff;
	GTextLog *Log;
	GTabView *Tabs;

	AppPriv()  : Opts(GOptionsFile::PortableMode, AppName)
	{
		Lst = NULL;
		Tree = NULL;
		Files = NULL;
		Diff = NULL;
		Log = NULL;
		Msg = NULL;
		Tabs = NULL;
		CurFolder = NULL;
	}

	void ClearFiles()
	{
		if (Files)
			Files->Empty();
		if (Diff)
			Diff->Name(NULL);		
	}
};

class BlameUi : public GWindow
{
	struct BlameUiPriv *d;

public:
	BlameUi(AppPriv *priv, VersionCtrl Vc, GString Output);
	~BlameUi();
};

class DropDownBtn : public GDropDown, public ResObject
{
	struct DropDownBtnPriv *d;
	class DropLst *Pu;

public:
	DropDownBtn();
	~DropDownBtn();

	GString::Array GetList();
	bool SetList(int EditCtrl, GString::Array a);
	bool OnLayout(GViewLayoutInfo &Inf);
};

extern VersionCtrl DetectVcs(const char *Path);
extern bool ConvertEol(const char *Path, bool Cr);
extern int GetEol(const char *Path);
extern GString::Array GetProgramsInPath(const char *Program);

#include "VcFile.h"
#include "VcCommit.h"
#include "VcFolder.h"

#endif
