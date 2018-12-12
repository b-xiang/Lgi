/// \file
/// \author Matthew Allen
/// \brief Growable, type-safe array.

#ifndef _GARRAY_H_
#define _GARRAY_H_

#include <stdlib.h>
#include <assert.h>

#define GARRAY_MIN_SIZE			16

#if defined(LGI_CHECK_MALLOC) && !defined(LGI_MEM_DEBUG)
#error "Include GMem.h first"
#endif

#if defined(PLATFORM_MINGW)
#ifdef __cplusplus
extern "C" {
#endif
#ifndef _QSORT_S_DEFINED
#define _QSORT_S_DEFINED
_CRTIMP void __cdecl qsort_s(void *_Base,
							size_t _NumOfElements,
							size_t _SizeOfElements,
							int (__cdecl *_PtFuncCompare)(void *,const void *,const void *),
							void *_Context);
#endif
#ifdef __cplusplus
}
#endif
#endif

#include "GRange.h"

/// \brief Growable type-safe array.
/// \ingroup Base
///
/// You can store simple objects inline in this array, but all their contents are initialized 
/// to the octet 0x00. Which limits use to objects that don't have a virtual table and
/// don't need construction (constructors are not called).
///
/// The objects are copied around during use so you can't assume their pointer
/// will remain the same over time either. However when objects are deleted from the
/// array their destructors WILL be called. This allows you to have simple objects that
/// have dynamically allocated pointers that are freed properly. A good example of this
/// type of object is the GVariant or GAutoString class.
///
/// If you want to store objects with a virtual table, or that need their constructor
/// to be called then you should create the GArray with pointers to the objects instead
/// of inline objects. And to clean up the memory you can call GArray::DeleteObjects or
/// GArray::DeleteArrays.
template <class Type>
class GArray
{
	Type *p;
	size_t len;
	size_t alloc;

#ifdef _DEBUG
public:
	int GetAlloc() { return alloc; }
#endif

protected:
	bool fixed;

public:
	typedef Type ItemType;

	/// Constructor
	GArray(size_t PreAlloc = 0)
	{
		p = 0;
		alloc = len = PreAlloc;
		fixed = false;
		if (alloc)
		{
			size_t Bytes = sizeof(Type) * alloc;
			p = (Type*) malloc(Bytes);
			if (p)
			{
				memset(p, 0, Bytes);
			}
			else
			{
				alloc = len = 0;
			}
		}
	}

	GArray(const GArray<Type> &c)
	{
		p = 0;
		alloc = len = 0;
		fixed = false;
		*this = c;
	}

	/// Destructor	
	~GArray()
	{
		Length(0);
	}	

	/// Does a range check on a pointer...
	/// \returns true if the pointer is pointing to a valid object
	/// in this array.
	bool PtrCheck(void *Ptr)
	{
		return	p != NULL &&
				Ptr >= p &&
				Ptr < &p[len];
	}

	/// Does a range check on an index...
	bool IdxCheck(ssize_t i)
	{
		return i >= 0 && i < len;
	}
	
	/// Returns the number of used entries
	size_t Length() const
	{
		return len;
	}
	
	/// Makes the length fixed..
	void SetFixedLength(bool fix = true)
	{
		fixed = fix;
	}

	/// Emtpies the array of all objects.
	bool Empty()
	{
		return Length(0);
	}

	/// Sets the length of available entries
	bool Length(size_t i)
	{
		if (i > 0)
		{
			if (i > len && fixed)
			{
				assert(!"Attempt to enlarged fixed array.");
				return false;
			}

			size_t nalloc = alloc;
			if (i < len)
			{
			    // Shrinking
			}
			else
			{
			    // Expanding
			    nalloc = 0x10;
				while (nalloc < i)
					nalloc <<= 1;
			    assert(nalloc >= i);
			}
			
			if (nalloc != alloc)
			{
				Type *np = (Type*)malloc(sizeof(Type) * nalloc);
				if (!np)
				{
					return false;
				}

				memset(np + len, 0, (nalloc - len) * sizeof(Type));
				if (p)
				{
					// copy across common elements
					memcpy(np, p, MIN(len, i) * sizeof(Type));
					free(p);
				}
				p = np;
				alloc = nalloc;
			}

			if (i > len)
			{
				// zero new elements
				memset(p + len, 0, sizeof(Type) * (nalloc - len));
			}
			else if (i < len)
			{
				for (size_t n=i; n<len; n++)
				{
					p[n].~Type();
				}
			}

			len = i;
		}
		else
		{
			if (p)
			{
				size_t Length = len;
				for (size_t i=0; i<Length; i++)
				{
					p[i].~Type();
				}
				free(p);
				p = 0;
			}
			len = alloc = 0;
		}

		return true;
	}

	GArray<Type> &operator =(const GArray<Type> &a)
	{
		Length(a.Length());
		if (p && a.p)
		{
			for (uint32 i=0; i<len; i++)
			{
				p[i] = a.p[i];
			}
		}
		return *this;
	}

	Type &First()
	{
		if (len <= 0)
		{
			LgiAssert(!"Must have one element.");
			static Type t;
			return t;
		}

		return p[0];
	}

	Type &Last()
	{
		if (len <= 0)
		{
			LgiAssert(!"Must have one element.");
			static Type t;
			return t;
		}
		
		return p[len-1];
	}

	/// This can be used instead of the [] operator in situations
	/// where the array is const.
	const Type &ItemAt(size_t i) const
	{
		if (i >= 0 && (uint32)i < len)
			return p[i];

		static Type t;
		return t;
	}

	// Returns the address of an item or NULL if index is out of range
	Type *AddressOf(size_t i = 0)
	{
		return i < len ? p + i : NULL;
	}

	/// \brief Returns a reference a given entry.
	///
	/// If the entry is off the end of the array and "fixed" is false,
	/// it will grow to make it valid.
	Type &operator [](size_t i)
	{
		static Type t;

		if
		(
			(fixed && (uint32)i >= len)
		)
		{
			memset(&t, 0, sizeof(t));
			if (fixed && (uint32)i >= len)
			{
				assert(!"Attempt to enlarged fixed array.");
			}
			return t;
		}
		
		if (i >= (int)alloc)
		{
			// increase array length
			size_t nalloc = MAX(alloc, GARRAY_MIN_SIZE);
			while (nalloc <= (uint32)i)
			{
				nalloc <<= 1;
			}

			#if 0
			if (nalloc > 1<<30)
			{
				#if defined(_DEBUG) && defined(_MSC_VER)
				LgiAssert(0);
				#endif
				
				ZeroObj(t);
				return t;
			}
			#endif

			// alloc new array
			Type *np = (Type*) malloc(sizeof(Type) * nalloc);
			if (np)
			{
				// clear new cells
				memset(np + len, 0, (nalloc - len) * sizeof(Type));
				if (p)
				{
					// copy across old cells
					memcpy(np, p, len * sizeof(Type));
					
					// clear old array
					free(p);
				}
				
				// new values
				p = np;
				alloc = nalloc;
			}
			else
			{
				static Type *t = 0;
				return *t;
			}
		}

		// adjust length of the the array
		if ((uint32)i + 1 > len)
		{
			len = i + 1;
		}
		
		return p[i];
	}

	/// Delete all the entries as if they are pointers to objects
	void DeleteObjects()
	{
		if (len > 0)
		{
			size_t InitialLen = len;
			delete p[0];
			if (InitialLen == len)
			{
				// Non self deleting
				for (uint i=1; i<len; i++)
				{
					delete p[i];
					p[i] = 0;
				}
				Length(0);
			}
			else
			{
				// Self deleting object...
				while (len)
				{
					delete p[0];
				}
			}
		}
	}
	
	/// Delete all the entries as if they are pointers to arrays
	void DeleteArrays()
	{
		for (uint32 i=0; i<len; i++)
		{
			DeleteArray(p[i]);
		}
		Length(0);
	}
	
	/// Find the index of entry 'n'
	ssize_t IndexOf(Type n)
	{
		for (uint i=0; i<len; i++)
		{
			if (p[i] == n) return i;
		}
		
		return -1;
	}

	/// Returns true if the item 'n' is in the array
	bool HasItem(Type n)
	{
		return IndexOf(n) >= 0;
	}

	/// Removes last element	
	bool PopLast()
	{
		if (len <= 0)
			return false;
		return Length(len - 1);
	}
	
	/// Deletes an entry
	bool DeleteAt
	(
		/// The index of the entry to delete
		size_t Index,
		/// true if the order of the array matters, otherwise false.
		bool Ordered = false
	)
	{
		if (p && Index < len)
		{
			// Delete the object
			p[Index].~Type();

			// Move the memory up
			if (Index < len - 1)
			{
				if (Ordered)
				{
					memmove(p + Index, p + Index + 1, (len - Index - 1) * sizeof(Type) );
				}
				else
				{
					p[Index] = p[len-1];
				}
			}

			// Adjust length
			len--;

			// Kill the element at the end... otherwise New() returns non-zero data.
			memset(p + len, 0, sizeof(Type));
			return true;
		}
		
		return false;
	}

	/// Deletes the entry 'n'
	bool Delete
	(
		/// The value of the entry to delete
		Type n,
		/// true if the order of the array matters, otherwise false.
		bool Ordered = false
	)
	{
		ssize_t i = IndexOf(n);
		if (p && i >= 0)
		{
			return DeleteAt(i, Ordered);
		}
		
		return false;
	}

	/// Appends an element
	void Add
	(
		/// Item to insert
		const Type &n
	)
	{
		(*this)[len] = n;
	}

	/// Appends multiple elements
	bool Add
	(
		/// Items to insert
		Type *s,
		/// Length of array
		ssize_t count
		
	)
	{
		if (!s || count < 1)
			return false;
			
		ssize_t i = len;
		if (!Length(len + count))
			return false;

		Type *d = p + i;
		while (count--)
		{
			*d++ = *s++;
		}

		return true;
	}

	/// Appends an array of elements
	bool Add
	(
		/// Array to insert
		GArray<Type> &a
		
	)
	{
		ssize_t old = len;
		if (Length(len + a.Length()))
		{
			for (unsigned i=0; i<a.Length(); i++, old++)
				p[old] = a[i];
		}
		else return false;
		return true;
	}

	GArray<Type> &operator +(GArray<Type> &a)
	{
		Add(a);
		return *this;
	}

	/// Inserts an element into the array
	bool AddAt
	(
		/// Item to insert before
		size_t Index,
		/// Item to insert
		Type n
	)
	{
		// Make room
		if (Length(len + 1))
		{
			if (Index >= 0 && (uint32)Index < len - 1)
			{
				// Shift elements after insert point up one
				memmove(p + Index + 1, p + Index, (len - Index - 1) * sizeof(Type) );
			}
			else
			{
				// Add at the end, not after the end...
				Index = len - 1;
			}

			// Insert item
			memset(p + Index, 0, sizeof(*p));
			p[Index] = n;

			return true;
		}

		return false;
	}

	/// Sorts the array
	void Sort(int (*Compare)(Type*, Type*))
	{
		typedef int (*qsort_compare)(const void *, const void *);
		qsort(p, len, sizeof(Type), (qsort_compare)Compare);
	}

	// Sorts the array with a comparison function (can I get a standard here?)
	#if defined(_MSC_VER) || defined(PLATFORM_MINGW)
	
		#define DeclGArrayCompare(func_name, type, user_type) \
			int func_name(user_type *param, type *a, type *b)
		
		template<typename U>
		void Sort(int (*Compare)(U *user_param, Type*, Type*), U *user_param)
		{
			typedef int (*qsort_s_compare)(void *, const void *, const void *);
			qsort_s(p, len, sizeof(Type), (qsort_s_compare)Compare, user_param);
		}
	
	#elif defined(MAC)

		#define DeclGArrayCompare(func_name, type, user_type) \
			int func_name(user_type *param, type *a, type *b)
		
		template<typename U>
		void Sort(int (*Compare)(U *user_param, Type*, Type*), U *user_param)
		{
			typedef int (*qsort_r_compare)(void *, const void *, const void *);
			qsort_r(p, len, sizeof(Type), user_param, (qsort_r_compare)Compare);
		}
	
	#elif !defined(BEOS) // POSIX?
	
		#define DeclGArrayCompare(func_name, type, user_type) \
			int func_name(type *a, type *b, user_type *param)
		
		template<typename T>
		void Sort(int (*Compare)(Type*, Type*, T *user_param), T *user_param)
		{
			typedef int (*qsort_r_compare)(const void *, const void *, void *);
			qsort_r(p, len, sizeof(Type), (qsort_r_compare)Compare, user_param);
		}
	
	#endif

	/// \returns a reference to a new object on the end of the array
	///
	/// Never assign this to an existing variable. e.g:
	///		GArray<MyObject> a;
	///		MyObject &o = a.New();
	///		o.Type = something;
	///		o = a.New();
	///		o.Type = something else;
	///
	/// This causes the first object to be overwritten with a blank copy.
	Type &New()
	{
		return (*this)[len];
	}

	/// Returns the memory held by the array and sets itself to empty
	Type *Release()
	{
		Type *Ptr = p;
		p = 0;
		len = alloc = 0;
		return Ptr;
	}

	void Swap(GArray<Type> &other)
	{
		LSwap(p, other.p);
		LSwap(len, other.len);
		LSwap(alloc, other.alloc);
	}

	/// Swaps a range of elements between this array and 'b'
	bool SwapRange
	(
		// The range of 'this' to swap out
		GRange aRange,
		// The other array to swap with
		GArray<Type> &b,
		// The range of 'b' to swap with this array
		GRange bRange
	)
	{
		GArray<Type> Tmp;

		// Store entries in this that will be swapped
		Tmp.Add(AddressOf(aRange.Start), aRange.Len);

		// Copy b's range into this
		ssize_t Common = MIN(bRange.Len, aRange.Len);
		ssize_t aIdx = aRange.Start;
		ssize_t bIdx = bRange.Start;
		for (int i=0; i<Common; i++) // First the common items
			(*this)[aIdx++] = b[bIdx++];
		if (aRange.Len < bRange.Len)
		{
			// Grow range to fit 'b'
			ssize_t Items = bRange.Len - aRange.Len;
			for (ssize_t i=0; i<Items; i++)
				if (!AddAt(aIdx++, b[bIdx++]))
					return false;
		}
		else if (aRange.Len > bRange.Len)
		{
			// Shrink the range in this to fit 'b'
			ssize_t Del = aRange.Len - bRange.Len;
			for (ssize_t i=0; i<Del; i++)
				if (!DeleteAt(aIdx, true))
					return false;
		}

		// Now copy Tmp into b
		int TmpIdx = 0;
		bIdx = bRange.Start;
		for (TmpIdx=0; TmpIdx<Common; TmpIdx++) // First the common items.
			b[bIdx++] = Tmp[TmpIdx];
		if (aRange.Len > bRange.Len)
		{
			// Grow range to fit this
			ssize_t Add = aRange.Len - bRange.Len;
			for (ssize_t i=0; i<Add; i++)
				if (!b.AddAt(bIdx++, Tmp[TmpIdx++]))
					return false;
		}
		else if (aRange.Len < bRange.Len)
		{
			// Shrink the range in this to fit 'b'
			ssize_t Del = bRange.Len - aRange.Len;
			for (ssize_t i=0; i<Del; i++)
				if (!b.DeleteAt(bIdx, true))
					return false;
		}

		return true;
	}

	template <class T>
	class Iter
	{
		ssize_t i;
		char each_dir;
		GArray<T> *a;

	public:
		Iter(GArray<T> *arr) // 'End' constructor
		{
			i = -1;
			a = arr;
			each_dir = 0;
		}

		Iter(GArray<T> *arr, size_t pos)
		{
			i = pos;
			a = arr;
			each_dir = 0;
		}

		bool operator ==(const Iter<T> &it) const
		{
			int x = (int)In() + (int)it.In();
			if (x == 2)
				return (a == it.a) && (i == it.i);
			return x == 0;
		}

		bool operator !=(const Iter<T> &it) const
		{
			return !(*this == it);
		}

		operator bool() const
		{
			return In();
		}
		
		bool In() const
		{
			return i >= 0 && i < (ssize_t)a->Length();
		}
		
		bool End() const
		{
			return i < 0 || i >= a->Length();
		}
		
		T &operator *() { return (*a)[i]; }
		Iter<T> &operator ++() { i++; return *this; }
		Iter<T> &operator --() { i--; return *this; }
		Iter<T> &operator ++(int) { i++; return *this; }
		Iter<T> &operator --(int) { i--; return *this; }
	};

	typedef Iter<Type> I;
	I begin() { return I(this, 0); }
	I rbegin() { return I(this, len-1); }
	I end() { return I(this); }

	/*
	To use this iteration method in a for loop:

	for (T *Ptr = NULL; Array.Iterate(Ptr); )
	{
		// Use 'Ptr' here
	}
	*/
	bool Iterate(Type *&Ptr)
	{
		if (!len || !p)
			return false;
		
		return	Ptr
				?
				PtrCheck(++Ptr)
				:
				(Ptr = p) != NULL;
	}

	/*
	To use this iteration method in a for loop:

	for (T *Ptr = NULL, size_t Idx; Array.Iterate(Idx, Ptr); )
	{
		// Use 'Ptr' here
	}
	*/
	template<typename T>
	bool IteratePtr(size_t &Idx, T &Ptr)
	{
		if (!len || !p)
			return false;
		
		if (!Ptr)
		{
			Ptr = dynamic_cast<T>(*p);
			Idx = 0;
			return true;
		}

		Ptr = p + (++Idx);
		return PtrCheck(Ptr);		
	}
};

#endif
