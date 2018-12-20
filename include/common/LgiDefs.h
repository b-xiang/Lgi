/**
	\file
	\author Matthew Allen
	\date 24/9/1999
	\brief Defines and types
	Copyright (C) 1999-2004, <a href="mailto:fret@memecode.com">Matthew Allen</a>
*/

#ifndef _LGIDEFS_H_
#define _LGIDEFS_H_

#include "LgiInc.h"

#if defined(WIN32) && defined(__GNUC__)
#define PLATFORM_MINGW
#endif

// Unsafe typedefs, for backward compatibility
typedef		unsigned char				uchar;
typedef		unsigned short				ushort;
typedef		unsigned int				uint;
typedef		unsigned long				ulong;

// Length safe typedesf, use these in new code

#ifndef BEOS
	/// 8-bit signed int type (size safe, guaranteed to be 8 bits)
	typedef		signed char				int8;
	/// 8-bit unsigned int type (size safe, guaranteed to be 8 bits)
	typedef		unsigned char			uint8;
#else
	#include <AppKit.h>
#endif

/// 16-bit signed int type (size safe, guaranteed to be 16 bits)
typedef		short						int16;
/// 16-bit unsigned int type (size safe, guaranteed to be 16 bits)
typedef		unsigned short				uint16;

#ifndef BEOS
	/// 32-bit signed int type (size safe, guaranteed to be 32 bits)
	typedef		int						int32;
	/// 32-bit unsigned int type (size safe, guaranteed to be 32 bits)
	typedef		unsigned int			uint32;
#endif

#ifdef _MSC_VER
	/// 64-bit signed int type (size safe, guaranteed to be 64 bits)
	typedef		signed __int64				int64;
	/// 64-bit unsigned int type (size safe, guaranteed to be 64 bits)
	typedef		unsigned __int64			uint64;

	#ifndef _WCHAR_T_DEFINED
	#include <crtdefs.h>
	#endif

	#pragma warning(error:4263)

	#ifdef  _WIN64
		typedef signed __int64		ssize_t;
	#else
		typedef signed int			ssize_t;
	#endif

#else
	/// 64-bit signed int type (size safe, guaranteed to be 64 bits)
	typedef		signed long long			int64;
	/// 64-bit unsigned int type (size safe, guaranteed to be 64 bits)
	typedef		unsigned long long			uint64;
#endif

#ifndef __cplusplus

	#include <stddef.h>
	#if defined(_MSC_VER) && _MSC_VER<1800
		typedef unsigned char bool;
		#define true	1
		#define false	0
	#else
		#include <stdbool.h>
	#endif

#endif

/// \brief Wide unicode char
///
/// This is 16 bits on Win32 and Mac, but 32 bits on unix platforms. There are a number
/// of wide character string function available for manipulating wide char strings.
///
/// Firstly to convert to and from utf-8 there is:
/// <ul>
/// 	<li> Utf8ToWide()
///		<li> WideToUtf8()
/// </ul>
///
/// Wide versions of standard library functions are available:
/// <ul>
///		<li> StrchrW()
///		<li> StrrchrW()
///		<li> StrnchrW()
///		<li> StrstrW()
///		<li> StristrW()
///		<li> StrnstrW()
///		<li> StrnistrW()
///		<li> StrcmpW()
///		<li> StricmpW()
///		<li> StrncmpW()
///		<li> StrnicmpW()
///		<li> StrcpyW()
///		<li> StrncpyW()
///		<li> StrlenW()
///		<li> StrcatW()
///		<li> HtoiW()
///		<li> NewStrW()
///		<li> TrimStrW()
///		<li> ValidStrW()
///		<li> MatchStrW()
/// </ul>
#include <wchar.h>
typedef		wchar_t						char16;

#if !WINNATIVE
#ifdef UNICODE
typedef		char16						TCHAR;
#ifndef		_T
#define		_T(arg)						L##arg
#endif
#else
typedef		char						TCHAR;
#ifndef		_T
#define		_T(arg)						arg
#endif
#endif
#endif

#if defined(_MSC_VER)

	#if _MSC_VER >= 1400

		#ifdef  _WIN64
		typedef __int64             NativeInt;
		typedef unsigned __int64    UNativeInt;
		#else
		typedef _W64 int			NativeInt;
		typedef _W64 unsigned int	UNativeInt;
		#endif

	#else

		typedef int					NativeInt;
		typedef unsigned int		UNativeInt;

	#endif

#else

	#if __LP64__
		typedef int64				NativeInt;
		typedef uint64				UNativeInt;
	#else
		typedef int					NativeInt;
		typedef unsigned int		UNativeInt;
	#endif

#endif

/// Generic pointer to any base type. Used when addressing continuous data of
/// different types.
typedef union
{
	int8 *s8;
	uint8 *u8;
	int16 *s16;
	uint16 *u16;
	int32 *s32;
	uint32 *u32;
	int64 *s64;
	uint64 *u64;
	NativeInt *ni;
	UNativeInt *uni;

	char *c;
	char16 *w;
	float *f;
	double *d;
	#ifdef __cplusplus
	bool *b;
	#else
	unsigned char *b;
	#endif
	void **vp;
	int i;

}	GPointer;

// Basic macros
#define limit(i,l,u)					(((i)<(l)) ? (l) : (((i)>(u)) ? (u) : (i)))
#define makelong(a, b)					((a)<<16 | (b&0xFFFF))
#define loword(a)						(a&0xFFFF)
#define hiword(a)						(a>>16)
#define LgiSwap(a, b)					{ int n = a; a = b; b = n; }

#undef ABS
#ifdef __cplusplus
template<typename T>
inline T ABS(T v)
{
	if (v < 0) return -v;
	return v;
}
#else
#define ABS(v) ((v) < 0 ? -(v) : (v))
#endif

/// Returns true if 'c' is an ascii character
#define IsAlpha(c)					    (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))

/// Returns true if 'c' is a digit (number)
#define IsDigit(c)					    ((c) >= '0' && (c) <= '9')

/// Returns true if 'c' is a hexadecimal digit
#define IsHexDigit(c)					( \
											((c) >= '0' && (c) <= '9') || \
											((c) >= 'a' && (c) <= 'f') || \
											((c) >= 'A' && (c) <= 'F') \
										)

// Byte swapping
#define LgiSwap16(a)					( (((a) & 0xff00) >> 8) | \
										  (((a) & 0x00ff) << 8) )

#define LgiSwap32(a)					( (((a) & 0xff000000) >> 24) | \
										  (((a) & 0x00ff0000) >> 8)  | \
										  (((a) & 0x0000ff00) << 8)  | \
										  (((a) & 0x000000ff) << 24) )

#ifdef __GNUC__
#define LgiSwap64(a)					( (((a) & 0xff00000000000000LLU) >> 56) | \
										  (((a) & 0x00ff000000000000LLU) >> 40) | \
										  (((a) & 0x0000ff0000000000LLU) >> 24) | \
										  (((a) & 0x000000ff00000000LLU) >> 8)  | \
										  (((a) & 0x00000000ff000000LLU) << 8)  | \
										  (((a) & 0x0000000000ff0000LLU) << 24) | \
										  (((a) & 0x000000000000ff00LLU) << 40) | \
										  (((a) & 0x00000000000000ffLLU) << 56) )
#else
#define LgiSwap64(a)					( (((a) & 0xff00000000000000) >> 56) | \
										  (((a) & 0x00ff000000000000) >> 40) | \
										  (((a) & 0x0000ff0000000000) >> 24) | \
										  (((a) & 0x000000ff00000000) >> 8)  | \
										  (((a) & 0x00000000ff000000) << 8)  | \
										  (((a) & 0x0000000000ff0000) << 24) | \
										  (((a) & 0x000000000000ff00) << 40) | \
										  (((a) & 0x00000000000000ff) << 56) )
#endif

// Asserts
LgiFunc void							_lgi_assert(bool b, const char *test, const char *file, int line);
#ifdef _DEBUG
#define LgiAssert(b)					_lgi_assert(b, #b, __FILE__, __LINE__)
#else
#define LgiAssert(b)					while (0)
#endif


// Good ol NULLy
#ifndef NULL
#define NULL							0
#endif

// Slashes and quotes
#define IsSlash(c)						(((c)=='/')||((c)=='\\'))
#define IsQuote(c)						(((c)=='\"')||((c)=='\''))

// Some objectish ones
#define ZeroObj(obj)					memset(&obj, 0, sizeof(obj))
#ifndef CountOf
#define CountOf(array)					(sizeof(array)/sizeof(array[0]))
#endif

#ifndef MEMORY_DEBUG

	#define DeleteObj(obj)				if (obj) { delete obj; obj = 0; }
	#define DeleteArray(obj)			if (obj) { delete [] obj; obj = 0; }

#endif

// Flags
#define SetFlag(i, f)					(i) |= (f)
#define ClearFlag(i, f)					(i) &= ~(f)
#define TestFlag(i, f)					(((i) & (f)) != 0)

// Defines
/// Enum of all the operating systems we might be running on.
enum LgiOs
{
	/// \brief Unknown OS
	/// \sa LgiGetOs
	LGI_OS_UNKNOWN = 0,

	/// \brief Windows 95, 98[se] or ME. (Not supported)
	/// \sa LgiGetOs
	LGI_OS_WIN9X,

	/// \brief 32bit NT, 2k, XP, Vista, 7, 8 or later. (XP and later supported)
	/// \sa LgiGetOs
	LGI_OS_WIN32,

	/// \brief 64bit NT, 2k, XP, Vista, 7, 8 or later. (XP and later supported)
	/// \sa LgiGetOs
	LGI_OS_WIN64,

	/// \brief BeOS/Haiku. (Somewhat supported)
	/// \sa LgiGetOs
	LGI_OS_HAIKU,

	/// \brief Linux. (Kernels v2.4 and up supported)
	/// \sa LgiGetOs
	LGI_OS_LINUX,

	/// \brief There was an Atheos port at one point. (Not supported)
	/// \sa LgiGetOs
	LGI_OS_MAC_OS_X,

	/// One higher than the maximum OS define
	LGI_OS_MAX,
};

// System Colours

/// Black
#define LC_BLACK						LgiColour(0)
/// Dark grey
#define LC_DKGREY						LgiColour(1)
/// Medium grey
#define LC_MIDGREY						LgiColour(2)
/// Light grey
#define LC_LTGREY						LgiColour(3)
/// White
#define LC_WHITE						LgiColour(4)

/// 3d dark shadow
#define LC_SHADOW						LgiColour(5)
/// 3d light shadow
#define LC_LOW							LgiColour(6)
/// Flat colour for dialogs, windows and buttons
#define LC_MED							LgiColour(7)
/// 3d dark hilight
#define LC_HIGH							LgiColour(8)
/// 3d light hilight
#define LC_LIGHT						LgiColour(9)

/// Dialog colour
#define LC_DIALOG						LgiColour(10)
/// Workspace area
#define LC_WORKSPACE					LgiColour(11)
/// Default text colour
#define LC_TEXT							LgiColour(12)
/// Selection background colour when in focus
#define LC_FOCUS_SEL_BACK				LgiColour(13)
/// Selection foreground colour when in focus
#define LC_FOCUS_SEL_FORE				LgiColour(14)

#define LC_ACTIVE_TITLE					LgiColour(15)
#define LC_ACTIVE_TITLE_TEXT			LgiColour(16)
#define LC_INACTIVE_TITLE				LgiColour(17)
#define LC_INACTIVE_TITLE_TEXT			LgiColour(18)

#define LC_MENU_BACKGROUND				LgiColour(19)
#define LC_MENU_TEXT					LgiColour(20)

/// Selection background colour when not in focus
#define LC_NON_FOCUS_SEL_BACK			LgiColour(21)
/// Selection forground colour when not in focus
#define LC_NON_FOCUS_SEL_FORE			LgiColour(22)

#define LC_DEBUG_CURRENT_LINE			LgiColour(23)

#define LC_MAXIMUM						24

#define LC_TOOL_TIP						Rgb24(255, 255, 231)

// Edge types
enum LgiEdge
{
	EdgeNone,
	EdgeXpSunken,
	EdgeXpRaised,
	EdgeXpChisel,
	EdgeXpFlat,
	EdgeWin7FocusSunken,
	EdgeWin7Sunken,
};

#define DefaultSunkenEdge			EdgeWin7Sunken
#define DefaultRaisedEdge			EdgeXpRaised

// Cursors
enum LgiCursor
{
	/// Blank/invisible cursor
	LCUR_Blank,
	/// Normal arrow
	LCUR_Normal,
	/// Upwards arrow
	LCUR_UpArrow,
	/// Downwards arrow
	LCUR_DownArrow,
	/// Left arrow
	LCUR_LeftArrow,
	/// Right arrow
	LCUR_RightArrow,
	/// Crosshair
	LCUR_Cross,
	/// Hourglass/watch
	LCUR_Wait,
	/// Ibeam/text entry
	LCUR_Ibeam,
	/// Vertical resize (|)
	LCUR_SizeVer,
	/// Horizontal resize (-)
	LCUR_SizeHor,
	/// Diagonal resize (/)
	LCUR_SizeBDiag,
	/// Diagonal resize (\)
	LCUR_SizeFDiag,
	/// All directions resize
	LCUR_SizeAll,
	/// Vertical splitting
	LCUR_SplitV,
	/// Horziontal splitting
	LCUR_SplitH,
	/// A pointing hand
	LCUR_PointingHand,
	/// A slashed circle
	LCUR_Forbidden,
	/// Copy Drop
	LCUR_DropCopy,
	/// Copy Move
	LCUR_DropMove,
};

// General Event Flags
#define LGI_EF_LCTRL					0x00000001
#define LGI_EF_RCTRL					0x00000002
#define LGI_EF_CTRL						(LGI_EF_LCTRL | LGI_EF_RCTRL)

#define LGI_EF_LALT						0x00000004
#define LGI_EF_RALT						0x00000008
#define LGI_EF_ALT						(LGI_EF_LALT | LGI_EF_RALT)

#define LGI_EF_LSHIFT					0x00000010
#define LGI_EF_RSHIFT					0x00000020
#define LGI_EF_SHIFT					(LGI_EF_LSHIFT | LGI_EF_RSHIFT)

#define LGI_EF_DOWN						0x00000040
#define LGI_EF_DOUBLE					0x00000080
#define LGI_EF_CAPS_LOCK				0x00000100
#define LGI_EF_IS_CHAR					0x00000200
#define LGI_EF_IS_NOT_CHAR				0x00000400
#define LGI_EF_SYSTEM					0x00000800 // Windows key/Apple key etc

// Mouse Event Flags
#define LGI_EF_LEFT						0x00001000
#define LGI_EF_MIDDLE					0x00002000
#define LGI_EF_RIGHT					0x00004000
#define LGI_EF_MOVE						0x00008000

// Emit compiler warnings
#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#define __LOC__ __FILE__ "("__STR1__(__LINE__)") : Warning: "
// To use just do #pragma message(__LOC__"My warning message")

// Simple definition of breakable unicode characters
#define LGI_BreakableChar(c)			(											\
											(c) == '\n' ||							\
											(c) == ' ' ||							\
											(c) == '\t' ||							\
											( (c) >= 0x3040 && (c) <= 0x30FF ) ||	\
											( (c) >= 0x3300 && (c) <= 0x9FAF )		\
										)

// Os metrics

enum LgiSystemMetric
{
	/// Get the standard window horizontal border size
	/// \sa GApp::GetMetric()
	LGI_MET_DECOR_X = 1,
	/// Get the standard window vertical border size including caption bar.
	/// \sa GApp::GetMetric()
	LGI_MET_DECOR_Y,
	/// Get the standard window caption bar height only.
	/// \sa GApp::GetMetric()
	LGI_MET_DECOR_CAPTION,
	/// Get the height of a single line menu bar
	/// \sa GApp::GetMetric()
	LGI_MET_MENU,
	/// This is non-zero if the system is theme aware
	LGI_MET_THEME_AWARE
};

/// \brief Types of system paths available for querying
/// \sa LgiGetSystemPath
enum LgiSystemPath
{
	LSP_ROOT,
	/// The location of the operating system folder
	///		[Win32] = e.g. C:\Windows
	///		[Mac] = /System
	///		[Linux] /boot
	LSP_OS,

	/// The system library folder
	///		[Win32] = e.g. C:\Windows\System32
	///		[Mac] = /Library
	///		[Linux] = /usr/lib
	LSP_OS_LIB,

	/// A folder for storing temporary files. These files are usually
	/// deleted automatically later by the system.
	///		[Win32] = ~\Local Settings\Temp
	///		[Mac] = ~/Library/Caches/TemporaryItems
	///		[Linux] = /tmp
	LSP_TEMP,

	/// System wide application data
	///		[Win32] = ~\..\All Users\Application Data
	///		[Mac] = /System/Library
	///		[Linux] = /usr
	LSP_COMMON_APP_DATA,

	/// User specific application data
	///		[Win32] = ~\Application Data
	///		[Mac] = ~/Library
	///		[Linux] = /usr
	LSP_USER_APP_DATA,

	/// Machine + user specific application data (probably should not use)
	///		[Win32] = ~\Local Settings\Application Data
	///		[Mac] = ~/Library
	///		[Linux] = /usr/local
	LSP_LOCAL_APP_DATA,

	/// Desktop dir
	///		i.e. ~/Desktop
	LSP_DESKTOP,

	/// Home dir
	///		i.e. ~
	LSP_HOME,

	/// Application install folder:
	///		[Win]   c:\Program Files
	///		[Mac]   /Applications
	///		[Linux] /usr/bin
	LSP_USER_APPS,

	/// The running application's path.
	/// [Mac] This doesn't include the "Contents/MacOS" part of the path inside the bundle.
	LSP_EXE,

	/// The system trash folder.
	LSP_TRASH,

	/// The app's install folder.
	///		[Win32] = $ExePath (sans '\Release' or '\Debug')
	///		[Mac/Linux] = $ExePath
	/// Where $ExePath is the folder the application is running from
	LSP_APP_INSTALL,

	/// The app's root folder (Where config and data should be stored)
	/// GOptionsFile uses LSP_APP_ROOT as the default location.
	///		[Win32] = ~\Application Data\Roaming\$AppName
	///		[Mac] = ~/Library/$AppName
	///		[Linux] = ~/.$AppName
	/// Where $AppName = GApp::GetName.
	/// If the given folder doesn't exist it will be created.
	LSP_APP_ROOT,

	/// This is the user's documents folder
	///		[Win32] ~\My Documents
	///		[Mac] ~\Documents
	LSP_USER_DOCUMENTS,

	/// This is the user's music folder
	///		[Win32] ~\My Music
	///		[Mac] ~\Music
	LSP_USER_MUSIC,

	/// This is the user's video folder
	///		[Win32] ~\My Videos
	///		[Mac] ~\Movies
	LSP_USER_VIDEO,

	/// This is the user's download folder
	///		~\Downloads
	LSP_USER_DOWNLOADS,

	/// This is the user's links folder
	///		[Win32] = ~\Links
	///		[Mac]   = ???
	///		[Linux] = ???
	LSP_USER_LINKS,

	/// User's pictures/photos folder
	LSP_USER_PICTURES,
	///		[Win32] = C:\Users\%HOME%\Pictures
	///		[Mac]   = ???
	///		[Linux] = ~\Pictures
};

// Deprecated method defines
#if defined(__GNUC__) || defined(__clang__)
	#define DEPRECATED_PRE
	#define DEPRECATED_POST	__attribute__((deprecated))
#elif defined(_MSC_VER)
	#define DEPRECATED_PRE	__declspec(deprecated)
	#define DEPRECATED_POST
#else
	#pragma message("WARNING: You need to implement DEPRECATED for this compiler")
	#define DEPRECATED_PRE
	#define DEPRECATED_POST
#endif

//
#ifdef _DEBUG
	#define DeclDebugArgs				, const char *_file, int _line
	#define PassDebugArgs				, __FILE__, __LINE__
#else
	#define DeclDebugArgs
	#define PassDebugArgs
#endif

#define _FL							__FILE__, __LINE__

#define CALL_MEMBER_FN(obj, memFn)	((obj).*(memFn)) 

#include "GAutoPtr.h"

#endif
