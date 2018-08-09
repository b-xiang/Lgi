#ifndef _DOC_EDIT_H_
#define _DOC_EDIT_H_

#include "GTextView3.h"
#include "IdeDoc.h"

#define IDM_FILE_COMMENT			100
#define IDM_FUNC_COMMENT			101

enum SourceType
{
	SrcUnknown,
	SrcPlainText,
	SrcCpp,
	SrcPython,
	SrcXml,
	SrcHtml,
};

class DocEdit :
	public GTextView3,
	public GDocumentEnv,
	public LThread
{
	IdeDoc *Doc;
	int CurLine;
	GdcPt2 MsClick;
	SourceType FileType;

	enum WordType
	{
		KNone,
		KLang,
		KType
	};

	enum ThreadState
	{
		KWaiting,
		KStyling,
		KCancel,
		KExiting,
	};

	struct Node
	{
		Node *Next[26 + 10 + 1];
		WordType Type;

		int Map(char16 c)
		{
			if (IsAlpha(c))
				return ToLower(c) - 'a';
			if (IsDigit(c))
				return c - '0' + 26;
			if (c == '_')
				return 26+10;
			// LgiAssert(0);
			return -1;
		}

		Node()
		{
			ZeroObj(Next);
			Type = KNone;
		}

		~Node()
		{
			for (int i=0; i<CountOf(Next); i++)
				DeleteObj(Next[i]);
		}
	};

	struct StylingParams
	{
		GTextView3 *View;
		size_t PourStart;
		ssize_t PourSize;
		GArray<char16> Text;
		GArray<GStyle> Styles;
		GRect Dirty;

		StylingParams(GTextView3 *view)
		{
			View = view;
		}

		void StyleString(char16 *&s, char16 *e, GColour c)
		{
			auto st = Styles.New().Construct(View, STYLE_IDE);
			st.Start = s - Text.AddressOf();
			st.Font = View->GetFont();

			char16 Delim = *s++;
			while (s < e && *s != Delim)
			{
				if (*s == '\\')
					s++;
				s++;
			}

			st.Len = (s - Text.AddressOf()) - st.Start + 1;
			st.Fore = c;
		}
	};

	// Styling data...
	LThreadEvent Event;
	ThreadState ParentState, WorkerState;
	// Lock before access:
		StylingParams Params;
	// EndLock
	// Thread only
		Node Root;
		GArray<GStyle> PrevStyle;
	// End Thread only

	// Styling functions..
	int Main();
	void OnApplyStyles();
	void StyleCpp(StylingParams &p);
	void StylePython(StylingParams &p);
	void StyleDefault(StylingParams &p);
	void StyleXml(StylingParams &p);
	void StyleHtml(StylingParams &p);
	void AddKeywords(const char **keys, bool IsType);

	// Full refresh triggers
	int RefreshSize;
	const char **RefreshEdges;

	int CountRefreshEdges(size_t At, ssize_t Len);

public:
	static int LeftMarginPx;
	enum HtmlType
	{
		CodeHtml,
		CodePhp,
		CodeCss,
		CodeComment,
		CodePre,
	};

	DocEdit(IdeDoc *d, GFontType *f);
	~DocEdit();

	char *Name() { return GTextView3::Name(); }
	bool Name(const char *s) { return GTextView3::Name(s); }
	bool SetPourEnabled(bool b);
	int GetTopPaddingPx();
	void InvalidateLine(int Idx);
	char *TemplateMerge(const char *Template, char *Name, List<char> *Params);
	GColour ColourFromType(HtmlType t);
	bool GetVisible(GStyle &s);

	// Overrides
	bool AppendItems(GSubMenu *Menu, int Base) override;
	bool DoGoto() override;
	void OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour) override;
	void OnMouseClick(GMouse &m) override;
	bool OnKey(GKey &k) override;	
	bool OnMenu(GDocView *View, int Id, void *Context) override;
	GMessage::Result OnEvent(GMessage *m) override;
	void SetCaret(size_t i, bool Select, bool ForceFullUpdate = false) override;
	void PourStyle(size_t Start, ssize_t EditSize) override;
	bool Pour(GRegion &r) override;
	bool Insert(size_t At, char16 *Data, ssize_t Len) override;
	bool Delete(size_t At, ssize_t Len) override;
};

#endif