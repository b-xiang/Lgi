/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A tree/heirarchy control

#ifndef __GTREE2_H
#define __GTREE2_H

#include "GItemContainer.h"
#include <functional>

enum GTreeItemRect
{
	TreeItemPos,
	TreeItemThumb,
	TreeItemText,
	TreeItemIcon
};

class GTreeItem;

class LgiClass GTreeNode
{
protected:
	GTree *Tree;
	GTreeItem *Parent;
	List<GTreeItem> Items;

	virtual GTreeItem *Item() { return 0; }
	virtual GRect *Pos() { return 0; }
	virtual void _ClearDs(int Col);
	void _Visible(bool v);
	void SetLayoutDirty();

public:
	GTreeNode();
	virtual ~GTreeNode();

	/// Inserts a tree item as a child at 'Pos'
	GTreeItem *Insert(GTreeItem *Obj = NULL, int Pos = -1);
	/// Removes this node from it's parent, for permanent separation.
	void Remove();
	/// Detachs the item from the tree so it can be re-inserted else where.
	void Detach();
	/// Gets the node after this one at the same level.
	GTreeItem *GetNext();
	/// Gets the node before this one at the same level.
	GTreeItem *GetPrev();
	/// Gets the first child node.
	GTreeItem *GetChild();
	/// Gets the parent of this node.
	GTreeItem *GetParent() { return Parent; }
	/// Gets the owning tree. May be NULL if not attached to a tree.
	GTree *GetTree() { return Tree; }
	/// Returns true if this is the root node.
	bool IsRoot();
	/// Returns the index of this node in the list of item owned by it's parent.
	ssize_t IndexOf();
	/// \returns number of child.
	size_t GetItems();

	/// Iterate all children
	template<typename T>
	bool Iterate(T *&ptr)
	{
		if (ptr)
			ptr = dynamic_cast<T*>(ptr->GetNext());
		else
			ptr = dynamic_cast<T*>(GetChild());
		return ptr != NULL;
	}
	
	/// Sorts the child items
	template<typename T>
	bool Sort(int (*Compare)(GTreeItem*, GTreeItem*, T user_param), T user_param)
	{
		if (!Compare)
			return false;
		
		Items.Sort(Compare, user_param);
		SetLayoutDirty();
		return true;
	}

	/// Calls a f(n) for each
	int ForEach(std::function<void(GTreeItem*)> Fn);

	virtual bool Expanded() { return false; }
	virtual void Expanded(bool b) {}
	virtual void OnVisible(bool v) {}
};

/// The item class for a tree. This defines a node in the heirarchy.
class LgiClass GTreeItem : public GItem, public GTreeNode
{
	friend class GTree;
	friend class GTreeNode;

protected:
	class GTreeItemPrivate *d;

	// Private methods
	void _RePour();
	void _Pour(GdcPt2 *Limit, int ColumnPx, int Depth, bool Visible);
	void _Remove();
	void _MouseClick(GMouse &m);
	void _SetTreePtr(GTree *t);
	GTreeItem *_HitTest(int x, int y, bool Debug = false);
	GRect *_GetRect(GTreeItemRect Which);
	GdcPt2 _ScrollPos();
	GTreeItem *Item() { return this; }
	GRect *Pos();

	virtual void _PourText(GdcPt2 &Size);
	virtual void _PaintText(GItem::ItemPaintCtx &Ctx);
	void _ClearDs(int Col);
	virtual void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *c);
	int GetColumnSize(int Col);

protected:
	GArray<GAutoString> Str;
	int Sys_Image;

public:
	GTreeItem();
	virtual ~GTreeItem();

	GItemContainer *GetContainer();
	
	/// \brief Get the text for the node
	///
	/// You can either return a string stored internally to your
	/// object by implementing this function in your item class 
	/// or use the SetText function to store the string in this 
	/// class.
	char *GetText(int i=0);
	/// \brief Sets the text for the node.
	///
	/// This will allocate and store the string in this class.
	bool SetText(const char *s, int i=0);
	/// Returns the icon index into the parent tree's GImageList.
	int GetImage(int Flags = 0);
	/// Sets the icon index into the parent tree's GImageList.
	void SetImage(int i);
	/// Tells the item to update itself on the screen when the
	/// GTreeItem::GetText data has changed.
	void Update();
	/// Returns true if the tree item is currently selected.
	bool Select();
	/// Selects or deselects the tree item.
	void Select(bool b);
	/// Returns true if the node has children and is open.
	bool Expanded();
	/// Opens or closes the node to show or hide the children.
	void Expanded(bool b);
	/// Scrolls the tree view so this node is visible.
	void ScrollTo();
	/// Gets the bounding box of the item.
	GRect *GetPos(int Col = -1);
	/// Sort the child folder
	bool SortChildren(int (*compare)(GTreeItem *a, GTreeItem *b, NativeInt UserData), NativeInt UserData = 0);
	/// True if the node is the drop target
	bool IsDropTarget();

	/// Called when the node expands/contracts to show or hide it's children.
	virtual void OnExpand(bool b);

	/// Paints the item
	void OnPaint(ItemPaintCtx &Ctx);
	void OnPaint(GSurface *pDC) { LgiAssert(0); }
};

/// A tree control.
class LgiClass GTree :
	public GItemContainer,
	public ResObject,
	public GTreeNode
{
	friend class GTreeItem;
	friend class GTreeNode;

	class GTreePrivate *d;

	// Private methods
	void _Pour();
	void _OnSelect(GTreeItem *Item);
	void _Update(GRect *r = 0, bool Now = false);
	void _UpdateBelow(int y, bool Now = false);
	void _UpdateScrollBars();
	List<GTreeItem>	*GetSelLst();

protected:
	// Options
	bool Lines;
	bool Buttons;
	bool LinesAtRoot;
	bool EditLabels;
	
	GRect rItems;
	
	GdcPt2 _ScrollPos();
	GTreeItem *GetAdjacent(GTreeItem *From, bool Down);
	void OnDragEnter();
	void OnDragExit();
	void ClearDs(int Col);
	
public:
	GTree(int id, int x = 0, int y = 0, int cx = 100, int cy = 100, const char *name = NULL);
	~GTree();

	const char *GetClass() { return "GTree"; }

	/// Called when an item is clicked
	virtual void OnItemClick(GTreeItem *Item, GMouse &m);
	/// Called when an item is dragged from it's position
	virtual void OnItemBeginDrag(GTreeItem *Item, int Flags);
	/// Called when an item is expanded/contracted to show or hide it's children
	virtual void OnItemExpand(GTreeItem *Item, bool Expand);
	/// Called when an item is selected
	virtual void OnItemSelect(GTreeItem *Item);
	
	// Implementation
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	bool OnMouseWheel(double Lines);
	void OnPaint(GSurface *pDC);
	void OnFocus(bool b);
	void OnPosChange();
	bool OnKey(GKey &k);
	int OnNotify(GViewI *Ctrl, int Flags);
	GMessage::Result OnEvent(GMessage *Msg);
	void OnPulse();
	int GetContentSize(int ColumnIdx);
	LgiCursor GetCursor(int x, int y);

	/// Add a item to the tree
	GTreeItem *Insert(GTreeItem *Obj = 0, int Pos = -1);
	/// Remove and delete an item
	bool Delete(GTreeItem *Obj);
	/// Remove but don't delete an item
	bool Remove(GTreeItem *Obj);
	/// Gets the item at an index
	GTreeItem *ItemAt(size_t Pos) { return Items.ItemAt(Pos); }

	/// Select the item 'Obj'
	bool Select(GTreeItem *Obj);
	/// Returns the first selected item
	GTreeItem *Selection();

	/// Gets the whole selection and puts it in 'n'
	template<class T>
	bool GetSelection(GArray<T*> &n)
	{
		n.Empty();
		auto s = GetSelLst();
		for (auto i : *s)
		{
			T *ptr = dynamic_cast<T*>(i);
			if (ptr)
				n.Add(ptr);
		}
		return n.Length() > 0;
	}
	
	/// Gets an array of all items
	template<class T>
	bool GetAll(GArray<T*> &n)
	{
		n.Empty();
		return ForAllItems([&n](GTreeItem *item)
			{
				T *t = dynamic_cast<T*>(item);
				if (t)
					n.Add(t);
			});
	}
	
	/// Call a function for every item
	bool ForAllItems(std::function<void(GTreeItem*)> Callback);

	/// Returns the item at an x,y location
	GTreeItem *ItemAtPoint(int x, int y, bool Debug = false);
	/// Temporarily selects one of the items as the drop target during a
	/// drag and drop operation. Call SelectDropTarget(0) when done.
	void SelectDropTarget(GTreeItem *Item);

	/// Delete all items (frees the items)
	void Empty();
	/// Remove reference to items (doesn't free the items)
	void RemoveAll();
	/// Call 'Update' on all tree items
	void UpdateAllItems();
	
	// Visual style
	enum ThumbStyle
	{
		TreePlus,
		TreeTriangle
	};

	void SetVisualStyle(ThumbStyle Btns, bool JoiningLines);
};

#endif

