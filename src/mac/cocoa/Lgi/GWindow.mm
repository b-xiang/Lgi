#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GToken.h"
#include "GPopup.h"
#include "GDisplayString.h"
#include "LCocoaView.h"

extern void NextTabStop(GViewI *v, int dir);
extern void SetDefaultFocus(GViewI *v);

#define DEBUG_KEYS			0

#define objc_dynamic_cast(TYPE, object) \
  ({ \
      TYPE *dyn_cast_object = (TYPE*)(object); \
      [dyn_cast_object isKindOfClass:[TYPE class]] ? dyn_cast_object : nil; \
  })

///////////////////////////////////////////////////////////////////////
class HookInfo
{
public:
	int Flags;
	GView *Target;
};

class GWindowContent : public GView
{
	GWindow *w;

public:
	const char *GetClass() { return "GWindowContent"; }

	GWindowContent(GWindow *wnd)
	{
		w = wnd;
		// printf("%s:%i - w=%p %s\n", _FL, w, w->GetClass());
	}
	
	void OnPaint(GSurface *pDC)
	{
		// printf("%s:%i, paint c=%i\n", _FL, (int) w->WindowHandle().p.retainCount);
		w->OnPaint(pDC);
	}
	
	GWindowContent &operator=(NSView *v)
	{
		_View.p = v;
		return *this;
	}
	
	GViewIterator *IterateViews()
	{
		return w->IterateViews();
	}
};


@interface LWindowDelegate : NSObject <NSWindowDelegate>
{
}

- (id)init;
- (void)dealloc;
- (void)windowDidResize:(NSNotification *)aNotification;
- (void)windowWillClose:(NSNotification*)aNotification;
- (BOOL)windowShouldClose:(id)sender;
- (void)windowDidBecomeMain:(NSNotification*)notification;
- (void)windowDidResignMain:(NSNotification*)notification;

@end

@interface LNsWindow : NSWindow
{
}

@property GWindowPrivate *d;
@property GWindowContent *content;

- (id)init:(GWindowPrivate*)priv Frame:(NSRect)rc;
- (void)dealloc;

@end

LWindowDelegate *Delegate = nil;

class GWindowPrivate
{
public:
	GWindow *Wnd;
	GDialog *ChildDlg;
	GMenu *EmptyMenu;
	GViewI *Focus;

	int Sx, Sy;

	GKey LastKey;
	GArray<HookInfo> Hooks;

	uint64 LastMinimize;
	uint64 LastDragDrop;

	bool Dynamic;
	bool SnapToEdge;
	bool DeleteWhenDone;
	bool InitVisible;
	bool CloseRequestDone;
	bool Closing;
	
	GWindowPrivate(GWindow *wnd)
	{
		Focus = NULL;
		InitVisible = false;
		LastMinimize = 0;
		Wnd = wnd;
		LastDragDrop = 0;
		DeleteWhenDone = false;
		ChildDlg = 0;
		Sx = Sy = -1;
		Dynamic = true;
		SnapToEdge = false;
		EmptyMenu = 0;
		CloseRequestDone = false;
		Closing = false;
	}
	
	~GWindowPrivate()
	{
		DeleteObj(EmptyMenu);
	}
	
	ssize_t GetHookIndex(GView *Target, bool Create = false)
	{
		for (int i=0; i<Hooks.Length(); i++)
		{
			if (Hooks[i].Target == Target) return i;
		}
		
		if (Create)
		{
			HookInfo *n = &Hooks[Hooks.Length()];
			if (n)
			{
				n->Target = Target;
				n->Flags = 0;
				return Hooks.Length() - 1;
			}
		}
		
		return -1;
	}
	
	void OnResize()
	{
		NSWindow *nsw = Wnd->WindowHandle().p;
		
		Wnd->Pos = nsw.frame;
		Wnd->PourAll();
		Wnd->OnPosChange();

		nsw.contentView.needsLayout = YES;
		
		/*
		GAutoPtr<GViewIterator> views(Wnd->IterateViews());
		for (auto c = views->First(); c; c = views->Next())
		{
			OsView h = c->Handle();
			if (h)
			{
				GRect Flip = c->GetPos();
				if (h.p.superview)
					Flip = LFlip(h.p.superview, Flip);
				[h.p setFrame:Flip];
			}
		}
		*/
	}
};

@implementation LNsWindow

- (id)init:(GWindowPrivate*)priv Frame:(NSRect)rc
{
	NSUInteger windowStyleMask = NSTitledWindowMask | NSResizableWindowMask |
								 NSClosableWindowMask | NSMiniaturizableWindowMask;
	if ((self = [super initWithContentRect:rc
					styleMask:windowStyleMask
					backing:NSBackingStoreBuffered
					defer:NO ]) != nil)
	{
		self.d = priv;
		
		#if 0
		auto old = self.contentView.frame;
		
		self.content = new GWindowContent(priv->Wnd);
		GRect r = old;
		self.content->SetPos(r);
		
		auto ctrl = [[NSViewController alloc] init];
		ctrl.view = [[LCocoaView alloc] init:self.content];
		ctrl.view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
		*self.content = ctrl.view;
		self.contentViewController = ctrl;
		//[ctrl release];
		#endif
	}
	return self;
}

- (void)dealloc
{
	[super dealloc];
	printf("LNsWindow dealloc...\n");
}

@end

@implementation LWindowDelegate

- (id)init
{
    if ((self = [super init]) != nil)
    {
    }
    return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (void)windowDidResize:(NSNotification*)event
{
	LNsWindow *w = event.object;
	if (w && w.d)
	{
		w.d->OnResize();
		if (w.content)
			[w.content->Handle().p layout];
	}
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
	LNsWindow *w = objc_dynamic_cast(LNsWindow, sender);
	if (w && w.d && w.d->Wnd)
		return w.d->Wnd->OnRequestClose(false);
	
	return YES;
}

- (void)windowWillClose:(NSNotification*)event
{
	LNsWindow *w = event.object;
	if (w && w.d && !w.d->Closing)
		w.d->Wnd->Quit();
}

- (void)windowDidBecomeMain:(NSNotification*)event
{
	LNsWindow *w = event.object;
	if (w && w.d)
		w.d->Wnd->OnFrontSwitch(true);
}

- (void)windowDidResignMain:(NSNotification*)event
{
	LNsWindow *w = event.object;
	if (w && w.d)
		w.d->Wnd->OnFrontSwitch(false);
}

@end

///////////////////////////////////////////////////////////////////////
#define GWND_CREATE		0x0010000

#if __has_feature(objc_arc)
#error "NO ARC!"
#endif

GWindow::GWindow() : GView(NULL)
{
	d = new GWindowPrivate(this);
	_QuitOnClose = false;
	Wnd = NULL;
	Menu = 0;
	_Default = 0;
	_Window = this;
	WndFlags |= GWND_CREATE;
	GView::Visible(false);
	
	_Lock = new LMutex;
	
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	
	GRect pos(0, 50, 200, 100);
	NSRect frame = pos;
	Wnd.p = [[LNsWindow alloc] init:d Frame:frame];
	if (Wnd)
	{
		if (!Delegate)
			Delegate = [[LWindowDelegate alloc] init];
		[Wnd.p makeKeyAndOrderFront:NSApp];
		Wnd.p.delegate = Delegate;
	}
	
	[pool release];
}

GWindow::~GWindow()
{
	if (LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}
	
	_Delete();
	
	if (Wnd)
	{
		LNsWindow *w = objc_dynamic_cast(LNsWindow, Wnd.p);
		if (w)
			w.d = NULL;
		auto c = Wnd.p.retainCount;
		printf("~GWindow %i\n", (int)c);
		[Wnd.p release];
		Wnd.p = nil;
	}
	
	DeleteObj(Menu);
	DeleteObj(d);
	DeleteObj(_Lock);
}

bool &GWindow::CloseRequestDone()
{
	return d->CloseRequestDone;
}

bool GWindow::SetIcon(const char *FileName)
{
	return false;
}

GViewI *GWindow::GetFocus()
{
	return d->Focus;
}

void GWindow::SetFocus(GViewI *ctrl, FocusType type)
{
	const char *TypeName = NULL;
	switch (type)
	{
		case GainFocus: TypeName = "Gain"; break;
		case LoseFocus: TypeName = "Lose"; break;
		case ViewDelete: TypeName = "Delete"; break;
	}
	
	switch (type)
	{
		case GainFocus:
		{
			// Check if the control already has focus
			if (d->Focus == ctrl)
				return;
			
			if (d->Focus)
			{
				GView *v = d->Focus->GetGView();
				if (v) v->WndFlags &= ~GWF_FOCUS;
				d->Focus->OnFocus(false);
				d->Focus->Invalidate();
				
#if DEBUG_SETFOCUS
				GAutoString _foc = DescribeView(d->Focus);
				LgiTrace(".....defocus: %s\n",
						 _foc.Get());
#endif
			}
			
			d->Focus = ctrl;
			
			if (d->Focus)
			{
				GView *v = d->Focus->GetGView();
				if (v) v->WndFlags |= GWF_FOCUS;
				d->Focus->OnFocus(true);
				d->Focus->Invalidate();
				
#if DEBUG_SETFOCUS
				GAutoString _set = DescribeView(d->Focus);
				LgiTrace("GWindow::SetFocus(%s, %s) focusing\n",
						 _set.Get(),
						 TypeName);
#endif
			}
			break;
		}
		case LoseFocus:
		{
			if (ctrl == d->Focus)
			{
				GView *v = d->Focus->GetGView();
				if (v)
				{
					if (v->WndFlags & GWF_FOCUS)
					{
						// View thinks it has focus
						v->WndFlags &= ~GWF_FOCUS;
						d->Focus->OnFocus(false);
						// keep d->Focus pointer, as we want to be able to re-focus the child
						// view when we get focus again
						
#if DEBUG_SETFOCUS
						GAutoString _ctrl = DescribeView(ctrl);
						GAutoString _foc = DescribeView(d->Focus);
						LgiTrace("GWindow::SetFocus(%s, %s) keep_focus: %s\n",
								 _ctrl.Get(),
								 TypeName,
								 _foc.Get());
#endif
					}
					// else view doesn't think it has focus anyway...
				}
				else
				{
					// Non GView handler
					d->Focus->OnFocus(false);
					d->Focus->Invalidate();
					d->Focus = NULL;
				}
			}
			else
			{
				/*
				 LgiTrace("GWindow::SetFocus(%p.%s, %s) error on losefocus: %p(%s)\n",
					ctrl,
					ctrl ? ctrl->GetClass() : NULL,
					TypeName,
					d->Focus,
					d->Focus ? d->Focus->GetClass() : NULL);
				 */
			}
			break;
		}
		case ViewDelete:
		{
			if (ctrl == d->Focus)
			{
#if DEBUG_SETFOCUS
				LgiTrace("GWindow::SetFocus(%p.%s, %s) delete_focus: %p(%s)\n",
						 ctrl,
						 ctrl ? ctrl->GetClass() : NULL,
						 TypeName,
						 d->Focus,
						 d->Focus ? d->Focus->GetClass() : NULL);
#endif
				
				d->Focus = NULL;
			}
			break;
		}
	}
}

void GWindow::SetDragHandlers(bool On)
{
	#if 0
	if (Wnd && _View)
		SetAutomaticControlDragTrackingEnabledForWindow(Wnd, On);
	#endif
}

/*
static void _ClearChildHandles(GViewI *v)
{
	GViewIterator *it = v->IterateViews();
	if (it)
	{
		for (GViewI *v = it->First(); v; v = it->Next())
		{
			_ClearChildHandles(v);
			v->Detach();
		}
	}
	DeleteObj(it);
}
*/

void GWindow::Quit(bool DontDelete)
{
	if (_QuitOnClose)
	{
		LgiCloseApp();
	}
	
	d->DeleteWhenDone = !DontDelete;
	if (Wnd)
	{
		SetDragHandlers(false);
		d->Closing = true;
		[Wnd.p close];
	}
}

void GWindow::SetChildDialog(GDialog *Dlg)
{
	d->ChildDlg = Dlg;
}

bool GWindow::GetSnapToEdge()
{
	return d->SnapToEdge;
}

void GWindow::SetSnapToEdge(bool s)
{
	d->SnapToEdge = s;
}

void GWindow::OnFrontSwitch(bool b)
{
	if (b && d->InitVisible)
	{
		#if 0
		if (!IsWindowVisible(WindowHandle()))
		{
			ShowWindow(WindowHandle());
			SelectWindow(WindowHandle());
		}
		else if (IsWindowCollapsed(WindowHandle()))
		{
			uint64 Now = LgiCurrentTime();
			if (Now - d->LastMinimize < 1000)
			{
				// printf("%s:%i - CollapseWindow ignored via timeout\n", _FL);
			}
			else
			{
				// printf("%s:%i - CollapseWindow "LGI_PrintfInt64","LGI_PrintfInt64"\n", _FL, Now, d->LastMinimize);
				CollapseWindow(WindowHandle(), false);
			}
		}
		#endif
	}
}

bool GWindow::Visible()
{
	if (!Wnd)
		return false;
	
	return [Wnd.p isVisible];
}

void GWindow::Visible(bool i)
{
	if (!Wnd)
		return;

	if (i)
	{
		d->InitVisible = true;
		PourAll();

		[Wnd.p makeKeyAndOrderFront:NULL];
		[NSApp activateIgnoringOtherApps:YES];

		SetDefaultFocus(this);
		OnPosChange();
	}
	else
	{
		[Wnd.p orderOut:Wnd.p];
	}
}

void GWindow::_SetDynamic(bool i)
{
	d->Dynamic = i;
}

void GWindow::_OnViewDelete()
{
	if (d->Dynamic)
	{
		delete this;
	}
}

void GWindow::_Delete()
{
	SetDragHandlers(false);
	GView::_Delete();
}

void GWindow::SetAlwaysOnTop(bool b)
{
}

bool GWindow::PostEvent(int Event, GMessage::Param a, GMessage::Param b)
{
	bool Status = false;
	
	#if 0
	if (Wnd)
	{
		EventRef Ev;
		OSStatus e = CreateEvent(NULL,
								 kEventClassUser,
								 kEventUser,
								 0, // EventTime
								 kEventAttributeUserEvent,
								 &Ev);
		if (e)
		{
			printf("%s:%i - CreateEvent failed with %i\n", _FL, (int)e);
		}
		else
		{
			EventTargetRef t = GetWindowEventTarget(Wnd);
			
			e = SetEventParameter(Ev, kEventParamLgiEvent, typeUInt32, sizeof(Event), &Event);
			if (e) printf("%s:%i - error %i\n", _FL, (int)e);
			e = SetEventParameter(Ev, kEventParamLgiA, typeUInt32, sizeof(a), &a);
			if (e) printf("%s:%i - error %i\n", _FL, (int)e);
			e = SetEventParameter(Ev, kEventParamLgiB, typeUInt32, sizeof(b), &b);
			if (e) printf("%s:%i - error %i\n", _FL, (int)e);
			
			// printf("Sending event to window %i,%i,%i\n", Event, a, b);
			
#if 1
			
			e = SetEventParameter(Ev, kEventParamPostTarget, typeEventTargetRef, sizeof(t), &t);
			if (e) printf("%s:%i - error %i\n", _FL, (int)e);
			e = PostEventToQueue(GetMainEventQueue(), Ev, kEventPriorityStandard);
			if (e) printf("%s:%i - error %i\n", _FL, (int)e);
			Status = e == 0;
			
#else
			
			e = SendEventToEventTarget(Ev, GetControlEventTarget(Wnd));
			if (e) printf("%s:%i - error %i\n", _FL, e);
			
#endif
			
			ReleaseEvent(Ev);
		}
	}
	#endif
	
	return Status;
}

#if 0
#define InRect(r, x, y) \
( (x >= r.left) && (y >= r.top) && (x <= r.right) && (y <= r.bottom) )

int LgiWindowProc(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
	OSStatus result = eventNotHandledErr;
	
	GView *v = (GView*)inUserData;
	if (!v) return result;
	
	UInt32 eventClass = GetEventClass( inEvent );
	UInt32 eventKind = GetEventKind( inEvent );
	
#if 0
	UInt32 ev = LgiSwap32(eventClass);
	printf("WndProc %4.4s-%i\n", (char*)&ev, eventKind);
#endif
	
	switch (eventClass)
	{
		case kEventClassCommand:
		{
			if (eventKind == kEventProcessCommand)
			{
				GWindow *w = v->GetWindow();
				if (w)
				{
					HICommand command;
					GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(command), NULL, &command);
					if (command.commandID != kHICommandSelectWindow)
					{
#if 0
						uint32 c = command.commandID;
#ifndef __BIG_ENDIAN__
						c = LgiSwap32(c);
#endif
						if (c != '0000')
							printf("%s:%i - Cmd='%4.4s'\n", _FL, (char*)&c);
#endif
						
						extern int *ReturnFloatCommand;
						if (ReturnFloatCommand)
						{
							*ReturnFloatCommand = command.commandID;
						}
						else if (command.commandID == kHICommandQuit)
						{
							LgiCloseApp();
						}
						else if (command.commandID == kHICommandMinimizeWindow ||
								 command.commandID == kHICommandMinimizeAll)
						{
							w->d->LastMinimize = LgiCurrentTime();
							CollapseWindow(w->WindowHandle(), true);
						}
						else if (command.commandID == kHICommandClose)
						{
							GWindow *w = dynamic_cast<GWindow*>(v);
							if (w && (w->CloseRequestDone() || w->OnRequestClose(false)))
							{
								w->CloseRequestDone() = true;
								DeleteObj(v);
							}
						}
						else if (command.commandID == kHICommandHide)
						{
							ProcessSerialNumber PSN;
							OSErr e;
							
							e = MacGetCurrentProcess(&PSN);
							if (e)
								printf("MacGetCurrentProcess failed with %i\n", e);
							else
							{
								e = ShowHideProcess(&PSN, false);
								if (e)
									printf("ShowHideProcess failed with %i\n", e);
							}
						}
						else if (command.commandID == kHICommandRotateWindowsForward ||
								 command.commandID == kHICommandRotateFloatingWindowsBackward)
						{
							return eventNotHandledErr;
						}
						else
						{
							w->OnCommand(command.commandID, 0, 0);
						}
						
						result = noErr;
					}
				}
			}
			break;
		}
		case kEventClassWindow:
		{
			switch (eventKind)
			{
				case kEventWindowDispose:
				{
					GWindow *w = v->GetWindow();
					v->OnDestroy();
					
					if (w && w->d && w->d->DeleteWhenDone)
					{
						w->Wnd = 0;
						DeleteObj(v);
					}
					
					result = noErr;
					break;
				}
				case kEventWindowClose:
				{
					GWindow *w = dynamic_cast<GWindow*>(v);
					if (w && (w->CloseRequestDone() || w->OnRequestClose(false)))
					{
						w->CloseRequestDone() = true;
						DeleteObj(v);
					}
					
					result = noErr;
					break;
				}
				case kEventWindowCollapsing:
				{
					GWindow *w = v->GetWindow();
					if (w)
						w->d->LastMinimize = LgiCurrentTime();
					break;
				}
				case kEventWindowActivated:
				{
					GWindow *w = v->GetWindow();
					if (w)
					{
						GMenu *m = w->GetMenu();
						if (m)
						{
							OSStatus e = SetRootMenu(m->Handle());
							if (e)
							{
								printf("%s:%i - SetRootMenu failed (e=%i)\n", _FL, (int)e);
							}
						}
						else
						{
							if (!w->d->EmptyMenu)
							{
								w->d->EmptyMenu = new GMenu;
							}
							
							if (w->d->EmptyMenu)
							{
								OSStatus e = SetRootMenu(w->d->EmptyMenu->Handle());
								if (e)
								{
									printf("%s:%i - SetRootMenu failed (e=%i)\n", _FL, (int)e);
								}
							}
						}
					}
					break;
				}
				case kEventWindowBoundsChanged:
				{
					// kEventParamCurrentBounds
					HIRect r;
					GetEventParameter(inEvent, kEventParamCurrentBounds, typeHIRect, NULL, sizeof(HIRect), NULL, &r);
					v->Pos.x1 = r.origin.x;
					v->Pos.y1 = r.origin.y;
					v->Pos.x2 = v->Pos.x1 + r.size.width - 1;
					v->Pos.y2 = v->Pos.y1 + r.size.height - 1;
					
					v->Invalidate();
					v->OnPosChange();
					result = noErr;
					break;
					
				}
			}
			break;
		}
		case kEventClassMouse:
		{
			switch (eventKind)
			{
				case kEventMouseDown:
				{
					GWindow *Wnd = dynamic_cast<GWindow*>(v->GetWindow());
					if (Wnd && !Wnd->d->ChildDlg)
					{
						OsWindow Hnd = Wnd->WindowHandle();
						if (!IsWindowActive(Hnd))
						{
							ProcessSerialNumber Psn;
							GetCurrentProcess(&Psn);
							SetFrontProcess(&Psn);
							
							SelectWindow(Hnd);
						}
					}
					// Fall thru
				}
				case kEventMouseUp:
				{
					UInt32		modifierKeys = 0;
					UInt32		Clicks = 0;
					Point		Pt;
					UInt16		Btn = 0;
					Rect		Client, Grow;
					
					GetWindowBounds(v->WindowHandle(), kWindowContentRgn, &Client);
					GetWindowBounds(v->WindowHandle(), kWindowGrowRgn, &Grow);
					
					// the point parameter is in view-local coords.
					OSStatus status = GetEventParameter (inEvent, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &Pt);
					//if (status) printf("error(%i) getting kEventParamMouseLocation\n", status);
					status = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifierKeys), NULL, &modifierKeys);
					//if (status) printf("error(%i) getting kEventParamKeyModifiers\n", status);
					status = GetEventParameter(inEvent, kEventParamClickCount, typeUInt32, NULL, sizeof(Clicks), NULL, &Clicks);
					//if (status) printf("error(%i) getting kEventParamClickCount\n", status);
					status = GetEventParameter(inEvent, kEventParamMouseButton, typeMouseButton, NULL, sizeof(Btn), NULL, &Btn);
					//if (status) printf("error(%i) getting kEventParamMouseButton\n", status);
					
					if (InRect(Grow, Pt.h, Pt.v))
					{
						break;
					}
					if (!InRect(Client, Pt.h, Pt.v))
					{
						break;
					}
					
					GMouse m;
					m.ViewCoords = false;
					m.x = Pt.h; // - Client.left;
					m.y = Pt.v; // - Client.top;
					m.SetModifer(modifierKeys);
					m.Down(eventKind == kEventMouseDown);
					m.Double(m.Down() && Clicks > 1);
					m.SetButton(Btn);
					
#if 0
					printf("Client=%i,%i,%i,%i Pt=%i,%i\n",
						   Client.left, Client.top, Client.right, Client.bottom,
						   (int)Pt.h, (int)Pt.v);
#endif
					
#if 0
					printf("click %i,%i down=%i, left=%i right=%i middle=%i, ctrl=%i alt=%i shift=%i Double=%i\n",
						   m.x, m.y,
						   m.Down(), m.Left(), m.Right(), m.Middle(),
						   m.Ctrl(), m.Alt(), m.Shift(), m.Double());
#endif
					
					int Cx = m.x - Client.left;
					int Cy = m.y - Client.top;
					
					m.Target = v->WindowFromPoint(Cx, Cy);
					if (m.Target)
					{
						if (v->GetWindow()->HandleViewMouse(v, m))
						{
							m.ToView();
							GView *v = m.Target->GetGView();
							if (v)
							{
								if (v->_Mouse(m, false))
									result = noErr;
							}
							else printf("%s:%i - Not a GView\n", _FL);
						}
					}
					else printf("%s:%i - No target window for mouse event.\n", _FL);
					
					break;
				}
				case kEventMouseMoved:
				case kEventMouseDragged:
				{
					UInt32		modifierKeys;
					Point		Pt;
					Rect		Client;
					
					GetWindowBounds(v->WindowHandle(), kWindowContentRgn, &Client);
					
					// the point parameter is in view-local coords.
					OSStatus status = GetEventParameter(inEvent, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &Pt);
					if (status) printf("error(%i) getting kEventParamMouseLocation\n", (int)status);
					status = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifierKeys), NULL, &modifierKeys);
					if (status) printf("error(%i) getting kEventParamKeyModifiers\n", (int)status);
					
					GMouse m;
					m.ViewCoords = false;
					m.x = Pt.h;
					m.y = Pt.v;
					m.SetModifer(modifierKeys);
					m.Down(eventKind == kEventMouseDragged);
					if (m.Down())
						m.SetButton(GetCurrentEventButtonState());
					
#if 0
					printf("move %i,%i down=%i, left=%i right=%i middle=%i, ctrl=%i alt=%i shift=%i Double=%i\n",
						   m.x, m.y,
						   m.Down(), m.Left(), m.Right(), m.Middle(),
						   m.Ctrl(), m.Alt(), m.Shift(), m.Double());
#endif
					
#if 1
					if ((m.Target = v->WindowFromPoint(m.x - Client.left, m.y - Client.top)))
					{
						m.ToView();
						
						GView *v = m.Target->GetGView();
						if (v)
						{
							v->_Mouse(m, true);
						}
					}
#endif
					break;
				}
				case kEventMouseWheelMoved:
				{
					UInt32		modifierKeys;
					Point		Pt;
					Rect		Client;
					SInt32		Delta;
					GViewI		*Target;
					
					GetWindowBounds(v->WindowHandle(), kWindowContentRgn, &Client);
					
					OSStatus status = GetEventParameter(inEvent, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &Pt);
					if (status) printf("error(%i) getting kEventParamMouseLocation\n", (int)status);
					status = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifierKeys), NULL, &modifierKeys);
					if (status) printf("error(%i) getting kEventParamKeyModifiers\n", (int)status);
					status = GetEventParameter(inEvent, kEventParamMouseWheelDelta, typeSInt32, NULL, sizeof(Delta), NULL, &Delta);
					if (status) printf("error(%i) getting kEventParamMouseWheelDelta\n", (int)status);
					
					if ((Target = v->WindowFromPoint(Pt.h - Client.left, Pt.v - Client.top)))
					{
						GView *v = Target->GetGView();
						if (v)
						{
							v->OnMouseWheel(Delta < 0 ? 3 : -3);
						}
						else printf("%s:%i - no GView\n", __FILE__, __LINE__);
					}
					else printf("%s:%i - no target\n", __FILE__, __LINE__);
					break;
				}
			}
			break;
		}
		case kEventClassControl:
		{
			switch (eventKind)
			{
				case kEventControlDragEnter:
				{
					printf("kEventControlDragEnter\n");
					bool acceptDrop = true;
					SetEventParameter(	inEvent,
									  kEventParamControlWouldAcceptDrop,
									  typeBoolean,
									  sizeof(acceptDrop),
									  &acceptDrop);
					result = noErr;
					break;
				}
				case kEventControlDragWithin:
				{
					printf("kEventControlDragWithin\n");
					result = noErr;
					break;
				}
				case kEventControlDragLeave:
				{
					printf("kEventControlDragLeave\n");
					result = noErr;
					break;
				}
				case kEventControlDragReceive:
				{
					printf("kEventControlDragReceive\n");
					result = noErr;
					break;
				}
			}
			break;
		}
		case kEventClassUser:
		{
			if (eventKind == kEventUser)
			{
				GMessage m;
				
				OSStatus status = GetEventParameter(inEvent, kEventParamLgiEvent, typeUInt32, NULL, sizeof(UInt32), NULL, &m.m);
				status = GetEventParameter(inEvent, kEventParamLgiA, typeUInt32, NULL, sizeof(UInt32), NULL, &m.a);
				status = GetEventParameter(inEvent, kEventParamLgiB, typeUInt32, NULL, sizeof(UInt32), NULL, &m.b);
				
				v->OnEvent(&m);
				
				if (m.m == M_MOUSE_TRACK_UP && GView::_Capturing)
				{
					GMouse m;
					GView::_Capturing->GetMouse(m, false);
					GView::_Capturing->OnMouseClick(m);
				}
				
				result = noErr;
			}
			break;
		}
	}
	
	return result;
}

pascal OSStatus LgiRootCtrlProc(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
	OSStatus result = eventNotHandledErr;
	
	GView *v = (GView*)inUserData;
	if (!v)
	{
		LgiTrace("%s:%i - no gview\n", __FILE__, __LINE__);
		return result;
	}
	
	UInt32 eventClass = GetEventClass( inEvent );
	UInt32 eventKind = GetEventKind( inEvent );
	
	switch (eventClass)
	{
		case kEventClassControl:
		{
			switch (eventKind)
			{
				case kEventControlDraw:
				{
					CGContextRef Ctx = 0;
					result = GetEventParameter (inEvent,
												kEventParamCGContextRef,
												typeCGContextRef,
												NULL,
												sizeof(CGContextRef),
												NULL,
												&Ctx);
					if (Ctx)
					{
						GScreenDC s(v->GetWindow(), Ctx);
						v->_Paint(&s);
					}
					else
					{
						LgiTrace("%s:%i - No context.\n", __FILE__, __LINE__);
					}
					break;
				}
			}
			break;
		}
	}
	
	return result;
}
#endif

bool GWindow::Attach(GViewI *p)
{
	bool Status = false;
	
	if (Wnd)
	{
		if (GBase::Name())
			Name(GBase::Name());
		
		#if 0
		EventTypeSpec	WndEvents[] =
		{
			{ kEventClassCommand, kEventProcessCommand },
			
			{ kEventClassWindow, kEventWindowClose },
			{ kEventClassWindow, kEventWindowInit },
			{ kEventClassWindow, kEventWindowDispose },
			{ kEventClassWindow, kEventWindowBoundsChanged },
			{ kEventClassWindow, kEventWindowActivated },
			{ kEventClassWindow, kEventWindowShown },
			{ kEventClassWindow, kEventWindowCollapsing },
			
			{ kEventClassMouse, kEventMouseDown },
			{ kEventClassMouse, kEventMouseUp },
			{ kEventClassMouse, kEventMouseMoved },
			{ kEventClassMouse, kEventMouseDragged },
			{ kEventClassMouse, kEventMouseWheelMoved },
			
			{ kEventClassControl, kEventControlDragEnter },
			{ kEventClassControl, kEventControlDragWithin },
			{ kEventClassControl, kEventControlDragLeave },
			{ kEventClassControl, kEventControlDragReceive },
			
			{ kEventClassUser, kEventUser }
			
		};
		
		EventHandlerRef Handler = 0;
		OSStatus e = InstallWindowEventHandler(	Wnd,
											   d->EventUPP = NewEventHandlerUPP(LgiWindowProc),
											   GetEventTypeCount(WndEvents),
											   WndEvents,
											   (void*)this,
											   &Handler);
		if (e) LgiTrace("%s:%i - InstallEventHandler failed (%i)\n", _FL, e);
		
		e = HIViewFindByID(HIViewGetRoot(Wnd), kHIViewWindowContentID, &_View);
		if (_View)
		{
			EventTypeSpec	CtrlEvents[] =
			{
				{ kEventClassControl, kEventControlDraw },
			};
			EventHandlerRef CtrlHandler = 0;
			e = InstallEventHandler(GetControlEventTarget(_View),
									NewEventHandlerUPP(LgiRootCtrlProc),
									GetEventTypeCount(CtrlEvents),
									CtrlEvents,
									(void*)this,
									&CtrlHandler);
			if (e)
			{
				LgiTrace("%s:%i - InstallEventHandler failed (%i)\n", _FL, e);
			}
			
			HIViewChangeFeatures(_View, kHIViewIsOpaque, 0);
		}
		#endif
		
		Status = true;
		
		// Setup default button...
		if (!_Default)
		{
			_Default = FindControl(IDOK);
			if (_Default)
			{
				_Default->Invalidate();
			}
		}
		
		OnCreate();
		OnAttach();
		OnPosChange();
		
		// Set the first control as the focus...
		NextTabStop(this, 0);
	}
	
	return Status;
}

bool GWindow::OnRequestClose(bool OsShuttingDown)
{
	if (GetQuitOnClose())
	{
		LgiCloseApp();
	}
	
	return GView::OnRequestClose(OsShuttingDown);
}

bool GWindow::HandleViewMouse(GView *v, GMouse &m)
{
#ifdef MAC
	if (m.Down())
	{
#if 0
		
		GAutoPtr<GViewIterator> it(IterateViews());
		for (GViewI *n = it->Last(); n; n = it->Prev())
		{
			GPopup *p = dynamic_cast<GPopup*>(n);
			if (p)
			{
				GRect pos = p->GetPos();
				if (!pos.Overlap(m.x, m.y))
				{
					printf("Closing Popup, m=%i,%i not over pos=%s\n", m.x, m.y, pos.GetStr());
					p->Visible(false);
				}
			}
			else break;
		}
		
#else
		
		bool ParentPopup = false;
		GViewI *p = m.Target;
		while (p->GetParent())
		{
			if (dynamic_cast<GPopup*>(p))
			{
				ParentPopup = true;
				break;
			}
			p = p->GetParent();
		}
		if (!ParentPopup)
		{
			for (int i=0; i<GPopup::CurrentPopups.Length(); i++)
			{
				GPopup *pu = GPopup::CurrentPopups[i];
				if (pu->Visible())
					pu->Visible(false);
			}
		}
		
#endif
		
		if (!m.IsMove() && LgiApp)
		{
			GMouseHook *mh = LgiApp->GetMouseHook();
			if (mh)
				mh->TrackClick(v);
		}
	}
#endif
	
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GMouseEvents)
		{
			if (!d->Hooks[i].Target->OnViewMouse(v, m))
			{
				return false;
			}
		}
	}
	
	return true;
}


bool GWindow::HandleViewKey(GView *v, GKey &k)
{
	bool Status = false;
	GViewI *Ctrl = 0;
	
	// Give key to popups
	if (LgiApp &&
		LgiApp->GetMouseHook() &&
		LgiApp->GetMouseHook()->OnViewKey(v, k))
	{
		goto AllDone;
	}
	
	// Allow any hooks to see the key...
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GKeyEvents)
		{
			if (d->Hooks[i].Target->OnViewKey(v, k))
			{
				Status = true;
				
#if DEBUG_KEYS
				printf("Hook ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
#endif
				
				goto AllDone;
			}
		}
	}
	
	// Give the key to the window...
	if (v->OnKey(k))
	{
#if DEBUG_KEYS
		printf("View ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
			   k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
#endif
		
		Status = true;
		goto AllDone;
	}
	
	// Window didn't want the key...
	switch (k.c16)
	{
		case VK_RETURN:
		{
			Ctrl = _Default;
			break;
		}
		case VK_ESCAPE:
		{
			Ctrl = FindControl(IDCANCEL);
			break;
		}
	}
	
	if (Ctrl && Ctrl->Enabled())
	{
		if (Ctrl->OnKey(k))
		{
			Status = true;
			
#if DEBUG_KEYS
			printf("Default Button ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
				   k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
#endif
			
			goto AllDone;
		}
	}
	
	if (Menu)
	{
		Status = Menu->OnKey(v, k);
		if (Status)
		{
#if DEBUG_KEYS
			printf("Menu ate '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
#endif
		}
	}
	
	// Command+W closes the window... if it doesn't get nabbed earlier.
	if (k.Down() &&
		k.System() &&
		tolower(k.c16) == 'w')
	{
		// Close
		if (d->CloseRequestDone || OnRequestClose(false))
		{
			d->CloseRequestDone = true;
			delete this;
			return true;
		}
	}
	
AllDone:
	d->LastKey = k;
	
	return Status;
}


void GWindow::Raise()
{
	if (Wnd)
	{
		// BringToFront(Wnd);
	}
}

GWindowZoom GWindow::GetZoom()
{
	if (Wnd)
	{
		#if 0
		bool c = IsWindowCollapsed(Wnd);
		// printf("IsWindowCollapsed=%i\n", c);
		if (c)
			return GZoomMin;
		
		c = IsWindowInStandardState(Wnd, NULL, NULL);
		// printf("IsWindowInStandardState=%i\n", c);
		if (!c)
			return GZoomMax;
		#endif
	}
	
	return GZoomNormal;
}

void GWindow::SetZoom(GWindowZoom i)
{
	#if 0
	OSStatus e = 0;
	switch (i)
	{
		case GZoomMin:
		{
			e = CollapseWindow(Wnd, true);
			if (e) printf("%s:%i - CollapseWindow failed with %i\n", _FL, (int)e);
			// else printf("GZoomMin ok.\n");
			break;
		}
		default:
		case GZoomNormal:
		{
			e = CollapseWindow(Wnd, false);
			if (e) printf("%s:%i - [Un]CollapseWindow failed with %i\n", _FL, (int)e);
			// else printf("GZoomNormal ok.\n");
			break;
		}
	}
	#endif
}

GViewI *GWindow::GetDefault()
{
	return _Default;
}

void GWindow::SetDefault(GViewI *v)
{
	if (v &&
		v->GetWindow() == (GViewI*)this)
	{
		if (_Default != v)
		{
			GViewI *Old = _Default;
			_Default = v;
			
			if (Old) Old->Invalidate();
			if (_Default) _Default->Invalidate();
		}
	}
	else
	{
		_Default = 0;
	}
}

bool GWindow::Name(const char *n)
{
	bool Status = GBase::Name(n);
	
	if (Wnd)
	{
		NSString *ns = [NSString stringWithCString:n encoding:NSUTF8StringEncoding];
		Wnd.p.title = ns;
		[ns release];
	}
	
	return Status;
}

char *GWindow::Name()
{
	return GBase::Name();
}

GRect &GWindow::GetClient(bool ClientSpace)
{
	static GRect r;
	if (Wnd)
	{
		r = Wnd.p.contentView.frame;
		if (ClientSpace)
			r.Offset(-r.x1, -r.y1);
	}
	else
	{
		r.ZOff(-1, -1);
	}
	return r;
}

bool GWindow::SerializeState(GDom *Store, const char *FieldName, bool Load)
{
	if (!Store || !FieldName)
		return false;
	
	if (Load)
	{
		GVariant v;
		if (Store->GetValue(FieldName, v) && v.Str())
		{
			GRect Position(0, 0, -1, -1);
			GWindowZoom State = GZoomNormal;
			
			GToken t(v.Str(), ";");
			for (int i=0; i<t.Length(); i++)
			{
				char *Var = t[i];
				char *Value = strchr(Var, '=');
				if (Value)
				{
					*Value++ = 0;
					
					if (stricmp(Var, "State") == 0)
						State = (GWindowZoom) atoi(Value);
					else if (stricmp(Var, "Pos") == 0)
						Position.SetStr(Value);
				}
				else return false;
			}
			
			if (Position.Valid())
			{
				// int Sy = GdcD->Y();
				// Position.y2 = min(Position.y2, Sy - 50);
				SetPos(Position);
			}
			
			SetZoom(State);
		}
		else return false;
	}
	else
	{
		char s[256];
		GWindowZoom State = GetZoom();
		sprintf(s, "State=%i;Pos=%s", State, GetPos().GetStr());
		
		GVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}
	
	return true;
}

GRect &GWindow::GetPos()
{
	if (Wnd)
		Pos = [Wnd.p frame];
	
	return Pos;
}

bool GWindow::SetPos(GRect &p, bool Repaint)
{
	int x = GdcD->X();
	int y = GdcD->Y();
	
	GRect r = p;
	#if 0
	int MenuY = GetMBarHeight();
	
	if (r.y1 < MenuY)
		r.Offset(0, MenuY - r.y1);
	#endif
	if (r.y2 > y)
		r.y2 = y - 1;
	if (r.X() > x)
		r.x2 = r.x1 + x - 1;
	
	Pos = r;
	/*
	if (Wnd)
		[Wnd.p setFrame:Pos display:YES];
		*/
	
	return true;
}

void GWindow::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	if (dynamic_cast<GPopup*>(Wnd))
	{
		printf("%s:%i - Ignoring GPopup in OnChildrenChanged handler.\n", _FL);
		return;
	}
	PourAll();
}

void GWindow::OnCreate()
{
}

void GWindow::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

void GWindow::OnPosChange()
{
	GView::OnPosChange();
	
	if (d->Sx != X() ||	d->Sy != Y())
	{
		PourAll();
		d->Sx = X();
		d->Sy = Y();
	}
}

#define IsTool(v) \
( \
	dynamic_cast<GView*>(v) \
	&& \
	dynamic_cast<GView*>(v)->_IsToolBar \
)

void GWindow::PourAll()
{
	GRect r = GetClient();
	// printf("::Pour r=%s\n", r.GetStr());
	GRegion Client(r);
	
	GRegion Update(Client);
	bool HasTools = false;
	GViewI *v;
	List<GViewI>::I Lst = Children.begin();
	
	{
		GRegion Tools;
		
		for (v = *Lst; v; v = *++Lst)
		{
			if (IsTool(v))
			{
				GRect OldPos = v->GetPos();
				Update.Union(&OldPos);
				
				if (HasTools)
				{
					// 2nd and later toolbars
					if (v->Pour(Tools))
					{
						if (!v->Visible())
						{
							v->Visible(true);
						}
						
						if (OldPos != v->GetPos())
						{
							// position has changed update...
							v->Invalidate();
						}
						
						Tools.Subtract(&v->GetPos());
						Update.Subtract(&v->GetPos());
					}
				}
				else
				{
					// First toolbar
					if (v->Pour(Client))
					{
						HasTools = true;
						
						if (!v->Visible())
						{
							v->Visible(true);
						}
						
						if (OldPos != v->GetPos())
						{
							v->Invalidate();
						}
						
						GRect Bar(v->GetPos());
						Bar.x2 = GetClient().x2;
						
						Tools = Bar;
						Tools.Subtract(&v->GetPos());
						Client.Subtract(&Bar);
						Update.Subtract(&Bar);
					}
				}
			}
		}
	}
	
	Lst = Children.begin();
	for (GViewI *v = *Lst; v; v = *++Lst)
	{
		if (!IsTool(v))
		{
			GRect OldPos = v->GetPos();
			Update.Union(&OldPos);
			
			if (v->Pour(Client))
			{
				// GRect p = v->GetPos();
				
				if (!v->Visible())
				{
					v->Visible(true);
				}
				
				v->Invalidate();
				
				Client.Subtract(&v->GetPos());
				Update.Subtract(&v->GetPos());
			}
			else
			{
				// non-pourable
			}
		}
	}
	
	for (int i=0; i<Update.Length(); i++)
	{
		Invalidate(Update[i]);
	}
	
}

GMessage::Result GWindow::OnEvent(GMessage *m)
{
	switch (MsgCode(m))
	{
		case M_CLOSE:
		{
			if (d->CloseRequestDone || OnRequestClose(false))
			{
				d->CloseRequestDone = true;
				Quit();
				return 0;
			}
			break;
		}
	}
	
	return GView::OnEvent(m);
}

bool GWindow::RegisterHook(GView *Target, GWindowHookType EventType, int Priority)
{
	bool Status = false;
	
	if (Target && EventType)
	{
		ssize_t i = d->GetHookIndex(Target, true);
		if (i >= 0)
		{
			d->Hooks[i].Flags = EventType;
			Status = true;
		}
	}
	
	return Status;
}

bool GWindow::UnregisterHook(GView *Target)
{
	ssize_t i = d->GetHookIndex(Target);
	if (i >= 0)
	{
		d->Hooks.DeleteAt(i);
		return true;
	}
	return false;
}

GViewI *GWindow::WindowFromPoint(int x, int y, bool Debug)
{
	for (int i=0; i<GPopup::CurrentPopups.Length(); i++)
	{
		GPopup *p = GPopup::CurrentPopups[i];
		if (p->Visible())
		{
			GRect r = p->GetPos();
			if (r.Overlap(x, y))
			{
				// printf("WindowFromPoint got %s click (%i,%i)\n", p->GetClass(), x, y);
				return p->WindowFromPoint(x - r.x1, y - r.y1, Debug);
			}
		}
	}
	
	return GView::WindowFromPoint(x, y, Debug);
}

int GWindow::OnCommand(int Cmd, int Event, OsView SrcCtrl)
{
	#if 0
	OsView v;
	switch (Cmd)
	{
		case kHICommandCut:
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_CUT);
			break;
		}
		case kHICommandCopy:
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_COPY);
			break;
		}
		case kHICommandPaste:
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_PASTE);
			break;
		}
		case 'dele':
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_DELETE);
			break;
		}
	}
	#endif
	
	return 0;
}

void GWindow::OnTrayClick(GMouse &m)
{
	if (m.Down() || m.IsContextMenu())
	{
		GSubMenu RClick;
		OnTrayMenu(RClick);
		if (GetMouse(m, true))
		{
#if WINNATIVE
			SetForegroundWindow(Handle());
#endif
			int Result = RClick.Float(this, m.x, m.y);
#if WINNATIVE
			PostMessage(Handle(), WM_NULL, 0, 0);
#endif
			OnTrayMenuResult(Result);
		}
	}
}

bool GWindow::Obscured()
{
	if (!Wnd)
		return false;

	auto s = [Wnd.p occlusionState];
	return !(s & NSWindowOcclusionStateVisible);
}
