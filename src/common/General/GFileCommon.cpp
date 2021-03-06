//
//  GFileCommon.cpp
//
//  Created by Matthew Allen on 4/05/14.
//  Copyright (c) 2014 Memecode. All rights reserved.
//

#include "Lgi.h"
#include "GVariant.h"

/////////////////////////////////////////////////////////////////////////////////
bool GFileSystem::SetCurrentFolder(const char *PathName)
{
	#ifdef WINDOWS
		bool Status = false;
		GAutoWString w(Utf8ToWide(PathName));
		if (w)
			Status = ::SetCurrentDirectoryW(w) != 0;
		return Status;
	#else
		return chdir(PathName) == 0;
	#endif
}

GString GFileSystem::GetCurrentFolder()
{
	GString Cwd;

	#ifdef WINDOWS
		char16 w[DIR_PATH_SIZE+1];
		if (::GetCurrentDirectoryW(DIR_PATH_SIZE, w) > 0)
			Cwd = w;
	#else
	char p[MAX_PATH];
	if (getcwd(p, sizeof(p)))
		Cwd = p;
	#endif

	return Cwd;
}

/////////////////////////////////////////////////////////////////////////////////
bool GFile::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = LgiStringToDomProp(Name);
	switch (p)
	{
		case ObjType: // Type: String
			Value = "File";
			break;
		case ObjName: // Type: String
			Value = GetName();
			break;
		case ObjLength: // Type: Int64
			Value = GetSize();
			break;
		case FilePos: // Type: Int64
			Value = GetPos();
			break;
		default:
			return false;
	}
	
	return true;
}

bool GFile::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = LgiStringToDomProp(Name);
	switch (p)
	{
		case ObjLength:
			SetSize(Value.CastInt64());
			break;
		case FilePos:
			SetPos(Value.CastInt64());
			break;
		default:
			return false;
	}
	
	return true;
}

bool GFile::CallMethod(const char *Name, GVariant *Dst, GArray<GVariant*> &Arg)
{
	GDomProperty p = LgiStringToDomProp(Name);
	switch (p)
	{
		case ObjLength: // Type: ([NewLength])
		{
			if (Arg.Length() == 1)
				*Dst = SetSize(Arg[0]->CastInt64());
			else
				*Dst = GetSize();
			break;
		}
		case FilePos: // Type: ([NewPosition])
		{
			if (Arg.Length() == 1)
				*Dst = SetPos(Arg[0]->CastInt64());
			else
				*Dst = GetPos();
			break;
		}
		case ObjType:
		{
			*Dst = "File";
			break;
		}
		case FileOpen: // Type: (Path[, Mode])
		{
			if (Arg.Length() >= 1)
			{
				int Mode = O_READ;
				if (Arg.Length() == 2)
				{
					char *m = Arg[1]->CastString();
					if (m)
					{
						bool Rd = strchr(m, 'r') != NULL;
						bool Wr = strchr(m, 'w') != NULL;
						if (Rd && Wr)
							Mode = O_READWRITE;
						else if (Wr)
							Mode = O_WRITE;
						else
							Mode = O_READ;
					}
				}
				
				*Dst = Open(Arg[0]->CastString(), Mode);
			}
			break;
		}
		case FileClose:
		{
			*Dst = Close();
			break;
		}
		case FileRead: // Type: ([ReadLength[, ReadType = 0 - string, 1 - integer]])
		{
			int64 RdLen = 0;
			int RdType = 0; // 0 - string, 1 - int
			
			Dst->Empty();
			
			switch (Arg.Length())
			{
				default:
				case 0:
					RdLen = GetSize() - GetPos();
					break;
				case 2:
					RdType = Arg[1]->CastInt32();
					// fall thru
				case 1:
					RdLen = Arg[0]->CastInt64();
					break;
			}
			
			if (RdType)
			{
				// Int type
				switch (RdLen)
				{
					case 1:
					{
						uint8 i;
						if (Read(&i, sizeof(i)) == sizeof(i))
							*Dst = i;
						break;
					}
					case 2:
					{
						uint16 i;
						if (Read(&i, sizeof(i)) == sizeof(i))
							*Dst = i;
						break;
					}
					case 4:
					{
						uint32 i;
						if (Read(&i, sizeof(i)) == sizeof(i))
							*Dst = (int)i;
						break;
					}
					case 8:
					{
						int64 i;
						if (Read(&i, sizeof(i)) == sizeof(i))
							*Dst = i;
						break;
					}
				}
			}
			else if (RdLen > 0)
			{
				// String type
				if ((Dst->Value.String = new char[RdLen + 1]))
				{
					ssize_t r = Read(Dst->Value.String, (int)RdLen);
					if (r > 0)
					{
						Dst->Type = GV_STRING;
						Dst->Value.String[r] = 0;
					}
					else
					{
						DeleteArray(Dst->Value.String);
					}
				}
			}
			else *Dst = -1;
			break;
		}
		case FileWrite: // Type: (Data[, WriteLength])
		{
			GVariant *v;
			if (Arg.Length() < 1 ||
				Arg.Length() > 2 ||
				!(v = Arg[0]))
			{
				*Dst = 0;
				return true;
			}

			switch (Arg.Length())
			{
				case 1:
				{
					// Auto-size write length to the variable.
					switch (v->Type)
					{
						case GV_INT32:
							*Dst = Write(&v->Value.Int, sizeof(v->Value.Int));
							break;
						case GV_INT64:
							*Dst = Write(&v->Value.Int64, sizeof(v->Value.Int64));
							break;
						case GV_STRING:
							*Dst = Write(&v->Value.String, strlen(v->Value.String));
							break;
						case GV_WSTRING:
							*Dst = Write(&v->Value.WString, StrlenW(v->Value.WString) * sizeof(char16));
							break;
						default:
							*Dst = 0;
							return true;
					}
					break;
				}
				case 2:
				{
					int64 WrLen = Arg[1]->CastInt64();
					switch (v->Type)
					{
						case GV_INT32:
						{
							if (WrLen == 1)
							{
								uint8 i = v->Value.Int;
								*Dst = Write(&i, sizeof(i));
							}
							else if (WrLen == 2)
							{
								uint16 i = v->Value.Int;
								*Dst = Write(&i, sizeof(i));
							}
							else
							{
								*Dst = Write(&v->Value.Int, sizeof(v->Value.Int));
							}
							break;
						}
						case GV_INT64:
						{
							if (WrLen == 1)
							{
								uint8 i = (uint8) v->Value.Int64;
								*Dst = Write(&i, sizeof(i));
							}
							else if (WrLen == 2)
							{
								uint16 i = (uint16) v->Value.Int64;
								*Dst = Write(&i, sizeof(i));
							}
							else if (WrLen == 4)
							{
								uint32 i = (uint32)v->Value.Int64;
								*Dst = Write(&i, sizeof(i));
							}
							else
							{
								*Dst = Write(&v->Value.Int64, sizeof(v->Value.Int64));
							}
							break;
						}
						case GV_STRING:
						{
							size_t Max = strlen(v->Value.String) + 1;
							*Dst = Write(&v->Value.String, MIN(Max, (size_t)WrLen));
							break;
						}
						case GV_WSTRING:
						{
							size_t Max = (StrlenW(v->Value.WString) + 1) * sizeof(char16);
							*Dst = Write(&v->Value.WString, MIN(Max, (size_t)WrLen));
							break;
						}
						default:
						{
							*Dst = 0;
							return true;
						}
					}
					break;
				}
				default:
				{
					*Dst = 0;
					return true;
				}
			}
			break;
		}
		default:
			return false;
	}
	return true;
}

const char *LgiGetLeaf(const char *Path)
{
	if (!Path)
		return NULL;

	const char *l = NULL;
	for (const char *s = Path; *s; s++)
	{
		if (*s == '/' || *s == '\\')
			l = s;
	}

	return l ? l + 1 : Path;
}

char *LgiGetLeaf(char *Path)
{
	if (!Path)
		return NULL;

	char *l = NULL;
	for (char *s = Path; *s; s++)
	{
		if (*s == '/' || *s == '\\')
			l = s;
	}

	return l ? l + 1 : Path;
}

GString LGetPhysicalDevice(const char *Path)
{
	GString Ph;

	#ifdef WINDOWS
	GAutoWString w(Utf8ToWide(Path));
	char16 VolPath[256];
	if (GetVolumePathNameW(w, VolPath, CountOf(VolPath)))
	{
		char16 Name[256] = L"", FsName[256] = L"";
		DWORD VolumeSerialNumber = 0, MaximumComponentLength = 0, FileSystemFlags = 0;
		if (GetVolumeInformationW(VolPath, Name, CountOf(Name), &VolumeSerialNumber, &MaximumComponentLength, &FileSystemFlags, FsName, CountOf(FsName)))
		{
			if (VolumeSerialNumber)
				Ph.Printf("/volume/%x", VolumeSerialNumber);
			else
				Ph = VolPath;
		}
	}
	#else
	struct stat s;
	ZeroObj(s);
	if (!lstat(Path, &s))
	{
		Ph.Printf("/dev/%i", s.st_dev);
		// Find dev in '/proc/partitions'?
	}
	#endif

	return Ph;
}

