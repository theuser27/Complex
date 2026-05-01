
// Created: 2022-07-22 17:34:59

#pragma once

#include "stl_utils.hpp"
#include "satomi.hpp"

namespace utils
{
  struct bumpArena;
}

extern thread_local utils::bumpArena *localScratch;
extern utils::bumpArena *globalArena;

namespace utils
{
  enum class AllocatorType : u8 { General, BumpArena };

  template<typename Signature, usize MaxSize, usize Alignment>
  class fn;

  template<typename T>
  concept AllocatorConcept = requires(T *allocator, usize size,
    usize alignment, bool clean, const void *existingAllocation)
  {
    // AllocatorType tag
    requires utils::is_same_v<decltype(T::type), const utils::AllocatorType>;

    // basic usage functions
    T::insert(allocator, size, alignment, clean);
    T::remove(existingAllocation);

    // extracting an effective allocator from an existing allocation and being able to use it
    T::insert(static_cast<decltype(T::fromAllocation(existingAllocation))>(nullptr), size, alignment, clean);
  };

  struct bumpArena
  {
    static constexpr auto type = AllocatorType::BumpArena;

    // instead of trying to give the preceding allocation/free list node the unused space
    // caused by the alignment shift, insert a new free list node if the size allows for it
    // so when the preceding allocation gets freed:
    //  we can collapse the new free list into it
    //  if it doesn't fit then we can check that and collapse the succeeding one into it (when it eventually gets freed)

    struct node
    {
      // size of the allocation, including the node in front
      u32 size{};
      union
      {
        u32 offsetToArena{};
        // next is shift to next free node relative to the arena
        u32 next;
      };
    };

    satomi::atomic<bool> lock{};
    bool threadSafe = true;
    u8 flags{};

    // reserved and committed sizes stored are size - 1 to be able to represent up to 4 GB
    // but otherwise every other index represents its actual value
    u32 reservedSize;
    u32 committedSize;
    u32 freeNodeStart;

    bumpArena *nextArena{};

    static usize getUsedSize(bumpArena *arena);
    static strict_inline bumpArena *
    fromAllocation(const void *data)
    {
      auto *node = utils::launder((bumpArena::node *)((byte *)data - sizeof(bumpArena::node)));
      return utils::launder((bumpArena *)((byte *)node - node->offsetToArena));
    }
    static strict_inline usize
    getAllocationSize(const void *data)
    {
      auto *node = utils::launder((bumpArena::node *)((byte *)data - sizeof(bumpArena::node)));
      return node->size - sizeof(bumpArena::node);
    }

    [[nodiscard]] static byte *insert(bumpArena *arena, usize size, usize alignment, bool clean = false);
    [[nodiscard]] static strict_inline byte *
    insert(const void *data, usize size, usize alignment, bool clean = false)
    {
      return insert(fromAllocation(data), size, alignment, clean);
    }
    static void remove(const void *data);
    [[nodiscard]] static byte *resize(const void *data, usize newSize, bool cleanNewSpace = false);
    static void shrinkToFit(bumpArena *arena, bool shrinkToCommitted);

    [[nodiscard]] static strict_inline bumpArena *
    create(usize reservedSize = COMPLEX_MB(64), usize commitSize = COMPLEX_KB(64))
    {
      reservedSize = utils::clamp(reservedSize, sizeof(bumpArena), usize(u32(-1)) + 1);
      commitSize = utils::clamp(commitSize, sizeof(bumpArena), reservedSize);
      
      byte *memory = reserveMemory(reservedSize);
      commitMemory(memory, commitSize);

      (void)new(memory + sizeof(bumpArena)) node{ .size = (u32)(commitSize - sizeof(bumpArena)) };

      return new(memory) bumpArena{ .reservedSize = (u32)(reservedSize - 1),
        .committedSize = (u32)(commitSize - 1), .freeNodeStart = (u32)sizeof(bumpArena) };
    }
    template<typename T>
    [[nodiscard]] static strict_inline bumpArena *
    createNested(T *differentArena, usize allocateSize) requires requires { T::insert(differentArena, usize(0), usize(0)); }
    {
      allocateSize = utils::min(allocateSize + sizeof(bumpArena), usize(u32(-1)) + 1);

      byte *memory = T::insert(differentArena, allocateSize, alignof(bumpArena));
      (void)new(memory + sizeof(bumpArena)) node{ .size = (u32)(allocateSize - sizeof(bumpArena)) };

      u8 flags = (u8)T::type;

      // bumpArena is nested in an arena
      return new(memory) bumpArena{ .flags = flags, .reservedSize = (u32)(allocateSize - 1),
        .committedSize = (u32)(allocateSize - 1), .freeNodeStart = (u32)sizeof(bumpArena) };
    }

    static void clear(bumpArena *arena);

    static void destroy(bumpArena *arena);

    static void logBlocks(bumpArena *arena);
  };

  // anew, the replacement to operator new (with arenas/allocators)
  // using placement new on memory resource, provided by an arena implementation

  #define anew(allocator, T, ...) (new(allocator->insert(allocator, sizealignof(T))) T __VA_ARGS__)
  #define arranew(allocator, T, count, ...) (new(allocator->insert(allocator, sizealignof(T, count))) T[count] __VA_ARGS__)

  struct Allocator
  {
    struct AllocatorVtable
    {
      byte *(*insert)(void *allocator, usize size, usize alignment, bool clean);
      void (*remove)(const void *allocation);
      Allocator (*fromAllocation)(const void *allocation);
      AllocatorType type;
    };

  private:
    static Allocator getGeneralAllocator(const void *) { return Allocator{ &generalAllocatorVtable, nullptr }; }
    template<typename T>
    static Allocator
    getAllocatorFromFunction(const void *allocation)
    {
      auto *allocator = T::fromAllocation(allocation);
      return { &allocatorTable[(usize)allocator->type], allocator };
    }
    template<AllocatorConcept T>
    static constexpr AllocatorVtable 
    createVtable()
    {
      return AllocatorVtable
      {
        .insert = [](void *allocator, usize size, usize alignment, bool clean)
        { return T::insert((T *)allocator, size, alignment, clean); },
        .remove = T::remove,
        .fromAllocation = Allocator::getAllocatorFromFunction<T>,
        .type = T::type
      };
    }
  public:

    static constexpr AllocatorVtable generalAllocatorVtable
    {
      .insert = [](void *, usize size, usize alignment, bool clean)
      {
        return utils::allocate(size, alignment, clean);
      },
      .remove = [](const void *allocation) { utils::deallocate(allocation); },
      .fromAllocation = getGeneralAllocator,
      .type = AllocatorType::General
    };


    static constexpr AllocatorVtable allocatorTable[] = { generalAllocatorVtable, createVtable<utils::bumpArena>() };

    sourceLocation location{};
    const AllocatorVtable *vtable{};
    void *allocator{};
    bool freeingDestructor = true;

    constexpr Allocator() = default;
    constexpr Allocator(AllocatorConcept auto *allocator, bool freeingDestructor = true, 
      utils::sourceLocation location = utils::sourceLocation::current()) : location{ location },
      vtable{ &allocatorTable[(usize)allocator->type] }, allocator{ allocator },
      freeingDestructor{ freeingDestructor } { }
    constexpr Allocator(const AllocatorVtable *vtable, void *allocator, bool freeingDestructor = true,
      utils::sourceLocation location = utils::sourceLocation::current()) : location{ location },
      vtable{ vtable }, allocator{ allocator }, freeingDestructor{ freeingDestructor } { }

    [[nodiscard]] strict_inline byte *
    insert(usize size, usize alignment, bool clean = true) const
    { return vtable->insert(allocator, size, alignment, clean); }
    strict_inline void remove(const void *allocation) const { if (allocation) vtable->remove(allocation); }
    strict_inline Allocator fromAllocation(const void *allocation) const { return vtable->fromAllocation(allocation); }
    strict_inline AllocatorType type() const { return vtable->type; }

    static constexpr Allocator fromType(AllocatorType type) { return { &allocatorTable[(usize)type], nullptr }; }

    explicit operator bool() const { return vtable; }
  };

  inline constexpr Allocator generalAllocator{ &Allocator::generalAllocatorVtable, nullptr };


  template<typename T>
  constexpr void contiguousMoveElements(T *destination, const T *source, usize size)
  {
    if (utils::is_constant_evaluated() || !utils::is_trivially_move_assignable_v<T>)
    {
      if (destination > source)
        for (usize i = size; i > 0; --i)
          (void)new(destination + i - 1) T{ COMPLEX_MOVE(source[i - 1]) };
      else if (destination < source)
        for (usize i = 0; i < size; ++i)
          (void)new(destination + i) T{ COMPLEX_MOVE(source[i]) };
    }
    else if (size)
      ::memmove(destination, source, size * sizeof(T));
  }

  template<typename T>
  constexpr void contiguousDestroy(T *data, usize size)
  {
    if (utils::is_constant_evaluated() || !utils::is_trivially_destructible_v<T>)
    {
      if (data && size)
        for (usize i = 0; i < size; ++i)
          data[i].~T();
    }
  }

  template<typename T>
  inline void contiguousReserve(Allocator allocator, Allocator previousAllocator,
    usize desiredCapacity, T *&existingData, usize existingSize, usize &existingCapacity)
  {
    if (!desiredCapacity || desiredCapacity <= existingCapacity)
      return;

    T *data = (T *)allocator.insert(sizealignof(T, desiredCapacity));

    if (existingData)
    {
      if constexpr (utils::is_trivially_copy_constructible_v<T>)
      {
        if (existingSize)
          ::valcpy(data, existingData, existingSize);
      }
      else
      {
        for (usize i = 0; i < existingSize; ++i)
          (void)new(data + i) T{ COMPLEX_MOVE(existingData[i]) };
      }

      previousAllocator.remove(existingData);
    }

    existingData = data;
    existingCapacity = desiredCapacity;
  }

  template<typename T>
  void contiguousClone(Allocator allocator, Allocator previousAllocator, 
    const T *dataToClone, usize dataSize, T *&existingData, usize existingSize, usize &existingCapacity)
  {
    if constexpr (!utils::is_trivially_destructible_v<T>)
    {
      if (existingData && existingSize)
        for (usize i = 0; i < existingSize; ++i)
          existingData[i].~T();
    }

    if (existingCapacity < dataSize)
    {
      if (existingData)
        previousAllocator.remove(existingData);

      existingData = (T *)allocator.insert(sizealignof(T, dataSize));
    }

    if constexpr (utils::is_trivially_copy_constructible_v<T>)
    {
      if (existingSize)
        ::valcpy(existingData, dataToClone, dataSize);
    }
    else
      for (usize i = 0; i < dataSize; ++i)
        (void)new(existingData + i) T{ COMPLEX_MOVE(dataToClone[i]) };

    existingCapacity = dataSize;
    existingSize = dataSize;
  }

  template<typename T>
  class vectornd
  {
    T *data_{};
    usize	allocatorType_ : 1 = (usize)AllocatorType::General;
    usize freeingDestructor_ : 1 = true;
    usize	size_ : 62{};
    usize	capacity_{};

  protected:
    void destructor(bool force = true)
    {
      if (capacity_)
      {
        utils::contiguousDestroy(data_, size_);
        if (freeingDestructor_ || force)
          Allocator::fromType((AllocatorType)allocatorType_).remove(data_);
      }

      data_ = {};
      size_ = {};
      capacity_ = {};
    }

  public:
    using value_type = T;
    using size_type = usize;
    using difference_type = isize;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using iterator = T *;
    using const_iterator = const T *;

    static constexpr usize kStartingCapacity = 16;

    vectornd(Allocator allocator, usize reserveSpace = 0) { reserve(allocator, reserveSpace); }
    vectornd(Allocator allocator, utils::span<const T> data)
    {
      utils::contiguousClone(allocator, allocator, data.data(), data.size(), data_, size_, capacity_);
      allocatorType_ = (usize)allocator.type();
      freeingDestructor_ = allocator.freeingDestructor;
      size_ = data.size();
    }

    constexpr vectornd() = default;
    vectornd(const vectornd &other) = delete;
    vectornd &operator=(const vectornd &other) = delete;
    constexpr vectornd(vectornd &&other) noexcept { swap(other); }
    vectornd &operator=(vectornd &&other) noexcept
    {
      if (this == &other)
        return *this;

      destructor();
      swap(other);

      return *this;
    }

    [[nodiscard]] vectornd
    clone() const
    {
      vectornd ret{};

      auto oldAllocator = Allocator::fromType((AllocatorType)allocatorType_).fromAllocation(data_);
      utils::contiguousClone(oldAllocator, oldAllocator, data(), size(), ret.data_, ret.size_, ret.capacity_);
      ret.allocatorType_ = (usize)oldAllocator.type();
      ret.freeingDestructor_ = freeingDestructor_;
      ret.size_ = size();

      return ret;
    }

    void reserve(usize capacity) { reserve({}, capacity); }
    void reserve(Allocator allocator, usize capacity)
    {
      // are we caching allocator for future use?
      if (!capacity_ && !capacity)
      {
        allocatorType_ = (usize)allocator.type();
        data_ = (decltype(data_))allocator.allocator;
      }

      if (!capacity || capacity <= capacity_)
        return;

      // increase size by at least double the current one
      capacity = utils::max(2 * capacity_, capacity);

      Allocator oldAllocator;
      // have we cached the arena we want to allocate in?
      if (!capacity_ && data_)
      {
        oldAllocator = Allocator::fromType((AllocatorType)allocatorType_);
        oldAllocator.allocator = data_;
        data_ = nullptr;
      }
      else
        oldAllocator = Allocator::fromType((AllocatorType)allocatorType_).fromAllocation(data_);

      allocator = (allocator) ? allocator : oldAllocator;
      utils::contiguousReserve(allocator, oldAllocator, capacity, data_, size_, capacity_);
      allocatorType_ = (usize)allocator.type();
      freeingDestructor_ = allocator.freeingDestructor;
    }

    constexpr void clear()
    {
      contiguousDestroy(data_, size_);
      size_ = 0;
    }

    constexpr usize size() const { return size_; }
    constexpr usize capacity() const { return capacity_; }
    constexpr bool empty() const { return size_ == 0; }

    // returning void * in order to force error if someone wants to do assignment at call site
    void *
    pushBack(usize count = 1)
    {
      COMPLEX_HARD_ASSERT(count > 0, "Inserting 0 elements doesn't make sense");

      if (capacity_ == 0)
        reserve(utils::max(kStartingCapacity, count));
      else if (size_ + count > capacity_)
        reserve(utils::max(2 * capacity_, size_ + count));

      auto oldSize = size_;
      size_ += count;
      return &data_[oldSize];
    }

    T &emplaceBack(auto &&... args) { return *new(pushBack()) T{ COMPLEX_FWD(args)... }; }

    void *
    insert(iterator iter)
    {
      usize index = (usize)(iter - begin());

      COMPLEX_HARD_ASSERT(index <= size());

      if (capacity_ == 0)
        reserve(kStartingCapacity);
      else if (size_ + 1 > capacity_)
        reserve(utils::max(2 * capacity_, size_ + 1));

      utils::contiguousMoveElements(data_ + index + 1, data_ + index, size_ - index);

      ++size_;
      return data_ + index;
    }

    T &emplace(iterator iter, auto &&... args) { return *new(insert(iter)) T{ COMPLEX_FWD(args)... }; }

    constexpr void popFront()
    {
      if (!size_)
        return;

      front().~T();
      --size_;
      utils::contiguousMoveElements(data_, data_ + 1, size_);
    }
    constexpr void popBack()
    {
      if (!size_)
        return;

      back().~T();
      --size_;
    }

    void erase(iterator iter) { erase(iter, 1); }
    void erase(iterator start, usize count)
    {
      usize index = (usize)(start - begin());

      COMPLEX_HARD_ASSERT(index < size());

      size_ -= count;
      for (usize i = 0; i < count; ++i)
        (start + (isize)i)->~T();
      
      utils::contiguousMoveElements(start, start + count, data_ + size_ - start);
    }

    void erase(const T &element) { eraseIf([&element](const auto &data) { return element == data; }); }
    void eraseIf(const auto &predicate)
    {
      usize offset{};
      usize lastShift = usize(-1);
      for (usize i = 0; i < size_; ++i)
      {
        if (predicate(data_[i]))
        {
          utils::contiguousDestroy(&data_[i], 1);

          if (lastShift != usize(-1) && (i - lastShift) > 1)
            utils::contiguousMoveElements(data_ + lastShift, data_ + lastShift + offset, i - lastShift);

          lastShift = i;
          ++offset;
        }
      }

      if (lastShift != usize(-1) && (size_ - lastShift) > 1)
        utils::contiguousMoveElements(data_ + lastShift, data_ + lastShift + offset, size_ - lastShift);
    }

    constexpr void swap(vectornd &other)
    {
      COMPLEX_SWAP_MEMBERS(data_, other);
      COMPLEX_SWAP_MEMBERS(allocatorType_, other);
      COMPLEX_SWAP_MEMBERS(freeingDestructor_, other);
      COMPLEX_SWAP_MEMBERS(size_, other);
      COMPLEX_SWAP_MEMBERS(capacity_, other);
    }

    constexpr T &operator[](usize i) { COMPLEX_HARD_ASSERT(i < size_); return data_[i]; }
    constexpr const T &operator[](usize i) const { COMPLEX_HARD_ASSERT(i < size_); return data_[i]; }

    constexpr T &front() { COMPLEX_HARD_ASSERT(size_); return data_[0]; }
    constexpr const T &front() const { COMPLEX_HARD_ASSERT(size_); return data_[0]; }
    constexpr T &back() { COMPLEX_HARD_ASSERT(size_); return data_[size_ - 1]; }
    constexpr const T &back() const { COMPLEX_HARD_ASSERT(size_); return data_[size_ - 1]; }

    constexpr T *data() { return (size_) ? data_ : nullptr; }
    constexpr const T *data() const { return (size_) ? data_ : nullptr; }

    constexpr const iterator begin() const { return (size_) ? data_ : nullptr; }
    constexpr iterator begin() { return (size_) ? data_ : nullptr; }
    constexpr const iterator end() const { return begin() + size_; }
    constexpr iterator end() { return begin() + size_; }
  };

  template<typename T>
  class vector : public vectornd<T>
  {
  public:
    ~vector() { vectornd<T>::destructor(false); }

    using vectornd<T>::vectornd;

    constexpr vector() = default;
    constexpr vector(vectornd<T> &&other) noexcept { swap(other); }
    vector &
    operator=(vector &&other) noexcept
    {
      (void)vectornd<T>::operator=(COMPLEX_MOVE(other));
      return *this;
    }
  };

  template<typename Key, typename Value>
  struct vector_map
  {
    utils::vector<utils::pair<Key, Value>> data{};

    const Value &operator[](usize index) const { return data[index].second; }
    Value &operator[](usize index) { return data[index].second; }

    //---------------------------------------------------------
    // the following functions assume keys ARE unique

    constexpr auto find(const Key &key) 
    { return utils::findIf(data, [&key](const auto &v) { return v.first == key; }); }

    constexpr auto find(const Key &key) const 
    { return utils::findIf(data, [&key](const auto &v) { return v.first == key; }); }

    constexpr auto findIf(const auto &functor)  { return utils::findIf(data, functor); }
    constexpr auto findIf(const auto &functor) const  { return utils::findIf(data, functor); }

    constexpr auto add(Key key, Value value)
    {
      data.emplaceBack(COMPLEX_MOVE(key), COMPLEX_MOVE(value));
      return data.end() - 1;
    }

    constexpr void update(const Key &key, utils::pair<Key, Value> updatedEntry) 
    {
      auto iter = find(key);
      if (iter == data.end())
        return;

      (*iter) = COMPLEX_MOVE(updatedEntry);
    }

    constexpr void erase(const Key &key) 
    {
      auto iter = utils::findIf(data, [&key](const auto &v) { return v.first == key; });
      if (iter != data.end())
        data.erase(iter);
    }

  private:
    static constexpr auto binary_search(auto &container, auto &element, const auto &predicate)
    {
      auto low = container.begin();
      auto end = container.end();

      if (low != end)
      {
        auto high = end - 1;      
        while (low <= high)
        {
          auto mid = low + ((high - low) / 2);

          // Check if element is present at mid
          if (predicate(element, *mid))
            return utils::pair{ mid, true };

          Key key;
          if constexpr (requires{ element.first; })
            key = element.first;
          else
            key = element;

          // If element greater, ignore left half
          if ((*mid).first < key)
            low = mid + 1;
          else // If element is smaller, ignore right half
            high = mid - 1;
        }
      }

      return utils::pair{ low, false };
    }
  public:

    //---------------------------------------------------------
    // the following functions assume keys are NOT unique

    constexpr auto binary_search(const utils::pair<Key, Value> &element) const 
    {
      auto [iter, wasFound] = binary_search(data, element, [](const auto &element, const auto &test) { return element == test; });
      return (wasFound) ? iter : data.end();
    }
    constexpr auto binary_search(const utils::pair<Key, Value> &element) 
    {
      auto [iter, wasFound] = binary_search(data, element, [](const auto &element, const auto &test) { return element == test; });
      return (wasFound) ? iter : data.end();
    }

    constexpr auto get_first_of(const Key &key)
    {
      auto [iter, wasFound] = binary_search(data, key, [](const auto &element, const auto &test) { return element == test.first; });
      if (wasFound)
      {
        // find the last element with this key and insert the new element thereafter
        while (iter != data.begin() && iter->first == key)
          --iter;
        if (iter->first == key)
          return iter;
      }
      return data.end();
    }

    constexpr auto get_last_of(const Key &key)
    {
      auto [iter, _] = binary_search(data, key, [](const auto &element, const auto &test) { return element == test.first; });
      // find the last element with this key and insert the new element thereafter
      while (iter != data.end() && iter->first == key)
        ++iter;
      return iter;
    }

    // this will work correctly only if you ever use the ordered functions
    // returns iterator of the added element
    constexpr auto add_ordered(Key key, Value value) 
    {
      // find AN occurance of specified key
      auto [iter, _] = binary_search(data, key, [](const auto &element, const auto &test) { return element == test.first; });
      while (iter != data.end() && iter->first == key)
        ++iter;

      auto index = iter - data.begin();
      data.emplace(iter, COMPLEX_MOVE(key), COMPLEX_MOVE(value));
      return data.begin() + index;
    }


    // this will work correctly only if you ever use the ordered functions
    constexpr void update_ordered(const utils::pair<Key, Value> &entry, utils::pair<Key, Value> updatedEntry) 
    {
      auto iter = binary_search(entry);
      if (iter == data.end())
        return;

      (*iter) = COMPLEX_MOVE(updatedEntry);
    }

    // this will work correctly only if you ever use the ordered functions
    constexpr void erase_ordered(const utils::pair<Key, Value> &element) 
    {
      auto iter = binary_search(element);
      if (iter != data.end())
        data.erase(iter);
    }
  };

  struct type_map : public vector_map<typeInfo, void *>
  {
    using base = vector_map<typeInfo, void *>;

    constexpr void iterate(const auto &lambda)
    {
      using T = typename detail::signature<decltype(&remove_cvref_t<decltype(lambda)>::operator())>::type;
      constexpr auto type = typeId(T);
      for (auto iter = get_first_of(type);
        iter != data.end() && iter->first == type; ++iter)
        lambda(static_cast<T>(iter->second));
    }

    constexpr auto add_ordered(auto entry) 
    { return base::add_ordered(typeId(decltype(entry)), (void *)COMPLEX_MOVE(entry)); }

    template<typename T>
    constexpr void update_ordered(T entry, T updatedEntry) 
    {
      auto iter = get_first_of(typeId(T));
      if (iter == data.end())
        return;

      while (iter != data.end() || iter->second != entry)
        ++iter;

      if (iter == data.end())
        return;

      (*iter) = pair{ typeId(T), COMPLEX_MOVE(updatedEntry) };
    }

    constexpr void erase_ordered(auto entry) 
    {
      auto iter = get_first_of(typeId(decltype(entry)));
      if (iter == data.end())
        return;

      while (iter != data.end() || iter->second != entry)
        ++iter;

      if (iter == data.end())
        return;

      data.erase(iter);
    }
  };


  class stringnd
  {
    char *data_{};
    usize	allocatorType_ : 1 = (usize)AllocatorType::General;
    usize	freeingDestructor_ : 1 = true;
    usize size_ : 62 {};
    usize capacity_{};
  
  protected:
    void destructor(bool force = true)
    {
      if (capacity_ && (freeingDestructor_ || force))
        Allocator::fromType((AllocatorType)allocatorType_).remove(data_);

      data_ = {};
      size_ = {};
      capacity_ = {};
    }

  public:
    using value_type = char;
    using size_type = usize;
    using difference_type = isize;
    using iterator = char *;
    using const_iterator = const char *;

    static constexpr auto npos = size_type(-1);
    static constexpr auto minimumCapacity = size_type((1 << 4) - 1);

    static stringnd 
    create(Allocator allocator, const char *format, auto &&... args)
    {
      stringnd ret{};
      ret.size_ = (size_type)::stbsp_snprintf(nullptr, 0, format, COMPLEX_FWD(args)...);
      ret.reserve(allocator, ret.size_);
      ::stbsp_snprintf(ret.data_, (int)ret.capacity_ + 1, format, COMPLEX_FWD(args)...);
      return ret;
    }

    // preallocating storage or only caching allocator for future use
    stringnd(Allocator allocator, size_type reserveSpace = 0) : stringnd{ allocator, nullptr, reserveSpace } { }
    stringnd(Allocator allocator, utils::string_view data, size_type start = 0, size_type size = npos) : 
      stringnd{ allocator, data.data() + utils::min(data.size(), start), utils::min(data.size(), size) - utils::min(data.size(), start) } { }
    // every other constructor calls this one
    stringnd(Allocator allocator, const char *data, size_type size)
    {
      reserve(allocator, size);
      if (data)
      {
        utils::contiguousMoveElements(data_, data, size);
        size_ = size;
      }
    }

    constexpr stringnd() = default;
    stringnd(const stringnd &other) = delete;
    stringnd &operator=(const stringnd &other) = delete;
    constexpr stringnd(stringnd &&other) noexcept { swap(other); }
    stringnd &
    operator=(stringnd &&other) noexcept
    {
      if (this != &other)
      {
        destructor();
        swap(other);
      }
      return *this;
    }

    [[nodiscard]] stringnd 
    clone(usize position = 0, usize length = npos) const
    {
      COMPLEX_ASSERT(position <= size_, "Position out of range in string::clone(position, length)");
      COMPLEX_ASSERT(length == npos || length <= size_ - position,
        "Length out of range in string::clone(position, length)");

      stringnd ret{};

      auto oldAllocator = Allocator::fromType((AllocatorType)allocatorType_).fromAllocation(data_);
      auto newSize = utils::min(length, size() - position);
      utils::contiguousClone(oldAllocator, oldAllocator, 
        data() + position, 1 + newSize,
        ret.data_, ret.size_, ret.capacity_);
      ret.allocatorType_ = (usize)oldAllocator.type();
      ret.freeingDestructor_ = freeingDestructor_;
      ret.size_ = newSize;
      ret.data_[newSize + 1] = '\0';
      --ret.capacity_;

      return ret;
    }

    stringnd &
    copy(utils::string_view other)
    {
      clear();
      return append(other);
    }

    void reserve(usize capacity = minimumCapacity) { reserve({}, capacity); }
    void reserve(Allocator allocator, usize capacity = minimumCapacity)
    {
      // are we caching allocator for future use?
      if (!capacity_ && !capacity)
      {
        allocatorType_ = (usize)allocator.type();
        data_ = (decltype(data_))allocator.allocator;
      }

      if (!capacity || capacity <= capacity_)
        return;

      // increase size by at least double the current one
      capacity = utils::max(2 * capacity_, capacity);

      // round capacity but keep null terminator in mind
      capacity = utils::roundUpToMultiple(capacity + 1, minimumCapacity + 1);

      Allocator oldAllocator;
      // have we cached the arena we want to allocate in?
      if (!capacity_ && data_)
      {
        oldAllocator = Allocator::fromType((AllocatorType)allocatorType_);
        oldAllocator.allocator = data_;
        data_ = nullptr;
      }
      else
        oldAllocator = Allocator::fromType((AllocatorType)allocatorType_).fromAllocation(data_);

      allocator = (allocator) ? allocator : oldAllocator;
      utils::contiguousReserve(allocator, oldAllocator, capacity, data_, size_, capacity_);
      allocatorType_ = (usize)allocator.type();
      freeingDestructor_ = allocator.freeingDestructor;

      --capacity_;
      ::zeroset(data_ + size_, capacity_ + 1 - size_);
    }

    void clear() { size_ = 0; }

    [[nodiscard]] size_type size() const { return size_; }
    [[nodiscard]] size_type capacity() const { return capacity_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }

    [[nodiscard]] const char *data() const { return data_; }
    [[nodiscard]] char *data() { return data_; }

    [[nodiscard]] const char &
    operator[](size_type position) const
    {
      COMPLEX_ASSERT(position < size_, "View index out of range");
      return data_[position];
    }
    [[nodiscard]] char &
    operator[](size_type position)
    {
      COMPLEX_ASSERT(position < size_, "View index out of range");
      return data_[position];
    }
    [[nodiscard]] const char &
    front() const
    {
      COMPLEX_ASSERT(size_ > 0, "Front of empty view");
      return data_[0];
    }
    [[nodiscard]] const char & 
    back() const
    {
      COMPLEX_ASSERT(size_ > 0, "Back of empty view");
      return data_[size_ - 1];
    }

    [[nodiscard]] iterator begin() { return (size_) ? data_ : nullptr; }
    [[nodiscard]] const_iterator begin() const { return (size_) ? data_ : nullptr; }
    [[nodiscard]] iterator end() { return (size_) ? (data_ + size_) : nullptr; }
    [[nodiscard]] const_iterator end() const { return (size_) ? (data_ + size_) : nullptr; }

    // insertions
    
    stringnd &
    insert(size_type start, utils::string_view other, size_type repetitions = 1)
    {
      reserve(size_ + repetitions * other.size());
      utils::contiguousMoveElements(data_ + start + repetitions * other.size(), data_ + start, size_ - start);
      for (size_type i = 0; i < repetitions; ++i)
        utils::contiguousMoveElements(data_ + start + i * other.size(), other.data(), other.size());

      size_ += repetitions * other.size();

      return *this;
    }
    stringnd &
    append(utils::string_view other, size_type repetitions = 1) { return insert(size_, other, repetitions); }
    stringnd &
    prepend(utils::string_view other, size_type repetitions = 1) { return insert(0, other, repetitions); }
    stringnd &
    appendFormat(const char *format, auto &&... args)
    {
      reserve(size_ + (size_type)::stbsp_snprintf(nullptr, 0, format, COMPLEX_FWD(args)...));
      size_ += (size_type)::stbsp_snprintf(data_ + size_, (int)capacity_ + 1, format, COMPLEX_FWD(args)...);
      return *this;
    }
    stringnd &
    prependFormat(const char *format, auto &&... args)
    {
      size_type sizeToAdd = (size_type)::stbsp_snprintf(nullptr, 0, format, COMPLEX_FWD(args)...);
      reserve(size_ + sizeToAdd);
      char zeroedOutChar = data_[0];
      utils::contiguousMoveElements(data_ + sizeToAdd, data_, size_);
      size_ += (size_type)::stbsp_snprintf(data_, (int)capacity_ + 1, format, COMPLEX_FWD(args)...);
      // undoing null termination
      data_[sizeToAdd] = zeroedOutChar;
      return *this;
    }

    // simple removals

    stringnd &
    remove(size_type index, size_type length)
    {
      COMPLEX_ASSERT(size_ >= index + length, "Range is outside of string in string::remove(index, length)");
      utils::contiguousMoveElements(data_ + index, data_ + index + length, size_ - index - length);
      size_ -= length;
      data_[size_] = '\0';
      return *this;
    }
    stringnd &
    removePrefix(size_type length)
    {
      COMPLEX_ASSERT(size_ >= length, "Length out of range in string::remove_prefix(length)");
      size_ -= length;
      utils::contiguousMoveElements(data_, data_ + length, size_);
      return *this;
    }
    stringnd &
    removeSuffix(size_type length)
    {
      COMPLEX_ASSERT(size_ >= length, "Length out of range in string::remove_suffix(length)");
      size_ -= length;
      data_[size_] = '\0';
      return *this;
    }

    // transformations

    stringnd &
    filterOut(utils::string_view characters)
    {
      for (usize i = size_; i > 0; --i)
        if (utils::find(characters, data_[i - 1]) != characters.end())
          utils::contiguousMoveElements(data_ + i - 1, data_ + i, size_ - i);

      return *this;
    }
    stringnd &
    filterIn(utils::string_view characters)
    {
      for (usize i = size_; i > 0; --i)
        if (utils::find(characters, data_[i - 1]) == characters.end())
          utils::contiguousMoveElements(data_ + i - 1, data_ + i, size_ - i);

      return *this;
    }
    stringnd &
    trim()
    {
      usize i;

      for (i = size_; i > 0; --i)
      {
        if (!(data_[i - 1] == ' ' || data_[i - 1] == '\t' ||
          (data_[i - 1] >= '\n' && data_[i - 1] <= '\r')))
          break;
      }
      removeSuffix(size_ - i);

      for (i = 0; i < size_; ++i)
      {
        if (!(data_[i - 1] == ' ' || data_[i - 1] == '\t' ||
          (data_[i - 1] >= '\n' && data_[i - 1] <= '\r')))
          break;
      }
      removePrefix(i);

      return *this;
    }
    stringnd &
    toLower()
    {
      for (usize i = 0; i < size_; ++i)
        if (data_[i] >= 'A' && data_[i] < 'Z')
          data_[i] |= (char)(1 << 5);

      return *this;
    }
    stringnd &
    toUpper()
    {
      for (usize i = 0; i < size_; ++i)
        if (data_[i] >= 'a' && data_[i] < 'z')
          data_[i] &= (char)(~(1 << 5));

      return *this;
    }

    // searches

    [[nodiscard]] size_type
    find(utils::string_view substring, size_type position = 0) const
    { return utils::string_view{ *this }.find(substring, position); }

    [[nodiscard]] size_type 
    rfind(utils::string_view substring, size_type position = npos) const
    { return utils::string_view{ *this }.rfind(substring, position); }
    
    // comparisons

    [[nodiscard]] constexpr int 
    compare(utils::string_view other) const { return utils::string_view{ *this }.compare(other); }

    [[nodiscard]] constexpr bool 
    operator==(utils::string_view other) const { return utils::string_view{ *this } == other; }

    [[nodiscard]] constexpr bool 
    operator<(utils::string_view other) const { return compare(other) < 0; }

    constexpr void swap(stringnd &other)
    {
      COMPLEX_SWAP_MEMBERS(data_, other);
      COMPLEX_SWAP_MEMBERS(allocatorType_, other);
      COMPLEX_SWAP_MEMBERS(freeingDestructor_, other);
      COMPLEX_SWAP_MEMBERS(size_, other);
      COMPLEX_SWAP_MEMBERS(capacity_, other);
    }
  };

  class string : public stringnd
  {
  public:
    ~string() { destructor(false); }

    using stringnd::stringnd;

    constexpr string() = default;
    constexpr string(stringnd &&other) noexcept { swap(other); }
    string &
    operator=(string &&other) noexcept
    {
      (void)stringnd::operator=(COMPLEX_MOVE(other));
      return *this;
    }
  };

  inline usize
  floatToString(double value, char *string, usize maximumStringLength,
    usize maximumDecimalLength = 5, bool keepPlus = false, bool clearTrailingZeroes = false)
  {
    COMPLEX_ASSERT(maximumStringLength > 0);
    const char *format = keepPlus ? "%+.*f" : "%.*f";

    usize size = (usize)::stbsp_snprintf(string, (int)maximumStringLength, format, (int)maximumDecimalLength, value);

    if (clearTrailingZeroes && maximumDecimalLength)
    {
      usize i;
      for (i = size - 1; i > 0; --i)
        if (string[i] != '0' && string[i] != ' ' && string[i] != '\0')
          break;

      size -= (size - 1) - i;
      if (string[size - 1] == '.')
        --size;
      string[size] = '\0';
    }

    return size;
  }

  inline utils::string
  floatToString(Allocator allocator, double value, 
    usize maximumDecimalLength = 5, bool keepPlus = false)
  {
    char buffer[48]{};
    usize size = floatToString(value, buffer, sizeof(buffer), maximumDecimalLength, keepPlus);
    return utils::string{ allocator, buffer, size };
  }

  inline void floatToString(utils::string &preallocatedString, double value,
    usize maximumDecimalLength = 5, bool keepPlus = false)
  {
    char buffer[48]{};
    usize size = floatToString(value, buffer, sizeof(buffer), maximumDecimalLength, keepPlus);
    preallocatedString.copy({ buffer, size });
  }

  struct MemoryLogger
  {
    mutable satomi::atomic<i32> writeLock{};
    utils::vector<void *> toLog{};

    void add(void *pointer);
    void erase(void *pointer);
    bool contains(void *pointer) const;
  };
}

extern utils::MemoryLogger memoryLogger;
