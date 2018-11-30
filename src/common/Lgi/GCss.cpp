#include <ctype.h>
#include "Lgi.h"
#include "LgiCommon.h"

#define DEBUG_CSS_LOGGING		0

#define IsWhite(s)				((s) && strchr(WhiteSpace, (s)) != NULL)
#define SkipWhite(s) 			while (IsWhite(*s)) s++;

#undef IsNumeric
#define IsNumeric(s) \
	( \
		(*(s)) && \
		( \
			strchr("-.", *(s)) || \
			(*(s) >= '0' && *(s) <= '9') \
		) \
	)

#undef IsAlpha
#define IsAlpha(s) \
	( \
		((s) >= 'a' && (s) <= 'z') || \
		((s) >= 'A' && (s) <= 'Z') || \
		(((uint8)s) > 0xa0) \
	)

#define CopyPropOnSave(Type, Id) \
	{ \
		Type *e = (Type*)Props.Find(Id); \
		if (e) *e = *(t); \
		else Props.Add(Id, new Type(*(t))); \
	}
#define ReleasePropOnSave(Type, Id) \
	{ \
		Type *e = (Type*)Props.Find(Id); \
		if (e) *e = *(t); \
		else Props.Add(Id, (t).Release()); \
	}


LHashTbl<ConstStrKey<char,false>, GCss::PropType> GCss::Lut;
LHashTbl<IntKey<int>, GCss::PropType> GCss::ParentProp;

const char *GCss::PropName(PropType p)
{
	// const char *s;
	// for (PropType t = Lut.First(&s); t; t = Lut.Next(&s))
	for (auto i : Lut)
	{
		if (p == i.value)
			return i.key;
	}

	LgiAssert(!"Add this field to the LUT");
	return 0;
}

double GCss::FontSizeTable[7] =
{
	0.6, // SizeXXSmall
	0.75, // SizeXSmall
	0.85, // SizeSmall
	1.0, // SizeMedium
	1.2, // SizeLarge
	1.5, // SizeXLarge
	2.0, // SizeXXLarge
};

/////////////////////////////////////////////////////////////////////////////
static bool ParseWord(const char *&s, const char *word)
{
	const char *doc = s;
	while (*doc && *word)
	{
		if (tolower(*doc) == tolower(*word))
		{
			doc++;
			word++;
		}
		else return false;
	}
	
	if (*word)
		return false;

	if (*doc && (IsAlpha(*doc) || IsDigit(*doc)))
		return false;

	s = doc;
	return true;
}

bool ParseProp(char *&s, char *w)
{
	char *doc = s;
	char *word = w;
	while (*doc && *word)
	{
		if (tolower(*doc) == tolower(*word))
		{
			doc++;
			word++;
		}
		else return false;
	}
	
	if (*word)
		return false;

	SkipWhite(doc);
	if (*doc != ':')
		return false;

	s = doc + 1;
	return true;
}

static int ParseComponent(const char *&s)
{
	int ret = 0;

	SkipWhite(s);

	if (!strnicmp(s, "0x", 2))
	{
		ret = htoi(s);
		while (*s && (IsDigit(*s) || *s == 'x' || *s == 'X')) s++;
	}
	else
	{
		ret = atoi(s);
		while (*s && (IsDigit(*s) || *s == '.' || *s == '-')) s++;

		SkipWhite(s);
		if (*s == '%')
		{
			s++;
			ret = ret * 255 / 100;
		}
	}

	SkipWhite(s);

	return ret;
}

static char *ParseString(const char *&s)
{
	char *ret = 0;

	if (*s == '\'' || *s == '\"')
	{
		char delim = *s++;
		char *e = strchr((char*)s, delim);
		if (!e)
			return 0;

		ret = NewStr(s, e - s);
		s = e + 1;
	}
	else
	{
		const char *e = s;
		while (*e && (IsAlpha(*e) || IsDigit(*e) || strchr("_-", *e)))
			e++;

		ret = NewStr(s, e - s);
		s = e;
	}

	return ret;
}

/////////////////////////////////////////////////////////////////////////////
GCss::GCss() : Props(32)
{
	if (Lut.Length() == 0)
	{
		Lut.Add("letter-spacing", PropLetterSpacing);
		Lut.Add("word-wrap", PropWordWrap);
		Lut.Add("list-style", PropListStyle);
		Lut.Add("list-style-type", PropListStyleType);
		Lut.Add("text-align", PropTextAlign);
		Lut.Add("text-decoration", PropTextDecoration);
		Lut.Add("display", PropDisplay);
		Lut.Add("float", PropFloat);
		Lut.Add("position", PropPosition);
		Lut.Add("overflow", PropOverflow);
		Lut.Add("visibility", PropVisibility);
		Lut.Add("font", PropFont);
		Lut.Add("font-size", PropFontSize);
		Lut.Add("fontsize", PropFontSize); // Really, do we need this in the real world??
		Lut.Add("font-style", PropFontStyle);
		Lut.Add("font-variant", PropFontVariant);
		Lut.Add("font-weight", PropFontWeight);

		Lut.Add("z-index", PropZIndex);
		Lut.Add("width", PropWidth);
		Lut.Add("min-width", PropMinWidth);
		Lut.Add("max-width", PropMaxWidth);
		Lut.Add("height", PropHeight);
		Lut.Add("min-height", PropMinHeight);
		Lut.Add("max-height", PropMaxHeight);
		Lut.Add("top", PropTop);
		Lut.Add("right", PropRight);
		Lut.Add("bottom", PropBottom);
		Lut.Add("left", PropLeft);
		Lut.Add("margin", PropMargin);
		Lut.Add("margin-top", PropMarginTop);
		Lut.Add("margin-right", PropMarginRight);
		Lut.Add("margin-bottom", PropMarginBottom);
		Lut.Add("margin-left", PropMarginLeft);
		Lut.Add("padding", PropPadding);
		Lut.Add("padding-top", PropPaddingTop);
		Lut.Add("padding-right", PropPaddingRight);
		Lut.Add("padding-bottom", PropPaddingBottom);
		Lut.Add("padding-left", PropPaddingLeft);

		Lut.Add("background", PropBackground);
		Lut.Add("background-color", PropBackgroundColor);
		Lut.Add("background-image", PropBackgroundImage);
		Lut.Add("background-repeat", PropBackgroundRepeat);
		Lut.Add("background-attachment", PropBackgroundAttachment);
		Lut.Add("background-x", PropBackgroundX);
		Lut.Add("background-y", PropBackgroundY);
		Lut.Add("background-position", PropBackgroundPos);

		Lut.Add("border", PropBorder);
		Lut.Add("border-style", PropBorderStyle);
		Lut.Add("border-color", PropBorderColor);
		Lut.Add("border-radius", PropBorderRadius);
		Lut.Add("border-collapse", PropBorderCollapse);
		Lut.Add("border-spacing", PropBorderSpacing);

		Lut.Add("border-top", PropBorderTop);
		Lut.Add("border-top-color", PropBorderTopColor);
		Lut.Add("border-top-style", PropBorderTopStyle);
		Lut.Add("border-top-width", PropBorderTopWidth);

		Lut.Add("border-right", PropBorderRight);
		Lut.Add("border-right-color", PropBorderRightColor);
		Lut.Add("border-right-style", PropBorderRightStyle);
		Lut.Add("border-right-width", PropBorderRightWidth);

		Lut.Add("border-bottom", PropBorderBottom);
		Lut.Add("border-bottom-color", PropBorderBottomColor);
		Lut.Add("border-bottom-style", PropBorderBottomStyle);
		Lut.Add("border-bottom-width", PropBorderBottomWidth);

		Lut.Add("border-left", PropBorderLeft);
		Lut.Add("border-left-color", PropBorderLeftColor);
		Lut.Add("border-left-style", PropBorderLeftStyle);
		Lut.Add("border-left-width", PropBorderLeftWidth);

		Lut.Add("line-height", PropLineHeight);
		Lut.Add("vertical-align", PropVerticalAlign);
		Lut.Add("clip", PropClip);
		Lut.Add("x-rect", PropXSubRect);
		Lut.Add("color", PropColor);
		Lut.Add("no-paint-color", PropNoPaintColor);
		Lut.Add("font-family", PropFontFamily);
		
		Lut.Add("cellpadding", Prop_CellPadding);
	}
	
	if (ParentProp.Length() == 0)
	{
		ParentProp.Add(PropBorderTopColor, PropBorderTop);
		ParentProp.Add(PropBorderTopStyle, PropBorderTop);
		ParentProp.Add(PropBorderTopWidth, PropBorderTop);

		ParentProp.Add(PropBorderLeftColor, PropBorderLeft);
		ParentProp.Add(PropBorderLeftStyle, PropBorderLeft);
		ParentProp.Add(PropBorderLeftWidth, PropBorderLeft);

		ParentProp.Add(PropBorderRightColor, PropBorderRight);
		ParentProp.Add(PropBorderRightStyle, PropBorderRight);
		ParentProp.Add(PropBorderRightWidth, PropBorderRight);

		ParentProp.Add(PropBorderBottomColor, PropBorderBottom);
		ParentProp.Add(PropBorderBottomStyle, PropBorderBottom);
		ParentProp.Add(PropBorderBottomWidth, PropBorderBottom);

		/*
		ParentProp.Add(PropBackgroundColor, PropBackground);
		ParentProp.Add(PropBackgroundImage, PropBackground);
		ParentProp.Add(PropBackgroundRepeat, PropBackground);
		ParentProp.Add(PropBackgroundAttachment, PropBackground);
		ParentProp.Add(PropBackgroundX, PropBackground);
		ParentProp.Add(PropBackgroundY, PropBackground);
		*/
	}
}

GCss::GCss(const GCss &c)
{
	*this = c;
}

GCss::~GCss()
{
	Empty();
}

int GCss::Len::ToPx(int Box, GFont *Font, int Dpi)
{
	switch (Type)
	{
		default:
		case LenInherit:
		{
			return 0;
		}
		case LenAuto:
		case LenNormal:
		case LenPx:
		{
			return (int) Value;
		}
		case LenPt:
		{
			int EffectiveDpi = Dpi > 0 ? Dpi : LgiScreenDpi();
			return (int) (Value * EffectiveDpi / 72.0);
		}
		case LenCm:
		{
			int EffectiveDpi = Dpi > 0 ? Dpi : LgiScreenDpi();
			return (int) (Value * EffectiveDpi / 2.54);
		}
		case LenEm:
		{
			int FntHt = Font ? Font->GetHeight() : 18;
			return (int) (Value * FntHt);
		}
		case LenEx:
		{
			double FntAsc = Font ? Font->Ascent() : 18.0;
			return (int) (Value * FntAsc); // haha I don't care.
		}		
		case LenPercent:
		{
			if (Box > 0)
				return (int) (Box * Value / 100.0);
			else
				return 0; // No idea...
		}
	}
}

bool GCss::Len::ToString(GStream &p)
{
	const char *Unit = 0;
	switch (Type)
	{
		case LenPx:			Unit = "px"; break;
		case LenPt:			Unit = "pt"; break;
		case LenEm:			Unit = "em"; break;
		case LenEx:			Unit = "ex"; break;
		case LenPercent:	Unit = "%"; break;
		case LenCm:			Unit = "cm"; break;
		default: break;
	}
	if (Unit)
	{
		return p.Print("%g%s", Value, Unit) > 0;
	}

	switch (Type)
	{
		case LenInherit:		Unit = "Inherit"; break;
		case LenAuto:			Unit = "Auto"; break;
		case LenNormal:			Unit = "Normal"; break;

		case AlignLeft:			Unit = "left"; break;
		case AlignRight:		Unit = "right"; break;
		case AlignCenter:		Unit = "center"; break;
		case AlignJustify:		Unit = "justify"; break;

		case VerticalBaseline:	Unit = "baseline"; break;
		case VerticalSub:		Unit = "sub"; break;
		case VerticalSuper:		Unit = "super"; break;
		case VerticalTop:		Unit = "top"; break;
		case VerticalTextTop:	Unit = "text-top"; break;
		case VerticalMiddle:	Unit = "middle"; break;
		case VerticalBottom:	Unit = "bottom"; break;
		case VerticalTextBottom: Unit = "text-bottom"; break;
		default: break;
	}
	if (!Unit)
	{
		LgiAssert(!"Impl missing length enum.");
		return false;
	}
	
	return p.Print("%s", Unit) > 0;
}

bool GCss::ColorDef::ToString(GStream &p)
{
	switch (Type)
	{
		case ColorInherit:
		{
			p.Print("inherit");
			break;
		}
		case ColorRgb:
		{
			p.Print("#%02.2x%02.2x%02.2x", R32(Rgb32), G32(Rgb32), B32(Rgb32));
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			return false;
		}
	}

	return true;
}

const char *GCss::ToString(DisplayType dt)
{
	switch (dt)
	{
		case DispInherit: return "inherit";
		case DispBlock: return "block";
		case DispInline: return "inline";
		case DispInlineBlock: return "inline-block";
		case DispListItem: return "list-item";
		case DispNone: return "none";
		default: return NULL;
	}
}

GAutoString GCss::ToString()
{
	GStringPipe p;

	// PropType Prop;
	// for (void *v = Props.First((int*)&Prop); v; v = Props.Next((int*)&Prop))
	for (auto v : Props)
	{
		PropType Prop = (PropType) v.key;
		switch (v.key >> 8)
		{
			case TypeEnum:
			{
				const char *s = 0;
				const char *Name = PropName(Prop);
				switch (v.key)
				{
					case PropFontWeight:
					{
						FontWeightType *b = (FontWeightType*)v.value;
						switch (*b)
						{
							case FontWeightInherit: s = "inherit"; break;
							case FontWeightNormal: s = "normal"; break;
							case FontWeightBold: s = "bold"; break;
							case FontWeightBolder: s = "bolder"; break;
							case FontWeightLighter: s = "lighter"; break;
							case FontWeight100: s = "100"; break;
							case FontWeight200: s = "200"; break;
							case FontWeight300: s = "300"; break;
							case FontWeight400: s = "400"; break;
							case FontWeight500: s = "500"; break;
							case FontWeight600: s = "600"; break;
							case FontWeight700: s = "700"; break;
							case FontWeight800: s = "800"; break;
							case FontWeight900: s = "900"; break;
						}
						break;
					}
					case PropFontStyle:
					{
						FontStyleType *t = (FontStyleType*)v.value;
						switch (*t)
						{
							case FontStyleInherit: s = "inherit"; break;
							case FontStyleNormal: s = "normal"; break;
							case FontStyleItalic: s = "italic"; break;
							case FontStyleOblique: s = "oblique"; break;
						}
						break;
					}
					case PropTextDecoration:
					{
						TextDecorType *d = (TextDecorType*)v.value;
						switch (*d)
						{
							case TextDecorInherit: s= "inherit"; break;
							case TextDecorNone: s= "none"; break;
							case TextDecorUnderline: s= "underline"; break;
							case TextDecorOverline: s= "overline"; break;
							case TextDecorLineThrough: s= "line-Through"; break;
							case TextDecorSquiggle: s= "squiggle"; break;
						}
						break;
					}
					case PropDisplay:
					{
						DisplayType *d = (DisplayType*)v.value;
						s = ToString(*d);
						break;
					}
					case PropFloat:
					{
						FloatType *d = (FloatType*)v.value;
						switch (*d)
						{
							case FloatInherit: s = "inherit"; break;
							case FloatLeft: s = "left"; break;
							case FloatRight: s = "right"; break;
							case FloatNone: s = "none"; break;
						}
						break;
					}
					case PropPosition:
					{
						PositionType *d = (PositionType*)v.value;
						switch (*d)
						{
							case PosInherit: s = "inherit"; break;
							case PosStatic: s = "static"; break;
							case PosRelative: s = "relative"; break;
							case PosAbsolute: s = "absolute"; break;
							case PosFixed: s = "fixed"; break;
						}
						break;
					}
					case PropOverflow:
					{
						OverflowType *d = (OverflowType*)v.value;
						switch (*d)
						{
							case OverflowInherit: s = "inherit"; break;
							case OverflowVisible: s = "visible"; break;
							case OverflowHidden: s = "hidden"; break;
							case OverflowScroll: s = "scroll"; break;
							case OverflowAuto: s = "auto"; break;
						}
						break;
					}
					case PropVisibility:
					{
						VisibilityType *d = (VisibilityType*)v.value;
						switch (*d)
						{
							case VisibilityInherit: s = "inherit"; break;
							case VisibilityVisible: s = "visible"; break;
							case VisibilityHidden: s = "hidden"; break;
							case VisibilityCollapse: s = "collapse"; break;
						}
						break;
					}
					case PropFontVariant:
					{
						FontVariantType *d = (FontVariantType*)v.value;
						switch (*d)
						{
							case FontVariantInherit: s = "inherit"; break;
							case FontVariantNormal: s = "normal"; break;
							case FontVariantSmallCaps: s = "small-caps"; break;
						}
						break;
					}
					case PropBackgroundRepeat:
					{
						RepeatType *d = (RepeatType*)v.value;
						switch (*d)
						{
							default: s = "inherit"; break;
							case RepeatBoth: s = "repeat"; break;
							case RepeatX: s = "repeat-x"; break;
							case RepeatY: s = "repeat-y"; break;
							case RepeatNone: s = "none"; break;
						}
						break;
					}
					case PropBackgroundAttachment:
					{
						AttachmentType *d = (AttachmentType*)v.value;
						switch (*d)
						{
							default: s = "inherit"; break;
							case AttachmentScroll: s = "scroll"; break;
							case AttachmentFixed: s = "fixed"; break;
						}
						break;
					}
					case PropListStyleType:
					{
						ListStyleTypes *w = (ListStyleTypes*)v.value;
						switch (*w)
						{
							default: s = "inherit"; break;
						    case ListNone: s = "none"; break;
							case ListDisc: s = "disc"; break;
							case ListCircle: s = "circle"; break;
							case ListSquare: s = "square"; break;
							case ListDecimal: s = "decimal"; break;
							case ListDecimalLeadingZero: s = "decimalleadingzero"; break;
							case ListLowerRoman: s = "lowerroman"; break;
							case ListUpperRoman: s = "upperroman"; break;
							case ListLowerGreek: s = "lowergreek"; break;
							case ListUpperGreek: s = "uppergreek"; break;
							case ListLowerAlpha: s = "loweralpha"; break;
							case ListUpperAlpha: s = "upperalpha"; break;
							case ListArmenian: s = "armenian"; break;
							case ListGeorgian: s = "georgian"; break;
						}
						break;
					}
					case PropBorderCollapse:
					{
						BorderCollapseType *w = (BorderCollapseType*)v.value;
						switch (*w)
						{
							default: s = "inherit"; break;
							case CollapseCollapse: s = "Collapse"; break;
							case CollapseSeparate: s = "Separate"; break;
						}
						break;
					}
					default:
					{
						LgiAssert(!"Impl me.");
						break;
					}
				}
				
				if (s) p.Print("%s: %s;\n", Name, s);
				break;
			}
			case TypeLen:
			{
				Len *l = (Len*)v.value;
				const char *Name = PropName(Prop);
				p.Print("%s: ", Name);
				l->ToString(p);
				p.Print(";\n");
				break;
			}
			case TypeGRect:
			{
				GRect *r = (GRect*)v.value;
				const char *Name = PropName(Prop);
				p.Print("%s: rect(%s);\n", Name, r->GetStr());
				break;
			}
			case TypeColor:
			{
				ColorDef *c = (ColorDef*)v.value;
				const char *Name = PropName(Prop);
				p.Print("%s: ", Name);
				c->ToString(p);
				p.Print(";\n");
				break;
			}
			case TypeImage:
			{
				ImageDef *i = (ImageDef*)v.value;
				const char *Name = PropName(Prop);
				switch (i->Type)
				{
					case ImageInherit:
					{
						p.Print("%s: inherit;\n", Name);
						break;
					}
					case ImageOwn:
					case ImageRef:
					{
						if (i->Uri)
							p.Print("%s: url(%s);\n", Name, i->Uri.Get());
						break;
					}
					default:
						break;
				}
				break;
			}
			case TypeBorder:
			{
				BorderDef *b = (BorderDef*)v.value;
				const char *Name = PropName(Prop);

				p.Print("%s:", Name);
				b->ToString(p);

				const char *s = 0;
				switch (b->Style)
				{
					case BorderNone: s = "none"; break;
					case BorderHidden: s = "hidden"; break;
					case BorderDotted: s = "dotted"; break;
					case BorderDashed: s = "dashed"; break;
					case BorderDouble: s = "double"; break;
					case BorderGroove: s = "groove"; break;
					case BorderRidge: s = "ridge"; break;
					case BorderInset: s = "inset"; break;
					case BorderOutset: s = "outset"; break;
					default:
					case BorderSolid: s = "solid"; break;
				}
				p.Print(" %s", s);
				if (b->Color.Type != ColorInherit)
				{
					p.Print(" ");
					b->Color.ToString(p);
				}
				p.Print(";\n");
				break;
			}
			case TypeStrings:
			{
				StringsDef *s = (StringsDef*)v.value;
				const char *Name = PropName(Prop);
				p.Print("%s: ", Name);
				for (int i=0; i<s->Length(); i++)
				{
					p.Print("%s%s", i?",":"", (*s)[i]);
				}
				p.Print(";\n");
				break;
			}
			default:
			{
				LgiAssert(!"Invalid type.");
				break;
			}
		}
	}

	return GAutoString(p.NewStr());
}

bool GCss::InheritCollect(GCss &c, PropMap &Contrib)
{
	int StillInherit = 0;

    // int p;
	// for (PropArray *a = Contrib.First(&p); a; a = Contrib.Next(&p))
	for (auto a : Contrib)
	{
		switch (a.key >> 8)
		{
			#define InheritEnum(prop, type, inherit)	\
				case prop:								\
				{										\
					type *Mine = (type*)Props.Find(a.key); \
					if (!Mine || *Mine == inherit) \
					{									\
						type *Theirs = (type*)c.Props.Find(a.key); \
						if (Theirs) \
						{ \
							if (!Mine) Props.Add(a.key, Mine = new type); \
							*Mine = *Theirs; \
						}									\
						else StillInherit++;				\
					} \
					break;								\
				}

			#define InheritClass(prop, type, inherit)	\
				case prop:								\
				{										\
					type *Mine = (type*)Props.Find(a.key); \
					if (!Mine || Mine->Type == inherit) \
					{									\
						type *Theirs = (type*)c.Props.Find(a.key); \
						if (Theirs) \
						{ \
							if (!Mine) Props.Add(a.key, Mine = new type); \
							*Mine = *Theirs; \
						}									\
						else StillInherit++;				\
					} \
					break;								\
				}



			case TypeEnum:
			{
				switch (a.key)
				{
					InheritEnum(PropFontStyle, FontStyleType, FontStyleInherit);
					InheritEnum(PropFontVariant, FontVariantType, FontVariantInherit);
					InheritEnum(PropFontWeight, FontWeightType, FontWeightInherit);
					InheritEnum(PropTextDecoration, TextDecorType, TextDecorInherit);
					default:
					{
						LgiAssert(!"Not impl.");
						break;
					}
				}
				break;
			}
			case TypeLen:
			{
				Len *Mine = (Len*)Props.Find(a.key);
				if (!Mine || Mine->IsDynamic())
				{
					Len *Cur = (Len*)c.Props.Find(a.key);
					if (Cur && Cur->Type != LenInherit)
					{
						if (!Mine) Props.Add(a.key, Mine = new Len);
						*Mine = *Cur;						
						a.value->Add(Cur);
					}
					else StillInherit++;
				}
				break;
			}
			case TypeGRect:
			{
				GRect *Mine = (GRect*)Props.Find(a.key);
				if (!Mine || !Mine->Valid())
				{
					GRect *Theirs = (GRect*)c.Props.Find(a.key);
					if (Theirs)
					{
						if (!Mine) Props.Add(a.key, Mine = new GRect);
						*Mine = *Theirs;
					}
					else StillInherit++;
				}
				break;
			}
			case TypeStrings:
			{
				StringsDef *Mine = (StringsDef*)Props.Find(a.key);
				if (!Mine || Mine->Length() == 0)
				{
					StringsDef *Theirs = (StringsDef*)c.Props.Find(a.key);
					if (Theirs)
					{
						if (!Mine) Props.Add(a.key, Mine = new StringsDef);
						*Mine = *Theirs;
					}
					else StillInherit++;
				}
				break;
			}
			InheritClass(TypeColor, ColorDef, ColorInherit);
			InheritClass(TypeImage, ImageDef, ImageInherit);
			InheritClass(TypeBorder, BorderDef, LenInherit);
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
		}
	}

	return StillInherit > 0;
}

bool GCss::InheritResolve(PropMap &Contrib)
{
    // int p;
	// for (PropArray *a = Contrib.First(&p); a; a = Contrib.Next(&p))
	for (auto a : Contrib)
	{
	    switch (a.key >> 8)
	    {
            case TypeLen:
            {
				Len *Mine = 0;
			    for (ssize_t i=a.value->Length()-1; i>=0; i--)
			    {
			        Len *Cur = (Len*)(*a.value)[i];
			        if (!Mine)
			        {
						Props.Add(a.key, Mine = new Len(*Cur));
						continue;
			        }
			        
			        if (Cur->IsDynamic())
			        {
			            switch (Cur->Type)
			            {
							case LenAuto:
							{
								i = 0;
								break;
							}
			                case SizeSmaller:
			                {
			                    switch (Mine->Type)
			                    {
			                        case LenPt:
			                        case LenPx:
			                        {
			                            Mine->Value = Mine->Value - 1;
			                            break;
			                        }
			                        case SizeXXSmall:
			                        {
										// No smaller sizes..
										break;
			                        }
			                        case SizeXSmall:
			                        {
										Mine->Value = SizeXXSmall;
										break;
			                        }
			                        case SizeSmall:
			                        {
										Mine->Value = SizeXSmall;
										break;
			                        }
			                        case SizeMedium:
			                        {
										Mine->Value = SizeSmall;
										break;
			                        }
			                        case SizeLarge:
			                        {
										Mine->Value = SizeMedium;
										break;
			                        }
			                        case SizeXLarge:
			                        {
										Mine->Value = SizeLarge;
										break;
			                        }
			                        case SizeXXLarge:
			                        {
										Mine->Value = SizeXLarge;
										break;
			                        }
			                        case SizeSmaller:
			                        {
										// Just stick with smaller...
										break;
			                        }
			                        default:
			                        {
			                            LgiAssert(!"Not impl");
			                            break;
			                        }
			                    }
			                    break;
			                }
			                case SizeLarger:
			                {
			                    switch (Mine->Type)
			                    {
			                        case LenPt:
			                        case LenPx:
			                        {
			                            Mine->Value = Mine->Value + 1;
			                            break;
			                        }
			                        case SizeXXSmall:
			                        {
										Mine->Value = SizeXSmall;
										break;
			                        }
			                        case SizeXSmall:
			                        {
										Mine->Value = SizeSmall;
										break;
			                        }
			                        case SizeSmall:
			                        {
										Mine->Value = SizeMedium;
										break;
			                        }
			                        case SizeMedium:
			                        {
										Mine->Value = SizeLarge;
										break;
			                        }
			                        case SizeLarge:
			                        {
										Mine->Value = SizeXLarge;
										break;
			                        }
			                        case SizeXLarge:
			                        {
										Mine->Value = SizeXXLarge;
										break;
			                        }
			                        case SizeXXLarge:
			                        {
										// No higher sizess...
										break;
			                        }
			                        case SizeLarger:
			                        {
										// Just stick with larger....
										break;
			                        }
			                        default:
			                        {
			                            LgiAssert(!"Not impl");
			                            break;
			                        }
			                    }
			                    break;
			                }
			                case LenPercent:
			                {
								if (Cur->Value == 100)
									break;

			                    switch (Mine->Type)
			                    {
			                        case LenPt:
			                        case LenPercent:
			                        case LenPx:
			                        case LenEm:
			                        case LenEx:
			                        case LenCm:
			                        {
			                            Mine->Value *= Cur->Value / 100;
			                            break;
			                        }
									case SizeXXSmall:
									case SizeXSmall:
									case SizeSmall:
									case SizeMedium:
									case SizeLarge:
									case SizeXLarge:
									case SizeXXLarge:
									{
										int Idx = (int)Mine->Type - SizeXXSmall;
										if (Idx >= 0 && Idx < CountOf(FontSizeTable))
										{
											double Sz = FontSizeTable[Idx];
											double NewSz = Sz * Cur->Value / 100;
											Mine->Value = (float) NewSz;
											Mine->Type = LenEm;
										}
										else LgiAssert(0);										
										break;
									}
			                        default:
			                        {
			                            LgiAssert(!"Not impl");
			                            break;
			                        }
			                    }
			                    break;
			                }
	                        default:
	                        {
	                            LgiAssert(!"Not impl");
	                            break;
	                        }
			            }
			        }
			        else
			        {
			            *Mine = *Cur;
			        }
			    }
				break;
            }
	    }
	}
	
    return false;
}

GCss &GCss::operator -=(const GCss &c)
{
	// Removes all props in 'cc' from this Css store...
	GCss &cc = (GCss&)c;
	// int Prop;
	// for (void *p=cc.Props.First(&Prop); p; p=cc.Props.Next(&Prop))
	for (auto p : cc.Props)
	{
		DeleteProp((PropType)p.key);
	}

	return *this;
}

bool GCss::CopyStyle(const GCss &c)
{
	GCss &cc = (GCss&)c;
	// int Prop;
	// for (void *p=cc.Props.First(&Prop); p; p=cc.Props.Next(&Prop))
	for (auto p : cc.Props)
	{
		switch (p.key >> 8)
		{
			#define CopyProp(TypeId, Type) \
				case TypeId: \
				{ \
					Type *n = (Type*)Props.Find(p.key); \
					if (!n) n = new Type; \
					*n = *(Type*)p.value; \
					Props.Add(p.key, n); \
					break; \
				}

			case TypeEnum:
			{
				void *n = new DisplayType;
				*(uint32*)n = *(uint32*)p.value;
				Props.Add(p.key, n);
				break;
			}
			CopyProp(TypeLen, Len);
			CopyProp(TypeGRect, GRect);
			CopyProp(TypeColor, ColorDef);
			CopyProp(TypeImage, ImageDef);
			CopyProp(TypeBorder, BorderDef);
			CopyProp(TypeStrings, StringsDef);
			default:
			{
				LgiAssert(!"Invalidate property type.");
				return false;
			}
		}
	}

	return true;
}

bool GCss::operator ==(GCss &c)
{
	if (Props.Length() != c.Props.Length())
		return false;

	// Check individual types
	bool Eq = true;
	// PropType Prop;
	// for (void *Local=Props.First((int*)&Prop); Local && Eq; Local=Props.Next((int*)&Prop))
	for (auto p : Props)
	{
		void *Other = c.Props.Find(p.key);
		if (!Other)
			return false;

		switch (p.key >> 8)
		{
			#define CmpType(id, type) \
				case id: \
				{ \
					if ( *((type*)p.value) != *((type*)Other)) \
						Eq = false; \
					break; \
				}

			CmpType(TypeEnum, uint32);
			CmpType(TypeLen, Len);
			CmpType(TypeGRect, GRect);
			CmpType(TypeColor, ColorDef);
			CmpType(TypeImage, ImageDef);
			CmpType(TypeBorder, BorderDef);
			CmpType(TypeStrings, StringsDef);
			default:
				LgiAssert(!"Unknown type.");
				break;
		}
	}

	return Eq;
}

void GCss::DeleteProp(PropType p)
{
	void *Data = Props.Find(p);
	if (Data)
	{
		DeleteProp(p, Data);
		Props.Delete(p);
	}
}

void GCss::DeleteProp(PropType Prop, void *Data)
{
	if (!Data)
		return;

	int Type = Prop >> 8;
	switch (Type)
	{
		case TypeEnum:
			delete ((PropType*)Data);
			break;
		case TypeLen:
			delete ((Len*)Data);
			break;
		case TypeGRect:
			delete ((GRect*)Data);
			break;
		case TypeColor:
			delete ((ColorDef*)Data);
			break;
		case TypeImage:
			delete ((ImageDef*)Data);
			break;
		case TypeBorder:
			delete ((BorderDef*)Data);
			break;
		case TypeStrings:
			delete ((StringsDef*)Data);
			break;
		default:
			LgiAssert(!"Unknown property type.");
			break;
	}
}

void GCss::Empty()
{
	// int Prop;
	// for (void *Data=Props.First(&Prop); Data; Data=Props.Next(&Prop))
	for (auto p : Props)
	{
		DeleteProp((PropType)p.key, p.value);
	}
	Props.Empty();
}

/// This returns true if there is a non default font style. Useful for checking if you
/// need to create a font...
bool GCss::HasFontStyle()
{
	auto Fam = FontFamily();
	if (Fam.Length() > 0)
		return true;
	auto Sz = FontSize();
	if (Sz.IsValid())
		return true;
	auto Style = FontStyle();
	if (Style != FontStyleInherit)
		return true;
	auto Var = FontVariant();
	if (Var != FontVariantInherit)
		return true;
	auto Wt = FontWeight();
	if (Wt != FontWeightInherit && Wt != FontWeightNormal)
		return true;
	auto Dec = TextDecoration();
	if (Dec != TextDecorInherit)
		return true;

	return false;
}

void GCss::OnChange(PropType Prop)
{
}

bool GCss::ParseFontStyle(PropType PropId, const char *&s)
{
	FontStyleType *w = (FontStyleType*)Props.Find(PropId);
	if (!w) Props.Add(PropId, w = new FontStyleType);

		 if (ParseWord(s, "inherit")) *w = FontStyleInherit;
	else if (ParseWord(s, "normal")) *w = FontStyleNormal;
	else if (ParseWord(s, "italic")) *w = FontStyleItalic;
	else if (ParseWord(s, "oblique")) *w = FontStyleOblique;
	else
	{
		Props.Delete(PropId);
		DeleteObj(w);
		return false;
	}

	return true;
}

bool GCss::ParseFontVariant(PropType PropId, const char *&s)
{
	FontVariantType *w = (FontVariantType*)Props.Find(PropId);
	if (!w) Props.Add(PropId, w = new FontVariantType);

		 if (ParseWord(s, "inherit")) *w = FontVariantInherit;
	else if (ParseWord(s, "normal")) *w = FontVariantNormal;
	else if (ParseWord(s, "small-caps")) *w = FontVariantSmallCaps;
	else
	{
		Props.Delete(PropId);
		DeleteObj(w);
		return false;
	}
	return true;
}

bool GCss::ParseFontWeight(PropType PropId, const char *&s)
{
	FontWeightType *w = (FontWeightType*)Props.Find(PropId);
	if (!w) Props.Add(PropId, w = new FontWeightType);

	if (ParseWord(s, "Inherit")) *w = FontWeightInherit;
	else if (ParseWord(s, "Normal")) *w = FontWeightNormal;
	else if (ParseWord(s, "Bold")) *w = FontWeightBold;
	else if (ParseWord(s, "Bolder")) *w = FontWeightBolder;
	else if (ParseWord(s, "Lighter")) *w = FontWeightLighter;
	else if (ParseWord(s, "100")) *w = FontWeight100;
	else if (ParseWord(s, "200")) *w = FontWeight200;
	else if (ParseWord(s, "300")) *w = FontWeight300;
	else if (ParseWord(s, "400")) *w = FontWeight400;
	else if (ParseWord(s, "500")) *w = FontWeight500;
	else if (ParseWord(s, "600")) *w = FontWeight600;
	else if (ParseWord(s, "700")) *w = FontWeight700;
	else if (ParseWord(s, "800")) *w = FontWeight800;
	else if (ParseWord(s, "900")) *w = FontWeight900;
	else
	{
		Props.Delete(PropId);
		DeleteObj(w);
		return false;
	}
	return true;
}

bool GCss::ParseBackgroundRepeat(const char *&s)
{
	RepeatType *w = (RepeatType*)Props.Find(PropBackgroundRepeat);
	if (!w) Props.Add(PropBackgroundRepeat, w = new RepeatType);

	     if (ParseWord(s, "inherit")) *w = RepeatInherit;
	else if (ParseWord(s, "repeat-x")) *w = RepeatX;
	else if (ParseWord(s, "repeat-y")) *w = RepeatY;
	else if (ParseWord(s, "no-repeat")) *w = RepeatNone;
	else if (ParseWord(s, "repeat")) *w = RepeatBoth;
	else return false;
	
	return true;
}

bool GCss::ParseDisplayType(const char *&s)
{
	DisplayType *t = (DisplayType*)Props.Find(PropDisplay);
	if (!t) Props.Add(PropDisplay, t = new DisplayType);
	
	if (ParseWord(s, "block")) *t = DispBlock;
	else if (ParseWord(s, "inline-block")) *t = DispInlineBlock;
	else if (ParseWord(s, "inline")) *t = DispInline;
	else if (ParseWord(s, "list-item")) *t = DispListItem;
	else if (ParseWord(s, "none")) *t = DispNone;
	else
	{
		*t = DispInherit;
		return false;
	}
	return true;
}

void GCss::ParsePositionType(const char *&s)
{
	PositionType *t = (PositionType*)Props.Find(PropPosition);
	if (!t) Props.Add(PropPosition, t = new PositionType);
	
	if (ParseWord(s, "static")) *t = PosStatic;
	else if (ParseWord(s, "relative")) *t = PosRelative;
	else if (ParseWord(s, "absolute")) *t = PosAbsolute;
	else if (ParseWord(s, "fixed")) *t = PosFixed;
	else *t = PosInherit;
}

bool GCss::Parse(const char *&s, ParsingStyle Type)
{
	if (!s) return false;

	while (*s && *s != '}')
	{
		// Parse the prop name out
		while (*s && !IsAlpha(*s) && !strchr("-_", *s))
			s++;

		char Prop[64], *p = Prop, *end = Prop + sizeof(Prop) - 1;
		if (!*s)
			break;
		while (*s && (IsAlpha(*s) || strchr("-_", *s)))
		{
			if (p < end)
				*p++ = *s++;
			else
				s++;
		}
		*p++ = 0;
		SkipWhite(s);
		if (*s != ':')
			return false;
		s++;			
		PropType PropId = Lut.Find(Prop);
		PropTypes PropType = (PropTypes)((int)PropId >> 8);
		SkipWhite(s);
		// const char *ValueStart = s;

		// Do the data parsing based on type
		switch (PropType)
		{
			case TypeEnum:
			{
				switch (PropId)
				{
					case PropDisplay:
						ParseDisplayType(s);
						break;
					case PropPosition:
						ParsePositionType(s);
						break;
					case PropFloat:
					{
						FloatType *t = (FloatType*)Props.Find(PropId);
						if (!t) Props.Add(PropId, t = new FloatType);
						
						if (ParseWord(s, "left")) *t = FloatLeft;
						else if (ParseWord(s, "right")) *t = FloatRight;
						else if (ParseWord(s, "none")) *t = FloatNone;
						else *t = FloatInherit;
						break;
					}
					case PropFontStyle:
					{
						ParseFontStyle(PropId, s);
						break;
					}
					case PropFontVariant:
					{
						ParseFontVariant(PropId, s);
						break;
					}
					case PropFontWeight:
					{
						ParseFontWeight(PropId, s);
						break;
					}
					case PropTextDecoration:
					{
						TextDecorType *w = (TextDecorType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new TextDecorType);

						     if (ParseWord(s, "inherit")) *w = TextDecorInherit;
						else if (ParseWord(s, "none")) *w = TextDecorNone;
						else if (ParseWord(s, "underline")) *w = TextDecorUnderline;
						else if (ParseWord(s, "overline")) *w = TextDecorOverline;
						else if (ParseWord(s, "line-through")) *w = TextDecorLineThrough;
						else if (ParseWord(s, "squiggle")) *w = TextDecorSquiggle;
						break;
					}
					case PropWordWrap:
					{
						WordWrapType *w = (WordWrapType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new WordWrapType);

						if (ParseWord(s, "break-word")) *w = WrapBreakWord;
						else *w = WrapNormal;
						break;
					}
					case PropBorderCollapse:
					{
						BorderCollapseType *w = (BorderCollapseType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new BorderCollapseType);

						     if (ParseWord(s, "inherit")) *w = CollapseInherit;
						else if (ParseWord(s, "Collapse")) *w = CollapseCollapse;
						else if (ParseWord(s, "Separate")) *w = CollapseSeparate;
						break;
					}
					case PropBackgroundRepeat:
					{
						ParseBackgroundRepeat(s);
						break;
					}
					case PropListStyle:
					{
						// Fall thru
					}
					case PropListStyleType:
					{
						ListStyleTypes *w = (ListStyleTypes*)Props.Find(PropListStyleType);
						if (!w) Props.Add(PropListStyleType, w = new ListStyleTypes);

						     if (ParseWord(s, "none")) *w = ListNone;
						else if (ParseWord(s, "disc")) *w = ListDisc;
						else if (ParseWord(s, "circle")) *w = ListCircle;
						else if (ParseWord(s, "square")) *w = ListSquare;
						else if (ParseWord(s, "decimal")) *w = ListDecimal;
						else if (ParseWord(s, "decimalleadingzero")) *w = ListDecimalLeadingZero;
						else if (ParseWord(s, "lowerroman")) *w = ListLowerRoman;
						else if (ParseWord(s, "upperroman")) *w = ListUpperRoman;
						else if (ParseWord(s, "lowergreek")) *w = ListLowerGreek;
						else if (ParseWord(s, "uppergreek")) *w = ListUpperGreek;
						else if (ParseWord(s, "loweralpha")) *w = ListLowerAlpha;
						else if (ParseWord(s, "upperalpha")) *w = ListUpperAlpha;
						else if (ParseWord(s, "armenian")) *w = ListArmenian;
						else if (ParseWord(s, "georgian")) *w = ListGeorgian;
						else *w = ListInherit;
						break;
					}
					case PropLetterSpacing:
					{
						// Fixme: do not care right now...
						break;
					}
					case PropBackgroundAttachment:
					{
						AttachmentType *w = (AttachmentType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new AttachmentType);

						     if (ParseWord(s, "inherit")) *w = AttachmentInherit;
						else if (ParseWord(s, "scroll")) *w = AttachmentScroll;
						else if (ParseWord(s, "fixed")) *w = AttachmentFixed;
						break;						
					}
					case PropOverflow:
					{
						OverflowType *w = (OverflowType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new OverflowType);

						     if (ParseWord(s, "inherit")) *w = OverflowInherit;
						else if (ParseWord(s, "visible")) *w = OverflowVisible;
						else if (ParseWord(s, "hidden")) *w = OverflowHidden;
						else if (ParseWord(s, "scroll")) *w = OverflowScroll;
						else if (ParseWord(s, "auto")) *w = OverflowAuto;
						break;
					}
					case PropVisibility:
					{
						VisibilityType *w = (VisibilityType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new VisibilityType);

						     if (ParseWord(s, "inherit")) *w = VisibilityInherit;
						else if (ParseWord(s, "visible")) *w = VisibilityVisible;
						else if (ParseWord(s, "hidden")) *w = VisibilityHidden;
						else if (ParseWord(s, "collapse")) *w = VisibilityCollapse;
						break;
					}
					case PropBorderTopStyle:
					case PropBorderRightStyle:
					case PropBorderBottomStyle:
					case PropBorderLeftStyle:
					{
						GCss::PropType Parent = ParentProp.Find(PropId);
						if (Parent)
						{
							BorderDef *b = (BorderDef*) Props.Find(Parent);
							if (!b && (b = new BorderDef))
								Props.Add(Parent, b);
							if (b)
							{
								if (!b->ParseStyle(s))
								{
									if (Type == ParseStrict)
										LgiAssert(!"Colour parsing failed.");
								}
							}
						}
						break;
					}
					case PropFont:
					{
						// Clear any existing style info.
						DeleteProp(PropFontStyle);
						DeleteProp(PropFontVariant);
						DeleteProp(PropFontWeight);
						DeleteProp(PropFontSize);
						DeleteProp(PropLineHeight);
						DeleteProp(PropFontFamily);

						bool ApplySize = true;
						while (*s)
						{
							// Try and guess the parts in any order...
							SkipWhite(s);
							if (*s == ';')
								break;

							if (*s == '/')
							{
								ApplySize = false;
								s++;
							}
							else if (*s == ',')
							{
								s++;
							}
							else
							{
								// Point size...?
								GAutoPtr<Len> Pt(new Len);
								if (Pt->Parse(s, ApplySize ? PropFontSize : PropLineHeight, Type))
								{
									if (ApplySize)
										FontSize(*Pt);
									else
										LineHeight(*Pt);
								}
								else if (!ParseFontStyle(PropFontStyle, s) &&
										 !ParseFontVariant(PropFontVariant, s) &&
										 !ParseFontWeight(PropFontWeight, s))
								{
									// Face name...
									GAutoPtr<StringsDef> Fam(new StringsDef);
									if (Fam->Parse(s))
										FontFamily(*Fam);
									else
										break;
								}
							}
						}
						break;
					}
					default:
					{
						LgiAssert(!"Prop parsing support not implemented.");
					}						
				}
				break;
			}
			case TypeLen:
			{
				GArray<Len*> Lengths;
				SkipWhite(s);
				while (*s && *s != ';')
				{
					GAutoPtr<Len> t(new Len);
					if (t->Parse(s, PropId, PropId == PropZIndex ? ParseRelaxed : Type))
					{
						Lengths.Add(t.Release());
						SkipWhite(s);
					}
					else
					{
						if (Type == ParseStrict)
							LgiAssert(!"Parsing failed.");
						break;
					}
				}

				SkipWhite(s);
				
				bool Mismatch = false;
				switch (PropId)
				{
					case PropBorderTopWidth:
					case PropBorderRightWidth:
					case PropBorderBottomWidth:
					case PropBorderLeftWidth:
					{
						GCss::PropType Parent = ParentProp.Find(PropId);
						if (Parent)
						{
							BorderDef *b = (BorderDef*) Props.Find(Parent);
							if (!b && (b = new BorderDef))
								Props.Add(Parent, b);
							
							if (b && Lengths.Length() == 1)
							{
								*((GCss::Len*&)b) = *Lengths[0];
							}
							else if (Type == ParseStrict)
							{
								LgiAssert(0);
							}
						}
						break;
					}
					case PropPadding:
					{
						if (Lengths.Length() == 4)
						{
							StoreProp(PropPaddingTop, Lengths[0], true);
							StoreProp(PropPaddingRight, Lengths[1], true);
							StoreProp(PropPaddingBottom, Lengths[2], true);
							StoreProp(PropPaddingLeft, Lengths[3], true);
						}
						else if (Lengths.Length() == 3)
						{
							StoreProp(PropPaddingTop, Lengths[0], true);
							StoreProp(PropPaddingLeft, Lengths[1], false);
							StoreProp(PropPaddingRight, Lengths[1], true);
							StoreProp(PropPaddingBottom, Lengths[2], true);
						}
						else if (Lengths.Length() == 2)
						{
							StoreProp(PropPaddingTop, Lengths[0], false);
							StoreProp(PropPaddingBottom, Lengths[0], true);
							StoreProp(PropPaddingRight, Lengths[1], false);
							StoreProp(PropPaddingLeft, Lengths[1], true);
						}
						else if (Lengths.Length() == 1)
						{
							StoreProp(PropPadding, Lengths[0], true);
							DeleteProp(PropPaddingLeft);
							DeleteProp(PropPaddingTop);
							DeleteProp(PropPaddingRight);
							DeleteProp(PropPaddingBottom);
						}
						else Mismatch = true;
						if (!Mismatch)
						{
							Lengths.Length(0);
							OnChange(PropPadding);
						}
						break;
					}
					case PropMargin:
					{
						if (Lengths.Length() == 4)
						{
							StoreProp(PropMarginTop, Lengths[0], true);
							StoreProp(PropMarginRight, Lengths[1], true);
							StoreProp(PropMarginBottom, Lengths[2], true);
							StoreProp(PropMarginLeft, Lengths[3], true);
						}
						else if (Lengths.Length() == 3)
						{
							StoreProp(PropMarginTop, Lengths[0], true);
							StoreProp(PropMarginLeft, Lengths[1], false);
							StoreProp(PropMarginRight, Lengths[1], true);
							StoreProp(PropMarginBottom, Lengths[2], true);
						}
						else if (Lengths.Length() == 2)
						{
							StoreProp(PropMarginTop, Lengths[0], false);
							StoreProp(PropMarginBottom, Lengths[0], true);
							StoreProp(PropMarginRight, Lengths[1], false);
							StoreProp(PropMarginLeft, Lengths[1], true);
						}
						else if (Lengths.Length() == 1)
						{
							StoreProp(PropMargin, Lengths[0], true);
							DeleteProp(PropMarginTop);
							DeleteProp(PropMarginBottom);
							DeleteProp(PropMarginRight);
							DeleteProp(PropMarginLeft);
						}
						else Mismatch = true;
						if (!Mismatch)
						{
							Lengths.Length(0);
							OnChange(PropMargin);
						}
					}
					default:
					{
						LgiAssert(ParentProp.Find(PropId) == PropNull);
						
						if (Lengths.Length() > 0)
						{
							StoreProp(PropId, Lengths[0], true);
							Lengths.DeleteAt(0);
							OnChange(PropId);
						}
						break;
					}
				}
				
				Lengths.DeleteObjects();
				break;
			}
			case TypeBackground:
			{
				while (*s && !strchr(";}", *s))
				{
					const char *Start = s;

					ImageDef Img;
					if (Img.Parse(s))
					{
						BackgroundImage(Img);
						continue;;
					}
					
					Len x, y;
					if (x.Parse(s))
					{
						y.Parse(s);
						BackgroundX(x);
						BackgroundY(y);
						continue;
					}

					if (ParseBackgroundRepeat(s))
					{
						continue;
					}

					ColorDef Color;
					if (Color.Parse(s) || OnUnhandledColor(&Color, s))
					{
						BackgroundColor(Color);
						continue;
					}				
					
					SkipWhite(s);
					while (*s && *s != ';' && !IsWhite(*s))
						s++;
						
					if (Start == s)
					{
						LgiAssert(!"Parsing hang.");
						break;
					}
				}
				break;
			}
			case TypeColor:
			{
				GAutoPtr<ColorDef> t(new ColorDef);
				if (t->Parse(s) || OnUnhandledColor(t, s))
				{
					switch (PropId)
					{
						case PropBorderTopColor:
						case PropBorderRightColor:
						case PropBorderBottomColor:
						case PropBorderLeftColor:
						{
							GCss::PropType Parent = ParentProp.Find(PropId);
							if (Parent)
							{
								BorderDef *b = (BorderDef*) Props.Find(Parent);
								if (!b && (b = new BorderDef))
									Props.Add(Parent, b);
								if (b)
									b->Color = *t;
							}
							else LgiAssert(0);
							break;
						}
						default:
						{					
							LgiAssert(ParentProp.Find(PropId) == PropNull);
							
							ColorDef *e = (ColorDef*)Props.Find(PropId);
							if (e) *e = *t;
							else Props.Add(PropId, t.Release());
							break;
						}
					}
				}
				else if (Type == ParseStrict)
					LgiAssert(!"Parsing failed.");
				break;
			}
			case TypeStrings:
			{
				GAutoPtr<StringsDef> t(new StringsDef);
				if (t->Parse(s))
				{
					StringsDef *e = (StringsDef*)Props.Find(PropId);
					if (e) *e = *t;
					else Props.Add(PropId, t.Release());
				}
				else LgiAssert(!"Parsing failed.");
				break;
			}
			case TypeBorder:
			{
				if (PropId == PropBorderStyle)
				{
					GCss::BorderDef b;
					if (b.ParseStyle(s))
					{
						GCss::BorderDef *db;
						GetOrCreate(db, PropBorderLeft)->Style = b.Style;
						GetOrCreate(db, PropBorderRight)->Style = b.Style;
						GetOrCreate(db, PropBorderTop)->Style = b.Style;
						GetOrCreate(db, PropBorderBottom)->Style = b.Style;
					}
				}
				else if (PropId == PropBorderColor)
				{
					ColorDef c;
					if (c.Parse(s))
					{
						GCss::BorderDef *db;
						GetOrCreate(db, PropBorderLeft)->Color = c;
						GetOrCreate(db, PropBorderRight)->Color = c;
						GetOrCreate(db, PropBorderTop)->Color = c;
						GetOrCreate(db, PropBorderBottom)->Color = c;
					}
				}
				else
				{				
					GAutoPtr<BorderDef> t(new BorderDef);
					if (t->Parse(this, s))
					{
						ReleasePropOnSave(BorderDef, PropId);
					}
				}
				break;
			}
			case TypeGRect:
			{
				GRect r;
				if (ParseWord(s, "rect"))
				{
					SkipWhite(s);
					if (*s == '(')
					{
						const char *Start = ++s;
						while (*s && *s != ')' && *s != ';')
							s++;
						if (*s == ')')
						{
							GString tmp(Start, s - Start);
							r.SetStr(tmp);
							s++;

							GRect *e = (GRect*)Props.Find(PropId);
							if (e) *e = r;
							else Props.Add(PropId, new GRect(r));
						}
						return false;
					}
				}
				break;
			}
			case TypeImage:
			{
				GAutoPtr<ImageDef> Img(new ImageDef);
				if (Img->Parse(s))
				{
					ImageDef *i = (ImageDef*)Props.Find(PropId);
					if (i) *i = *Img;
					else Props.Add(PropId, Img.Release());
				}
				else if (Type == ParseStrict)
				{
					LgiAssert(!"Failed to parse Image definition");
					return false;
				}
				break;
			}
			default:
			{
				if (Type == ParseStrict)
				{
					LgiAssert(!"Unsupported property type.");
					return false;
				}
				else
				{
					#if DEBUG_CSS_LOGGING
					LgiTrace("%s:%i - Unsupported CSS property: %s\n", _FL, Prop);
					#endif
				}
				break;
			}
		}

		// End of property delimiter
		while (*s && *s != ';') s++;
		if (*s != ';')
			break;
		s++;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////
bool GCss::Len::Parse(const char *&s, PropType Prop, ParsingStyle ParseType)
{
	if (!s) return false;
	
	SkipWhite(s);
	if (ParseWord(s, "inherit")) Type = LenInherit;
	else if (ParseWord(s, "auto")) Type = LenAuto;
	else if (ParseWord(s, "normal")) Type = LenNormal;

	else if (ParseWord(s, "center")) Type = AlignCenter;
	else if (ParseWord(s, "left")) Type = AlignLeft;
	else if (ParseWord(s, "right")) Type = AlignRight;
	else if (ParseWord(s, "justify")) Type = AlignJustify;

	else if (ParseWord(s, "xx-small")) Type = SizeXXSmall;
	else if (ParseWord(s, "x-small")) Type = SizeXSmall;
	else if (ParseWord(s, "small")) Type = SizeSmall;
	else if (ParseWord(s, "medium")) Type = SizeMedium;
	else if (ParseWord(s, "large")) Type = SizeLarge;
	else if (ParseWord(s, "x-large")) Type = SizeXLarge;
	else if (ParseWord(s, "xx-large")) Type = SizeXXLarge;
	else if (ParseWord(s, "smaller")) Type = SizeSmaller;
	else if (ParseWord(s, "larger")) Type = SizeLarger;

	else if (ParseWord(s, "baseline")) Type = VerticalBaseline;
	else if (ParseWord(s, "sub")) Type = VerticalSub;
	else if (ParseWord(s, "super")) Type = VerticalSuper;
	else if (ParseWord(s, "top")) Type = VerticalTop;
	else if (ParseWord(s, "text-top")) Type = VerticalTextTop;
	else if (ParseWord(s, "middle")) Type = VerticalMiddle;
	else if (ParseWord(s, "bottom")) Type = VerticalBottom;
	else if (ParseWord(s, "text-bottom")) Type = VerticalTextBottom;

	else if (IsNumeric(s))
	{
		Value = (float) atof(s);
		while (IsNumeric(s)) s++;
		SkipWhite(s);
		if (*s == '%')
		{
			Type = LenPercent;
			s++;
		}
		else if (ParseWord(s, "px"))Type = LenPx;
		else if (ParseWord(s, "pt")) Type = LenPt;
		else if (ParseWord(s, "em")) Type = LenEm;
		else if (ParseWord(s, "ex")) Type = LenEx;
		else if (ParseWord(s, "cm")) Type = LenCm;
		else if (IsAlpha(*s))
		{
			// Unknown unit, in the case of a missing ';' we should
			// reset to "inherit" as it's less damaging to the layout
			Type = LenInherit;
			return false;
		}
		else if (ParseType == ParseRelaxed)
		{
			if (Prop == PropLineHeight)
			{
				Type = LenPercent;
				Value *= 100;
			}
			else
			{
				Type = LenPx;
			}
		}
		else
			return false;
	}
	else return false;

	return true;
}

bool GCss::ColorDef::Parse(const char *&s)
{
	if (!s) return false;

	#define NamedColour(Name, Value) \
		else if (ParseWord(s, #Name)) { Type = ColorRgb; Rgb32 = Value; return true; }
	#define ParseExpect(s, ch) \
		if (*s != ch) return false; \
		else s++;

	SkipWhite(s);
	if (*s == '#')
	{
		s++;
		int v = 0;
		const char *e = s;
		while (*e && e < s + 6)
		{
			if (*e >= 'a' && *e <= 'f')
			{
				v <<= 4;
				v |= *e - 'a' + 10;
				e++;
			}
			else if (*e >= 'A' && *e <= 'F')
			{
				v <<= 4;
				v |= *e - 'A' + 10;
				e++;
			}
			else if (*e >= '0' && *e <= '9')
			{
				v <<= 4;
				v |= *e - '0';
				e++;
			}
			else break;
		}

		if (e == s + 3)
		{
			Type = ColorRgb;
			int r = (v >> 8) & 0xf;
			int g = (v >> 4) & 0xf;
			int b = v & 0xf;
			Rgb32 = Rgb32( (r << 4 | r), (g << 4 | g), (b << 4 | b) );
			s = e;
		}
		else if (e == s + 6)
		{
			Type = ColorRgb;
			int r = (v >> 16) & 0xff;
			int g = (v >> 8) & 0xff;
			int b = v & 0xff;
			Rgb32 = Rgb32(r, g, b);
			s = e;
		}
		else return false;
	}
	else if (ParseWord(s, "transparent"))
	{
		Type = ColorTransparent;
	}
	else if (ParseWord(s, "rgb") && *s == '(')
	{
		s++;
		int r = ParseComponent(s);
		ParseExpect(s, ',');
		int g = ParseComponent(s);
		ParseExpect(s, ',');
		int b = ParseComponent(s);
		ParseExpect(s, ')');
		
		Type = ColorRgb;
		Rgb32 = Rgb32(r, g, b);
	}
	else if (ParseWord(s, "rgba") && *s == '(')
	{
		s++;
		int r = ParseComponent(s);
		ParseExpect(s, ',');
		int g = ParseComponent(s);
		ParseExpect(s, ',');
		int b = ParseComponent(s);
		ParseExpect(s, ',');
		int a = ParseComponent(s);
		ParseExpect(s, ')');
		
		Type = ColorRgb;
		Rgb32 = Rgba32(r, g, b, a);
	}
	else if (ParseWord(s, "-webkit-gradient("))
	{
		GAutoString GradientType(ParseString(s));
		ParseExpect(s, ',');
		if (!GradientType)
			return false;
		if (!stricmp(GradientType, "radial"))
		{
		}
		else if (!stricmp(GradientType, "linear"))
		{
			Len StartX, StartY, EndX, EndY;
			if (!StartX.Parse(s, PropNull) || !StartY.Parse(s, PropNull))
				return false;
			ParseExpect(s, ',');
			if (!EndX.Parse(s, PropNull) || !EndY.Parse(s, PropNull))
				return false;
			ParseExpect(s, ',');
			SkipWhite(s);
			while (*s)
			{
				if (*s == ')')
				{
					Type = ColorLinearGradient;
					break;
				}
				else
				{
					GAutoString Stop(ParseString(s));
					if (!Stop) return false;
					if (!stricmp(Stop, "from"))
					{
					}
					else if (!stricmp(Stop, "to"))
					{
					}
					else if (!stricmp(Stop, "stop"))
					{
					}
					else return false;
				}
			}
		}
		else return false;
	}
	NamedColour(black, Rgb32(0x00, 0x00, 0x00))
	NamedColour(white, Rgb32(0xff, 0xff, 0xff))
	NamedColour(gray, Rgb32(0x80, 0x80, 0x80))
	NamedColour(red, Rgb32(0xff, 0x00, 0x00))
	NamedColour(yellow, Rgb32(0xff, 0xff, 0x00))
	NamedColour(green, Rgb32(0x00, 0x80, 0x00))
	NamedColour(orange, Rgb32(0xff, 0xA5, 0x00))
	NamedColour(blue, Rgb32(0x00, 0x00, 0xff))
	NamedColour(maroon, Rgb32(0x80, 0x00, 0x00))
	NamedColour(olive, Rgb32(0x80, 0x80, 0x00))
	NamedColour(purple, Rgb32(0x80, 0x00, 0x80))
	NamedColour(fuchsia, Rgb32(0xff, 0x00, 0xff))
	NamedColour(lime, Rgb32(0x00, 0xff, 0x00))
	NamedColour(navy, Rgb32(0x00, 0x00, 0x80))
	NamedColour(aqua, Rgb32(0x00, 0xff, 0xff))
	NamedColour(teal, Rgb32(0x00, 0x80, 0x80))
	NamedColour(silver, Rgb32(0xc0, 0xc0, 0xc0))
	else
		return false;

	return true;
}

GCss::ImageDef::~ImageDef()
{
	if (Type == ImageOwn)
		DeleteObj(Img);
}

bool GCss::ImageDef::Parse(const char *&s)
{
	SkipWhite(s);

	if (*s == '-')
		s++;

	if (ParseWord(s, "initial"))
	{
		Type = ImageInherit;
	}
	else if (ParseWord(s, "none"))
	{
		Type = ImageNone;
	}
	else if (!strnicmp(s, "url(", 4))
	{
		s += 4;
		char *e = strchr((char*)s, ')');
		if (!e)
			return false;
		Uri.Set(s, e - s);
		s = e + 1;
		Type = ImageRef;
	}
	else
	{
		return false;
	}

	return true;
}


bool GCss::ImageDef::operator !=(const ImageDef &i)
{
	if (Type != i.Type)
		return false;
	if (Uri.Get() && i.Uri.Get())
		return Uri.Equals(i.Uri);
	return true;
}

GCss::ImageDef &GCss::ImageDef::operator =(const ImageDef &o)
{
	if (Type == ImageOwn)
		DeleteObj(Img);
		
	if (o.Img)
	{
		Img = o.Img;
		Type = ImageRef;
	}
	else if ((Uri = o.Uri))
	{
		Type = ImageUri;
	}
	
	return *this;
}

bool GCss::BorderDef::Parse(GCss *Css, const char *&s)
{
	if (!s)
		return false;

	const char *Start = NULL;
	while (*s && *s != ';')
	{
		SkipWhite(s);
		if (Start == s)
		{
			LgiAssert(0);
			return false;
		}
		Start = s;
		
		if (Len::Parse(s, PropBorder, ParseRelaxed))
			continue;
			
		if (ParseStyle(s))
			continue;

		if (Color.Parse(s))
			continue;
		
		// Ok running out of ideas here....
		
		// Is it a weird colour?
		if (Css && Css->OnUnhandledColor(&Color, s))
			continue;

		// Unknown token... try and parse over it?
		while (*s && *s != ';' && !strchr(WhiteSpace, *s))
			s++;
	}

	return true;
}

bool GCss::BorderDef::ParseStyle(const char *&s)
{
	if (ParseWord(s, "Hidden")) Style = BorderHidden;
	else if (ParseWord(s, "Solid")) Style = BorderSolid;
	else if (ParseWord(s, "Dotted")) Style = BorderDotted;
	else if (ParseWord(s, "Dashed")) Style = BorderDashed;
	else if (ParseWord(s, "Double")) Style = BorderDouble;
	else if (ParseWord(s, "Groove")) Style = BorderGroove;
	else if (ParseWord(s, "Ridge")) Style = BorderRidge;
	else if (ParseWord(s, "Inset")) Style = BorderInset;
	else if (ParseWord(s, "Outset")) Style = BorderOutset;
	else if (ParseWord(s, "None")) Style = BorderNone;
	else if (ParseWord(s, "!important")) Important = false;
	else return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
GCss::Selector &GCss::Selector::operator =(const GCss::Selector &s)
{
	Parts.Length(0);
	for (int i=0; i<s.Parts.Length(); i++)
	{
		Part &n = Parts.New();
		n = ((GCss::Selector&)s).Parts[i];
	}
	return *this;
}

bool GCss::Selector::TokString(GAutoString &a, const char *&s)
{
	// const char *Init = s;
	const char *e = s;
	while
	(
		*e &&
		(
			IsAlpha(*e)
			||
			IsDigit(*e)
			||
			strchr("-_", *e) // I removed ',' from this because it causes
							 // css selector parsing to miss the end of the
							 // selector: i.e.
							 // table#id td.class,table#id td.class2
		)
	)
		e++;
	
	if (e <= s)
	{
		#if DEBUG_CSS_LOGGING
		LgiTrace("Stuck at '%s'\n", e);
		#endif
		// LgiAssert(!"Failed to tokenise string.");
		return false;
	}
	a.Reset(NewStr(s, e - s));
	s = e;
	return true;
}

const char *GCss::Selector::PartTypeToString(PartType p)
{
	switch (p)
	{
		case SelType: return "Type";
		case SelUniversal: return "Uni";
		case SelAttrib: return "Attrib";
		case SelClass: return "Cls";
		case SelMedia: return "Media";
		case SelFontFace: return "FontFace";
		case SelPage: return "Page";
		case SelID: return "ID";
		case SelPseudo: return "Pseudo";
		case CombDesc: return "Desc";
		case CombChild: return ">";
		case CombAdjacent: return "+";
		default: break;
	}
	return "<error>";
};

GAutoString GCss::Selector::Print()
{
	GStringPipe p;
	for (int i=0; i<Parts.Length(); i++)
	{
		Part &n = Parts[i];
		p.Print("%s %s, ", PartTypeToString(n.Type), n.Value.Get());
	}
	return GAutoString(p.NewStr());
}

bool GCss::Selector::IsAtMedia()
{
	if (Parts.Length() <= 0)
		return false;
	
	Part &p = Parts[0];
	return p.Type == SelMedia;
}

uint32 GCss::Selector::GetSpecificity()
{
	uint8 s[4] = {0};
	
	for (unsigned i=0; i<Parts.Length(); i++)
	{
		Part &p = Parts[i];
		switch (p.Type)
		{
			case SelType:
			case SelPseudo:
				s[0]++;
				break;
			case SelClass:
			case SelAttrib:
				s[1]++;
				break;
			case SelID:
				s[2]++;
				break;
			default:
				break;
		}
	}
	
	return	((uint32)s[3]<<24) |
			((uint32)s[2]<<16) |
			((uint32)s[1]<<8) |
			((uint32)s[0]);
}

bool GCss::Selector::ToString(GStream &p)
{
	// Output the selector parts...
	for (unsigned i=0; i<Parts.Length(); i++)
	{
		Part &pt = Parts[i];
		switch (pt.Type)
		{
			case SelType:
				p.Print("%s", pt.Value.Get());
				break;
			case SelUniversal:
				p.Print("*");
				break;
			case SelAttrib:
				p.Print("%s", pt.Value.Get());
				break;
			case SelClass:
				p.Print(".%s", pt.Value.Get());
				break;
			case SelMedia:
				p.Print("@%s", pt.Value.Get());
				break;
			case SelID:
				p.Print("#%s", pt.Value.Get());
				break;
			case SelPseudo:
				p.Print(":%s", pt.Value.Get());
				break;
			case CombDesc:
				p.Print(" ");
				break;
			case CombChild:
				p.Print(">");
				break;
			case CombAdjacent:
				p.Print("+");
				break;
			default:
				LgiAssert(0);
				break;
		}
	}

	// And now the rules...
	p.Print(" {\n%s\n}\n", Style ? Style : "");
	
	return true;
}

bool GCss::Selector::Parse(const char *&s)
{
	if (!s)
		return false;

	const char *Start = s, *Prev = s;
	GArray<int> Offsets;
	GStringPipe p;
	while (*s)
	{
		SkipWhite(s);
		if (*s == '{' ||
			*s == ',')
		{
			break;
		}
		else if (*s == '/')
		{
			if (s[1] != '*')
				return false;
			
			s += 2;
			char *End = strstr((char*)s, "*/");
			if (!End)
				return false;
			
			s = End + 2;
			continue;
		}
		else if (*s == '<')
		{
			if (s[1] != '!' &&
				s[2] != '-' &&
				s[3] != '-')
				return false;
			
			s += 4;
			char *End = strstr((char*)s, "-->");
			if (!End)
				return false;
			
			s = End + 3;
			continue;
		}
		else if (*s == ':')
		{
			s++;
			if (*s == ':')
			{
				s++;
			}
			else if (!IsAlpha(*s))
			{
				s++;
				break;
			}

			Part &n = Parts.New();
			n.Type = SelPseudo;
			if (!TokString(n.Value, s))
				return false;
			if (*s == '(')
			{
				char *e = strchr(s + 1, ')');
				if (e && e - s < 100)
				{
					s++;
					n.Param.Reset(NewStr(s, e - s));
					s = e + 1;
				}
			}			
		}
		else if (*s == '#')
		{
			s++;

			Part &n = Parts.New();
			n.Type = SelID;
			if (!TokString(n.Value, s))
				return false;
		}
		else if (*s == '.')
		{
			s++;
			while (*s && !IsAlpha(*s))
				s++;

			Part &n = Parts.New();
			n.Type = SelClass;
			if (!TokString(n.Value, s))
				return false;
		}
		else if (*s == '@')
		{
			s++;

			Part &n = Parts.New();
			n.Media = MediaNull;
			GAutoString Str;
			if (!TokString(Str, s))
				return false;
			if (!Str)
				return false;

			if (!_stricmp(Str, "media"))
				n.Type = SelMedia;
			else if (!_stricmp(Str, "font-face"))
				n.Type = SelFontFace;
			else if (!_stricmp(Str, "page"))
				n.Type = SelPage;
			else if (!_stricmp(Str, "list"))
				n.Type = SelList;
			else if (!_stricmp(Str, "import"))
				n.Type = SelImport;
			else if (!_stricmp(Str, "keyframes"))
				n.Type = SelKeyFrames;
			else
				n.Type = SelIgnored;
			
			SkipWhite(s);
			while (*s && !strchr(";{", *s))
			{
				if (*s == '(')
				{
					const char *e = strchr(s, ')');
					if (e)
					{
						s = e + 1;
						SkipWhite(s);
						continue;
					}
				}
				
				if (*s == ',')
				{
					s++;
					SkipWhite(s);
					continue;
				}
					
				if (!TokString(Str, s))
				{
					// Skip bad char...
					s++;
					break;
				}
				SkipWhite(s);
				
				if (!Str)
					break;
				if (!stricmp(Str, "screen"))
					n.Media |= MediaScreen;
				else if (!stricmp(Str, "print"))
					n.Media |= MediaPrint;
			}
		}
		else if (s[0] == '*')
		{
			s++;

			Part &n = Parts.New();
			n.Type = SelUniversal;
		}
		else if (IsAlpha(*s))
		{
			Part &n = Parts.New();
			n.Type = SelType;
			if (!TokString(n.Value, s))
				return false;
		}
		else if (*s == '[')
		{
			s++;

			Part &n = Parts.New();
			n.Type = SelAttrib;
			
			char *End = strchr((char*)s, ']');
			if (!End)
				return false;
				
			n.Value.Reset(NewStr(s, End - s));
			s = End + 1;
		}
		else
		{
			// Unexpected character
			s++;
			continue;
		}

		const char *Last = s;
		SkipWhite(s);
		if (*s == '+')
		{
			s++;
			LgiAssert(Parts.Length() > 0);
			Combs.Add(Parts.Length());
			Part &n = Parts.New();
			n.Type = CombAdjacent;
		}
		else if (*s == '>')
		{
			s++;
			LgiAssert(Parts.Length() > 0);
			Combs.Add(Parts.Length());
			Part &n = Parts.New();
			n.Type = CombChild;
		}
		else if (s > Last &&
				(IsAlpha(*s) || strchr(".:#", *s)))
		{
			LgiAssert(Parts.Length() > 0);
			Combs.Add(Parts.Length());
			Part &n = Parts.New();
			n.Type = CombDesc;
		}
		
		if (*s && s == Prev)
		{
			LgiAssert(!"Parsing is stuck.");
			return false;
		}
		Prev = s;
	}

	Raw.Reset(NewStr(Start, s - Start));

	return Parts.Length() > 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SkipWhiteSpace(s)			while (*s && strchr(" \t\r\n", *s)) s++;
static const char *SkipComment(const char *c)
{
	// Skip comment
	SkipWhiteSpace(c);
	if (c[0] == '/' &&
		c[1] == '*')
	{
		char *e = strstr((char*)c + 2, "*/");
		if (e) c = e + 2;
		else c += 2;
		SkipWhiteSpace(c);
	}
	return c;
}

bool GCss::Store::Dump(GStream &out)
{
	const char *MapNames[] = {"TypeMap", "ClassMap", "IdMap", NULL};
	SelectorMap *Maps[] = {&TypeMap, &ClassMap, &IdMap, NULL};
	for (int i=0; Maps[i]; i++)
	{
		SelectorMap *m = Maps[i];
		out.Print("%s = {\n", MapNames[i]);
		
		// const char *Key;
		// for (SelArray *a = m->First(&Key); a; a = m->Next(&Key))
		for (auto a : *m)
		{
			out.Print("\t'%s' -> ", a.key);
			for (int n=0; n<a.value->Length(); n++)
			{
				GCss::Selector *sel = (*a.value)[n];
				if (n) out.Print("\t\t");
				out.Print("%i of %i: %s\n", n, a.value->Length(), sel->Raw.Get());
				// out.Print("\t\t{ %s }\n", sel->Style);
			}
		}
	}
	
	return true;
}



int GCssSelectorCmp(class GCss::Selector **a, GCss::Selector **b)
{
	uint32 as = (*a)->GetSpecificity();
	uint32 bs = (*b)->GetSpecificity();
	if (as == bs)
		return (*a)->SourceIndex - (*b)->SourceIndex;
	return as - bs;
}

void GCss::Store::SortStyles(GCss::SelArray &Styles)
{
	if (Styles.Length() > 1)
		Styles.Sort(GCssSelectorCmp);
}

bool GCss::Store::ToString(GStream &p)
{
	SelectorMap *Maps[] = {&TypeMap, &ClassMap, &IdMap, NULL};
	for (int i=0; Maps[i]; i++)
	{
		SelectorMap *m = Maps[i];
		// for (SelArray *a = m->First(); a; a = m->Next())
		for (auto a : *m)
		{
			for (unsigned n=0; n<a.value->Length(); n++)
			{
				GCss::Selector *sel = (*a.value)[n];
				if (!sel->ToString(p))
					return false;
			}
		}
	}
	
	// Output all the other styles not in the maps...
	for (unsigned n=0; n<Other.Length(); n++)
	{
		GCss::Selector *sel = Other[n];
		if (!sel->ToString(p))
			return false;
	}
	
	return true;
}

bool GCss::Store::Parse(const char *&c, int Depth)
{
	int SelectorIndex = 1;
	
	if (!c)
		return false;

	// const char *Start = c;
	c = SkipComment(c);
	if (!strncmp(c, "<!--", 4))
	{
		c += 4;
	}

	while (c && *c)
	{
		c = SkipComment(c);

		if (!strncmp(c, "-->", 3))
			break;

		SkipWhiteSpace(c);
		if (*c == '}' && Depth > 0)
		{
			c++;
			return true;
		}

		// read selector
		GArray<GCss::Selector*> Selectors;
		GCss::Selector *Cur = new GCss::Selector;
		
		if (Cur->Parse(c))
		{
			Cur->SourceIndex = SelectorIndex++;
			Selectors.Add(Cur);
		}
		else
		{
			DeleteObj(Cur);
			if (*c)
				return false;
		}

		while (*c)
		{
			SkipWhiteSpace(c);
			if (*c == ',')
			{
				c++;
				Cur = new GCss::Selector;
				if (Cur && Cur->Parse(c))
				{
					Cur->SourceIndex = SelectorIndex++;
					Selectors.Add(Cur);
				}
				else
				{
					DeleteObj(Cur);
				}
			}
			else if (*c == '/')
			{
				const char *n = SkipComment(c);
				if (n == c)
					c++;
				else
					c = n;
			}
			else break;
		}

		SkipWhiteSpace(c);

		// read styles
		if (*c == '{')
		{
			c++;
			SkipWhiteSpace(c);
			
			if (Cur && Cur->IsAtMedia())
			{
				// At media rules, so create a child store and put all the rules in there...
				if (Cur->Children.Reset(new GCss::Store))
				{
					if (!Cur->Children->Parse(c, Depth + 1))
						return false;
				}
			}
			else
			{
				// Normal rule...
				const char *Start = c;
				while (*c && *c != '}') c++;
				char *Style = NewStr(Start, c - Start);
				Styles.Add(Style);
				c++;
				
				for (int i=0; i<Selectors.Length(); i++)
				{
					GCss::Selector *s = Selectors[i];
					s->Style = Style;
					
					ssize_t n = s->GetSimpleIndex();
					if (n >= (ssize_t) s->Parts.Length())
					{
						Error.Printf("ErrSimpleIndex %i>=%zi @ '%.80s'", n, s->Parts.Length(), c);
						LgiAssert(!"Part index too high.");
						return false;
					}
					GCss::Selector::Part &p = s->Parts[n];
					
					switch (p.Type)
					{
						case GCss::Selector::SelType:
						{
							if (p.Value)
							{
								SelArray *a = TypeMap.Get(p.Value);
								a->Add(s);
							}
							break;
						}
						case GCss::Selector::SelClass:
						{
							if (p.Value)
							{
								SelArray *a = ClassMap.Get(p.Value);
								a->Add(s);
							}
							break;
						}
						case GCss::Selector::SelID:
						{
							if (p.Value)
							{
								SelArray *a = IdMap.Get(p.Value);
								a->Add(s);
							}
							break;
						}
						default:
						{
							Other.Add(s);
							break;
						}
					}
				}
			}
		}
		else
		{
			Selectors.DeleteObjects();
			break;
		}
	}

	return true;
}
