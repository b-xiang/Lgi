#ifndef _GHTMLSTATIC_H_
#define _GHTMLSTATIC_H_

#include "GCss.h"
#include "GHashTable.h"

extern char16 GHtmlListItem[];

#define SkipWhiteSpace(s)			while (*s && IsWhiteSpace(*s)) s++;

enum HtmlTag
{
	CONTENT,
	CONDITIONAL,
	ROOT,
	TAG_UNKNOWN,
	TAG_HTML,
	TAG_HEAD,
	TAG_BODY,
	TAG_B,
	TAG_I,
	TAG_U,
	TAG_P,
	TAG_BR,
	TAG_UL,
	TAG_OL,
	TAG_LI,
	TAG_FONT,
	TAG_A,
	TAG_TABLE,
		TAG_TR,
		TAG_TD,
	TAG_IMG,
	TAG_DIV,
	TAG_SPAN,
	TAG_CENTER,
	TAG_META,
	TAG_TBODY,
	TAG_STYLE,
	TAG_SCRIPT,
	TAG_STRONG,
	TAG_BLOCKQUOTE,
	TAG_PRE,
	TAG_H1,
	TAG_H2,
	TAG_H3,
	TAG_H4,
	TAG_H5,
	TAG_H6,
	TAG_HR,
	TAG_IFRAME,
	TAG_LINK,
	TAG_BIG,
	TAG_INPUT,
	TAG_SELECT,
	TAG_LABEL,
	TAG_FORM,
	TAG_NOSCRIPT,
	TAG_LAST
};

/// Common element info
struct GHtmlElemInfo
{
public:
	enum InfoFlags
	{
		TI_NONE			= 0x00,
		TI_NEVER_CLOSES	= 0x01,
		TI_NO_TEXT		= 0x02,
		TI_BLOCK		= 0x04,
		TI_TABLE		= 0x08,
	};

	HtmlTag Id;
	const char *Tag;
	const char *ReattachTo;
	int Flags;

	bool NeverCloses()	{ return TestFlag(Flags, TI_NEVER_CLOSES); }
	bool NoText()		{ return TestFlag(Flags, TI_NO_TEXT); }
	bool Block()		{ return TestFlag(Flags, TI_BLOCK); }
};

/// Common data for HTML related classes
class GHtmlStatic
{
	friend class GHtmlStaticInst;

public:
	static GHtmlStatic *Inst;

	int Refs;
	GHashTbl<char16*,int>				 VarMap;
	GHashTbl<const char*,GCss::PropType> StyleMap;
	GHashTbl<const char*,int>			 ColourMap;
	GHashTbl<const char*,GHtmlElemInfo*> TagMap;

	GHtmlStatic();
	~GHtmlStatic();
};

/// Static data setup/pulldown
class GHtmlStaticInst
{
public:
	GHtmlStatic *Static;

	GHtmlStaticInst()
	{
		if (!GHtmlStatic::Inst)
		{
			GHtmlStatic::Inst = new GHtmlStatic;
		}
		if (GHtmlStatic::Inst)
		{
			GHtmlStatic::Inst->Refs++;
		}
		Static = GHtmlStatic::Inst;
	}

	~GHtmlStaticInst()
	{
		if (GHtmlStatic::Inst)
		{
			GHtmlStatic::Inst->Refs--;
			if (GHtmlStatic::Inst->Refs == 0)
			{
				DeleteObj(GHtmlStatic::Inst);
			}
		}
	}
};

/// Common base class for a HTML element
class GHtmlElement : public GDom, public GCss
{
	friend class GHtmlParser;
	uint8 WasClosed : 1;

public:
	GAutoWString Text;
	HtmlTag TagId;
	GAutoString Tag;
	GHtmlElemInfo *Info;
	GAutoString Condition;
	
	GHtmlElement *Parent;
	GArray<GHtmlElement*> Children;
	
	GHtmlElement(GHtmlElement *parent)
	{
		TagId = CONTENT;
		Parent = parent;
		Info = NULL;
		WasClosed = false;
	}

	bool Attach(GHtmlElement *Child, int Idx = -1)
	{
		if (TagId == CONTENT)
		{
			LgiAssert(!"Can't nest content tags.");
			return false;
		}

		if (!Child)
		{
			LgiAssert(!"Can't insert NULL tag.");
			return false;
		}

		Child->Detach();
		Child->Parent = this;
		if (!Children.HasItem(Child))
		{
			Children.AddAt(Idx, Child);
		}

		return true;
	}

	void GHtmlElement::Detach()
	{
		if (Parent)
		{
			Parent->Children.Delete(this);
			Parent = NULL;
		}
	}
};

#endif