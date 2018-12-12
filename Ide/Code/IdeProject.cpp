#if defined(WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "GDragAndDrop.h"
#include "GToken.h"
#include "resdefs.h"
#include "GProcess.h"
#include "GCombo.h"
#include "INet.h"
#include "LListItemCheckBox.h"
#include "FtpThread.h"
#include "GClipBoard.h"
#include "GDropFiles.h"
#include "GSubProcess.h"
#include "ProjectNode.h"
#include "WebFldDlg.h"
#include "GCss.h"
#include "GTableLayout.h"
#include "GTextLabel.h"
#include "GButton.h"

extern const char *Untitled;
const char SourcePatterns[] = "*.c;*.h;*.cpp;*.cc;*.java;*.d;*.php;*.html;*.css";
const char *AddFilesProgress::DefaultExt = "c,cpp,cc,cxx,h,hpp,hxx,html,css,json,js,jsx,txt,png,jpg,jpeg,rc,xml,mk,paths,makefile,py";

#define USE_OPEN_PROGRESS			1

#define STOP_BUILD_TIMEOUT			3000
#ifdef WINDOWS
#define LGI_STATIC_LIBRARY_EXT		"lib"
#else
#define LGI_STATIC_LIBRARY_EXT		"a"
#endif

const char *PlatformNames[] =
{
	"Windows",
	"Linux",
	"Mac",
	"Haiku",
	0
};

int PlatformCtrlId[] =
{
	IDC_WIN32,
	IDC_LINUX,
	IDC_MAC,
	IDC_HAIKU,
	0
};

char *ToUnixPath(char *s)
{
	if (s)
	{
		char *c;
		while ((c = strchr(s, '\\')))
		{
			*c = '/';
		}
	}
	return s;
}

const char *CastEmpty(char *s)
{
	return s ? s : "";
}

bool FindInPath(GString &Exe)
{
	GString::Array Path = GString(getenv("PATH")).Split(LGI_PATH_SEPARATOR);
	for (unsigned i=0; i<Path.Length(); i++)
	{
		char p[MAX_PATH];
		LgiMakePath(p, sizeof(p), Path[i], Exe);
		if (FileExists(p))
		{
			Exe = p;
			return true;
		}
	}
	
	return false;
}

GAutoString ToNativePath(const char *s)
{
	GAutoString a(NewStr(s));
	if (a)
	{
		if (strnicmp(a, "ftp://", 6))
		{
			for (char *c = a; *c; c++)
			{
				#ifdef WIN32
				if (*c == '/')
					*c = '\\';
				#else
				if (*c == '\\')
					*c = '/';
				#endif
			}
		}
	}
	return a;
}

int StrCmp(char *a, char *b, NativeInt d)
{
	return stricmp(a, b);
}

int StrSort(char **a, char **b)
{
	return stricmp(*a, *b);
}

//////////////////////////////////////////////////////////////////////////////////
class ProjectNode;

class BuildThread : public LThread, public GStream
{
	IdeProject *Proj;
	GString Makefile;
	bool Clean, Release, All;
	int WordSize;
	GAutoPtr<GSubProcess> SubProc;
	GString::Array BuildConfigs;
	GString::Array PostBuild;

	enum CompilerType
	{
		DefaultCompiler,
		VisualStudio,
		MingW,
		Gcc,
		CrossCompiler,
		PythonScript,
		IAR,
		Nmake
	}
		Compiler;

	enum ArchType
	{
		DefaultArch,
		ArchX32,
		ArchX64,
		ArchArm6,
		ArchArm7,
	}
		Arch;

public:
	BuildThread(IdeProject *proj, char *makefile, bool clean, bool release, bool all, int wordsize);
	~BuildThread();
	
	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0) override;
	GString FindExe();
	GAutoString WinToMingWPath(const char *path);
	int Main() override;
};

class IdeProjectPrivate
{
public:
	AppWnd *App;
	IdeProject *Project;
	bool Dirty, UserFileDirty;
	GString FileName;
	IdeProject *ParentProject;
	IdeProjectSettings Settings;
	GAutoPtr<BuildThread> Thread;
	LHashTbl<ConstStrKey<char,false>, ProjectNode*> Nodes;
	int NextNodeId;

	// Threads
	GAutoPtr<class MakefileThread> CreateMakefile;

	// User info file
	GString UserFile;
	LHashTbl<IntKey<int>,int> UserNodeFlags;

	IdeProjectPrivate(AppWnd *a, IdeProject *project) :
		Project(project),
		Settings(project)
	{
		App = a;
		Dirty = false;
		UserFileDirty = false;
		ParentProject = 0;
		NextNodeId = 1;
	}

	void CollectAllFiles(GTreeNode *Base, GArray<ProjectNode*> &Files, bool SubProjects, int Platform);
};

class MakefileThread : public LThread, public LCancel
{
	IdeProjectPrivate *d;
	IdeProject *Proj;
	IdePlatform Platform;
	bool BuildAfterwards;
	
public:
	MakefileThread(IdeProjectPrivate *priv, IdePlatform platform, bool Build) : LThread("MakefileThread")
	{
		d = priv;
		Proj = d->Project;
		Platform = platform;
		BuildAfterwards = Build;
		
		Run();
	}

	~MakefileThread()
	{
		Cancel();
		while (!IsExited())
			LgiSleep(1);
	}

	int Main()
	{
		const char *PlatformName = PlatformNames[Platform];
		const char *PlatformLibraryExt = NULL;
		const char *PlatformStaticLibExt = NULL;
		const char *PlatformExeExt = "";
		GString LinkerFlags;
		const char *TargetType = d->Settings.GetStr(ProjTargetType, NULL, Platform);
		const char *CompilerName = d->Settings.GetStr(ProjCompiler);
		GString CCompilerBinary = "gcc";
		GString CppCompilerBinary = "g++";
		GStream *Log = d->App->GetBuildLog();
		bool IsExecutableTarget = TargetType && !stricmp(TargetType, "Executable");
		bool IsDynamicLibrary	= TargetType && !stricmp(TargetType, "DynamicLibrary");
		
		LgiAssert(Log);
		if (!Log)
			return false;
		
		Log->Print("CreateMakefile for '%s'...\n", PlatformName);
		
		if (Platform == PlatformWin32)
		{
			LinkerFlags = ",--enable-auto-import";
		}
		else
		{
			if (IsDynamicLibrary)
			{
				LinkerFlags = ",-soname,$(TargetFile)";
			}
			LinkerFlags += ",-export-dynamic,-R.";
		}

		char Buf[256];
		GAutoString MakeFile = Proj->GetMakefile();
		if (!MakeFile)
		{
			MakeFile.Reset(NewStr("../Makefile"));
		}
		
		// LGI_LIBRARY_EXT
		switch (Platform)
		{
			case PlatformWin32:
				PlatformLibraryExt = "dll";
				PlatformStaticLibExt = "lib";
				PlatformExeExt = ".exe";
				break;
			case PlatformLinux:
			case PlatformHaiku:
				PlatformLibraryExt = "so";
				PlatformStaticLibExt = "a";
				break;
			case PlatformMac:
				PlatformLibraryExt = "dylib";
				PlatformStaticLibExt = "a";
				break;
			default:
				LgiAssert(0);
				break;
		}

		if (CompilerName)
		{
			if (!stricmp(CompilerName, "cross"))
			{
				GString CBin = d->Settings.GetStr(ProjCCrossCompiler, NULL, Platform);
				if (CBin && !FileExists(CBin))
					FindInPath(CBin);
				if (CBin && FileExists(CBin))
					CCompilerBinary = CBin;
				else
					Log->Print("%s:%i - Error: C cross compiler '%s' not found.\n", _FL, CBin.Get());
				
				GString CppBin = d->Settings.GetStr(ProjCppCrossCompiler, NULL, Platform);
				if (CppBin && !FileExists(CppBin))
					FindInPath(CppBin);
				if (CppBin && FileExists(CppBin))
					CppCompilerBinary = CppBin;
				else
					Log->Print("%s:%i - Error: C++ cross compiler '%s' not found.\n", _FL, CppBin.Get());
			}
		}
		
		GFile m;
		if (!m.Open(MakeFile, O_WRITE))
		{
			Log->Print("Error: Failed to open '%s' for writing.\n", MakeFile.Get());
			return false;
		}
		
		m.SetSize(0);
		
		m.Print("#!/usr/bin/make\n"
				"#\n"
				"# This makefile generated by LgiIde\n"
				"# http://www.memecode.com/lgi.php\n"
				"#\n"
				"\n"
				".SILENT :\n"
				"\n"
				"CC = %s\n"
				"CPP = %s\n",
				CCompilerBinary.Get(),
				CppCompilerBinary.Get());

		// Collect all files that require building
		GArray<ProjectNode*> Files;
		d->CollectAllFiles
		(
			Proj,
			Files,
			false,
			1 << Platform
		);
		
		if (IsExecutableTarget)
		{
			GString Exe = Proj->GetExecutable(Platform);
			if (Exe)
			{
				if (LgiIsRelativePath(Exe))
					m.Print("Target = %s\n", Exe.Get());
				else
				{
					GAutoString Base = Proj->GetBasePath();
					GAutoString RelExe = LgiMakeRelativePath(Base, Exe);
					if (Base && RelExe)
					{
						m.Print("Target = %s\n", RelExe.Get());
					}
					else
					{
						Log->Print("%s:%i - Error: Missing path (%s, %s).\n", _FL, Base.Get(), RelExe.Get());
						return false;
					}
				}
			}
			else
			{
				Log->Print("%s:%i - Error: No executable name specified (%s, %s).\n", _FL, TargetType, d->FileName.Get());
				return false;
			}
		}
		else
		{
			GString Target = Proj->GetTargetName(Platform);
			if (Target)
				m.Print("Target = %s\n", Target.Get());
			else
			{
				Log->Print("%s:%i - Error: No target name specified.\n", _FL);
				return false;
			}
		}

		// Output the build mode, flags and some paths
		int BuildMode = d->App->GetBuildMode();
		char *BuildModeName = BuildMode ? (char*)"Release" : (char*)"Debug";
		m.Print("ifndef Build\n"
				"	Build = %s\n"
				"endif\n",
				BuildModeName);
		
		GString sDefines[2];
		GString sLibs[2];
		GString sIncludes[2];
		const char *ExtraLinkFlags = NULL;
		const char *ExeFlags = NULL;
		if (Platform == PlatformWin32)
		{
			ExtraLinkFlags = "";
			ExeFlags = " -mwindows";
			m.Print("BuildDir = $(Build)\n"
					"\n"
					"Flags = -fPIC -w -fno-inline -fpermissive\n");
			
			const char *DefDefs = "-DWIN32 -D_REENTRANT";
			sDefines[0] = DefDefs;
			sDefines[1] = DefDefs;
		}
		else
		{
			char PlatformCap[32];
			strcpy_s(	PlatformCap,
						sizeof(PlatformCap),
						Platform == PlatformHaiku ? "BEOS" : PlatformName);
			strupr(PlatformCap);
			
			ExtraLinkFlags = "";
			ExeFlags = "";
			m.Print("BuildDir = $(Build)\n"
					"\n"
					"Flags = -fPIC -w -fno-inline -fpermissive\n" // -fexceptions
					);
			sDefines[0].Printf("-D%s -D_REENTRANT", PlatformCap);
			#ifdef LINUX
			sDefines[0] += " -D_FILE_OFFSET_BITS=64"; // >:-(
			sDefines[0] += " -DPOSIX";
			#endif
			sDefines[1] = sDefines[0];
		}

		List<IdeProject> Deps;
		Proj->GetChildProjects(Deps);

		const char *sConfig[] = {"Debug", "Release"};
		for (int Cfg = 0; Cfg < CountOf(sConfig); Cfg++)
		{
			// Set the config
			d->Settings.SetCurrentConfig(sConfig[Cfg]);
		
			// Get the defines setup
			const char *PDefs = d->Settings.GetStr(ProjDefines, NULL, Platform);
			if (ValidStr(PDefs))
			{
				GToken Defs(PDefs, " ;,\r\n");
				for (int i=0; i<Defs.Length(); i++)
				{
					GString s;
					s.Printf(" -D%s", Defs[i]);
					sDefines[Cfg] += s;
				}
			}
		
			// Collect all dependencies, output their lib names and paths
			GString PLibPaths = d->Settings.GetStr(ProjLibraryPaths, NULL, Platform);
			if (ValidStr(PLibPaths))
			{
				GString::Array LibPaths = PLibPaths.Split("\n");
				for (unsigned i=0; i<LibPaths.Length(); i++)
				{
					GString s, in = LibPaths[i].Strip();
					if (!in.Length())
						continue;
					
					if (in(0) == '-')
						s.Printf(" \\\n\t\t%s", in.Get());
					else
						s.Printf(" \\\n\t\t-L%s", ToUnixPath(in.Get()));
					sLibs[Cfg] += s;
				}
			}

			const char *PLibs = d->Settings.GetStr(ProjLibraries, NULL, Platform);
			if (ValidStr(PLibs))
			{
				GToken Libs(PLibs, "\r\n");
				for (int i=0; i<Libs.Length(); i++)
				{
					char *l = Libs[i];
					GString s;
					if (*l == '`' || *l == '-')
						s.Printf(" \\\n\t\t%s", Libs[i]);
					else
						s.Printf(" \\\n\t\t-l%s", ToUnixPath(Libs[i]));
					sLibs[Cfg] += s;
				}
			}

			for (IdeProject *dep=Deps.First(); dep; dep=Deps.Next())
			{
				GString Target = dep->GetTargetName(Platform);
				if (Target)
				{
					char t[MAX_PATH];
					strcpy_s(t, sizeof(t), Target);
					if (!strnicmp(t, "lib", 3))
						memmove(t, t + 3, strlen(t + 3) + 1);
					char *dot = strrchr(t, '.');
					if (dot)
						*dot = 0;
													
					GString s;
					s.Printf(" \\\n\t\t-l%s$(Tag)", ToUnixPath(t));
					sLibs[Cfg] += s;

					GAutoString Base = dep->GetBasePath();
					if (Base)
					{
						s.Printf(" \\\n\t\t-L%s/$(BuildDir)", ToUnixPath(Base));
						sLibs[Cfg] += s;
					}
				}
			}
			
			// Includes

			// Do include paths
			LHashTbl<StrKey<char>,bool> Inc;
			const char *ProjIncludes = d->Settings.GetStr(ProjIncludePaths, NULL, Platform);
			if (ValidStr(ProjIncludes))
			{
				// Add settings include paths.
				GToken Paths(ProjIncludes, "\r\n");
				for (int i=0; i<Paths.Length(); i++)
				{
					char *p = Paths[i];
					GAutoString pn = ToNativePath(p);
					if (!Inc.Find(pn))
					{
						Inc.Add(pn, true);
					}
				}
			}
			const char *SysIncludes = d->Settings.GetStr(ProjSystemIncludes, NULL, Platform);
			if (ValidStr(SysIncludes))
			{
				// Add settings include paths.
				GToken Paths(SysIncludes, "\r\n");
				for (int i=0; i<Paths.Length(); i++)
				{
					char *p = Paths[i];
					GAutoString pn = ToNativePath(p);
					if (!Inc.Find(pn))
					{
						Inc.Add(pn, true);
					}
				}
			}

			
			// Add paths of headers
			for (int i=0; i<Files.Length(); i++)
			{
				ProjectNode *n = Files[i];
				
				if (n->GetFileName())
				{
					char *e = LgiGetExtension(n->GetFileName());
					if (e && stricmp(e, "h") == 0)
					{
						for (char *Dir=n->GetFileName(); *Dir; Dir++)
						{
							if (*Dir == '/' || *Dir == '\\')
							{
								*Dir = DIR_CHAR;
							}
						}						

						char Path[256];
						strcpy_s(Path, sizeof(Path), n->GetFileName());

						LgiTrimDir(Path);
					
						char Rel[256];
						if (!Proj->RelativePath(Rel, Path))
						{
							strcpy(Rel, Path);
						}
						
						if (stricmp(Rel, ".") != 0)
						{
							GAutoString RelN = ToNativePath(Rel);
							if (!Inc.Find(RelN))
							{
								Inc.Add(RelN, true);
							}
						}
					}
				}
			}

			List<char> Incs;
			// char *i;
			// for (bool b=Inc.First(&i); b; b=Inc.Next(&i))
			for (auto i : Inc)
			{
				Incs.Insert(NewStr(i.key));
			}
			Incs.Sort(StrCmp);
			
			for (auto i = Incs.First(); i; i = Incs.Next())
			{
				GString s;
				if (*i == '`')
					s.Printf(" \\\n\t\t%s", i);
				else
					s.Printf(" \\\n\t\t-I%s", ToUnixPath(i));
				sIncludes[Cfg] += s;
			}
		}

		// Output the defs section for Debug and Release		

		// Debug specific
		m.Print("\n"
				"ifeq ($(Build),Debug)\n"
				"	Flags += -g -std=c++11\n"
				"	Tag = d\n"
				"	Defs = -D_DEBUG %s\n"
				"	Libs = %s\n"
				"	Inc = %s\n",
					CastEmpty(sDefines[0].Get()),
					CastEmpty(sLibs[0].Get()),
					CastEmpty(sIncludes[0].Get()));
		
		// Release specific
		m.Print("else\n"
				"	Flags += -s -Os -std=c++11\n"
				"	Defs = %s\n"
				"	Libs = %s\n"
				"	Inc = %s\n"
				"endif\n"
				"\n",
					CastEmpty(sDefines[1].Get()),
					CastEmpty(sLibs[1].Get()),
					CastEmpty(sIncludes[1].Get()));
		
		if (Files.First())
		{
			GArray<GString> IncPaths;
			if (Proj->BuildIncludePaths(IncPaths, false, false, Platform))
			{
				// Do dependencies
				m.Print("# Dependencies\n"
						"Depends =\t");
					
				for (int c = 0; c < Files.Length() && !IsCancelled(); c++)
				{
					ProjectNode *n = Files[c];
					if (n->GetType() == NodeSrc)
					{
						GAutoString f = ToNativePath(n->GetFileName());
						char *d = f ? strrchr(f, DIR_CHAR) : 0;
						char *file = d ? d + 1 : f;
						d = file ? strrchr(file, '.') : 0;
						if (d)
						{
							if (c) m.Print(" \\\n\t\t\t");
							m.Print("%.*s.o", d - file, file);
						}
					}
				}
				m.Print("\n\n");

				// Write out the target stuff
				m.Print("# Target\n");

				LHashTbl<StrKey<char,false>,bool> DepFiles;

				if (TargetType)
				{
					if (IsExecutableTarget)
					{
						m.Print("# Executable target\n"
								"$(Target) :");
								
						GStringPipe Rules;
						IdeProject *Dep;
						
						uint64 Last = LgiCurrentTime();
						int Count = 0;
						for (Dep=Deps.First(); Dep && !IsCancelled(); Dep=Deps.Next(), Count++)
						{
							// Get dependency to create it's own makefile...
							Dep->CreateMakefile(Platform, false);
						
							// Build a rule to make the dependency if any of the source changes...
							char t[MAX_PATH] = "";
							GAutoString DepBase = Dep->GetBasePath();
							GAutoString Base = Proj->GetBasePath();
							
							if (DepBase && Base && Dep->GetTargetFile(t, sizeof(t)))
							{
								char Rel[MAX_PATH] = "";
								if (!Proj->RelativePath(Rel, DepBase))
								{
									strcpy_s(Rel, sizeof(Rel), DepBase);
								}
								ToUnixPath(Rel);
								
								// Add tag to target name
								GToken Parts(t, ".");
								if (Parts.Length() == 2)
									sprintf_s(t, sizeof(t), "lib%s$(Tag).%s", Parts[0], Parts[1]);
								else
									sprintf_s(t, sizeof(t), "%s", Parts[0]);

								sprintf(Buf, "%s/$(BuildDir)/%s", Rel, t);
								m.Print(" %s", Buf);
								
								GArray<char*> AllDeps;
								Dep->GetAllDependencies(AllDeps, Platform);
								LgiAssert(AllDeps.Length() > 0);
								AllDeps.Sort(StrSort);

								Rules.Print("%s : ", Buf);
								for (int i=0; i<AllDeps.Length(); i++)
								{
									if (i)
										Rules.Print(" \\\n\t");
									
									char Rel[MAX_PATH];
									char *f = Proj->RelativePath(Rel, AllDeps[i]) ? Rel : AllDeps[i];
									ToUnixPath(f);
									Rules.Print("%s", f);
									
									// Add these dependencies to this makefiles dep list
									if (!DepFiles.Find(f))
										DepFiles.Add(f, true);
								}
								
								AllDeps.DeleteArrays();
								
								Rules.Print("\n\texport Build=$(Build); \\\n"
											"\t$(MAKE) -C %s",
											Rel);

								GAutoString Mk = Dep->GetMakefile();
								// RenameMakefileForPlatform(Mk, Platform);

								char *DepMakefile = strrchr(Mk, DIR_CHAR);
								if (DepMakefile)
								{
									Rules.Print(" -f %s", DepMakefile + 1);
								}

								Rules.Print("\n\n");
							}
							
							uint64 Now = LgiCurrentTime();
							if (Now - Last > 1000)
							{
								Last = Now;
								Log->Print("Building deps %i%%...\n", (int) (((int64)Count+1)*100/Deps.Length()));
							}
						}

						m.Print(" outputfolder $(Depends)\n"
								"	@echo Linking $(Target) [$(Build)]...\n"
								"	$(CPP)%s%s %s%s -o \\\n"
								"		$(Target) $(addprefix $(BuildDir)/,$(Depends)) $(Libs)\n",
								ExtraLinkFlags,
								ExeFlags,
								ValidStr(LinkerFlags) ? "-Wl" : "", LinkerFlags.Get());

						if (Platform == PlatformHaiku)
						{
							// Is there an application icon configured?
							const char *AppIcon = d->Settings.GetStr(ProjApplicationIcon, NULL, Platform);
							if (AppIcon)
							{
								m.Print("	addattr -f %s -t \"'VICN'\" \"BEOS:ICON\" $(Target)\n", AppIcon);
							}							
						}
						
						GString PostBuildCmds = d->Settings.GetStr(ProjPostBuildCommands, NULL, Platform);
						if (ValidStr(PostBuildCmds))
						{
							GString::Array a = PostBuildCmds.Split("\n");
							for (unsigned i=0; i<a.Length(); i++)
								m.Print("\t%s\n", a[i].Strip().Get());
						}

						m.Print("	@echo Done.\n"
								"\n");

						GAutoString r(Rules.NewStr());
						if (r)
						{
							m.Write(r, strlen(r));
						}

						// Various fairly global rules
						m.Print("# Create the output folder\n"
								"outputfolder :\n"
								"	-mkdir -p $(BuildDir) 2> /dev/null\n"
								"\n");
							
						m.Print("# Clean just this target\n"
								"clean :\n"
								"	rm -f $(BuildDir)/*.o $(Target)%s\n"
								"	@echo Cleaned $(BuildDir)\n"
								"\n",
								LGI_EXECUTABLE_EXT);
						
						m.Print("# Clean all targets\n"
								"cleanall :\n"
								"	rm -f $(BuildDir)/*.o $(Target)%s\n"
								"	@echo Cleaned $(BuildDir)\n",
								LGI_EXECUTABLE_EXT);
						
						for (IdeProject *d=Deps.First(); d; d=Deps.Next())
						{
							GAutoString mk = d->GetMakefile();
							if (mk)
							{
								GAutoString my_base = Proj->GetBasePath();
								GAutoString dep_base = d->GetBasePath();
								GAutoString rel_dir = LgiMakeRelativePath(my_base, dep_base);
								char *mk_leaf = strrchr(mk, DIR_CHAR);
								m.Print("	+make -C \"%s\" -f \"%s\" clean\n",
									ToUnixPath(rel_dir ? rel_dir.Get() : dep_base.Get()),
									ToUnixPath(mk_leaf ? mk_leaf + 1 : mk.Get()));
							}
						}
						m.Print("\n");
					}
					// Shared library
					else if (!stricmp(TargetType, "DynamicLibrary"))
					{
						m.Print("TargetFile = lib$(Target)$(Tag).%s\n"
								"$(TargetFile) : outputfolder $(Depends)\n"
								"	@echo Linking $(TargetFile) [$(Build)]...\n"
								"	$(CPP)$s -shared \\\n"
								"		%s%s \\\n"
								"		-o $(BuildDir)/$(TargetFile) \\\n"
								"		$(addprefix $(BuildDir)/,$(Depends)) \\\n"
								"		$(Libs)\n",
								PlatformLibraryExt,
								ValidStr(ExtraLinkFlags) ? "-Wl" : "", ExtraLinkFlags,
								LinkerFlags.Get());

						GString PostBuildCmds = d->Settings.GetStr(ProjPostBuildCommands, NULL, Platform);
						if (ValidStr(PostBuildCmds))
						{
							GString::Array a = PostBuildCmds.Split("\n");
							for (unsigned i=0; i<a.Length(); i++)
								m.Print("\t%s\n", a[i].Strip().Get());
						}

						m.Print("	@echo Done.\n"
								"\n");

						// Cleaning target
						m.Print("# Create the output folder\n"
								"outputfolder :\n"
								"	-mkdir -p $(BuildDir) 2> /dev/null\n"
								"\n"
								"# Clean out targets\n"
								"clean :\n"
								"	rm -f $(BuildDir)/*.o $(BuildDir)/$(TargetFile)\n"
								"	@echo Cleaned $(BuildDir)\n"
								"\n",
								PlatformLibraryExt);
					}
					// Static library
					else if (!stricmp(TargetType, "StaticLibrary"))
					{
						m.Print("TargetFile = lib$(Target)$(Tag).%s\n"
								"$(TargetFile) : outputfolder $(Depends)\n"
								"	@echo Linking $(TargetFile) [$(Build)]...\n"
								"	ar rcs $(BuildDir)/$(TargetFile) $(addprefix $(BuildDir)/,$(Depends))\n",
								PlatformStaticLibExt);

						GString PostBuildCmds = d->Settings.GetStr(ProjPostBuildCommands, NULL, Platform);
						if (ValidStr(PostBuildCmds))
						{
							GString::Array a = PostBuildCmds.Split("\n");
							for (unsigned i=0; i<a.Length(); i++)
								m.Print("\t%s\n", a[i].Strip().Get());
						}

						m.Print("	@echo Done.\n"
								"\n");

						// Cleaning target
						m.Print("# Create the output folder\n"
								"outputfolder :\n"
								"	-mkdir -p $(BuildDir) 2> /dev/null\n"
								"\n"
								"# Clean out targets\n"
								"clean :\n"
								"	rm -f $(BuildDir)/*.o $(BuildDir)/$(TargetFile)\n"
								"	@echo Cleaned $(BuildDir)\n"
								"\n",
								PlatformStaticLibExt);
					}
				}

				// Create dependency tree, starting with all the source files.
				for (int idx=0; idx<Files.Length() && !IsCancelled(); idx++)
				{
					ProjectNode *n = Files[idx];
					if (n->GetType() == NodeSrc)
					{
						GString Src = n->GetFullPath();
						if (Src)
						{
							char Part[256];
							
							char *d = strrchr(Src, DIR_CHAR);
							d = d ? d + 1 : Src.Get();
							strcpy(Part, d);
							char *Dot = strrchr(Part, '.');
							if (Dot) *Dot = 0;

							char Rel[MAX_PATH];
							
							if (Platform != PlatformHaiku)
							{
								if (!Proj->RelativePath(Rel, Src))
						 			strcpy_s(Rel, sizeof(Rel), Src);
							}
							else
							{
								// Use full path for Haiku because the Debugger needs it to
								// find the source correctly. As there are duplicate filenames
								// for different platforms it's better to rely on full paths
								// rather than filename index to find the right file.
					 			strcpy_s(Rel, sizeof(Rel), Src);
							}
							
							m.Print("%s.o : %s ", Part, ToUnixPath(Rel));

							GArray<char*> SrcDeps;
							if (Proj->GetDependencies(Src, IncPaths, SrcDeps, Platform))
							{
								for (int i=0; i<SrcDeps.Length() && !IsCancelled(); i++)
								{
									char *SDep = SrcDeps[i];
									
									if (stricmp(Src.Get(), SDep) != 0)
									{
										if (i) m.Print(" \\\n\t");
										m.Print("%s", SDep);
										if (!DepFiles.Find(SDep))
										{
											DepFiles.Add(SDep, true);
										}
									}
									else printf("%s:%i - not add dep: '%s' '%s'\n", _FL, Src.Get(), SDep);
								}
								SrcDeps.DeleteArrays();
							}

							char *Ext = LgiGetExtension(Src);
							const char *Compiler = Src && !stricmp(Ext, "c") ? "CC" : "CPP";

							m.Print("\n"
									"	@echo $(<F) [$(Build)]\n"
									"	$(%s) $(Inc) $(Flags) $(Defs) -c $< -o $(BuildDir)/$(@F)\n"
									"\n",
									Compiler);
						}
					}
				}
				
				// Do remaining include file dependencies
				bool Done = false;
				LHashTbl<StrKey<char,false>,bool> Processed;
				GAutoString Base = Proj->GetBasePath();
				while (!Done)
				{
					Done = true;
					// char *Src;
					// for (bool b=DepFiles.First(&Src); b; b=DepFiles.Next(&Src))
					for (auto it : DepFiles)
					{
						if (IsCancelled())
							break;
						if (Processed.Find(it.key))
							continue;

						Done = false;
						Processed.Add(it.key, true);
						
						char Full[MAX_PATH], Rel[MAX_PATH];
						if (LgiIsRelativePath(it.key))
						{
							LgiMakePath(Full, sizeof(Full), Base, it.key);
							strcpy_s(Rel, sizeof(Rel), it.key);
						}
						else
						{
							strcpy_s(Full, sizeof(Full), it.key);
							GAutoString a = LgiMakeRelativePath(Base, it.key);
							if (a)
							{
								strcpy_s(Rel, sizeof(Rel), a);
							}
							else
							{
								strcpy_s(Rel, sizeof(Rel), a);
								LgiTrace("%s:%i - Failed to make relative path '%s' '%s'\n",
									_FL,
									Base.Get(), it.key);
							}
						}
						
						char *c8 = ReadTextFile(Full);
						if (c8)
						{
							GArray<char*> Headers;
							if (BuildHeaderList(c8, Headers, IncPaths, false))
							{
								m.Print("%s : ", Rel);

								for (int n=0; n<Headers.Length() && !IsCancelled(); n++)
								{
									char *i = Headers[n];
									
									if (n) m.Print(" \\\n\t");
									
									char Rel[MAX_PATH];
									if (!Proj->RelativePath(Rel, i))
									{
				 						strcpy(Rel, i);
									}

									if (stricmp(i, Full) != 0)
										m.Print("%s", ToUnixPath(Rel));
									
									if (!DepFiles.Find(i))
									{
										DepFiles.Add(i, true);
									}
								}
								Headers.DeleteArrays();

								m.Print("\n\n");
							}
							else LgiTrace("%s:%i - Error: BuildHeaderList failed for '%s'\n", _FL, Full);
							
							DeleteArray(c8);
						}
						else LgiTrace("%s:%i - Error: Failed to read '%s'\n", _FL, Full);
						
						break;
					}
				}

				// Output VPATH
				m.Print("VPATH=%%.cpp \\\n");
				for (int i=0; i<IncPaths.Length() && !IsCancelled(); i++)
				{
					char *p = IncPaths[i];
					if (p && !strchr(p, '`'))
					{
						if (!LgiIsRelativePath(p))
						{
							GAutoString a = LgiMakeRelativePath(Base, p);
							m.Print("\t%s \\\n", a?a:p);
						}
						else
						{
							m.Print("\t%s \\\n", p);
						}
					}
				}
				m.Print("\t$(BuildDir)\n\n");

				const char *OtherMakefileRules = d->Settings.GetStr(ProjMakefileRules, NULL, Platform);
				if (ValidStr(OtherMakefileRules))
				{
					m.Print("\n%s\n", OtherMakefileRules);
				}
			}
		}
		else
		{
			m.Print("# No files require building.\n");
		}

		Log->Print("...Done: '%s'\n", MakeFile.Get());

		if (BuildAfterwards)
		{
			if (!Proj->GetApp()->PostEvent(M_START_BUILD))
				printf("%s:%i - PostEvent(M_START_BUILD) failed.\n", _FL);
		}

		return true;
	}
};


/////////////////////////////////////////////////////////////////////////////////////
NodeSource::~NodeSource()
{
	if (nView)
	{
		nView->nSrc = 0;
	}
}

NodeView::~NodeView()
{
	if (nSrc)
	{
		nSrc->nView = 0;
	}
}


//////////////////////////////////////////////////////////////////////////////////
int NodeSort(GTreeItem *a, GTreeItem *b, NativeInt d)
{
	ProjectNode *A = dynamic_cast<ProjectNode*>(a);
	ProjectNode *B = dynamic_cast<ProjectNode*>(b);
	if (A && B)
	{
		if
		(
			(A->GetType() == NodeDir)
			^
			(B->GetType() == NodeDir)
		)
		{
			return A->GetType() == NodeDir ? -1 : 1;
		}
		else
		{
			char *Sa = a->GetText(0);
			char *Sb = b->GetText(0);
			if (Sa && Sb)
			{
				return stricmp(Sa, Sb);
			}
		}
	}

	return 0;
}

int XmlSort(GXmlTag *a, GXmlTag *b, NativeInt d)
{
	GTreeItem *A = dynamic_cast<GTreeItem*>(a);
	GTreeItem *B = dynamic_cast<GTreeItem*>(b);
	return NodeSort(A, B, d);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool ReadVsProjFile(GString File, GString &Ver, GString::Array &Configs)
{
	const char *Ext = LgiGetExtension(File);
	if (!Ext || _stricmp(Ext, "vcxproj"))
		return false;

	GFile f;
	if (!f.Open(File, O_READ))
		return false;

	GXmlTree Io;
	GXmlTag r;
	if (Io.Read(&r, &f) &&
		r.IsTag("Project"))
	{
		Ver = r.GetAttr("ToolsVersion");

		GXmlTag *ItemGroup = r.GetChildTag("ItemGroup");
		if (ItemGroup)
			for (GXmlTag *c = ItemGroup->Children.First(); c; c = ItemGroup->Children.Next())
			{
				if (c->IsTag("ProjectConfiguration"))
				{
					char *s = c->GetAttr("Include");
					if (s)
						Configs.New() = s;
				}
			}
	}
	else return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BuildThread::BuildThread(IdeProject *proj, char *makefile, bool clean, bool release, bool all, int wordsize) : LThread("BuildThread")
{
	Proj = proj;
	Makefile = makefile;
	Clean = clean;
	Release = release;
	All = all;
	WordSize = wordsize;
	Arch = DefaultArch;
	Compiler = DefaultCompiler;

	GString Cmds = proj->d->Settings.GetStr(ProjPostBuildCommands, NULL);
	if (ValidStr(Cmds))
		PostBuild = Cmds.SplitDelimit("\r\n");

	char *Ext = LgiGetExtension(Makefile);
	if (Ext && !_stricmp(Ext, "py"))
	{
		Compiler = PythonScript;
	}
	else
	{
		GAutoString Comp(NewStr(Proj->GetSettings()->GetStr(ProjCompiler)));
		if (Comp)
		{
			// Use the specified compiler...
			if (!stricmp(Comp, "VisualStudio"))
				Compiler = VisualStudio;
			else if (!stricmp(Comp, "MingW"))
				Compiler = MingW;
			else if (!stricmp(Comp, "gcc"))
				Compiler = Gcc;
			else if (!stricmp(Comp, "cross"))
				Compiler = CrossCompiler;
			else if (!stricmp(Comp, "IAR"))
				Compiler = IAR;
			else
				LgiAssert(!"Unknown compiler.");
		}
	}

	if (Compiler == DefaultCompiler)
	{
		// Use default compiler for platform...
		#ifdef WIN32
		Compiler = VisualStudio;
		#else
		Compiler = Gcc;
		#endif
	}

	Run();
}

BuildThread::~BuildThread()
{
	if (SubProc)
		SubProc->Interrupt();

	uint64 Start = LgiCurrentTime();
	bool Killed = false;
	while (!IsExited())
	{
		LgiSleep(10);

		if (LgiCurrentTime() - Start > STOP_BUILD_TIMEOUT &&
			SubProc)
		{
			if (Killed)
			{
				// Thread is stuck as well... ok kill that too!!! Argh - kill all the things!!!!
				Terminate();
				break;
			}
			else
			{
				// Kill the sub-process...
				SubProc->Kill();
				Killed = true;
				Start = LgiCurrentTime();
			}
		}
	}
}

ssize_t BuildThread::Write(const void *Buffer, ssize_t Size, int Flags)
{
	if (Proj->GetApp())
	{
		Proj->GetApp()->PostEvent(M_APPEND_TEXT, (GMessage::Param)NewStr((char*)Buffer, Size), 0);
	}
	return Size;
}

#pragma comment(lib, "version.lib")

GString BuildThread::FindExe()
{
	GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);

	if (Compiler == PythonScript)
	{
		#if defined(WINDOWS)
		uint32 BestVer = 0;
		#endif
		GString Best;

		for (int i=0; i<p.Length(); i++)
		{
			char Path[MAX_PATH];
			LgiMakePath(Path, sizeof(Path), p[i], "python" LGI_EXECUTABLE_EXT);
			if (FileExists(Path))
			{
				// Check version
				#if defined(WINDOWS)
				DWORD Sz = GetFileVersionInfoSizeA(Path, NULL);
				void *Buf = malloc(Sz);
				if (GetFileVersionInfoA(Path, NULL, Sz, Buf))
				{
					LPVOID Ptr = NULL;
					UINT Bytes;
					if (VerQueryValueA(Buf, "\\", &Ptr, &Bytes))
					{
						VS_FIXEDFILEINFO *v = (VS_FIXEDFILEINFO *)Ptr;
						if (v->dwProductVersionMS > BestVer)
						{
							BestVer = v->dwProductVersionMS;
							Best = Path;
						}
					}
				}
				else if (!Best)
				{
					Best = Path;
				}
				free(Buf);
				#else
				Best = Path;
				break;
				#endif
			}

		}

		return Best;
	}
	else if (Compiler == VisualStudio)
	{
		// Find the version we need:
		double fVer = 0.0;

		GString ProjFile;
		const char *Ext = LgiGetExtension(Makefile);
		if (Ext && !_stricmp(Ext, "sln"))
		{
			GFile f;
			if (f.Open(Makefile, O_READ))
			{
				GString VerKey = "Format Version ";
				GString ProjKey = "Project(";
				ssize_t Pos;

				GString::Array Ln = f.Read().SplitDelimit("\r\n");
				for (size_t i = 0; i < Ln.Length(); i++)
				{
					GString s = Ln[i];
					if ((Pos = s.Find(VerKey)) > 0)
					{
						GString sVer = s(Pos + VerKey.Length(), -1);
						fVer = sVer.Float();
					}
					else if ((Pos = s.Find(ProjKey)) >= 0)
					{
						GString::Array p = s.SplitDelimit("(),=");
						if (p.Length() > 4)
						{
							ProjFile = p[4].Strip(" \t\'\"");
							if (LgiIsRelativePath(ProjFile))
							{
								char f[MAX_PATH];
								LgiMakePath(f, sizeof(f), Makefile, "..");
								LgiMakePath(f, sizeof(f), f, ProjFile);
								if (FileExists(f))
									ProjFile = f;
								else
									LgiAssert(0);
							}
							break;
						}
					}
				}
			}
		}
		else if (Ext && !_stricmp(Ext, "vcxproj"))
		{
			ProjFile = Makefile;
		}
		else
		{
			if (Arch == DefaultArch)
			{
				if (sizeof(size_t) == 4)
					Arch = ArchX32;
				else
					Arch = ArchX64;
			}

			#ifdef _MSC_VER
			// Nmake file..
			GString NmakePath;
			switch (_MSC_VER)
			{
				case _MSC_VER_VS2013:
				{
					if (Arch == ArchX32)
						NmakePath = "c:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\bin\\nmake.exe";
					else
						NmakePath = "c:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\bin\\amd64\\nmake.exe";
					break;
				}
				case _MSC_VER_VS2015:
				{
					if (Arch == ArchX32)
						NmakePath = "c:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\bin\\nmake.exe";
					else
						NmakePath = "c:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\bin\\amd64\\nmake.exe";
					break;
				}
				default:
				{
					LgiAssert(!"Impl me.");
					break;
				}
			}

			if (FileExists(NmakePath))
			{
				Compiler = Nmake;
				return NmakePath;
			}
			#endif
		}

		if (ProjFile && FileExists(ProjFile))
		{
			GString sVer;
			if (ReadVsProjFile(ProjFile, sVer, BuildConfigs))
			{
				fVer = sVer.Float();
			}
		}

		if (fVer > 0.0)
		{
			GString p;
			p.Printf("C:\\Program Files (x86)\\Microsoft Visual Studio %.1f\\Common7\\IDE\\devenv.com", fVer);
			if (FileExists(p))
			{
				return p;
			}
		}
	}
	else if (Compiler == IAR)
	{
		// const char *Def = "c:\\Program Files (x86)\\IAR Systems\\Embedded Workbench 7.0\\common\\bin\\IarBuild.exe";
		
		GString ProgFiles = LGetSystemPath(LSP_USER_APPS, 32);
		char p[MAX_PATH];
		if (!LgiMakePath(p, sizeof(p), ProgFiles, "IAR Systems"))
			return GString();
		GDirectory d;
		double LatestVer = 0.0;
		GString Latest;
		for (int b = d.First(p); b; b = d.Next())
		{
			if (d.IsDir())
			{
				GString n(d.GetName());
				GString::Array p = n.Split(" ");
				if (p.Length() == 3 &&
					p[2].Float() > LatestVer)
				{
					LatestVer = p[2].Float();
					Latest = n;
				}
			}
		}
		if (Latest &&
			LgiMakePath(p, sizeof(p), p, Latest) &&
			LgiMakePath(p, sizeof(p), p, "common\\bin\\IarBuild.exe"))
		{
			if (FileExists(p))
				return p;
		}
	}
	else
	{
		if (Compiler == MingW)
		{
			// Have a look in the default spot first...
			const char *Def = "C:\\MinGW\\msys\\1.0\\bin\\make.exe";
			if (FileExists(Def))
			{
				return Def;
			}				
		}
		
		for (int i=0; i<p.Length(); i++)
		{
			char Exe[MAX_PATH];
			LgiMakePath
			(
				Exe,
				sizeof(Exe),
				p[i],
				"make"
				#ifdef WIN32
				".exe"
				#endif
			);
			if (FileExists(Exe))
			{
				return Exe;
			}
		}
	}
	
	return GString();
}

GAutoString BuildThread::WinToMingWPath(const char *path)
{
	GToken t(path, "\\");
	GStringPipe a(256);
	for (int i=0; i<t.Length(); i++)
	{
		char *p = t[i];
		char *colon = strchr(p, ':');
		if (colon)
		{
			*colon = 0;
			StrLwr(p);
		}
		a.Print("/%s", p);				
	}
	return GAutoString(a.NewStr());
}

int BuildThread::Main()
{
	const char *Err = 0;
	char ErrBuf[256];

	GString Exe = FindExe();
	if (Exe)
	{
		bool Status = false;
		GString MakePath = Makefile.Get();
		GString InitDir = Makefile.Get();
		GVariant Jobs;
		if (!Proj->GetApp()->GetOptions()->GetValue(OPT_Jobs, Jobs) || Jobs.CastInt32() < 1)
			Jobs = 2;

		auto Pos = InitDir.RFind(DIR_STR);
		if (Pos)
			InitDir.Length(Pos);

		GString TmpArgs, Include, Lib, LibPath, Path;
		
		if (Compiler == VisualStudio)
		{
			// TmpArgs.Printf("\"%s\" /make \"All - Win32 Debug\"", Makefile.Get());
			GString BuildConf = "All - Win32 Debug";
			if (BuildConfigs.Length())
			{
				const char *Key = Release ? "Release" : "Debug";
				for (size_t i=0; i<BuildConfigs.Length(); i++)
				{
					GString c = BuildConfigs[i];
					if (c.Find(Key) >= 0)
					{
						BuildConf = c;
						break;
					}
				}
			}
			TmpArgs.Printf("\"%s\" %s \"%s\"", Makefile.Get(), Clean ? "/Clean" : "/Build", BuildConf.Get());
		}
		else if (Compiler == Nmake)
		{
			const char *DefInc[] = {
				"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\INCLUDE",
				"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\ATLMFC\\INCLUDE",
				"C:\\Program Files (x86)\\Windows Kits\\8.1\\include\\shared",
				"C:\\Program Files (x86)\\Windows Kits\\8.1\\include\\um",
				"C:\\Program Files (x86)\\Windows Kits\\8.1\\include\\winrt"
			};
			GString f;

			#define ADD_PATHS(out, in) \
				for (unsigned i=0; i<CountOf(in); i++) \
				{ \
					f.Printf("%s%s", i ? LGI_PATH_SEPARATOR : "", in[i]); \
					out += f; \
				}
			if (Arch == ArchX32)
			{
				const char *DefLib[] = {
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\LIB",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\ATLMFC\\LIB",
					"C:\\Program Files (x86)\\Windows Kits\\8.1\\lib\\winv6.3\\um\\x32"
				};
				const char *DefLibPath[] = {
					"C:\\WINDOWS\\Microsoft.NET\\Framework64\\v4.0.30319",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\LIB",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\ATLMFC\\LIB",
					"C:\\Program Files (x86)\\Windows Kits\\8.1\\References\\CommonConfiguration\\Neutral",
					"C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v8.1\\ExtensionSDKs\\Microsoft.VCLibs\\12.0\\References\\CommonConfiguration\\neutral"
				};
				const char *DefPath[] = {
					"c:\\Program Files (x86)\\Windows Kits\\8.1\\bin\\x86"
				};
			
				ADD_PATHS(Include, DefInc);
				ADD_PATHS(Lib, DefLib);
				ADD_PATHS(LibPath, DefLibPath);
				ADD_PATHS(Path, DefPath);
			}
			else if (Arch == ArchX64)
			{
				const char *DefLib[] = {
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\LIB\\amd64",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\ATLMFC\\LIB\\amd64",
					"C:\\Program Files (x86)\\Windows Kits\\8.1\\lib\\winv6.3\\um\\x64"
				};
				const char *DefLibPath[] = {
					"C:\\WINDOWS\\Microsoft.NET\\Framework64\\v4.0.30319",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\LIB\\amd64",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\ATLMFC\\LIB\\amd64",
					"C:\\Program Files (x86)\\Windows Kits\\8.1\\References\\CommonConfiguration\\Neutral",
					"C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v8.1\\ExtensionSDKs\\Microsoft.VCLibs\\12.0\\References\\CommonConfiguration\\neutral"
				};
				const char *DefPath[] = {
					"c:\\Program Files (x86)\\Windows Kits\\8.1\\bin\\x64"
				};
							
				ADD_PATHS(Include, DefInc);
				ADD_PATHS(Lib, DefLib);
				ADD_PATHS(LibPath, DefLibPath);
				ADD_PATHS(Path, DefPath);
			}
			else LgiAssert(!"Invalid Arch");

			TmpArgs.Printf(" -f \"%s\"", Makefile.Get());

			if (Clean)
				TmpArgs += " clean";
		}
		else if (Compiler == PythonScript)
		{
			TmpArgs = MakePath;
		}
		else if (Compiler == IAR)
		{
			GString Conf;

			GFile f;
			if (f.Open(Makefile, O_READ))
			{
				GXmlTree t;
				GXmlTag r;
				if (t.Read(&r, &f))
				{
					GXmlTag *c = r.GetChildTag("configuration");
					c = c ? c->GetChildTag("name") : NULL;
					if (c)
						Conf = c->GetContent();
				}				
			}

			TmpArgs.Printf("\"%s\" %s %s -log warnings", Makefile.Get(), Clean ? "-clean" : "-make", Conf.Get());
		}
		else
		{
			if (Compiler == MingW)
			{
				GString a;
				char *Dir = strrchr(MakePath, DIR_CHAR);
				#if 1
				TmpArgs.Printf("/C \"%s\"", Exe.Get());

				
				/*	As of MSYS v1.0.18 the support for multiple jobs causes make to hang:
					http://sourceforge.net/p/mingw/bugs/1950/
					http://mingw-users.1079350.n2.nabble.com/MSYS-make-freezes-td7579038.html
					
					Apparently it'll be "fixed" in v1.0.19. We'll see. >:-(
				
				if (Jobs.CastInt32() > 1)
					a.Print(" -j %i", Jobs.CastInt32());
				
				*/
				
				a.Printf(" -f \"%s\"", Dir ? Dir + 1 : MakePath.Get());
				TmpArgs += a;
				#else
				TmpArgs.Printf("/C set");
				#endif
				Exe = "C:\\Windows\\System32\\cmd.exe";
			}
			else
			{
				if (Jobs.CastInt32())
					TmpArgs.Printf("-j %i -f \"%s\"", Jobs.CastInt32(), MakePath.Get());
				else
					TmpArgs.Printf("-f \"%s\"", MakePath.Get());
			}

			if (Clean)
            {
				if (All)
                {
					TmpArgs += " cleanall";
                }
				else
                {
					TmpArgs += " clean";
                }
            }
			if (Release)
				TmpArgs += " Build=Release";
		}

		GString Msg;
		Msg.Printf("Making: %s\n", MakePath.Get());
		Proj->GetApp()->PostEvent(M_APPEND_TEXT, (GMessage::Param)NewStr(Msg), 0);

		if (SubProc.Reset(new GSubProcess(Exe, TmpArgs)))
		{
			SubProc->SetInitFolder(InitDir);
			if (Include)
				SubProc->SetEnvironment("INCLUDE", Include);
			if (Lib)
				SubProc->SetEnvironment("LIB", Lib);
			if (LibPath)
				SubProc->SetEnvironment("LIBPATHS", LibPath);
			if (Path)
			{
				GString Cur = getenv("PATH");
				GString New = Cur + LGI_PATH_SEPARATOR + Path;
				SubProc->SetEnvironment("PATH", New);
			}
			// SubProc->SetEnvironment("DLL", "1");

			if (Compiler == MingW)
				SubProc->SetEnvironment("PATH", "c:\\MingW\\bin;C:\\MinGW\\msys\\1.0\\bin;%PATH%");
				
			if ((Status = SubProc->Start(true, false)))
			{
				// Read all the output					
				char Buf[256];
				ssize_t rd;
				while ( (rd = SubProc->Read(Buf, sizeof(Buf))) > 0 )
				{
					Write(Buf, rd);
				}
					
				uint32 ex = SubProc->Wait();
				Print("Make exited with %i (0x%x)\n", ex, ex);

				if (Compiler == IAR &&
					ex == 0 &&
					PostBuild.Length())
				{
					for (auto Cmd : PostBuild)
					{
						auto p = Cmd.Split(" ", 1);
						if (p[0].Equals("cd"))
						{
							if (p.Length() > 1)
								FileDev->SetCurrentFolder(p[1]);
							else
								LgiAssert(!"No folder for cd?");
						}
						else
						{
							GSubProcess PostCmd(p[0], p.Length() > 1 ? p[1] : NULL);
							if (PostCmd.Start(true, false))
							{
								char Buf[256];
								ssize_t rd;
								while ( (rd = PostCmd.Read(Buf, sizeof(Buf))) > 0 )
								{
									Write(Buf, rd);
								}
							}
						}
					}
				}
			}
			else
			{
				// Create a nice error message.
				GAutoString ErrStr = LgiErrorCodeToString(SubProc->GetErrorCode());
				if (ErrStr)
				{
					char *e = ErrStr + strlen(ErrStr);
					while (e > ErrStr && strchr(" \t\r\n.", e[-1]))
						*(--e) = 0;
				}
					
				sprintf_s(ErrBuf, sizeof(ErrBuf), "Running make failed with %i (%s)\n",
					SubProc->GetErrorCode(),
					ErrStr.Get());
				Err = ErrBuf;
			}
		}
	}
	else
	{
		Err = "Couldn't find program to build makefile.";
		LgiTrace("%s,%i - %s.\n", _FL, Err);
	}

	AppWnd *w = Proj->GetApp();
	if (w)
	{
		w->PostEvent(M_BUILD_DONE);
		if (Err)
			Proj->GetApp()->PostEvent(M_BUILD_ERR, 0, (GMessage::Param)NewStr(Err));
	}
	else LgiAssert(0);
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////
IdeProject::IdeProject(AppWnd *App) : IdeCommon(NULL)
{
	Project = this;
	d = new IdeProjectPrivate(App, this);
	Tag = NewStr("Project");
}

IdeProject::~IdeProject()
{
	d->App->OnProjectDestroy(this);
	GXmlTag::Empty(true);
	DeleteObj(d);
}

bool IdeProject::OnNode(const char *Path, ProjectNode *Node, bool Add)
{
	if (!Path || !Node)
	{
		LgiAssert(0);
		return false;
	}

	char Full[MAX_PATH];
	if (LgiIsRelativePath(Path))
	{
		GAutoString Base = GetBasePath();
		if (LgiMakePath(Full, sizeof(Full), Base, Path))
		{
			Path = Full;
		}
	}

	bool Status = false;
	if (Add)
		Status = d->Nodes.Add(Path, Node);
	else
		Status = d->Nodes.Delete(Path);
	
	if (Status)
		d->App->OnNode(Path, Node, Add ? FindSymbolSystem::FileAdd : FindSymbolSystem::FileRemove);

	return Status;
}

void IdeProject::ShowFileProperties(const char *File)
{
	ProjectNode *Node = NULL;
	// char *fp = FindFullPath(File, &Node);
	if (Node)
	{
		Node->OnProperties();
	}
}

const char *IdeProject::GetFileComment()
{
	return d->Settings.GetStr(ProjCommentFile);
}

const char *IdeProject::GetFunctionComment()
{
	return d->Settings.GetStr(ProjCommentFunction);
}

IdeProject *IdeProject::GetParentProject()
{
	return d->ParentProject;
}

void IdeProject::SetParentProject(IdeProject *p)
{
	d->ParentProject = p;
}

bool IdeProject::GetChildProjects(List<IdeProject> &c)
{
	CollectAllSubProjects(c);
	return c.First() != 0;
}

bool IdeProject::RelativePath(char *Out, const char *In, bool Debug)
{
	if (Out && In)
	{
		GAutoString Base = GetBasePath();
		if (Base)
		{
if (Debug) LgiTrace("XmlBase='%s'\n		In='%s'\n", Base.Get(), In);
			
			GToken b(Base, DIR_STR);
			GToken i(In, DIR_STR);
			
if (Debug) LgiTrace("Len %i-%i\n", b.Length(), i.Length());
			
			auto ILen = i.Length() + (DirExists(In) ? 0 : 1);
			auto Max = MIN(b.Length(), ILen);
			int Common = 0;
			for (; Common < Max; Common++)
			{
				#ifdef WIN32
				#define StrCompare stricmp
				#else
				#define StrCompare strcmp
				#endif
				
if (Debug) LgiTrace("Cmd '%s'-'%s'\n", b[Common], i[Common]);
				if (StrCompare(b[Common], i[Common]) != 0)
				{
					break;
				}
			}
			
if (Debug) LgiTrace("Common=%i\n", Common);
			if (Common > 0)
			{
				if (Common < b.Length())
				{
					Out[0] = 0;
					auto Back = b.Length() - Common;
if (Debug) LgiTrace("Back=%i\n", (int)Back);
					for (int n=0; n<Back; n++)
					{
						strcat(Out, "..");
						if (n < Back - 1) strcat(Out, DIR_STR);
					}
				}
				else
				{
					strcpy(Out, ".");
				}
				for (int n=Common; n<i.Length(); n++)
				{
					sprintf(Out+strlen(Out), "%s%s", DIR_STR, i[n]);
				}
				
				return true;
			}	
			else
			{
				strcpy(Out, In);
				return true;
			}		
		}
	}
	
	return false;
}

bool IdeProject::GetExePath(char *Path, int Len)
{
	const char *PExe = d->Settings.GetStr(ProjExe);
	if (PExe)
	{
		if (LgiIsRelativePath(PExe))
		{
			GAutoString Base = GetBasePath();
			if (Base)
			{
				LgiMakePath(Path, Len, Base, PExe);
			}
			else return false;
		}
		else
		{
			strcpy_s(Path, Len, PExe);
		}
		
		return true;
	}
	else return false;
}

GAutoString IdeProject::GetMakefile()
{
	GAutoString Path;
	const char *PMakefile = d->Settings.GetStr(ProjMakefile);
	if (PMakefile)
	{
		if (LgiIsRelativePath(PMakefile))
		{
			GAutoString Base = GetBasePath();
			if (Base)
			{
				char p[MAX_PATH];
				LgiMakePath(p, sizeof(p), Base, PMakefile);
				Path.Reset(NewStr(p));
			}
		}
		else
		{
			Path.Reset(NewStr(PMakefile));
		}
	}
	
	return Path;
}

void IdeProject::Clean(bool All, bool Release)
{
	if (!d->Thread &&
		d->Settings.GetStr(ProjMakefile))
	{
		GAutoString m = GetMakefile();
		if (m)
			d->Thread.Reset(new BuildThread(this, m, true, Release, All, sizeof(ssize_t)*8));
	}
}

char *QuoteStr(char *s)
{
	GStringPipe p(256);
	while (s && *s)
	{
		if (*s == ' ')
		{
			p.Push("\\ ", 2);
		}
		else p.Push(s, 1);
		s++;
	}
	return p.NewStr();
}

class ExecuteThread : public LThread, public GStream
{
	IdeProject *Proj;
	char *Exe, *Args, *Path;
	int Len;
	ExeAction Act;

public:
	ExecuteThread(IdeProject *proj, const char *exe, const char *args, char *path, ExeAction act) : LThread("ExecuteThread")
	{
		Len = 32 << 10;
		Proj = proj;
		Act = act;
		Exe = NewStr(exe);
		Args = NewStr(args);
		Path = NewStr(path);
		DeleteOnExit = true;
		
		Run();
	}
	
	~ExecuteThread()
	{
		DeleteArray(Exe);
		DeleteArray(Args);
		DeleteArray(Path);
	}
	
	int Main() override
	{
		if (Proj->GetApp())
		{
			Proj->GetApp()->PostEvent(M_APPEND_TEXT, 0, 1);
		}
		
		if (Exe)
		{
			GProcess p;
			if (Act == ExeDebug)
			{
				char *a = QuoteStr(Exe);
				char *b = QuoteStr(Path);				
				
				p.Run("kdbg", a, b, true, 0, this);
				
				DeleteArray(a);
				DeleteArray(b);
			}
			else if (Act == ExeValgrind)
			{
				#ifdef LINUX
				GString ExePath = Proj->GetExecutable(GetCurrentPlatform());
				if (ExePath)
				{
					char Path[MAX_PATH];
					char *ExeLeaf = LgiGetLeaf(Exe);
					strcpy_s(Path, sizeof(Path), ExeLeaf ? ExeLeaf : Exe);
					LgiTrimDir(Path);
									
					char *Term = 0;
					char *WorkDir = 0;
					char *Execute = 0;
					switch (LgiGetWindowManager())
					{
						case WM_Kde:
							Term = "konsole";
							WorkDir = "--workdir ";
							Execute = "-e";
							break;
						case WM_Gnome:
							Term = "gnome-terminal";
							WorkDir = "--working-directory=";
							Execute = "-x";
							break;
					}
					
					if (Term && WorkDir && Execute)
					{					
						char *e = QuoteStr(ExePath);
						char *p = QuoteStr(Path);
						char *a = Proj->GetExeArgs() ? Proj->GetExeArgs() : (char*)"";
						char Args[512];
						sprintf(Args,
								"%s%s "
								"--noclose "
								"%s valgrind --tool=memcheck --num-callers=20 %s %s",
								WorkDir,
								p,
								Execute,
								e, a);
								
						LgiExecute(Term, Args);
					}
				}
				#endif
			}
			else
			{
				p.Run(Exe, Args, Path, true, 0, this);
			}
		}
		
		return 0;
	}

	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0) override
	{
		if (Len > 0)
		{
			if (Proj->GetApp())
			{
				Size = MIN(Size, Len);
				Proj->GetApp()->PostEvent(M_APPEND_TEXT, (GMessage::Param)NewStr((char*)Buffer, Size), 1);
				Len -= Size;
			}
			return Size;
		}
		
		return 0;
	}
};

GDebugContext *IdeProject::Execute(ExeAction Act)
{
	GAutoString Base = GetBasePath();
	if (d->Settings.GetStr(ProjExe) &&
		Base)
	{
		char e[MAX_PATH];
		if (GetExePath(e, sizeof(e)))
		{
			if (FileExists(e))
			{
				const char *Args = d->Settings.GetStr(ProjArgs);
				int RunAsAdmin = d->Settings.GetInt(ProjDebugAdmin);
				if (Act == ExeDebug)
				{
					return new GDebugContext(d->App, this, e, Args, RunAsAdmin != 0);
				}
				else
				{
					new ExecuteThread(this, e, Args, Base, Act);
				}
			}
			else
			{
				LgiMsg(Tree, "Executable '%s' doesn't exist.\n", AppName, MB_OK, e);
			}
		}
	}
	
	return NULL;
}

bool IdeProject::IsMakefileUpToDate()
{
	List<IdeProject> Proj;
	if (GetChildProjects(Proj))
	{
		Proj.Insert(this);
		
		for (IdeProject *p = Proj.First(); p; p = Proj.Next())
		{
			// Is the project file modified after the makefile?
			GAutoString Proj = p->GetFullPath();
			uint64 ProjModTime = 0, MakeModTime = 0;
			GDirectory dir;
			if (dir.First(Proj))
			{
				ProjModTime = dir.GetLastWriteTime();
				dir.Close();
			}

			GAutoString m = p->GetMakefile();
			if (!m)
			{		
				d->App->GetBuildLog()->Print("Error: no makefile? (%s:%i)\n", _FL);
				break;
			}

			if (dir.First(m))
			{
				MakeModTime = dir.GetLastWriteTime();
				dir.Close();
			}

			// printf("Proj=%s - Timestamps " LGI_PrintfInt64 " - " LGI_PrintfInt64 "\n", Proj.Get(), ProjModTime, MakeModTime);
			if (ProjModTime != 0 &&
				MakeModTime != 0 &&
				ProjModTime > MakeModTime)
			{
				// Need to rebuild the makefile...
				return false;
			}
		}
	}
	
	return true;
}

bool IdeProject::FindDuplicateSymbols()
{
	GStream *Log = d->App->GetBuildLog();	
	Log->Print("FindDuplicateSymbols starting...\n");

	List<IdeProject> Proj;
	CollectAllSubProjects(Proj);
	Proj.Insert(this);

	int Lines = 0;
	
	LHashTbl<StrKey<char,false>,int64> Map(200000);
	int Found = 0;
	for (IdeProject *p = Proj.First(); p; p = Proj.Next())
	{
		GString s = p->GetExecutable(GetCurrentPlatform());
		if (s)
		{
			GString Args;
			Args.Printf("--print-size --defined-only -C %s", s.Get());
			GSubProcess Nm("nm", Args);
			if (Nm.Start(true, false))
			{
				char Buf[256];
				GStringPipe q;
				for (ssize_t Rd = 0; (Rd = Nm.Read(Buf, sizeof(Buf))); )
					q.Write(Buf, Rd);
				GString::Array a = q.NewGStr().SplitDelimit("\r\n");
				LHashTbl<StrKey<char,false>,bool> Local(200000);
				for (GString *Ln = NULL; a.Iterate(Ln); Lines++)
				{
					GString::Array p = Ln->SplitDelimit(" \t", 3);
					if (!Local.Find(p.Last()))
					{
						Local.Add(p.Last(), true);
					
						// const char *Sz = p[1];
						int64 Ours = p[1].Int(16);
						int64 Theirs = Map.Find(p.Last());
						if (Theirs >= 0)
						{
							if (Ours != Theirs)
							{
								if (Found++ < 100)
									Log->Print("    %s (" LPrintfInt64 " -> " LPrintfInt64 ")\n",
										p.Last().Get(),
										Ours, Theirs);
							}
						}
						else if (Ours >= 0)
						{						
							Map.Add(p.Last(), Ours);
						}
						else
						{
							printf("Bad line: %s\n", Ln->Get());
						}
					}
				}
			}
		}
		else printf("%s:%i - GetExecutable failed.\n", _FL);
	}

	/*
	char *Sym;
	for (int Count = Map.First(&Sym); Count; Count = Map.Next(&Sym))
	{
		if (Count > 1)
			Log->Print("    %i: %s\n", Count, Sym);
	}
	*/

	Log->Print("FindDuplicateSymbols finished (%i lines)\n", Lines);
	return false;
}

bool IdeProject::FixMissingFiles()
{
	FixMissingFilesDlg(this);
	return true;
}

void IdeProject::Build(bool All, bool Release)
{
	if (d->Thread)
	{
		d->App->GetBuildLog()->Print("Error: Already building (%s:%i)\n", _FL);
		return;
	}

	GAutoString m = GetMakefile();
	if (!m)
	{		
		d->App->GetBuildLog()->Print("Error: no makefile? (%s:%i)\n", _FL);
		return;
	}

	if (GetApp())
		GetApp()->PostEvent(M_APPEND_TEXT, 0, 0);

	SetClean();

	if (!IsMakefileUpToDate())
		CreateMakefile(GetCurrentPlatform(), true);
	else
		// Start the build thread...
		d->Thread.Reset
		(
			new BuildThread
			(
				this,
				m,
				false,
				Release,
				All,
				sizeof(size_t)*8
			)
		);
}

void IdeProject::StopBuild()
{
	d->Thread.Reset();
}

bool IdeProject::Serialize(bool Write)
{
	return true;
}

AppWnd *IdeProject::GetApp()
{
	return d->App;
}

const char *IdeProject::GetIncludePaths()
{
	return d->Settings.GetStr(ProjIncludePaths);
}

const char *IdeProject::GetPreDefinedValues()
{
	return d->Settings.GetStr(ProjDefines);
}

const char *IdeProject::GetExeArgs()
{
	return d->Settings.GetStr(ProjArgs);
}

GString IdeProject::GetExecutable(IdePlatform Platform)
{
	GString Bin = d->Settings.GetStr(ProjExe, NULL, Platform);
	GAutoString Base = GetBasePath();

	if (Bin)
	{
		if (LgiIsRelativePath(Bin) && Base)
		{
			char p[MAX_PATH];
			if (LgiMakePath(p, sizeof(p), Base, Bin))
				Bin = p;
		}
		
		return Bin;
	}

	// Create binary name from target:	
	GString Target = GetTargetName(Platform);
	if (Target)
	{
		int BuildMode = d->App->GetBuildMode();
		const char *Name = BuildMode ? "Release" : "Debug";
		const char *Postfix = BuildMode ? "" : "d";
		
		switch (Platform)
		{			
			case PlatformWin32:
			{
				Bin.Printf("%s%s.dll", Target.Get(), Postfix);
				break;
			}
			case PlatformMac:
			{
				Bin.Printf("lib%s%s.dylib", Target.Get(), Postfix);
				break;
			}
			case PlatformLinux:
			case PlatformHaiku:
			{
				Bin.Printf("lib%s%s.so", Target.Get(), Postfix);
				break;
			}
			default:
			{
				LgiAssert(0);
				printf("%s:%i - Unknown platform.\n", _FL);
				return GString();
			}
		}
		
		// Find the actual file...
		if (!Base)
		{
			printf("%s:%i - GetBasePath failed.\n", _FL);
			return GString();
		}
			
		char Path[MAX_PATH];
		LgiMakePath(Path, sizeof(Path), Base, Name);
		LgiMakePath(Path, sizeof(Path), Path, Bin);
		if (FileExists(Path))
			Bin = Path;
		else
			printf("%s:%i - '%s' doesn't exist.\n", _FL, Path);
		
		
		return Bin;
	}
	
	return GString();
}

char *IdeProject::GetFileName()
{
	return d->FileName;
}

int IdeProject::GetPlatforms()
{
	return PLATFORM_ALL;
}
	
GAutoString IdeProject::GetFullPath()
{
	GAutoString Status;
	if (!d->FileName)
	{
		// LgiAssert(!"No path.");
		return Status;
	}

	GArray<char*> sections;
	IdeProject *proj = this;
	while (	proj &&
			proj->GetFileName() &&
			LgiIsRelativePath(proj->GetFileName()))
	{
		sections.AddAt(0, proj->GetFileName());
		proj = proj->GetParentProject();
	}

	if (!proj)
	{
		// LgiAssert(!"All projects have a relative path?");
		return Status; // No absolute path in the parent projects?
	}

	char p[MAX_PATH];
	strcpy_s(p, sizeof(p), proj->GetFileName()); // Copy the base path
	if (sections.Length() > 0)
		LgiTrimDir(p); // Trim off the filename
	for (int i=0; i<sections.Length(); i++) // For each relative section
	{
		LgiMakePath(p, sizeof(p), p, sections[i]); // Append relative path
		if (i < sections.Length() - 1)
			LgiTrimDir(p); // Trim filename off
	}
	
	Status.Reset(NewStr(p));
	return Status;
}

GAutoString IdeProject::GetBasePath()
{
	GAutoString a = GetFullPath();
	LgiTrimDir(a);
	return a;
}

void IdeProject::CreateProject()
{
	Empty();
	
	d->FileName.Empty();
	d->UserFile.Empty();
	d->App->GetTree()->Insert(this);
	
	ProjectNode *f = new ProjectNode(this);
	if (f)
	{
		f->SetName("Source");
		f->SetType(NodeDir);
		InsertTag(f);
	}
	
	f = new ProjectNode(this);
	if (f)
	{
		f->SetName("Headers");
		f->SetType(NodeDir);
		InsertTag(f);
	}
	
	d->Settings.Set(ProjEditorTabSize, 4);
	d->Settings.Set(ProjEditorIndentSize, 4);
	d->Settings.Set(ProjEditorUseHardTabs, true);
	
	d->Dirty = true;
	Expanded(true);	
}

ProjectStatus IdeProject::OpenFile(char *FileName)
{
	GProfile Prof("IdeProject::OpenFile");
	Prof.HideResultsIfBelow(1000);

	Empty();

	Prof.Add("Init");

	d->UserNodeFlags.Empty();
	if (LgiIsRelativePath(FileName))
	{
		char p[MAX_PATH];
		getcwd(p, sizeof(p));
		LgiMakePath(p, sizeof(p), p, FileName);
		d->FileName = p;
	}
	else
	{
		d->FileName = FileName;
	}
	d->UserFile = d->FileName + "." + LCurrentUserName();

	if (!d->FileName)
	{
		LgiTrace("%s:%i - No filename.\n", _FL);
		return OpenError;
	}
	
	Prof.Add("FileOpen");

	GFile f;
	if (!f.Open(d->FileName, O_READWRITE))
	{
		LgiTrace("%s:%i - Error: Can't open '%s'.\n", _FL, d->FileName.Get());
		return OpenError;
	}

	Prof.Add("Xml");

	GXmlTree x;
	GXmlTag r;
	if (!x.Read(&r, &f))
	{
		LgiTrace("%s:%i - Error: Can't read XML: %s\n", _FL, x.GetErrorMsg());
		LgiMsg(Tree, x.GetErrorMsg(), AppName);
		return OpenError;
	}

	Prof.Add("Progress Setup");

	#if DEBUG_OPEN_PROGRESS
	int64 Nodes = r.CountTags();
	GProgressDlg Prog(d->App, 1000);
	Prog.SetDescription("Loading project...");
	Prog.SetLimits(0, Nodes);
	Prog.SetYieldTime(1000);
	Prog.SetAlwaysOnTop(true);
	#endif

	Prof.Add("UserFile");

	if (FileExists(d->UserFile))
	{
		GFile Uf;
		if (Uf.Open(d->UserFile, O_READ))
		{
			GString::Array Ln = Uf.Read().SplitDelimit("\n");
			for (unsigned i=0; i<Ln.Length(); i++)
			{
				GString::Array p = Ln[i].SplitDelimit(",");
				if (p.Length() == 2)
					d->UserNodeFlags.Add((int)p[0].Int(), (int)p[1].Int(16));
			}
		}
	}
	
	if (!r.IsTag("Project"))
		return OpenError;

	Prof.Add("OnOpen");

	bool Ok = OnOpen(
		#if DEBUG_OPEN_PROGRESS
		&Prog,
		#else
		NULL,
		#endif
		&r);
	#if DEBUG_OPEN_PROGRESS
	if (Prog.IsCancelled())
		return OpenCancel;
	else 
	#endif
	if (!Ok)
		return OpenError;

	Prof.Add("Insert");

	d->App->GetTree()->Insert(this);
	Expanded(true);

	Prof.Add("Serialize");
	
	d->Settings.Serialize(&r, false /* read */);
	return OpenOk;
}

bool IdeProject::SaveFile()
{
	GAutoString Full = GetFullPath();

	printf("IdeProject::SaveFile %s %i\n", Full.Get(), d->Dirty);
	if (ValidStr(Full) && d->Dirty)
	{
		GFile f;
		if (f.Open(Full, O_WRITE))
		{
			f.SetSize(0);

			GXmlTree x;
			GProgressDlg Prog(d->App, 1000);
			Prog.SetAlwaysOnTop(true);
			Prog.SetDescription("Serializing project XML...");
			Prog.SetYieldTime(200);
			Prog.SetCanCancel(false);

			d->Settings.Serialize(this, true /* write */);
			
			GStringPipe Buf(4096);			
			if (x.Write(this, &Buf, &Prog))
			{
				GCopyStreamer Cp;
				Prog.SetDescription("Writing XML...");
				LgiYield();
				if (Cp.Copy(&Buf, &f))
					d->Dirty = false;
			}
			else LgiTrace("%s:%i - Failed to write XML.\n", _FL);
		}
		else LgiTrace("%s:%i - Couldn't open '%s' for writing.\n", _FL, Full.Get());
	}

	if (d->UserFileDirty)
	{
		GFile f;
		LgiAssert(d->UserFile.Get());
		if (f.Open(d->UserFile, O_WRITE))
		{
			f.SetSize(0);

			// Save user file details..
			// int Id;
			// for (int Flags = d->UserNodeFlags.First(&Id); Flags >= 0; Flags = d->UserNodeFlags.Next(&Id))
			for (auto i : d->UserNodeFlags)
			{
				f.Print("%i,%x\n", i.key, i.value);
			}

			d->UserFileDirty = false;
		}
	}

	printf("\tIdeProject::SaveFile %i %i\n", d->Dirty, d->UserFileDirty);

	return	!d->Dirty &&
			!d->UserFileDirty;
}

void IdeProject::SetDirty()
{
	d->Dirty = true;
	d->App->OnProjectChange();
}

void IdeProject::SetUserFileDirty()
{
	d->UserFileDirty = true;
}

bool IdeProject::GetExpanded(int Id)
{
	int f = d->UserNodeFlags.Find(Id);
	return f >= 0 ? f : false;
}

void IdeProject::SetExpanded(int Id, bool Exp)
{
	if (d->UserNodeFlags.Find(Id) != (int)Exp)
	{
		d->UserNodeFlags.Add(Id, Exp);
		SetUserFileDirty();
	}
}

int IdeProject::AllocateId()
{
	return d->NextNodeId++;
}

bool IdeProject::SetClean()
{
	// printf("IdeProject::SetClean %i %i\n", d->Dirty, d->UserFileDirty);
	if (d->Dirty || d->UserFileDirty)
	{
		if (!ValidStr(d->FileName))
		{
			GFileSelect s;
			s.Parent(Tree);
			s.Name("Project.xml");
			if (s.Save())
			{
				d->FileName = s.Name();
				d->UserFile = d->FileName + "." + LCurrentUserName();
				d->App->OnFile(d->FileName, true);
				Update();
			}
			else return false;
		}

		SaveFile();
	}

	ForAllProjectNodes(p)
	{
		p->SetClean();
	}

	return true;
}

char *IdeProject::GetText(int Col)
{
	if (d->FileName)
	{
		char *s = strrchr(d->FileName, DIR_CHAR);
		return s ? s + 1 : d->FileName.Get();
	}

	return (char*)Untitled;
}

int IdeProject::GetImage(int Flags)
{
	return 0;
}

void IdeProject::Empty()
{
	GXmlTag *t;
	while ((t = Children.First()))
	{
		ProjectNode *n = dynamic_cast<ProjectNode*>(t);
		if (n)
		{
			n->Remove();
		}
		DeleteObj(t);
	}
}

GXmlTag *IdeProject::Create(char *Tag)
{
	if (!stricmp(Tag, TagSettings))
		return NULL;

	return new ProjectNode(this);
}

void IdeProject::OnMouseClick(GMouse &m)
{
	if (m.IsContextMenu())
	{
		GSubMenu Sub;
		Sub.AppendItem("New Folder", IDM_NEW_FOLDER);
		Sub.AppendItem("New Web Folder", IDM_WEB_FOLDER);
		Sub.AppendSeparator();
		Sub.AppendItem("Build", IDM_BUILD);
		Sub.AppendItem("Clean Project", IDM_CLEAN_PROJECT);
		Sub.AppendItem("Clean All", IDM_CLEAN_ALL);
		Sub.AppendItem("Rebuild Project", IDM_REBUILD_PROJECT);
		Sub.AppendItem("Rebuild All", IDM_REBUILD_ALL);
		Sub.AppendSeparator();
		Sub.AppendItem("Sort Children", IDM_SORT_CHILDREN);
		Sub.AppendSeparator();
		Sub.AppendItem("Settings", IDM_SETTINGS, true);
		Sub.AppendItem("Insert Dependency", IDM_INSERT_DEP);

		m.ToScreen();
		GdcPt2 c = _ScrollPos();
		m.x -= c.x;
		m.y -= c.y;
		switch (Sub.Float(Tree, m.x, m.y))
		{
			case IDM_NEW_FOLDER:
			{
				GInput Name(Tree, "", "Name:", AppName);
				if (Name.DoModal())
				{
					GetSubFolder(this, Name.GetStr(), true);
				}
				break;
			}
			case IDM_WEB_FOLDER:
			{
				WebFldDlg Dlg(Tree, 0, 0, 0);
				if (Dlg.DoModal())
				{
					if (Dlg.Ftp && Dlg.Www)
					{
						IdeCommon *f = GetSubFolder(this, Dlg.Name, true);
						if (f)
						{
							f->SetAttr(OPT_Ftp, Dlg.Ftp);
							f->SetAttr(OPT_Www, Dlg.Www);
						}
					}
				}
				break;
			}
			case IDM_BUILD:
			{
				StopBuild();
				Build(true, d->App->IsReleaseMode());
				break;
			}
			case IDM_CLEAN_PROJECT:
			{
				Clean(false, d->App->IsReleaseMode());
				break;
			}
			case IDM_CLEAN_ALL:
			{
				Clean(true, d->App->IsReleaseMode());
				break;
			}
			case IDM_REBUILD_PROJECT:
			{
				StopBuild();
				Clean(false, d->App->IsReleaseMode());
				Build(false, d->App->IsReleaseMode());
				break;
			}
			case IDM_REBUILD_ALL:
			{
				StopBuild();
				Clean(true, d->App->IsReleaseMode());
				Build(true, d->App->IsReleaseMode());
				break;
			}
			case IDM_SORT_CHILDREN:
			{
				SortChildren();
				Project->SetDirty();
				break;
			}
			case IDM_SETTINGS:
			{
				if (d->Settings.Edit(Tree))
				{
					SetDirty();
				}
				break;
			}
			case IDM_INSERT_DEP:
			{
				GFileSelect s;
				s.Parent(Tree);
				s.Type("Project", "*.xml");
				if (s.Open())
				{
					ProjectNode *New = new ProjectNode(this);
					if (New)
					{
						New->SetFileName(s.Name());
						New->SetType(NodeDependancy);
						InsertTag(New);
						SetDirty();
					}
				}
				break;
			}
		}
	}
}

char *IdeProject::FindFullPath(const char *File, ProjectNode **Node)
{
	char *Full = 0;
	
	ForAllProjectNodes(c)
	{
		ProjectNode *n = c->FindFile(File, &Full);
		if (n)
		{
			if (Node)
				*Node = n;
			break;
		}
	}
	
	return Full;
}

bool IdeProject::HasNode(ProjectNode *Node)
{
	ForAllProjectNodes(c)
	{
		if (c->HasNode(Node))
			return true;
	}
	return false;
}

bool IdeProject::GetAllNodes(GArray<ProjectNode*> &Nodes)
{
	ForAllProjectNodes(c)
	{
		c->AddNodes(Nodes);
	}
	return true;
}

bool IdeProject::InProject(bool FuzzyMatch, const char *Path, bool Open, IdeDoc **Doc)
{
	if (!Path)
		return false;
		
	// Search complete path first...
	ProjectNode *n = d->Nodes.Find(Path);	
	if (!n && FuzzyMatch)
	{
		// No match, do partial matching.
		const char *Leaf = LgiGetLeaf(Path);
		auto PathLen = strlen(Path);
		auto LeafLen = strlen(Leaf);
		uint32 MatchingScore = 0;

		// Traverse all nodes and try and find the best fit.
		// const char *p;
		// for (ProjectNode *Cur = d->Nodes.First(&p); Cur; Cur = d->Nodes.Next(&p))
		for (auto Cur : d->Nodes)
		{
			int CurPlatform = Cur.value->GetPlatforms();
			uint32 Score = 0;

			if (stristr(Cur.key, Path))
			{
				Score += PathLen;
			}
			else if (stristr(Cur.key, Leaf))
			{
				Score += LeafLen;
			}

			const char *pLeaf = LgiGetLeaf(Cur.key);
			if (pLeaf && !stricmp(pLeaf, Leaf))
			{
				Score |= 0x80000000;
			}
			if (Score && (CurPlatform & PLATFORM_CURRENT) != 0)
			{
				Score |= 0x40000000;
			}

			if (Score > MatchingScore)
			{
				MatchingScore = Score;
				n = Cur.value;
			}
		}
	}
	
	if (n && Doc)
	{
		*Doc = n->Open();
	}
	
	return n != 0;
}

char *GetQuotedStr(char *Str)
{
	char *s = strchr(Str, '\"');
	if (s)
	{
		s++;
		char *e = strchr(s, '\"');
		if (e)
		{
			return NewStr(s, e - s);
		}
	}

	return 0;
}

void IdeProject::ImportDsp(char *File)
{
	if (File && FileExists(File))
	{
		char Base[256];
		strcpy(Base, File);
		LgiTrimDir(Base);
		
		char *Dsp = ReadTextFile(File);
		if (Dsp)
		{
			GToken Lines(Dsp, "\r\n");
			IdeCommon *Current = this;
			bool IsSource = false;
			for (int i=0; i<Lines.Length(); i++)
			{
				char *L = Lines[i];
				if (strnicmp(L, "# Begin Group", 13) == 0)
				{
					char *Folder = GetQuotedStr(L);
					if (Folder)
					{
						IdeCommon *Sub = Current->GetSubFolder(this, Folder, true);
						if (Sub)
						{
							Current = Sub;
						}
						DeleteArray(Folder);
					}
				}
				else if (strnicmp(L, "# End Group", 11) == 0)
				{
					IdeCommon *Parent = dynamic_cast<IdeCommon*>(Current->GetParent());
					if (Parent)
					{
						Current = Parent;
					}
				}
				else if (strnicmp(L, "# Begin Source", 14) == 0)
				{
					IsSource = true;
				}
				else if (strnicmp(L, "# End Source", 12) == 0)
				{
					IsSource = false;
				}
				else if (Current && IsSource && strncmp(L, "SOURCE=", 7) == 0)
				{
					ProjectNode *New = new ProjectNode(this);
					if (New)
					{
						char *Src = 0;
						if (strchr(L, '\"'))
						{
							Src = GetQuotedStr(L);
						}
						else
						{
							Src = NewStr(L + 7);
						}						
						if (Src)
						{
							// Make absolute path
							char Abs[256];
							LgiMakePath(Abs, sizeof(Abs), Base, ToUnixPath(Src));
							
							// Make relitive path
							New->SetFileName(Src);
							DeleteArray(Src);
						}

						Current->InsertTag(New);
						SetDirty();
					}
				}
			}
			
			DeleteArray(Dsp);
		}
	}
}

IdeProjectSettings *IdeProject::GetSettings()
{
	return &d->Settings;
}

bool IdeProject::BuildIncludePaths(GArray<GString> &Paths, bool Recurse, bool IncludeSystem, IdePlatform Platform)
{
	List<IdeProject> Projects;
	if (Recurse)
	{
		GetChildProjects(Projects);
	}
	Projects.Insert(this, 0);

	LHashTbl<StrKey<char>, bool> Map;
	
	for (IdeProject *p=Projects.First(); p; p=Projects.Next())
	{
		GString ProjInclude = d->Settings.GetStr(ProjIncludePaths, NULL, Platform);
		GAutoString Base = p->GetBasePath();
		
		const char *Delim = ",;\r\n";
		GString::Array In, Out;
		GString::Array a = ProjInclude.SplitDelimit(Delim);
		In = a;
		
		if (IncludeSystem)
		{
			GString SysInclude = d->Settings.GetStr(ProjSystemIncludes, NULL, Platform);
			a = SysInclude.SplitDelimit(Delim);
			In.SetFixedLength(false);
			In.Add(a);
		}
		
		for (unsigned i=0; i<In.Length(); i++)
		{
			GString p;
			#if DIR_CHAR == '\\'
			p = In[i].Replace("/", "\\").Strip();
			#else
			p = In[i].Replace("\\", "/").Strip();
			#endif

			char *Path = p;
			if (*Path == '`')
			{
				// Run config app to get the full path list...
				p = p.Strip("`");
				GString::Array a = p.Split(" ", 1);
				GProcess Proc;
				GStringPipe Buf;
				if (Proc.Run(a[0], a.Length() > 1 ? a[1].Get() : NULL, NULL, true, NULL, &Buf))
				{
					GString result = Buf.NewGStr();
					a = result.Split(" \t\r\n");
					for (int i=0; i<a.Length(); i++)
					{
						char *inc = a[i];
						if (inc[0] == '-' &&
							inc[1] == 'I')
						{
							Out.New() = a[i](2,-1);
						}
					}
				}
				else LgiTrace("%s:%i - Error: failed to run process for '%s'\n", _FL, p.Get());
			}
			else
			{
				// Add path literal
				Out.New() = Path;
			}
		}
			
		for (int i=0; i<Out.Length(); i++)
		{
			char *Path = Out[i];
			char *Full = 0, Buf[MAX_PATH];
			if
			(
				*Path != '/'
				&&
				!(
					IsAlpha(*Path)
					&&
					Path[1] == ':'
				)
			)
			{
				LgiMakePath(Buf, sizeof(Buf), Base, Path);
				Full = Buf;
			}
			else
			{
				Full = Out[i];
			}
			
			#if 0
			bool Has = false;
			for (int n=0; n<Paths.Length(); n++)
			{
				if (stricmp(Paths[n], Full) == 0)
				{
					Has = true;
					break;
				}
			}
			
			if (!Has)
			{
				Paths.Add(NewStr(Full));
			}
			#else
			if (!Map.Find(Full))
				Map.Add(Full, true);
			#endif
		}

		// Add paths for the headers in the project... bit of a hack but it'll
		// help it find them if the user doesn't specify the paths in the project.
		GArray<ProjectNode*> Nodes;
		if (p->GetAllNodes(Nodes))
		{
			GAutoString Base = p->GetFullPath();
			if (Base)
			{
				LgiTrimDir(Base);

				for (unsigned i=0; i<Nodes.Length(); i++)
				{
					ProjectNode *n = Nodes[i];
					if (n->GetType() == NodeHeader &&				// Only look at headers.
						(n->GetPlatforms() & (1 << Platform)) != 0)	// Exclude files not on this platform.
					{
						char *f = n->GetFileName();
						char p[MAX_PATH];
						if (f &&
							LgiMakePath(p, sizeof(p), Base, f))
						{
							char *l = strrchr(p, DIR_CHAR);
							if (l)
								*l = 0;
							if (!Map.Find(p))
							{
								Map.Add(p, true);
							}
						}
					}
				}
			}
		}
	}

	// char *p;
	// for (bool b = Map.First(&p); b; b = Map.Next(&p))
	for (auto p : Map)
		Paths.Add(p.key);

	return true;
}

void IdeProjectPrivate::CollectAllFiles(GTreeNode *Base, GArray<ProjectNode*> &Files, bool SubProjects, int Platform)
{
	for (GTreeItem *i = Base->GetChild(); i; i = i->GetNext())
	{
		IdeProject *Proj = dynamic_cast<IdeProject*>(i);
		if (Proj)
		{
			if (Proj->GetParentProject() && !SubProjects)
			{
				continue;
			}
		}
		else
		{
			ProjectNode *p = dynamic_cast<ProjectNode*>(i);
			if (p)
			{
				if (p->GetType() == NodeSrc ||
					p->GetType() == NodeHeader)
				{
					if (p->GetPlatforms() & Platform)
					{
						Files.Add(p);
					}
				}
			}
		}

		CollectAllFiles(i, Files, SubProjects, Platform);
	}
}

GString IdeProject::GetTargetName(IdePlatform Platform)
{
	GString Status;
	const char *t = d->Settings.GetStr(ProjTargetName, NULL, Platform);
	if (ValidStr(t))
	{
		// Take target name from the settings
		Status = t;
	}
	else
	{
		char *s = strrchr(d->FileName, DIR_CHAR);
		if (s)
		{
			// Generate the target executable name
			char Target[MAX_PATH];
			strcpy_s(Target, sizeof(Target), s + 1);
			s = strrchr(Target, '.');
			if (s) *s = 0;
			strlwr(Target);
			s = Target;
			for (char *i = Target; *i; i++)
			{
				if (*i != '.' && *i != ' ')
				{
					*s++ = *i;
				}
			}
			*s = 0;
			Status = Target;
		}
	}
	
	return Status;
}

bool IdeProject::GetTargetFile(char *Buf, int BufSize)
{
	bool Status = false;
	GString Target = GetTargetName(PlatformCurrent);
	if (Target)
	{
		const char *TargetType = d->Settings.GetStr(ProjTargetType);
		if (TargetType)
		{
			if (!stricmp(TargetType, "Executable"))
			{
				strcpy_s(Buf, BufSize, Target);
				Status = true;
			}
			else if (!stricmp(TargetType, "DynamicLibrary"))
			{
				char t[MAX_PATH];
				strcpy_s(t, sizeof(t), Target);
				char *ext = LgiGetExtension(t);
				if (!ext)
					sprintf(t + strlen(t), ".%s", LGI_LIBRARY_EXT);
				else if (stricmp(ext, LGI_LIBRARY_EXT))
					strcpy(ext, LGI_LIBRARY_EXT);
				strcpy_s(Buf, BufSize, t);
				Status = true;
			}
			else if (!stricmp(TargetType, "StaticLibrary"))
			{
				snprintf(Buf, BufSize, "lib%s.%s", Target.Get(), LGI_STATIC_LIBRARY_EXT);
				Status = true;
			}
		}
	}
	
	return Status;
}

struct Dependency
{
	bool Scanned;
	GAutoString File;
	
	Dependency(const char *f)
	{
		Scanned = false;
		File.Reset(NewStr(f));
	}
};

bool IdeProject::GetAllDependencies(GArray<char*> &Files, IdePlatform Platform)
{
	LHashTbl<StrKey<char>, Dependency*> Deps;
	GAutoString Base = GetBasePath();
	
	// Build list of all the source files...
	GArray<GString> Src;
	CollectAllSource(Src, Platform);
	
	// Get all include paths
	GArray<GString> IncPaths;
	BuildIncludePaths(IncPaths, false, false, Platform);
	
	// Add all source to dependencies
	for (int i=0; i<Src.Length(); i++)
	{
		char *f = Src[i];
		Dependency *dep = Deps.Find(f);
		if (!dep)
			Deps.Add(f, new Dependency(f));
	}
	
	// Scan all dependencies for includes
	GArray<Dependency*> Unscanned;
	do
	{
		// Find all the unscanned dependencies
		Unscanned.Length(0);
		// for (Dependency *d = Deps.First(); d; d = Deps.Next())
		for (auto d : Deps)
		{
			if (!d.value->Scanned)
				Unscanned.Add(d.value);
		}		

		for (int i=0; i<Unscanned.Length(); i++)
		{
			// Then scan source for includes...
			Dependency *d = Unscanned[i];
			d->Scanned = true;
			
			char *Src = d->File;
			char Full[MAX_PATH];
			if (LgiIsRelativePath(d->File))
			{
				LgiMakePath(Full, sizeof(Full), Base, d->File);
				Src = Full;
			}

			GArray<char*> SrcDeps;
			if (GetDependencies(Src, IncPaths, SrcDeps, Platform))
			{
				for (int n=0; n<SrcDeps.Length(); n++)
				{
					// Add include to dependencies...
					char *File = SrcDeps[n];

					if (LgiIsRelativePath(File))
					{
						LgiMakePath(Full, sizeof(Full), Base, File);
						File = Full;
					}

					if (!Deps.Find(File))
					{						
						Deps.Add(File, new Dependency(File));
					}
				}
				SrcDeps.DeleteArrays();
			}
		}
	}
	while (Unscanned.Length() > 0);
	
	// for (Dependency *d = Deps.First(); d; d = Deps.Next())
	for (auto d : Deps)
	{
		Files.Add(d.value->File.Release());
	}
	
	Deps.DeleteObjects();
	return true;
}

bool IdeProject::GetDependencies(const char *SourceFile, GArray<GString> &IncPaths, GArray<char*> &Files, IdePlatform Platform)
{
	if (!FileExists(SourceFile))
	{
		LgiTrace("%s:%i - can't read '%s'\n", _FL, SourceFile);
		return false;
	}

	GAutoString c8(ReadTextFile(SourceFile));
	if (!c8)
		return false;

	GArray<char*> Headers;
	if (!BuildHeaderList(c8, Headers, IncPaths, false))
		return false;
	
	for (int n=0; n<Headers.Length(); n++)
	{
		char *i = Headers[n];
		char p[MAX_PATH];
		if (!RelativePath(p, i))
		{
			strcpy_s(p, sizeof(p), i);
		}
		ToUnixPath(p);
		Files.Add(NewStr(p));
	}
	Headers.DeleteArrays();
	
	return true;
}

bool IdeProject::CreateMakefile(IdePlatform Platform, bool BuildAfterwards)
{
	if (d->CreateMakefile)
	{
		if (d->CreateMakefile->IsExited())
			d->CreateMakefile.Reset();
		else
		{
			d->App->GetBuildLog()->Print("%s:%i - Makefile thread still running.\n", _FL);
			return false;
		}
	}		

	return d->CreateMakefile.Reset(new MakefileThread(d, Platform, BuildAfterwards));
}

////////////////////////////////////////////////////////////////////////////////////////////
IdeTree::IdeTree() : GTree(IDC_PROJECT_TREE, 0, 0, 100, 100)
{
	Hit = 0;
	MultiSelect(true);
}

void IdeTree::OnCreate()
{
	SetWindow(this);
}

void IdeTree::OnDragExit()
{
	SelectDropTarget(0);
}

int IdeTree::WillAccept(List<char> &Formats, GdcPt2 p, int KeyState)
{
	static bool First = true;
	
	for (char *f=Formats.First(); f; )
	{
		/*
		if (First)
			LgiTrace("	  WillAccept='%s'\n", f);
		*/
		
		if (stricmp(f, NODE_DROP_FORMAT) == 0 ||
			stricmp(f, LGI_FileDropFormat) == 0)
		{
			f = Formats.Next();
		}
		else
		{
			Formats.Delete(f);
			DeleteArray(f);
			f = Formats.Current();
		}
	}
	
	First = false;
		
	if (Formats.Length() > 0)
	{
		Hit = ItemAtPoint(p.x, p.y);
		if (Hit)
		{
			if (!stricmp(Formats.First(), LGI_FileDropFormat))
			{
				SelectDropTarget(Hit);
				return DROPEFFECT_LINK;
			}
			else
			{
				IdeCommon *Src = dynamic_cast<IdeCommon*>(Selection());
				IdeCommon *Dst = dynamic_cast<IdeCommon*>(Hit);
				if (Src && Dst)
				{
					// Check this folder is not a child of the src
					for (IdeCommon *n=Dst; n; n=dynamic_cast<IdeCommon*>(n->GetParent()))
					{
						if (n == Src)
						{
							return DROPEFFECT_NONE;
						}
					}
				}

				// Valid target
				SelectDropTarget(Hit);
				return DROPEFFECT_MOVE;
			}
		}
	}
	else LgiTrace("%s:%i - No valid drop formats.\n", _FL);

	return DROPEFFECT_NONE;
}

int IdeTree::OnDrop(GArray<GDragData> &Data, GdcPt2 p, int KeyState)
{
	SelectDropTarget(0);

	if (!Hit)
		Hit = ItemAtPoint(p.x, p.y);

	if (!Hit)
		return DROPEFFECT_NONE;
	
	for (unsigned n=0; n<Data.Length(); n++)
	{
		GDragData &dd = Data[n];
		GVariant *Data = &dd.Data[0];
		if (dd.IsFormat(NODE_DROP_FORMAT))
		{
			if (Data->Type == GV_BINARY && Data->Value.Binary.Length == sizeof(ProjectNode*))
			{
				ProjectNode *Src = ((ProjectNode**)Data->Value.Binary.Data)[0];
				if (Src)
				{
					ProjectNode *Folder = dynamic_cast<ProjectNode*>(Hit);
					while (Folder && Folder->GetType() > NodeDir)
					{
						Folder = dynamic_cast<ProjectNode*>(Folder->GetParent());
					}

					IdeCommon *Dst = dynamic_cast<IdeCommon*>(Folder?Folder:Hit);
					if (Dst)
					{
						// Check this folder is not a child of the src
						for (IdeCommon *n=Dst; n; n=dynamic_cast<IdeCommon*>(n->GetParent()))
						{
							if (n == Src)
							{
								return DROPEFFECT_NONE;
							}
						}

						// Detach
						GTreeItem *i = dynamic_cast<GTreeItem*>(Src);
						i->Detach();
						if (Src->GXmlTag::Parent)
						{
							LgiAssert(Src->GXmlTag::Parent->Children.HasItem(Src));
							Src->GXmlTag::Parent->Children.Delete(Src);
						}
						
						// Attach
						Src->GXmlTag::Parent = Dst;
						Dst->Children.Insert(Src);
						Dst->Insert(Src);
						
						// Dirty
						Src->GetProject()->SetDirty();
					}
				
					return DROPEFFECT_MOVE;
				}
			}
		}
		else if (dd.IsFileDrop())
		{
			ProjectNode *Folder = dynamic_cast<ProjectNode*>(Hit);
			while (Folder && Folder->GetType() > NodeDir)
			{
				Folder = dynamic_cast<ProjectNode*>(Folder->GetParent());
			}
			
			IdeCommon *Dst = dynamic_cast<IdeCommon*>(Folder?Folder:Hit);
			if (Dst)
			{
				AddFilesProgress Prog(this);
				GDropFiles Df(dd);
				for (int i=0; i<Df.Length() && !Prog.Cancel; i++)
				{
					Dst->AddFiles(&Prog, Df[i]);
				}
			}
		}
		else
		{
			LgiTrace("%s:%i - Unknown drop format: %s.\n", _FL, dd.Format.Get());
		}
	}

	return DROPEFFECT_NONE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
AddFilesProgress::AddFilesProgress(GViewI *par)
{
	v = 0;
	Cancel = false;
	Msg = NULL;
	SetParent(par);
	Ts = LgiCurrentTime();
	GRect r(0, 0, 140, 100);
	SetPos(r);
	MoveSameScreen(par);
	Name("Importing files...");

	GString::Array a = GString(DefaultExt).SplitDelimit(",");
	for (unsigned i=0; i<a.Length(); i++)
	{
		Exts.Add(a[i], true);
	}

	GTableLayout *t = new GTableLayout(100);
	AddView(t);
		
	GLayoutCell *c = t->GetCell(0, 0);
	c->Add(new GTextLabel(-1, 0, 0, -1, -1, "Loaded:"));
		
	c = t->GetCell(1, 0);
	c->Add(Msg = new GTextLabel(-1, 0, 0, -1, -1, "..."));
		
	c = t->GetCell(0, 1, true, 2);
	c->TextAlign(GCss::Len(GCss::AlignRight));
	c->Add(new GButton(IDCANCEL, 0, 0, -1, -1, "Cancel"));
}

int64 AddFilesProgress::Value()
{
	return v;
}

void AddFilesProgress::Value(int64 val)
{
	v = val;
	if (Visible() && Msg)
	{
		Msg->Value(v);
		Msg->SendNotify(GNotifyTableLayout_Refresh);
	}

	uint64 Now = LgiCurrentTime();
	if (Visible())
	{
		if (Now - Ts > 150)
			LgiYield();
	}
	else if (Now - Ts > 1000)
	{
		DoModeless();
	}
}

int AddFilesProgress::OnNotify(GViewI *c, int f)
{
	if (c->GetId() == IDCANCEL)
		Cancel = true;
	return 0;
}

