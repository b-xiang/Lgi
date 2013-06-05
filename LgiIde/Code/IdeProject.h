#ifndef _IDE_PROJECT_H_
#define _IDE_PROJECT_H_

#include "GXmlTree.h"
#include "GDragAndDrop.h"
#include "GTree.h"
#include "GOptionsFile.h"

#define NODE_DROP_FORMAT			"Ide.ProjectNode"

#define TARGET_TYPE_EXE				0
#define TARGET_TYPE_SHARED_LIB		1
#define TARGET_TYPE_STATIC_LIB		2

#define OPT_Ftp						"ftp"
#define OPT_Www						"www"

enum ExeAction
{
	ExeRun,
	ExeDebug,
	ExeValgrind
};

class IdeCommon : public GTreeItem, public GXmlTag
{
	friend class IdeProject;

public:
	~IdeCommon();
	
	void CollectAllSubProjects(List<IdeProject> &c);
	void CollectAllSource(GArray<char*> &c);
	void SortChildren();
	void InsertTag(GXmlTag *t);
	void RemoveTag();
	virtual bool IsWeb() = 0;	
	IdeCommon *GetSubFolder(IdeProject *Project, char *Name, bool Create = false);
};

enum ProjSetting
{
	ProjNone,
	ProjMakefile,
	ProjExe,
	ProjArgs,
	ProjDefines,
	ProjCompiler,
	ProjIncludePaths,
	ProjLibraries,
	ProjLibraryPaths,
	ProjTargetType,
	ProjTargetName,
	ProjEditorTabSize,
	ProjEditorIndentSize,
	ProjEditorShowWhiteSpace,
	ProjEditorUseHardTabs,
	ProjCommentFile,
	ProjCommentFunction,
	ProjMakefileRules
};

class IdeProjectSettings
{
	struct IdeProjectSettingsPriv *d;

public:
	IdeProjectSettings(GOptionsFile *Opts);
	~IdeProjectSettings();

	void InitAllSettings(bool ClearCurrent = false);

	// Configuration
	const char *GetCurrentConfig();
	bool SetCurrentConfig(const char *Config);
	bool AddConfig(const char *Config);
	bool DeleteConfig(const char *Config);
	
	// UI
	void Edit(GViewI *parent);

	// Accessors
	const char *GetStr(ProjSetting Setting, const char *Default = NULL);
	int GetInt(ProjSetting Setting, int Default = NULL);
	bool Set(ProjSetting Setting, const char *Value);
	bool Set(ProjSetting Setting, int Value);
};

class IdeProject : public GXmlFactory, public IdeCommon
{
	friend class ProjectNode;
	class IdeProjectPrivate *d;

public:
	IdeProject(class AppWnd *App);
	~IdeProject();

	bool IsWeb() { return false; }
	char *GetFileName();
	const char *GetExecutable();
	const char *GetExeArgs();
	const char *GetIncludePaths();
	void CreateProject();
	bool OpenFile(char *FileName);
	bool SaveFile(char *FileName = 0);
	void SetClean();
	void SetDirty();
	char *GetText(int Col);
	int GetImage(int Flags);
	GXmlTag *Create(char *Tag);
	void Empty();
	void OnMouseClick(GMouse &m);
	AppWnd *GetApp();
	void ImportDsp(char *File);
	bool GetBasePath(char *Path, int Len);
	bool GetMakefile(char *Path, int Len);
	bool GetExePath(char *Path, int Len);
	bool RelativePath(char *Out, char *In);
	bool Serialize();
	void Build(bool All);
	void Clean();
	void Execute(ExeAction Act = ExeRun);
	IdeProject *GetParentProject();
	bool GetChildProjects(List<IdeProject> &c);
	void SetParentProject(IdeProject *p);
	char *FindFullPath(char *File);
	bool InProject(char *FullPath, bool Open, class IdeDoc **Doc = 0);
	const char *GetFileComment();
	const char *GetFunctionComment();
	bool CreateMakefile();
	bool GetTargetName(char *Buf, int BufSize);
	bool GetTargetFile(char *Buf, int BufSize);
	bool BuildIncludePaths(List<char> &Paths, bool Recurse);
	
	IdeProjectSettings *GetSettings();
};

class IdeTree : public GTree, public GDragDropTarget
{
	GTreeItem *Hit;

public:
	IdeTree();
	
	void OnAttach();
	void OnDragExit();
	int WillAccept(List<char> &Formats, GdcPt2 p, int KeyState);
	int OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState);
};

#endif
