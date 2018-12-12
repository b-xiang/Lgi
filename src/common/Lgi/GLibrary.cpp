#define __USE_GNU
#include <stdio.h>

#define DEBUG_LIB_MSGS		0

#include "Lgi.h"
#if defined(LINUX) || defined(MAC) || defined(BEOS)
#include <dlfcn.h>
#endif
#include "GToken.h"

GLibrary::GLibrary(const char *File, bool Quiet)
{
	FileName = 0;
	hLib = 0;
	if (File)
	{
		Load(File, Quiet);	
	}
}

GLibrary::~GLibrary()
{
	Unload();
}

bool GLibrary::Load(const char *File, bool Quiet)
{
	Unload();

	if (File)
	{
		char f[MAX_PATH];
		int ch = sprintf_s(f, sizeof(f), "%s", File);
		
		// check for default extension..
		// if you specify no extension in the application this will correctly set
		// it for the OS your running thus removing the need for #ifdef's in your
		// app. sweet!
		char *Ext = LgiGetExtension((char*)File);
		if (!Ext || _stricmp(Ext, LGI_LIBRARY_EXT))
		{
			ch += sprintf_s(f+ch, sizeof(f)-ch, ".%s", LGI_LIBRARY_EXT);
		}

		size_t Len = strlen(f) + 32;
		FileName = new char[Len];
		if (FileName)
		{
			memset(FileName, 0, Len);
			strcpy_s(FileName, Len, f); // FileName is always longer then 'f'
			
			#if defined WIN32

			SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

			GAutoWString w(Utf8ToWide(f));
			if (w)
			{
				hLib = LoadLibraryW(w);
				#if defined(_DEBUG) && DEBUG_LIB_MSGS
				if (!hLib)
					LgiTrace("LoadLibraryW(%S) failed with %i\n", w.Get(), GetLastError());
				#endif
			}
			else LgiTrace("%s:%i - Failed to convert string to wide.\n", _FL);

			SetErrorMode(0);


			#elif defined(LINUX) || defined(MAC) || defined(BEOS)
			
			char *f = strrchr(FileName, DIR_CHAR);
			if (f) f++;
			else f = FileName;
			bool IsLgi = stricmp(f, "lgilgi." LGI_LIBRARY_EXT) == 0 ||
						 stricmp(f, "lgilgid." LGI_LIBRARY_EXT) == 0;			
			if (!IsLgi) // Bad things happen when we load LGI again.
			{
				hLib = dlopen(FileName, RTLD_NOW);
				#if DEBUG_LIB_MSGS
				LgiTrace("%s:%i - dlopen('%s') = %p\n", _FL, FileName, hLib);
				#endif
				
				#ifdef LINUX
				if (!hLib)
				{
					// Try with an extra ".0"... just for fun.
					char *e = FileName + strlen(FileName);
					strcpy(e, ".0");
					hLib = dlopen(FileName, RTLD_NOW);
					#if DEBUG_LIB_MSGS
					LgiTrace("%s:%i - dlopen('%s') = %p\n", _FL, FileName, hLib);
					#endif
					*e = 0;
				}
				#endif	
				
				if (hLib)
				{
					#if 0 // defined(LINUX) && defined(__USE_GNU)
					char Path[MAX_PATH];
					int r = dlinfo(hLib, RTLD_DI_ORIGIN, Path);
					if (r == 0)
					{
						LgiMakePath(Path, sizeof(Path), Path, File);
						printf("GLibrary loaded: '%s'\n", Path);
					}
					else printf("%s:%i - dlinfo failed.\n", _FL);
					#endif
				}
				else
				{
					char *e = dlerror();
					if (!stristr(e, "No such file or directory") && !Quiet)
						LgiTrace("%s:%i - dlopen(%s) failed: %s\n", _FL, File, e);

					#if DEBUG_LIB_MSGS
					LgiTrace("%s:%i - dlerror='%s'\n", _FL, e);
					#endif

					#ifdef BEOS
					GToken t("/boot/system/develop/lib/x86", ":");
					#else
					GToken t("/opt/local/lib", ":");
					#endif
					for (int i=0; i<t.Length(); i++)
					{
						char full[MAX_PATH];
						LgiMakePath(full, sizeof(full), t[i], f);
						if (FileExists(full))
						{
							hLib = dlopen(full, RTLD_NOW);
							if (!Quiet)
								LgiTrace("%s:%i - dlopen(%s)=%p\n", _FL, full, hLib);
							if (hLib)
								break;
						}
						else if (!Quiet)
						{
							LgiTrace("%s doesn't exist\n", full);
						}

					}

					if (!hLib && !Quiet)
					{
						char *e = dlerror();
						if (!stristr(e, "No such file or directory"))
							LgiTrace("%s:%i - dlopen(%s) failed: %s\n", _FL, File, e);
					}
				}
			}

			#else

			LgiAssert(0);

			#endif
		}
	}

	return hLib != 0;
}

bool GLibrary::Unload()
{
	if (hLib)
	{
		#if defined WIN32
		FreeLibrary(hLib);
		#elif defined(LINUX) || defined(MAC) || defined(BEOS)
		dlclose(hLib);
		#else
		LgiAssert(0);
		#endif
		hLib = 0;
	}

	DeleteArray(FileName);

	return true;
}

void *GLibrary::GetAddress(const char *Resource)
{
	void *p = NULL;
	
	if (Resource && hLib)
	{
		#if defined WIN32
		p = (void*) GetProcAddress(hLib, Resource);
		#elif defined(LINUX) || defined(MAC) || defined(BEOS)
		p = dlsym(hLib, Resource);
		#else
		LgiAssert(0);
		#endif

		#if DEBUG_LIB_MSGS
		if (!p)
			LgiTrace("%s:%i - GetAddress('%s')=%p\n", _FL, Resource, p);
		#endif
	}

	return p;
}
