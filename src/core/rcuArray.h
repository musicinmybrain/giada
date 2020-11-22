/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2020 Giovanni A. Zuliani | Monocasual
 *
 * This file is part of Giada - Your Hardcore Loopmachine.
 *
 * Giada - Your Hardcore Loopmachine is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * Giada - Your Hardcore Loopmachine is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Giada - Your Hardcore Loopmachine. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------- */


#include <array>
#include <cassert>
#include <atomic>
#include <iterator>


/* RCUArray
Thread-safe array of T protected by a Read-Copy-Update (RCU) mechanism. */

template<typename T, size_t S>
class RCUArray
{
public:

	/* value_type
	A variable that holds the type of data contained in the list. Used for
	metaprogramming stuff. */

	using value_type = std::array<std::atomic<T*>, S>;

	/* ---------------------------------------------------------------------- */

	struct Iterator
	{
		using iterator_category = std::forward_iterator_tag; 
    	using difference_type   = std::ptrdiff_t;
		using value_type        = T*;
    	using pointer           = value_type;
		using reference         = T&;
		
		Iterator(std::atomic<T*>* ptr) : m_ptr(ptr) {}
		Iterator(const Iterator& o) : m_ptr(o.m_ptr) {}
	    Iterator& operator= (const Iterator& o) { m_ptr = o.m_ptr; return *this; }
		~Iterator() = default;

		reference operator* () const { return *m_ptr->load(); }  // TODO memory order
   		Iterator& operator++() { m_ptr++; return *this; }  // prefix increment
		friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
		friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; }; 

	private:

		std::atomic<T*>* m_ptr;
	};

	Iterator begin() { return Iterator(&m_data[0]); }
	Iterator end()   { return Iterator(&m_data[m_head.load()]); }  // TODO memory order


	/* ---------------------------------------------------------------------- */

	/* Lock
	Scoped lock structure. Not copyable, not moveable, not copy-constructible. 
	Same as std::scoped_lock: 
	https://en.cppreference.com/w/cpp/thread/scoped_lock .*/

	struct Lock
	{
		Lock(RCUArray<T, S>& r) : rcu(r) { rcu.lock(); }
		Lock(const Lock&) = delete;
		Lock(Lock&&) = delete;
		Lock& operator=(const Lock&) = delete;
		~Lock()	{ rcu.unlock();	}

		RCUArray<T, S>& rcu;
	};
	

	/* ---------------------------------------------------------------------- */


	RCUArray()
		: m_grace     (0)
		, m_writing   (false)
		, m_numReaders(0)
		, m_head      (0)
		, m_changed   (false)
	{
		m_readers[0].store(0);
		m_readers[1].store(0);
	}

	RCUArray(const RCUArray&)            = delete;
	RCUArray(RCUArray&&)                 = delete;
    RCUArray& operator=(const RCUArray&) = delete;
    RCUArray& operator=(RCUArray&&)      = delete;

	~RCUArray()
	{
		clear();
	}


	/* ---------------------------------------------------------------------- */

	/* unlock
	Increases current readers count. Always call lock() when reading data from 
	the list and unlock() when you are done. Or use the scoped version Lock 
	above. */

	void lock()
	{
		assert(m_readers[t_grace].load() >= 0 && "Negative reader");
		t_grace = m_grace.load(); // TODO memory order
		m_readers[t_grace]++;     // TODO memory order with fetch_add()
		m_numReaders.fetch_add(1, std::memory_order_relaxed);
	}


	/* ---------------------------------------------------------------------- */

	/* unlock
	Releases current readers count. */

	void unlock()
	{
		assert(m_readers[t_grace].load() > 0 && "Negative reader");
		m_readers[t_grace]--; // TODO memory order with fetch_sub()
		m_numReaders.fetch_sub(1, std::memory_order_relaxed);
	}


	/* ---------------------------------------------------------------------- */

	/* get
	Returns a reference to the data held by node 'i'. */

	T* get(std::size_t i=0) const
	{
		assert(i < m_head.load() && "Index overflow");
		assert(m_readers[t_grace].load() > 0 && "Forgot lock before reading");
		return m_data[i];
	}


	/* ---------------------------------------------------------------------- */

	/* Subscript operator []
	Same as above, for the [] syntax. */

	T* operator[] (std::size_t i) const	{ return get(i); }


	/* ---------------------------------------------------------------------- */

	/* back
	Return data held by the last node. */

	T* back() const	
	{ 
		assert(m_readers[t_grace].load() > 0 && "Forgot lock before reading"); 
		return m_data.back().load();  // TODO memory order
	}


	/* ---------------------------------------------------------------------- */

	/* clone
	Returns a new copy of the data held at index 'i'. */

	T clone(std::size_t i=0) const
	{
		assert(i < m_head.load() && "Index overflow");
		/* Make sure no one is writing (swapping, popping, pushing). 
		TODO is this necessary? If so, should this be protected with the spinlock? */
		assert(m_writing.load() == false);
		return *m_data[i];
	}


	/* ---------------------------------------------------------------------- */

	/* swap
	Exchanges data contained in node 'i' with new data 'data'. Returns true if 
	the operation is successful, i.e. the write is allowed. */

	bool swap(T&& data, std::size_t i=0)
	{
		assert(i < m_head.load() && "Index overflow");

		if (!beginWrite())
			return false;

		/* Enter the grace period. */

		std::int8_t oldgrace = flipGrace();

		/* Prepare local object. This will be swapped with the old one right 
		away. The old one will be deleted as soon as all old readers are gone. */

		T* curr = new T(std::move(data));
		T* prev = std::atomic_exchange(&m_data[i], curr);

		/* Wait for readers (if any) to finish reading. Then delete the previous 
		object. */

		wait(oldgrace);
		delete prev;

		endWrite();
		return true;
	}


	/* ---------------------------------------------------------------------- */

	/* push
	Moves a new element into the list. Returns true if the operation is 
	successful, i.e. there's enough space in the array for the insertion and
	the write is allowed. */

	bool push(T&& data)
	{
		if (!beginWrite())
			return false;

		std::size_t head = m_head.load(); // TODO memory order

		/* Make sure the underlying array is not full. */

		if (head == m_data.size()) {
			endWrite();
			return false;
		}

		/* No need for the grace period management here: we are writing to a new 
		memory location, not yet accessible by any reader out there. Just make
		sure to store the new head value only once the data is created and
		ready to be accessed. */

		m_data[head].store(new T(std::move(data)));
		m_head.store(head + 1); 

		endWrite();
		return true;	
	}


	/* ---------------------------------------------------------------------- */

	/* pop
	Removes the i-th element. */

	bool pop(std::size_t i)
	{
		assert(i < m_head.load() && "Index overflow");

		/* Only one concurrent writer allowed. */

		if (!beginWrite())
			return false;

		std::size_t head = m_head.load();  // TODO memory order
		std::size_t last = head - 1;

		/* Inform all readers the array is now shorter (even if it actually
		isn't). We need to wait for ALL readers to finish reading from it, 
		otherwise an iterator might fall off the edge. */

		while (m_numReaders.load(std::memory_order_relaxed) > 0); // Just spin
		m_head.store(head - 1);

		/* Store the target object to be deleted. */
		
		T* target = m_data[i].load();  // TODO memory order

		/* If i < last, writer wants to remove an element in the middle. The
		trick here is to swap it with the last one, and then remove the last
		element. This way you don't have to shift all the existing elements to 
		fill the gap. */

		if (i < last)
		{
			T* lastObj = m_data[last].load();  // TODO memory order
			m_data[i].exchange(lastObj);
		}

		/* Clear the last element: it is safe now, it's not accessible by other
		readers. */

		m_data[last].store(nullptr);

		/* Enter the grace period. */

		std::int8_t oldgrace = flipGrace();

		/* Wait for readers (if any) to finish reading. Then delete the target
		object. */

		wait(oldgrace);
		delete target;

		endWrite();
		return true;
	}


	/* ---------------------------------------------------------------------- */

	/* clear
	Removes all elements. */

	bool clear()
	{
		/* Only one concurrent writer allowed. */

		if (!beginWrite())
			return false;
	
		/* Enter the grace period. */

		std::int8_t oldgrace = flipGrace();

		/* Inform all readers the array is now empty (even if it actually
		isn't). We need to wait for ALL readers to finish reading from it, 
		otherwise an iterator might fall off the edge. */
		
		while (m_numReaders.load(std::memory_order_relaxed) > 0); // Just spin

		std::size_t head = m_head.load();  // TODO memory order
		m_head.store(0);

		/* Wait for current readers (if any) to finish reading. Then delete
		all objects. */

		wait(oldgrace);

		for (std::size_t i = 0; i < head; i++) {
			delete m_data[i].load();  // TODO memory order
			m_data[i].store(nullptr);
		}

		endWrite();
		return true;
	}


	/* ---------------------------------------------------------------------- */

	/* size
	Returns the number of valid items in the array. Note that size() != S. */

	std::size_t size() const
	{
		return m_head.load();  // TODO memory order
	}
	
private:

	/* flipGrace
	Flip the current grace bit. Now we have entered a new grace period with a 
	different number from the previous one. Return the old grace period, to be 
	used while wait()ing. */

	std::int8_t flipGrace()
	{
		return m_grace.fetch_xor(1); // A fetch_xor(1) flips the value.
	}


	/* ---------------------------------------------------------------------- */

	/* wait
	Wait until no readers from the previous grace period (oldgrace) are reading 
	the list. Avoid brutal spinlock with a tiny sleep. */

	void wait(std::int8_t oldgrace)
	{
		assert(m_writing.load() == true && "Wait() is meaningless if not writing");
		while (m_readers[oldgrace].load() > 0)  // TODO memory order
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}


	/* ---------------------------------------------------------------------- */

	/* beginWrite
	Makes sure only one concurrent writer is allowed. This is a primitive form 
	of try-lock: beginWrite->acquire, endWrite->release. std::atomic<T>exchange 
	returns the previous value: if it's 'true', a writing is already in 
	progress. */

	bool beginWrite()
	{
		return m_writing.exchange(true, std::memory_order_acquire) == false;
	}


	/* ---------------------------------------------------------------------- */
	

	void endWrite()
	{
		assert(m_writing.load() == true && "No write in progress");
		m_writing.store(false, std::memory_order_release);
		m_changed.store(true, std::memory_order_relaxed);
	}
	

	/* ---------------------------------------------------------------------- */
	
	/* m_data
	Underlying container. */

	value_type m_data;

	/* m_readers
	An array of 2 elements, where element 0 contains the number of readers in
	the current grace period, while element 1 the number of elements of the
	new grace period. */

	std::array<std::atomic<int>, 2> m_readers;

	/* The current grace period. Grace periods are 2: old or new. Actually
	m_grace should be an atomic boolean value, but unfortunately there's no way
	to atomically flip an boolean. So we use an integer: integers can be 
	atomically flipped with std::atomic<T>::fetch_xor(). */

	std::atomic<std::int8_t> m_grace;

	/* m_writing
	Tells if a writing operation currently in progress. */

	std::atomic<bool> m_writing;

	/* m_numReaders
	Number of overall readers, regardless the grace period. */

	std::atomic<int> m_numReaders;

	/* m_head
	Index for the next insertion. Always points to an empty slot. */

	std::atomic<std::size_t> m_head;

	/* changed
	Tells whether the list has been altered (swap, push, pop or clear). */

	std::atomic<bool> m_changed;

	/* t_grace
	Current grace flag. Each thread has its own copy of it (thread_local). */

	thread_local static int t_grace;
};

template<typename T, size_t S>
thread_local int RCUArray<T, S>::t_grace = 0;