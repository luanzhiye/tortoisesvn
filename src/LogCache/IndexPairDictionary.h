#pragma once

///////////////////////////////////////////////////////////////
// necessary includes
///////////////////////////////////////////////////////////////

#include "QuickHash.h"
#include "LogCacheGlobals.h"

///////////////////////////////////////////////////////////////
// forward declarations
///////////////////////////////////////////////////////////////

class IHierarchicalInStream;
class IHierarchicalOutStream;

///////////////////////////////////////////////////////////////
// begin namespace LogCache
///////////////////////////////////////////////////////////////

namespace LogCache
{

///////////////////////////////////////////////////////////////
//
// CIndexPairDictionary
//
//		efficiently stores unique pairs of integers in a pool.
//		Each pair is assigned an unique, immutable index.
//
//		Under most circumstances, O(1) lookup is provided.
//
///////////////////////////////////////////////////////////////

class CIndexPairDictionary
{
private:

	///////////////////////////////////////////////////////////////
	//
	// CHashFunction
	//
	//		A simple string hash function that satisfies quick_hash'
	//		interface requirements.
	//
	///////////////////////////////////////////////////////////////

	class CHashFunction
	{
	private:

		// the dictionary we index with the hash
		// (used to map index -> value)

		std::vector<std::pair<index_t, index_t> >* data;

	public:

		// simple construction

		CHashFunction (CIndexPairDictionary* aDictionary);

		// required typedefs and constants

		typedef std::pair<index_t, index_t> value_type;
		typedef index_t index_type;

		enum {NO_INDEX = LogCache::NO_INDEX};

		// the actual hash function

		size_t operator() (const value_type& value) const
		{
#ifdef _WIN64
			return reinterpret_cast<const size_t&>(value);
#else
			return (((size_t)value.first) << 24) + (((size_t)value.first) >> 8)
				 + (((size_t)value.first) << 16) + (((size_t)value.first) >> 16)
				 + (size_t)value.second;
#endif
		}

		// dictionary lookup

		const value_type& value (index_type index) const
		{
			assert (data->size() >= index);
			return (*data)[index];
		}

		// lookup and comparison

		bool equal (const value_type& value, index_type index) const
		{
			assert (data->size() >= index);
			return (*data)[index] == value;
		}
	};

	// sub-stream IDs

	enum
	{
		FIRST_STREAM_ID = 1,
		SECOND_STREAM_ID = 2
	};

	// our data pool

	std::vector<std::pair<index_t, index_t> > data;

	// the hash index (for faster lookup)

	quick_hash<CHashFunction> hashIndex;

public:

	// construction / destruction

	CIndexPairDictionary(void);
	virtual ~CIndexPairDictionary(void);

	// dictionary operations

	index_t size() const
	{
		return (index_t)data.size();
	}

	void reserve (index_t min_capacity);

	const std::pair<index_t, index_t>& operator[](index_t index) const
	{
		if (index >= data.size())
			throw std::exception ("pair dictionary index out of range");

		return data[index];
	}

	index_t Find (const std::pair<index_t, index_t>& value) const;
	index_t Insert (const std::pair<index_t, index_t>& value);
	index_t AutoInsert (const std::pair<index_t, index_t>& value);

	void Clear();

	// stream I/O

	friend IHierarchicalInStream& operator>> ( IHierarchicalInStream& stream
											 , CIndexPairDictionary& dictionary);
	friend IHierarchicalOutStream& operator<< ( IHierarchicalOutStream& stream
											  , const CIndexPairDictionary& dictionary);
};

// stream I/O

IHierarchicalInStream& operator>> ( IHierarchicalInStream& stream
								  , CIndexPairDictionary& dictionary);
IHierarchicalOutStream& operator<< ( IHierarchicalOutStream& stream
								   , const CIndexPairDictionary& dictionary);

///////////////////////////////////////////////////////////////
// end namespace LogCache
///////////////////////////////////////////////////////////////

}

