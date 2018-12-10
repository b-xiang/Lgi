/// \file
/// \author Matthew Allen, fret@memecode.com
#ifndef _MAIL_STORE_H_
#define _MAIL_STORE_H_

#include "Mail.h"
#include "Store3Defs.h"
#undef GetObject

/*
	Handling of attachments in the Store3 API
	-----------------------------------------

	Given the mail object ptr:

		GDataI *m;

	Query that the mail for it's root mime segment:

		GDataI *Seg = dynamic_cast<GDataI*>(m->GetObj(FIELD_MIME_SEG));

	Query a seg for it's children:

		GDataIt Children = Seg->GetList(FIELD_MIME_SEG);
		GDataI *FirstChild = Children->First();

	Access segment's charset and mimetype:

		char *Charset = Seg->GetStr(FIELD_CHARSET);
		char *MimeType = Seg->GetStr(FIELD_MIME_TYPE);

	Get/Set the segment for it's content:

		GAutoStreamI Data = Seg->GetStream(_FL); // get
		Seg->SetStream(new MyStream(Data)); // set

	Delete an mime segment:

		Seg->Delete();

	Add a new segment somewhere in the tree, including reparenting 
	it to another segments or mail object, even if the target parent it
	not attached yet:
		
		NewSeg->Save(ParentSeg);

*/


#include "LgiInterfaces.h"
#include "GMime.h"
#include "GOptionsFile.h"
#include "GVariant.h"

class GDataI;
class GDataFolderI;
class GDataStoreI;
class GDataPropI;
typedef GAutoPtr<GStreamI> GAutoStreamI;
void ParseIdList(char *In, List<char> &Out);

/// A storage event
///		a = StoreId
///     b = (void*)UserParam
/// \sa GDataEventsI::Post
#define M_STORAGE_EVENT				(M_USER+0x500)

/// The storage class has this property (positive properties are owned by the app
#define FIELD_IS_ONLINE				-100
#define FIELD_PROFILE_IMAP_LISTING	-101
#define FIELD_PROFILE_IMAP_SELECT	-102

#define GDATA_INT32_PROP(name, id) \
	int32 Get##name() { return GetObject() ? (int32)GetObject()->GetInt(id) : OnError(_FL); } \
	bool Set##name(int32 val) { return GetObject() ? GetObject()->SetInt(id, val) >= Store3Delayed : OnError(_FL); }

#define GDATA_INT64_PROP(name, id) \
	int64 Get##name() { return GetObject() ? GetObject()->GetInt(id) : OnError(_FL); } \
	bool Set##name(int64 val) { return GetObject() ? GetObject()->SetInt(id, val) >= Store3Delayed : OnError(_FL); }

#define GDATA_ENUM_PROP(name, id, type) \
	type Get##name() { return (type) (GetObject() ? GetObject()->GetInt(id) : OnError(_FL)); } \
	bool Set##name(type val) { return GetObject() ? GetObject()->SetInt(id, (int)val) >= Store3Delayed : OnError(_FL); }

#define GDATA_STR_PROP(name, id) \
	char *Get##name() { auto o = GetObject(); return o ? o->GetStr(id) : (char*)OnError(_FL); } \
	bool Set##name(const char *val) { return GetObject() ? GetObject()->SetStr(id, val) >= Store3Delayed : OnError(_FL); }

#define GDATA_DATE_PROP(name, id) \
	LDateTime *Get##name() { return (GetObject() ? GetObject()->GetDate(id) : (LDateTime*)OnError(_FL)); } \
	bool Set##name(LDateTime *val) { return GetObject() ? GetObject()->SetDate(id, val) >= Store3Delayed : OnError(_FL); }

#define GDATA_PERM_PROP(name, id) \
	ScribePerm Get##name() { return (ScribePerm) (GetObject() ? GetObject()->GetInt(id) : OnError(_FL)); } \
	bool Set##name(ScribePerm val) { return GetObject() ? GetObject()->SetInt(id, val) >= Store3Delayed : OnError(_FL); }


/// This class is an interface to a collection of objects (NOT thread-safe).
template <class T>
class GDataIterator
{
public:
	virtual ~GDataIterator() {}

	/// \returns an empty object of the right type.
	virtual T Create(GDataStoreI *Store) = 0;
	/// \returns the first object (NOT thread-safe)
	virtual T First() = 0;
	/// \returns the first object (NOT thread-safe)
	virtual T Next() = 0;
	/// \returns the number of items in the collection
	virtual size_t Length() = 0;
	/// \returns the 'nth' item in the collection
	virtual T operator [](size_t idx) = 0;
	/// \returns the index of the given item in the collection
	virtual ssize_t IndexOf(T n, bool NoAssert = false) = 0;
	/// Deletes an item
	/// \returns true on success
	virtual bool Delete(T ptr) = 0;
	/// Inserts an item at 'idx' or the end if not supplied.
	/// \returns true on success
	virtual bool Insert(T ptr, ssize_t idx = -1, bool NoAssert = false) = 0;
	/// Clears list, but doesn't delete objects.
	/// \returns true on success
	virtual bool Empty() = 0;
	/// Deletes all the objects from memory
	/// \returns true on success
	virtual bool DeleteObjects() = 0;
	/// Gets the current loading/loaded state.
	virtual Store3State GetState() = 0;
};


typedef GDataIterator<GDataPropI*> *GDataIt;

#define EmptyVirtual(t)		LgiAssert(0); return t
#define Store3CopyDecl		bool CopyProps(GDataPropI &p)
#define Store3CopyImpl(Cls)	bool Cls::CopyProps(GDataPropI &p)

/// A generic interface for getting / setting properties.
class GDataPropI : virtual public GDom
{
	virtual GDataPropI &operator =(GDataPropI &p) { return *this; }

public:
	virtual ~GDataPropI() {}

	/// Copy all the values from 'p' over to this object
	virtual bool CopyProps(GDataPropI &p) { return false; }

	/// Gets a string property
	virtual char *GetStr(int id) { EmptyVirtual(NULL); }
	/// Sets a string property, it will make a copy of the string, so you
	/// still retain ownership of the string you're passing in.
	virtual Store3Status SetStr(int id, const char *str) { EmptyVirtual(Store3Error); }

	/// Gets an integer property.
	virtual int64 GetInt(int id) { EmptyVirtual(false); }
	/// Sets an interger property.
	virtual Store3Status SetInt(int id, int64 i) { EmptyVirtual(Store3Error); }

	/// Gets a date property
	virtual LDateTime *GetDate(int id) { EmptyVirtual(NULL); }
	/// Sets a date property
	virtual Store3Status SetDate(int id, LDateTime *i) { EmptyVirtual(Store3Error); }

	/// Gets a variant
	virtual GVariant *GetVar(int id) { EmptyVirtual(NULL); }
	/// Sets a variant property
	virtual Store3Status SetVar(int id, GVariant *i) { EmptyVirtual(Store3Error); }

	/// Gets a sub object pointer
	virtual GDataPropI *GetObj(int id) { EmptyVirtual(NULL); }
	/// Sets a sub object pointer
	virtual Store3Status SetObj(int id, GDataPropI *i) { EmptyVirtual(Store3Error); }
	
	/// Gets an iterator interface to a list of sub-objects.
	virtual GDataIt GetList(int id) { EmptyVirtual(NULL); }
	/// Set the mime segments
	virtual Store3Status SetRfc822(GStreamI *Rfc822Msg) { LgiAssert(!"Pretty sure you should be implementing this"); return Store3Error; }
};

#pragma warning(default:4263)

class GDataUserI
{
	friend class GDataI;
	GDataI *Object;

public:
	GDataUserI();
	virtual ~GDataUserI();

	GDataI *GetObject();
	virtual bool SetObject(GDataI *o);
};

/// This class is an interface between the UI and the backend for things
/// like email, contacts, calendar events, groups and filters
class GDataI : virtual public GDataPropI
{
	friend class GDataUserI;
	virtual GDataI &operator =(GDataI &p) { return *this; }

public:
	GDataUserI *UserData;

	GDataI() { UserData = NULL; }
	virtual ~GDataI()
	{
		if (UserData)
			UserData->Object = NULL;
	}

	/// Returns the type of object
	/// \sa MAGIC_MAIL and it's like
	virtual int Type() = 0;
	/// \return true if the object has been written to disk. By default the object
	/// starts life in memory only.
	virtual bool IsOnDisk() = 0;
	/// \return true if the object is owned by some other object...
	virtual bool IsOrphan() = 0;
	/// \returns size of object on disk
	virtual uint64 Size() = 0;
	/// Saves the object to disk. If this function fails the object
	/// is deleted, so if it returns false, stop using the ptr you
	/// have to it.
	/// \returns true if successful.
	virtual Store3Status Save(GDataI *Parent = 0) = 0;
	/// Delete the on disk representation of the object. This will cause GDataEventsI::OnDelete
	/// to be called after which this object will be freed from heap memory automatically. So
	/// Once you call this method assume the object pointed at is gone.
	virtual Store3Status Delete(bool ToTrash = true) = 0;
	/// Gets the storage that this object belongs to.
	virtual GDataStoreI *GetStore() = 0;
	/// \returns a stream to access the data stored at this node. The caller
	/// is responsible to free the stream when finished with it.
	/// For Type == MAGIC_ATTACHMENT: the decoded body of the MIME segment.
	/// For Type == MAGIC_MAIL: is an RFC822 encoded version of the email.
	/// For other objects the stream is not defined.
	virtual GAutoStreamI GetStream(const char *file, int line) = 0;
	/// Sets the stream, which is used during the next call to GDataI::Save, which
	/// also deletes the object when it's used. The caller loses ownership of the
	/// object passed into this function.
	virtual bool SetStream(GAutoStreamI stream) { return false; }
	/// Parses the headers of the object and updates all the metadata fields
	virtual bool ParseHeaders() { return false; }
};

/// An interface to a folder structure
class GDataFolderI : virtual public GDataI
{
	virtual GDataFolderI &operator =(GDataFolderI &p) { return *this; }

public:
	virtual ~GDataFolderI() {}

	/// \returns an iterator for the sub-folders.
	virtual GDataIterator<GDataFolderI*> &SubFolders() = 0;
	/// \returns an iterator for the child objects
	virtual GDataIterator<GDataI*> &Children() = 0;
	/// \returns an iterator for the fields this folder defines
	virtual GDataIterator<GDataPropI*> &Fields() = 0;
	/// Deletes all child objects from disk and memory.
	/// \return true on success;
	virtual Store3Status DeleteAllChildren() { return Store3Error; }
	/// Frees all the memory used by children objects without deleting from disk
	virtual Store3Status FreeChildren() { return Store3Error; }
	/// Called when the user selects the folder in the UI
	virtual void OnSelect(bool s) {}
	/// Called when the user selects a relevant context menu command
	virtual void OnCommand(const char *Name) {}
};

#pragma warning(error:4263)

/// Event callback interface. Calls to these methods may be in a worker
/// thread, so make appropriate locking or pass the event off to the GUI
/// thread via a message.
class GDataEventsI
{
public:
	virtual ~GDataEventsI() {}

	/// Posts something to the GUI thread
	/// \sa M_STORAGE_EVENT
	virtual void Post(GDataStoreI *store, void *Param) {}
	/// \returns the system path
	virtual bool GetSystemPath(int Folder, GVariant &Path) { return false; }
	/// \returns the options object
	virtual GOptionsFile *GetOptions(bool Create = false) { return 0; }
	/// A new item is available
	virtual void OnNew(GDataFolderI *parent, GArray<GDataI*> &new_items, int pos, bool is_new) = 0;
	/// When an item is deleted
	virtual bool OnDelete(GDataFolderI *parent, GArray<GDataI*> &items) = 0;
	/// When an item is moved to a new folder
	virtual bool OnMove(GDataFolderI *new_parent, GDataFolderI *old_parent, GArray<GDataI*> &items) = 0;
	/// When an item changes
	virtual bool OnChange(GArray<GDataI*> &items, int FieldHint) = 0;
	/// Notifcation of property change
	virtual void OnPropChange(GDataStoreI *Store, int Prop, GVariantType Type) {}
	/// Get the logging stream
	virtual GStreamI *GetLogger(GDataStoreI *store) { return 0; }
	/// Search for a object by type and name
	virtual bool Match(GDataStoreI *store, GDataPropI *Addr, int ObjectType, GArray<GDom*> &Matches) { return 0; }

	#ifdef _DEBUG
	virtual void DebugDelete(GDataI *Obj) {}
	#endif
};

/// The virtual mail storage interface from which all mail stores inherit from.
///
/// The data store should implement GDataPropI::GetInt and handle these properties:
///	- FIELD_STATUS, acceptable returns value are:
///		* 0 - mail store is in error.
///		* 1 - mail store is ready to use / ok.
///		* 2 - mail store requires upgrading to use.
///		These are codified in the enum GDataStoreI::DataStoreStatus
/// - FIELD_READONLY, return values are:
///		* false - mail store is read/write
///		* true - mail store is read only
/// - FIELD_VERSION, existing return values are:
///		* 3 - a 'mail3' Scribe sqlite database.
///		* 10 - the Scribe IMAP implementation.
///		If you are create a new mail store use, values above 10 for your implementation
///		and optionally register them with Memecode for inclusion here.
///	- FIELD_IS_ONLINE, optionally returned if mail store is online or not.
/// - FIELD_ACCOUNT_ID, optionally return if the mail store is associated with an account.
///
/// The data store may optionally implement GDataPropI::SetInt to handle this property:
///	- FIELD_IS_ONLINE, acceptable values are:
///		* false - take the mail store offline.
///		* true - go online.
///		This is currently only implemented on the IMAP mail store.
class GDataStoreI : virtual public GDataPropI
{
public:
	static LHashTbl<IntKey<int>,GDataStoreI*> Map;
	int Id;

	class GDsTransaction
	{
	protected:
		GDataStoreI *Store;

	public:
		GDsTransaction(GDataStoreI *s)
		{
			Store = s;
		}

		virtual ~GDsTransaction() {}
	};

	typedef GAutoPtr<GDsTransaction> StoreTrans;

	GDataStoreI()
	{
		LgiAssert(LgiApp->InThread());
		while (Map.Find(Id = LgiRand(1000)))
			;
		Map.Add(Id, this);
	}

	virtual ~GDataStoreI()
	{
		LgiAssert(LgiApp->InThread());
		if (!Map.Delete(Id))
			LgiAssert(!"Delete failed.");
	}

	/// \returns size of object on disk
	virtual uint64 Size() = 0;
	
	/// Create a new data object that isn't written to disk yet
	virtual GDataI *Create(int Type) = 0;
	
	/// Get the root folder object
	virtual GDataFolderI *GetRoot(bool create = false) = 0;
	
	/// Move objects into a different folder.
	///
	/// Success:
	///		'Items' are owned by 'NewFolder', and not any previous folder.
	///		Any GDataEventsI interface owned by the data store has it's 'OnMove' method called.
	///
	/// Failure:
	///		'Item' is owned by it's previous folder.
	///
	/// \return true on success.
	virtual Store3Status Move
	(
		/// The folder to move the object to
		GDataFolderI *NewFolder,
		/// The object to move
		GArray<GDataI*> &Items
	) = 0;
	
	/// Deletes items, which results in either the items being moved to the local trash folder 
	/// or the items being completely deleted if there is no local trash. The items should all
	/// be in the same folder and of the same type.
	///
	/// Success:
	///		'Items' are either owned by the local trash and not any previous folder, and
	///		GDataEventsI::OnMove is called.
	///			-or-
	///		'Items' are completely deleted and removed from it's parent and
	///		GDataEventsI::OnDelete is called, after which the objects are freed.
	///
	/// Failure:
	///		'Items' are owned by it's previous folder.
	///
	/// \return true on success.
	virtual Store3Status Delete
	(
		/// The object to delete
		GArray<GDataI*> &Items,
		/// Send to the trash or not...
		bool ToTrash
	) = 0;
	
	/// Changes items, which results in either the items properties being adjusted.
	///
	/// Success:
	///		The items properties are changed, and the GDataEventsI::OnChange callback 
	///		is made.
	///
	/// Failure:
	///		Items are not changed. No callback is made.
	///
	/// \return true on success.
	virtual Store3Status Change
	(
		/// The object to change
		GArray<GDataI*> &Items,
		/// The property to change...
		int PropId,
		/// The value to assign
		/// (GV_INT32/64 -> SetInt, GV_DATETIME -> SetDateTime, GV_STRING -> SetStr)
		GVariant &Value
	) = 0;
	
	/// Compact the mail store
	virtual bool Compact
	(
		/// The parent window of the UI
		GViewI *Parent,
		/// The store should pass information up to the UI via setting various parameters from Store3UiFields
		GDataPropI *Props
	) = 0;
	
	/// Upgrades the mail store to the current version for this build. You should call this in response
	/// to getting Store3UpgradeRequired back from this->GetInt(FIELD_STATUS).
	virtual bool Upgrade
	(
		/// The parent window of the UI
		GViewI *Parent,
		/// The store should pass information up to the UI via setting various parameters from Store3UiFields
		GDataPropI *Props
	) { return false; }

	/// Tries to repair the database.
	virtual bool Repair
	(
		/// The parent window of the UI
		GViewI *Parent,
		/// The store should pass information up to the UI via setting various parameters from Store3UiFields
		GDataPropI *Props
	) { return false; }
	
	/// Set the sub-format
	virtual bool SetFormat
	(
		/// The parent window of the UI
		GViewI *Parent,
		/// The store should pass information up to the UI via setting various parameters from Store3UiFields
		GDataPropI *Props
	) { return false; }
	
	/// Called when event posted
	virtual void OnEvent(void *Param) = 0;
	
	/// Called when the application is not receiving messages.
	/// \returns false to wait for more messages.
	virtual bool OnIdle() = 0;
	
	/// Gets the events interface
	virtual GDataEventsI *GetEvents() = 0;
	
	/// Start a scoped transaction
	virtual StoreTrans StartTransaction() { return StoreTrans(0); }
};

/// Open a mail3 folder
/// \return a valid ptr or NULL on failure
extern GDataStoreI *OpenMail3
(
	/// The file to open
	char *Mail3Folder,
	/// Event interface,
	GDataEventsI *Callback,
	/// true if you want to create a new mail3 file.
	bool Create = false
);

/// Open am imap store
/// \return a valid ptr or NULL on failure
extern GDataStoreI *OpenImap
(
	/// The host name of the IMAP server
	char *Host,
	/// The port to connect to, or <= 0 means use default
	int Port,
	/// The user name of the account to connect to
	char *User,
	/// [Optional] The password of the user
	char *Pass,
	/// Various flags that control the type of connection made:
	/// \sa #MAIL_SSL, #MAIL_SECURE_AUTH
	int ConnectFlags,
	/// Callback interface for various events...
	GDataEventsI *Callback,
	/// This allows the IMAP client to request SSL support from the
	/// parent applications.
	GCapabilityClient *caps,
	/// Pointers to the progress info bars, or NULL if not needed.
	MailProtocolProgress *prog[2],
	/// The logging stream.
	GStream *Log,
	/// The identifier for the account
	int AccoundId,
	/// An interface into the persistant storage area.
	GAutoPtr<class ProtocolSettingStore> store
);

#ifdef WIN32
/// Open a MAPI store
/// \return a valid ptr or NULL on failure
extern GDataStoreI *OpenMapiStore
(
	/// The MAPI profile name
	const char *Profile,
	/// The username to login as
	const char *Username,
	/// Their password
	const char *Password,
	/// The account ID
	uint64 AccountId,
	/// Event interface,
	GDataEventsI *Callback
);
#endif

//////////////////////////////////////////////////////////////////////////////
// Common implementation of interfaces
template <class T>
class DNullIterator : public GDataIterator<T>
{
public:
	T First() { return 0; }
	T Next() { return 0; }
	int Length() { return 0; }
	T operator [](int idx) { return 0; }
	bool Delete(T ptr) { return 0; }
	bool Insert(T ptr, int idx = -1, bool NoAssert = false) { return 0; }
	bool DeleteObjects() { return 0; }
	bool Empty() { return false; }
	int IndexOf(T n, bool NoAssert = false) { return -1; }
};

template <typename TPub, typename TPriv, typename TStore>
class DIterator : public GDataIterator<TPub*>
{
	int Cur;

public:
	GArray<TPriv*> a;
	Store3State State;
	
	DIterator()
	{
		Cur = -1;
		State = Store3Unloaded;
	}

	Store3State GetState() { return State; }

	TPub *Create(GDataStoreI *Store)
	{
		LgiAssert(State == Store3Loaded);
		return new TPriv(dynamic_cast<TStore*>(Store));
	}

	TPub *First()
	{
		LgiAssert(State == Store3Loaded);
		Cur = 0;
		return (int)a.Length() > Cur ? a[Cur] : 0;
	}
	
	TPub *Next()
	{
		LgiAssert(State == Store3Loaded);
		Cur++;
		return (int)a.Length() > Cur ? a[Cur] : 0;
	}
	
	size_t Length()
	{
		return a.Length();
	}
	
	TPub *operator [](size_t idx)
	{
		LgiAssert(State == Store3Loaded);
		return a[idx];
	}
	
	bool Delete(TPub *pub_ptr)
	{
		LgiAssert(State == Store3Loaded);
		TPriv *priv_ptr = dynamic_cast<TPriv*>(pub_ptr);
		if (!priv_ptr)
		{
			LgiAssert(!"Not the right type of object.");
			return false;
		}

		ssize_t i = a.IndexOf(priv_ptr);
		if (i < 0)
			return false;

		a.DeleteAt(i, true);
		return true;
	}
	
	bool Insert(TPub *pub_ptr, ssize_t idx = -1, bool NoAssert = false)
	{
		if (!NoAssert)
			LgiAssert(State == Store3Loaded);
		TPriv *priv_ptr = dynamic_cast<TPriv*>(pub_ptr);
		if (!priv_ptr)
		{
			LgiAssert(!"Not the right type of object.");
			return false;
		}

		return a.AddAt(idx < 0 ? a.Length() : idx, priv_ptr);
	}
	
	bool Empty()
	{
		LgiAssert(State == Store3Loaded);
		a.Length(0);
		return true;
	}
	
	bool DeleteObjects()
	{
		a.DeleteObjects();
		return true;
	}
	
	ssize_t IndexOf(TPub *pub_ptr, bool NoAssert = false)
	{
		if (!NoAssert)
			LgiAssert(State == Store3Loaded);
		TPriv *priv_ptr = dynamic_cast<TPriv*>(pub_ptr);
		if (!priv_ptr)
		{
			LgiAssert(!"Not the right type of object.");
			return -1;
		}

		return a.IndexOf(priv_ptr);
	}
};

#endif
