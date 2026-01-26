
// Created: 2022-07-22 17:34:59

#pragma once

#include "stl_utils.hpp"
#include "satomi.hpp"

namespace utils
{
  enum class AllocatorType : u8 { General, BumpArena };

  template<typename T>
  concept AllocatorConcept = requires(T *allocator, usize size,
    usize alignment, bool clean, bool threadSafe, const void *existingAllocation)
  {
    // AllocatorType tag
    requires utils::is_same_v<decltype(T::type), const utils::AllocatorType>;

    // basic usage functions
    T::insert(allocator, size, alignment, clean, threadSafe);
    T::remove(existingAllocation, threadSafe);

    // extracting an effective allocator from an existing allocation and being able to use it
    T::insert(static_cast<decltype(T::fromAllocation(existingAllocation))>(nullptr), size, alignment, clean, threadSafe);
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
      u32 size{};
      union
      {
        u32 offsetToArena{};
        u32 next;
      };
    };

    satomi::atomic<bool> lock{};
    u8 flags{};

    // reserved and committed sizes stored are size - 1 to be able to represent up to 4 GB
    // but otherwise every other index represents its actual value
    u32 reservedSize;
    u32 committedSize;
    u32 freeNodeStart;

    bumpArena *nextArena{};

    static usize getUsedSize(bumpArena *arena) noexcept;
    static strict_inline bumpArena *
    fromAllocation(const void *data)
    {
      auto *node = utils::launder((bumpArena::node *)((byte *)data - sizeof(bumpArena::node)));
      return utils::launder((bumpArena *)((byte *)node - node->offsetToArena));
    }

    static byte *insert(bumpArena *arena, usize size, usize alignment, bool clean = false, bool threadSafe = true);
    static strict_inline byte *
    insert(const void *data, usize size, usize alignment, bool clean = false, bool threadSafe = true)
    {
      return insert(fromAllocation(data), size, alignment, clean, threadSafe);
    }
    static void remove(const void *data, bool threadSafe = true);
    static void shrinkToFit(bumpArena *arena, bool shrinkToCommitted, bool threadSafe = true);

    static strict_inline bumpArena *
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
    static strict_inline bumpArena *
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

    static void clear(bumpArena *arena, bool threadSafe = true);

    static void destroy(bumpArena *arena);
  };

  // TODO: archive this arena implementation
  struct arena
  {
    //static constexpr auto type = AllocatorType::Arena;

    struct node
    {
      //static constexpr auto type = AllocatorType::ArenaNode;

      u32 size{};
      u32 previous{};
      u32 next{};
      // -1 is write-exclusive, else read-shared
      satomi::atomic<i16> lock{};

      u8 alignment{};
      satomi::atomic<bool> reservedForInsertion{};

      satomi::atomic<node *> previousFree{};
      satomi::atomic<node *> nextFree{};
      satomi::atomic<node *> childrenFree{};

      static strict_inline arena::node *
      fromAllocation(const void *data)
      { return utils::launder((arena::node *)((byte *)data - sizeof(arena::node))); }
      // handling accidental identity
      static strict_inline arena::node *fromAllocation(arena::node *node) { return node; }
      
      static strict_inline byte *
      insert(node *node, usize size, usize alignment, bool clean = false, bool threadSafe = true)
      { return arena::insert(node, size, alignment, clean, threadSafe); }

      static strict_inline void remove(const void *data, bool threadSafe = true) { arena::removeNode(fromAllocation(data), threadSafe); }
      static strict_inline void remove(node *node, bool threadSafe = true) { arena::removeNode(node, threadSafe); }
    };

    node firstNode { .size = sizeof(data) };
    struct
    {
      const usize reservedSize;
      satomi::atomic<usize> committedSize{};
      satomi::atomic<node *> lastNode{};
      satomi::atomic<arena *> nextArena{};
    } data;

    static_assert(alignof(decltype(data)) <= alignof(node));

    static strict_inline arena::node *
    fromAllocation(const void *data)
    { return utils::launder((arena::node *)((byte *)data - sizeof(arena::node))); }

    static byte *insert(arena *arena, usize size, usize alignment, bool clean = false, bool threadSafe = true);
    static byte *insert(arena::node *node, usize size, usize alignment, bool clean = false, bool threadSafe = true);
    static strict_inline byte *
    insert(const void *data, usize size, usize alignment, bool clean = false, bool threadSafe = true)
    { return insert(fromAllocation(data), size, alignment, clean, threadSafe); }

    static void  removeNode(arena::node *node, bool threadSafe = true);
    static byte *resizeNode(arena::node *node, usize newSize, bool threadSafe = true);

    static strict_inline void remove(const void *data, bool threadSafe = true) { removeNode(fromAllocation(data), threadSafe); }
    static strict_inline void resize(void *data, usize newSize, bool threadSafe = true) { resizeNode(fromAllocation(data), newSize, threadSafe); }


    static strict_inline arena *
    create(usize reservedSize = COMPLEX_MB(128), usize commitSize = COMPLEX_KB(512))
    {
      COMPLEX_ASSERT(reservedSize > 0);
      COMPLEX_ASSERT(reservedSize >= commitSize);
      void *memory = reserveMemory(reservedSize);
      auto committedSize = utils::max(sizeof(arena), commitSize);
      commitMemory(memory, committedSize);

      return new(memory) arena{ .data = { .reservedSize = reservedSize,
        .committedSize = committedSize, .lastNode = (arena::node *)memory } };
    }
    template<AllocatorConcept T>
    static strict_inline arena *
    createNested(T *differentArena, usize allocateSize)
    {
      byte *memory = T::insert(differentArena, allocateSize + sizeof(arena), alignof(arena));
      arena *result = new(memory) arena{ .data = { .reservedSize = allocateSize + sizeof(arena),
        .committedSize = allocateSize + sizeof(arena), .lastNode = (arena::node *)memory } };
      result->firstNode.alignment = (u8)T::type;
      return result;
    }
    static void destroy(arena *arena);

    static void linkDeallocation(const void *parentResource, const void *childResource);
    static void unlinkDeallocation(const void *childResource);
    static void relinkDeallocation(const void *oldChildResource, const void *newChildResource);
  };

  // anew, the replacement to operator new (with arenas)
  // using placement new on memory resource, provided by an arena implementation

  #define anew(allocator, T, ...) (new(allocator->insert(allocator, sizealignof(T))) T __VA_ARGS__)
  #define arranew(allocator, T, count, ...) (new(allocator->insert(allocator, sizealignof(T, count))) T[count] __VA_ARGS__)

  struct Allocator
  {
    struct AllocatorVtable
    {
      byte *(*insert)(void *allocator, usize size, usize alignment, bool clean, bool threadSafe);
      void (*remove)(const void *allocation, bool threadSafe);
      Allocator(*fromAllocation)(const void *allocation);
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
    template<typename T>
    static constexpr AllocatorVtable 
    createVtable()
    {
      return AllocatorVtable
      {
        .insert = [](void *allocator, usize size, usize alignment, bool clean, bool threadSafe)
        { return T::insert((T *)allocator, size, alignment, clean, threadSafe); },
        .remove = T::remove,
        .fromAllocation = Allocator::getAllocatorFromFunction<T>,
        .type = T::type
      };
    }
  public:

    static constexpr AllocatorVtable generalAllocatorVtable
    {
      .insert = [](void *, usize size, usize alignment, bool clean, bool)
      {
        byte *memory = utils::allocate(size, alignment);
        if (clean)
          ::zeroset(memory, size);
        return memory;
      },
      .remove = [](const void *allocation, bool) { utils::deallocate(allocation); },
      .fromAllocation = getGeneralAllocator,
      .type = AllocatorType::General
    };


    static constexpr AllocatorVtable allocatorTable[] = { generalAllocatorVtable, createVtable<utils::bumpArena>() };

    const AllocatorVtable *vtable{};
    void *allocator{};
    bool freeingDestructor = true;

    constexpr Allocator() = default;
    template<typename T>
    constexpr Allocator(T *allocator, bool freeingDestructor = true) :
      vtable{ &allocatorTable[(usize)allocator->type] }, allocator{ allocator },
      freeingDestructor{ freeingDestructor } { }
    constexpr Allocator(const AllocatorVtable *vtable, void *allocator, 
      bool freeingDestructor = true) : vtable{ vtable }, allocator{ allocator },
      freeingDestructor{ freeingDestructor } { }

    strict_inline byte *
    insert(usize size, usize alignment, bool clean = true, bool threadSafe = true) const
    { return vtable->insert(allocator, size, alignment, clean, threadSafe); }
    strict_inline void remove(const void *allocation, bool threadSafe = true) const { if (allocation) vtable->remove(allocation, threadSafe); }
    strict_inline Allocator fromAllocation(const void *allocation) const { return vtable->fromAllocation(allocation); }
    strict_inline AllocatorType type() const noexcept { return vtable->type; }

    static constexpr Allocator fromType(AllocatorType type) { return { &allocatorTable[(usize)type], nullptr }; }

    explicit operator bool() const noexcept { return vtable; }
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
    if constexpr (utils::is_constant_evaluated() || !utils::is_trivially_destructible_v<T>)
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
  class vector
  {
    static constexpr usize kStartingCapacity = 16;

    T *data_{};
    usize	allocatorType_ : 1 = (usize)AllocatorType::General;
    usize freeingDestructor_ : 1 = true;
    usize	size_ : 62{};
    usize	capacity_{};

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

    ~vector()
    {
      utils::contiguousDestroy(data_, size_);
      if (freeingDestructor_)
        Allocator::fromType((AllocatorType)allocatorType_).remove(data_);

      data_ = {};
      size_ = 0;
      capacity_ = 0;
    }

    vector(Allocator allocator, usize reserveSpace) { reserve(allocator, reserveSpace); }
    vector(Allocator allocator, const auto &data) requires requires { data.data(); data.size(); }
    {
      utils::contiguousClone(allocator, allocator, data.data(), data.size(), data_, size_, capacity_);
      allocatorType_ = (usize)allocator.type();
      freeingDestructor_ = allocator.freeingDestructor;
      size_ = data.size();
    }

    constexpr vector() = default;
    vector(const vector &other) = delete;
    vector &operator=(const vector &other) = delete;
    constexpr vector(vector &&other) { swap(other); }
    vector &operator=(vector &&other)
    {
      if (this == &other)
        return *this;

      this->~vector();
      swap(other);

      return *this;
    }

    [[nodiscard]] vector
    clone() const
    {
      vector ret{};

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
      auto oldAllocator = Allocator::fromType((AllocatorType)allocatorType_).fromAllocation(data_);
      allocator = (allocator) ? allocator : oldAllocator;
      utils::contiguousReserve(allocator, oldAllocator, capacity, data_, size_, capacity_);
      allocatorType_ = (usize)allocator.type();
      freeingDestructor_ = allocator.freeingDestructor;
    }

    constexpr void clear()
    {
      for (auto &element : *this)
        element.~T();
      
      size_ = 0;
    }

    constexpr usize size() const { return size_; }
    constexpr usize capacity() const { return capacity_; }
    constexpr bool empty() const { return size_ == 0; }

    // returning void * in order to force error if someone wants to do assignment at call site
    void *
    push_back(usize count = 1)
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

    T &emplace_back(auto &&... args) { return *new(push_back()) T{ COMPLEX_FWD(args)... }; }

    void *
    insert(iterator iter)
    {
      usize index = (usize)(iter - data_);

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

    constexpr void pop_back()
    {
      if (!size_)
        return;

      back().~T();
      --size_;
    }

    void erase(iterator iter) { erase(iter, 1); }
    void erase(iterator start, usize count)
    {
      usize index = (usize)(start - data_);

      COMPLEX_HARD_ASSERT(index < size());

      size_ -= count;
      for (usize i = 0; i < count; ++i)
        (start + (isize)i)->~T();
      
      utils::contiguousMoveElements(start, start + count, data_ + size_ - start);
    }

    void erase(const T &element) 
    { erase_if([&element](const auto &data) { return element == data; }); }
    void erase_if(const auto &predicate)
    {
      usize offset{};
      usize lastShift = usize(-1);
      for (usize i = 0; i < size_; ++i)
      {
        if (predicate(data_[i]))
        {
          if (lastShift != usize(-1) && (i - lastShift) > 1)
            utils::contiguousMoveElements(data_ + lastShift, data_ + lastShift + offset, i - lastShift);

          lastShift = i;
          ++offset;
        }
      }

      if (lastShift != usize(-1) && (size_ - lastShift) > 1)
        utils::contiguousMoveElements(data_ + lastShift, data_ + lastShift + offset, size_ - lastShift);
    }

    constexpr void swap(vector &other) noexcept
    {
      COMPLEX_SWAP_MEMBERS(data_, other);
      COMPLEX_SWAP_MEMBERS(allocatorType_, other);
      COMPLEX_SWAP_MEMBERS(freeingDestructor_, other);
      COMPLEX_SWAP_MEMBERS(size_, other);
      COMPLEX_SWAP_MEMBERS(capacity_, other);
    }

    constexpr T &operator[](usize i) { COMPLEX_HARD_ASSERT(i < size_); return data_[i]; }
    constexpr const T &operator[](usize i) const { COMPLEX_HARD_ASSERT(i < size_); return data_[i]; }

    constexpr T &front() { return data_[0]; }
    constexpr const T &front() const { return data_[0]; }
    constexpr T &back() { return data_[size_ - 1]; }
    constexpr const T &back() const { return data_[size_ - 1]; }

    constexpr T *data() { return data_; }
    constexpr const T *data() const { return data_; }

    constexpr const iterator begin() const { return data_; }
    constexpr iterator begin() { return data_; }
    constexpr const iterator end() const { return begin() + size_; }
    constexpr iterator end() { return begin() + size_; }
  };
  
  template<typename Range>
  vector(Allocator allocator, const Range &) -> vector<remove_pointer_t<decltype(declval<Range>().data())>>;

  // a type alias to signal that the destructor doesn't have to run
  // to be used in arena context where freeing is automatic
  template<typename T>
  using vectornd = vector<T>;

  template<typename Key, typename Value>
  struct vector_map
  {
    utils::vector<utils::pair<Key, Value>> data{};

    const Value &operator[](usize index) const noexcept { return data[index].second; }
    Value &operator[](usize index) noexcept { return data[index].second; }

    //---------------------------------------------------------
    // the following functions assume keys are unique

    constexpr auto find(const Key &key) noexcept
    { return utils::find_if(data, [&key](const auto &v) { return v.first == key; }); }

    constexpr auto find(const Key &key) const noexcept
    { return utils::find_if(data, [&key](const auto &v) { return v.first == key; }); }

    constexpr auto find_if(const auto &functor) noexcept { return utils::find_if(data, functor); }
    constexpr auto find_if(const auto &functor) const noexcept { return utils::find_if(data, functor); }

    constexpr void add(Key key, Value value) noexcept
    { data.emplace_back(COMPLEX_MOVE(key), COMPLEX_MOVE(value)); }

    constexpr void update(const Key &key, utils::pair<Key, Value> updatedEntry) noexcept
    {
      auto iter = find(key);
      if (iter == data.end())
        return;

      (*iter) = COMPLEX_MOVE(updatedEntry);
    }

    constexpr void erase(const Key &key) noexcept
    {
      auto iter = utils::find_if(data, [&key](const auto &v) { return v.first == key; });
      if (iter != data.end())
        data.erase(iter);
    }

  private:
    static constexpr auto binary_search(auto &container, auto &element, const auto &predicate)
    {
      auto low = container.begin();
      auto high = container.end() - 1;

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

      return utils::pair{ low, false };
    }
  public:

    //---------------------------------------------------------
    // the following functions assume keys are NOT unique

    constexpr auto binary_search(const utils::pair<Key, Value> &element) const noexcept
    {
      auto [iter, wasFound] = binary_search(data, element, [](const auto &element, const auto &test) { return element == test; });
      return (wasFound) ? iter : data.end();
    }
    constexpr auto binary_search(const utils::pair<Key, Value> &element) noexcept
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
    constexpr auto add_ordered(Key key, Value value) noexcept
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
    constexpr void update_ordered(const utils::pair<Key, Value> &entry, utils::pair<Key, Value> updatedEntry) noexcept
    {
      auto iter = binary_search(entry);
      if (iter == data.end())
        return;

      (*iter) = COMPLEX_MOVE(updatedEntry);
    }

    // this will work correctly only if you ever use the ordered functions
    constexpr void erase_ordered(const utils::pair<Key, Value> &element) noexcept
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

    constexpr auto add_ordered(auto entry) noexcept
    { return base::add_ordered(typeId(decltype(entry)), (void *)COMPLEX_MOVE(entry)); }

    template<typename T>
    constexpr void update_ordered(T entry, T updatedEntry) noexcept
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

    constexpr void erase_ordered(auto entry) noexcept
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

  template<typename T>
  struct stable_vector
  {
    static_assert(is_default_constructible_v<T> || 
      is_trivially_destructible_v<T>, "T must be default constructible or trivially destructible");
    static constexpr u32 kSegmentsToSkip = 4;
  private:
    struct free_list
    {
      u32 nextIndex;
      u32 size;
    };
    static_assert(sizeof(T) >= sizeof(free_list));

  #define capacity_for_segments(segmentCount) ((1 << (kSegmentsToSkip + segmentCount)) - (1 << kSegmentsToSkip))
  #define size_of_segment(segment) (1 << (kSegmentsToSkip + segment))
  #define segment_for_index(index) (log2((index >> kSegmentsToSkip) + 1))

    strict_inline utils::pair<u32, u32> 
    getSegmentAndSlot(u32 index)
    {
      u32 segment = segment_for_index(index);
      u32 slot = index - capacity_for_segments(segment);
      return { segment, slot };
    }
  public:
    u8 usedSegments{};
    u32 firstFreeSlot = u32(-1);
    T *segments[sizeof(firstFreeSlot) * 8 - kSegmentsToSkip]{};

    ~stable_vector()
    {
      if constexpr (!is_trivially_destructible_v<T>)
      {
        u32 size = capacity_for_segments(usedSegments);
        for (u32 index = 0, nextIndex = firstFreeSlot; index < size;)
        {
          auto [segment, slot] = getSegmentAndSlot(index);

          if (nextIndex == index)
          {
            free_list list;
            ::memcpy(&list, segments[segment] + slot, sizeof(list));
            nextIndex = list.nextIndex;

            index += list.size;

            continue;
          }

          segments[segment][slot].~T();
          ++index;
        }
      }

      for (u8 i = 0; i < usedSegments; ++i)
        deallocate(segments[i]);
    }

    T &
    operator[](usize index)
    {
      auto [segment, slot] = getSegmentAndSlot(index);
      return segments[segment][slot];
    }

    void reserve(usize newSize)
    {
      COMPLEX_ASSERT(newSize > 0 && newSize < u32(-1));
      
      u8 segmentCount = (u8)segment_for_index((u32)newSize) + 1;
      if (usedSegments >= segmentCount)
        return;

      free_list list{ firstFreeSlot };
      u32 lastFreeList = firstFreeSlot;
      while (list.nextIndex != u32(-1))
      {
        lastFreeList = list.nextIndex;
        auto [segment, slot] = getSegmentAndSlot(lastFreeList);
        ::memcpy(&list, segments[segment] + slot, sizeof(list));
      }

      for (u8 i = usedSegments; i < segmentCount; ++i)
      {
        u32 segmentSize = size_of_segment(i);
        segments[i] = (T *)utils::allocate(sizealignof(T, segmentSize));

        auto capacity = capacity_for_segments(i);
        if (lastFreeList == u32(-1))
          firstFreeSlot = capacity;
        else
        {
          list.nextIndex = capacity;
          auto [segment, slot] = getSegmentAndSlot(lastFreeList);
          ::memcpy(segments[segment] + slot, &list, sizeof(list));
        }

        list = { u32(-1), segmentSize };
        lastFreeList = capacity;
      }

      auto [segment, slot] = getSegmentAndSlot(lastFreeList);
      ::memcpy(segments[segment] + slot, &list, sizeof(list));

      usedSegments = segmentCount;
    }

    // using the returned range with insert_at is mandatory
    usize
    get_contiguous_range(usize rangeSize)
    {
      u32 nextIndex = firstFreeSlot;
      free_list list{ u32(-1), u32(-1) };
      u32 indexBeforeBestFitting = firstFreeSlot;
      free_list bestFittingList{ u32(-1), u32(-1) };

      // search for space in the free list
      while (true)
      {
        if (nextIndex == u32(-1) || rangeSize == bestFittingList.size)
          break;
        
        auto previousIndex = list.nextIndex;
        auto [segment, slot] = getSegmentAndSlot(nextIndex);
        ::memcpy(&list, segments[segment] + slot, sizeof(list));
        
        {
          auto temp = list.nextIndex;
          list.nextIndex = nextIndex;
          nextIndex = temp;
        }

        u32 capacity = capacity_for_segments(segment);

        u32 beforeNextSegment = utils::min(capacity - list.nextIndex, list.size);
        u32 afterNextSegment = list.size - beforeNextSegment;

        if (beforeNextSegment >= rangeSize && beforeNextSegment < bestFittingList.size)
        {
          indexBeforeBestFitting = previousIndex;
          bestFittingList = { list.nextIndex, beforeNextSegment };
        }
        else if (afterNextSegment >= rangeSize && afterNextSegment < bestFittingList.size)
        {
          indexBeforeBestFitting = previousIndex;
          bestFittingList = { list.nextIndex, afterNextSegment };
        }
      }

      {
        auto temp = list.nextIndex;
        list.nextIndex = nextIndex;
        nextIndex = temp;
      }

      // insert new segments and find a suitable range
      while (bestFittingList.nextIndex == u32(-1))
      {
        u32 segmentSize = size_of_segment(usedSegments);
        segments[usedSegments] = (T *)utils::allocate(sizealignof(T, segmentSize));
        ++usedSegments;

        free_list newList{ list.nextIndex, segmentSize };
        list.nextIndex = capacity_for_segments(usedSegments - 1);
        if (segmentSize >= rangeSize)
        {
          indexBeforeBestFitting = nextIndex;
          bestFittingList = { list.nextIndex, (u32)rangeSize };
        }

        if (nextIndex == u32(-1))
          firstFreeSlot = list.nextIndex;
        else
        {
          auto [segment, slot] = getSegmentAndSlot(nextIndex);
          ::memcpy(segments[segment] + slot, &list, sizeof(list));
        }
        auto [newSegment, newSlot] = getSegmentAndSlot(list.nextIndex);
        ::memcpy(segments[newSegment] + newSlot, &newList, sizeof(newList));

        nextIndex = list.nextIndex;
        list = newList;

        if (segmentSize >= rangeSize)
          bestFittingList = list;
      }

      auto newIndex = bestFittingList.nextIndex;
      free_list storedFreeList;
      {
        auto [segment, slot] = getSegmentAndSlot(bestFittingList.nextIndex);
        ::memcpy(&storedFreeList, segments[segment] + slot, sizeof(storedFreeList));
        storedFreeList.size -= rangeSize;

        u32 capacity = capacity_for_segments(segment);
        if (capacity - bestFittingList.nextIndex == bestFittingList.size)
        {
          newIndex += rangeSize;
          segment = segment_for_index(newIndex);
          slot = newIndex - capacity_for_segments(segment);
        }

        ::memcpy(segments[segment] + slot, &storedFreeList, sizeof(storedFreeList));
      }

      if (bestFittingList.nextIndex != newIndex)
      {
        auto [segment, slot] = getSegmentAndSlot(indexBeforeBestFitting);
        free_list previousList;
        ::memcpy(&previousList, segments[segment] + slot, sizeof(storedFreeList));
        previousList.nextIndex = newIndex;
        ::memcpy(segments[segment] + slot, &storedFreeList, sizeof(storedFreeList));
      }

      return bestFittingList.nextIndex;
    }

    // only use this in combination with get_contiguous_range
    void insert_at(usize index, auto &&... args)
    {
      auto [segment, slot] = getSegmentAndSlot(index);
      (void)new(&segments[segment][slot]) T{ COMPLEX_FWD(args)... };
    }

  private:
    utils::pair<void *, usize>
    use_first_free_slot()
    {
      u32 index;
      u32 segment;
      u32 slot;
      if (firstFreeSlot != u32(-1))
      {
        index = firstFreeSlot;
        segment = segment_for_index(index);
        slot = index - capacity_for_segments(segment);
        free_list list;
        ::memcpy(&list, segments[segment] + slot, sizeof(list));
        if (list.size == 1)
          firstFreeSlot = list.nextIndex;
        else
        {
          firstFreeSlot = index + 1;
          list.size -= 1;
          auto [newSegment, newSlot] = getSegmentAndSlot(index + 1);
          ::memcpy(segments[newSegment] + newSlot, &list, sizeof(list));
        }
      }
      else
      {
        index = capacity_for_segments(usedSegments);
        u32 segmentSize = size_of_segment(usedSegments);
        segments[usedSegments] = (T *)utils::allocate(sizealignof(T, segmentSize));
        segment = usedSegments;
        slot = 0;

        free_list list{ firstFreeSlot, segmentSize - 1 };
        ::memcpy(segments[usedSegments] + 1, &list, sizeof(list));

        ++usedSegments;
      }

      return { &segments[segment][slot], index };
    }

  public:
    void *
    insert_wherever() { return use_first_free_slot().first; }

    usize
    insert_wherever(auto &&... args)
    {
      auto [memory, index] = use_first_free_slot();
      (void)new(memory) T{ COMPLEX_FWD(args)... };
      return index;
    }
  private:
    u32
    erase(u32 index)
    {
      free_list list{};
      list.nextIndex = firstFreeSlot;
      auto lastIndex = firstFreeSlot;
      u32 lastSegment;
      u32 lastSlot;

      while (list.nextIndex < index)
      {
        lastIndex = list.nextIndex;        
        lastSegment = segment_for_index(lastIndex);
        lastSlot = lastIndex - capacity_for_segments(lastSegment);
        ::memcpy(&list, segments[lastSegment] + lastSlot, sizeof(list));
      }
      COMPLEX_HARD_ASSERT((lastIndex + list.size) > index, 
        "Erasing an already erased element");

      auto [segment, slot] = getSegmentAndSlot(index);
      segments[segment][slot].~T();

      // coalesce current erased element with previous range
      if (lastIndex + list.size == index)
      {
        ++list.size;
        ::memcpy(segments[lastSegment] + lastSlot, &list, sizeof(list));
        segment = lastSegment;
        slot = lastSlot;
        index = lastIndex;
      }
      else
      {
        if (list.nextIndex == firstFreeSlot)
          firstFreeSlot = index;
        else
        {
          auto nextIndex = list.nextIndex;
          list.nextIndex = index;
          ::memcpy(segments[lastSegment] + lastSlot, &list, sizeof(list));
          list.nextIndex = nextIndex;
        }

        list.size = 1;
        ::memcpy(segments[segment] + slot, &list, sizeof(list));
      }

      // coalesce current erased element with next range
      if (index + list.size == list.nextIndex)
      {
        auto [nextSegment, nextSlot] = getSegmentAndSlot(list.nextIndex);

        free_list nextList;
        ::memcpy(&nextList, segments[nextSegment] + nextSlot, sizeof(nextList));

        list.nextIndex = nextList.nextIndex;
        list.size += nextList.size;
        ::memcpy(segments[segment] + slot, &list, sizeof(list));
      }

      return index;
    }
  public:
    void erase(usize index) { (void)erase((u32)index); }

    void erase(usize start, usize size)
    {
      COMPLEX_ASSERT(size > 0);

      auto index = erase((u32)start);
      auto [segment, slot] = getSegmentAndSlot(index);
      
      free_list list;
      ::memcpy(&list, segments[segment] + slot, sizeof(list));

      COMPLEX_HARD_ASSERT(list.nextIndex >= u32(start + size),
        "Erasing an already erased element");

      do 
      {
        ++index;
        ++slot;
        if (size_of_segment(segment) <= index)
        {
          ++segment;
          slot = 0;
        }

        segments[segment][slot].~T();
      } while (index < u32(start + size));

      list.size += size - 1;

      // coalesce current range with next range
      if (list.nextIndex == u32(start + size))
      {
        auto [nextSegment, nextSlot] = getSegmentAndSlot(list.nextIndex);

        free_list nextList;
        ::memcpy(&nextList, segments[nextSegment] + nextSlot, sizeof(nextList));

        list.nextIndex = nextList.nextIndex;
        list.size += nextList.size;
      }

      ::memcpy(segments[segment] + slot, &list, sizeof(list));
    }

    struct iterator
    {
      const stable_vector *vector{};
      mutable u32 index{};
      mutable u8 segment{};

      const iterator &operator++() const
      {
        ++index;
        COMPLEX_ASSERT(index < capacity_for_segments(vector->usedSegments));
        if (size_of_segment(segment) <= index)
          ++segment;

        return *this;
      }
      const iterator &operator--() const
      {
        COMPLEX_ASSERT(index > 0);

        --index;
        if (size_of_segment(segment - 1) > index)
          --segment;

        return *this;
      }
      iterator &operator++()
      { return const_cast<iterator &>(((const iterator *)this)->operator++()); }
      iterator &operator--()
      { return const_cast<iterator &>(((const iterator *)this)->operator--()); }
      iterator operator++(int) const
      {
        iterator old = *this;
        operator++();
        return old;
      }
      iterator operator--(int) const
      {
        iterator old = *this;
        operator--();
        return old;
      }
      const iterator &operator+=(isize offset) const
      {
        index += offset;
        COMPLEX_ASSERT(index < capacity_for_segments(vector->usedSegments));
        segment = (u8)log2((index >> kSegmentsToSkip) + 1);
        
        return *this;
      }
      iterator &operator+=(isize offset)
      { return const_cast<iterator &>(((const iterator *)this)->operator+=(offset)); }
      const iterator &operator-=(isize offset) const { return operator+=(-offset); }
      iterator &operator-=(isize offset) { return operator+=(-offset); }

      const iterator operator+(isize offset) const { auto copy = *this; copy += offset; return copy; }
      iterator operator+(isize offset) { auto copy = *this; copy += offset; return copy; }
      const iterator operator-(isize offset) const { return operator+(-offset); }
      iterator operator-(isize offset) { return operator+(-offset); }

      const T &operator*() const { return vector->segments[segment][index - capacity_for_segments(segment)]; }
      T &operator*() { return vector->segments[segment][index - capacity_for_segments(segment)]; }
      const T *operator->() const { return &vector->segments[segment][index - capacity_for_segments(segment)]; }
      T *operator->() { return &vector->segments[segment][index - capacity_for_segments(segment)]; }
    };

    const iterator begin() const { return iterator{ this }; }
    iterator begin() { return iterator{ this }; }
    const iterator end() const { return begin() + capacity_for_segments(usedSegments); }
    iterator end() { return begin() + capacity_for_segments(usedSegments); }

  #undef capacity_for_segments
  #undef size_of_segment
  #undef segment_for_index
  };


  class string
  {
    char *data_{};
    usize	allocatorType_ : 1 = (usize)AllocatorType::General;
    usize	freeingDestructor_ : 1 = true;
    usize size_ : 62 {};
    usize capacity_{};

  public:
    using value_type = char;
    using size_type = usize;
    using difference_type = isize;
    using iterator = char *;
    using const_iterator = const char *;

    static constexpr auto npos = size_type(-1);

    ~string()
    {
      utils::contiguousDestroy(data_, size_);
      if (freeingDestructor_)
        Allocator::fromType((AllocatorType)allocatorType_).remove(data_);

      data_ = {};
      size_ = {};
      capacity_ = {};
    }


    // convenience constructors, because it gets really annoying otherwise
    static string 
    create(const char *format, auto &&... args) 
    { return create(utils::generalAllocator, format, COMPLEX_FWD(args)...); }

    template<auto Size>
    string(const char(&data)[Size]) : string{ utils::generalAllocator, data, Size } { }
    string(const auto &data, size_type start = 0, size_type size = npos) : string{ utils::generalAllocator, data, start, size } { }


    static string 
    create(Allocator allocator, const char *format, auto &&... args)
    {
      string ret{};
      ret.size_ = (size_type)::stbsp_snprintf(nullptr, 0, format, COMPLEX_FWD(args)...);
      ret.reserve(allocator, ret.size_);
      ::stbsp_snprintf(ret.data_, (int)ret.capacity_ + 1, format, COMPLEX_FWD(args)...);
      return ret;
    }

    string(Allocator allocator, size_type reserveSpace) : string{ allocator, nullptr, reserveSpace } { }
    template<auto Size>
    string(Allocator allocator, const char(&data)[Size]) : string{ allocator, data, Size } { }
    string(Allocator allocator, const auto &data, size_type start = 0, size_type size = npos)
      requires utils::is_same_v<decltype(data.data()), const char *> : 
      string{ allocator, data.data() + utils::min(data.size(), start), utils::min(data.size(), size) - utils::min(data.size(), start) } { }
    // every other constructor calls this one
    string(Allocator allocator, const char *data, size_type size)
    {
      reserve(allocator, size);
      if (data)
      {
        utils::contiguousMoveElements(data_, data, size);
        size_ = size;
      }
    }

    constexpr string() noexcept = default;
    string(const string &other) = delete;
    string &operator=(const string &other) noexcept = delete;
    constexpr string(string &&other) noexcept { swap(other); }
    string &operator=(string &&other) noexcept
    {
      if (this == &other)
        return *this;

      this->~string();
      swap(other);

      return *this;
    }

    [[nodiscard]] string 
    clone(usize position = 0, usize length = npos) const
    {
      COMPLEX_ASSERT(position <= size_, "Position out of range in string::clone(position, length)");
      COMPLEX_ASSERT(length == npos || length <= size_ - position,
        "Length out of range in string::clone(position, length)");

      string ret{};

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

    constexpr void reserve(usize capacity) { reserve({}, capacity); }
    constexpr void reserve(Allocator allocator, usize capacity)
    {
      constexpr usize kRounding = 16;

      if (!capacity || capacity <= capacity_)
        return;

      // increase size by at least double the current one
      capacity = utils::max(2 * capacity_, capacity);

      // round capacity but keep null terminator in mind
      capacity = utils::roundUpToMultiple(capacity + 1, kRounding);

      auto oldAllocator = Allocator::fromType((AllocatorType)allocatorType_).fromAllocation(data_);
      allocator = (allocator) ? allocator : oldAllocator;
      utils::contiguousReserve(allocator, oldAllocator, capacity, data_, size_, capacity_);
      allocatorType_ = (usize)allocator.type();
      freeingDestructor_ = allocator.freeingDestructor;

      --capacity_;
      ::zeroset(data_ + size_, capacity_ + 1 - size_);
    }

    constexpr void clear() { size_ = 0; }

    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    [[nodiscard]] constexpr size_type length() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr const char *data() const noexcept { return data_; }
    [[nodiscard]] constexpr char *data() noexcept { return data_; }

    [[nodiscard]] constexpr const char &
    operator[](size_type position) const noexcept
    {
      COMPLEX_ASSERT(position < size_, "View index out of range");
      return data_[position];
    }
    [[nodiscard]] constexpr char &
    operator[](size_type position) noexcept
    {
      COMPLEX_ASSERT(position < size_, "View index out of range");
      return data_[position];
    }
    [[nodiscard]] constexpr const char &
    front() const noexcept
    {
      COMPLEX_ASSERT(size_ > 0, "Front of empty view");
      return data_[0];
    }
    [[nodiscard]] constexpr const char & 
    back() const noexcept
    {
      COMPLEX_ASSERT(size_ > 0, "Back of empty view");
      return data_[size_ - 1];
    }

    [[nodiscard]] constexpr iterator begin() noexcept { return (size_ == 0) ? iterator{} : iterator(data_); }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return (size_ == 0) ? const_iterator{} : const_iterator(data_); }
    [[nodiscard]] constexpr iterator end() noexcept { return (size_ == 0) ? iterator{} : iterator(data_ + size_); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return (size_ == 0) ? const_iterator{} : const_iterator(data_ + size_); }

    // insertions
    
    template<typename T>
    string &
    insert(size_type start, const T &other, size_type repetitions = 1) noexcept
    {
      auto view = utils::string_view{ other };

      reserve(size_ + repetitions * view.size());
      utils::contiguousMoveElements(data_ + start + repetitions * view.size(), data_ + start, size_ - start);
      for (usize i = 0; i < repetitions; ++i)
        utils::contiguousMoveElements(data_ + start + i * view.size(), view.data(), view.size());

      return *this;
    }
    string &
    append(const auto &other, size_type repetitions = 1) noexcept { return insert(size_, other, repetitions); }
    string &
    prepend(const auto &other, size_type repetitions = 1) noexcept { return insert(0, other, repetitions); }
    string &
    appendFormat(const char *format, auto &&... args) noexcept
    {
      reserve(size_ + (size_type)::stbsp_snprintf(nullptr, 0, format, COMPLEX_FWD(args)...));
      ::stbsp_snprintf(data_ + size_, (int)capacity_ + 1, format, COMPLEX_FWD(args)...);
      return *this;
    }
    string &
    prependFormat(const char *format, auto &&... args) noexcept
    {
      auto sizeToAdd = (size_type)::stbsp_snprintf(nullptr, 0, format, COMPLEX_FWD(args)...);
      reserve(size_ + sizeToAdd);
      utils::contiguousMoveElements(data_ + sizeToAdd, data_, size_);
      ::stbsp_snprintf(data_, (int)capacity_ + 1, format, COMPLEX_FWD(args)...);
      return *this;
    }

    // simple removals

    string &
    remove(size_type index, size_type length) noexcept
    {
      COMPLEX_ASSERT(size_ >= index + length, "Range is outside of string in string::remove(index, length)");
      utils::contiguousMoveElements(data_ + index, data_ + index + length, size_ - index - length);
      size_ -= length;
      return *this;
    }
    string &
    removePrefix(size_type length) noexcept
    {
      COMPLEX_ASSERT(size_ >= length, "Length out of range in string::remove_prefix(length)");
      size_ -= length;
      utils::contiguousMoveElements(data_, data_ + length, size_);
      return *this;
    }
    string &
    removeSuffix(size_type length) noexcept
    {
      COMPLEX_ASSERT(size_ >= length, "Length out of range in string::remove_suffix(length)");
      size_ -= length;
      return *this;
    }

    // transformations

    template<typename T>
    string &
    filterOut(const T &characters)
    {
      auto view = utils::string_view{ characters };

      for (usize i = size_; i > 0; --i)
        if (utils::find(view, data_[i - 1]) != view.end())
          utils::contiguousMoveElements(data_ + i - 1, data_ + i, size_ - i);
      return *this;
    }
    template<typename T>
    string &
    filterIn(const T &characters)
    {
      auto view = utils::string_view{ characters };

      for (usize i = size_; i > 0; --i)
        if (utils::find(view, data_[i - 1]) == view.end())
          utils::contiguousMoveElements(data_ + i - 1, data_ + i, size_ - i);

      return *this;
    }
    string &
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
    constexpr string &
    toLower()
    {
      for (usize i = 0; i < size_; ++i)
        if (data_[i] >= 'A' && data_[i] < 'Z')
          data_[i] |= (char)(1 << 5);

      return *this;
    }
    constexpr string &
    toUpper()
    {
      for (usize i = 0; i < size_; ++i)
        if (data_[i] >= 'a' && data_[i] < 'z')
          data_[i] &= (char)(~(1 << 5));

      return *this;
    }

    // searches

    [[nodiscard]] constexpr size_type 
    find(const char *substring, size_type size, size_type position = 0) const noexcept
    { return string_view{ *this }.find(substring, size, position); }
    template<typename T>
    [[nodiscard]] constexpr size_type
    find(const T &substring, size_type position = 0) const noexcept
    { return string_view{ *this }.find(utils::string_view{ substring }, position); }

    [[nodiscard]] constexpr size_type 
    rfind(const char *substring, size_type size, size_type position = npos) const noexcept
    { return string_view{ *this }.rfind(substring, size, position); }
    template<typename T>
    [[nodiscard]] constexpr size_type 
    rfind(const T &substring, size_type position = npos) const noexcept 
    { return string_view{ *this }.rfind(utils::string_view{ substring }, position); }
    
    // comparisons

    template<typename T>
    [[nodiscard]] constexpr int 
    compare(const T &other) const noexcept { return string_view{ *this }.compare(utils::string_view{ other }); }

    template<typename T>
    [[nodiscard]] constexpr bool 
    operator==(const T &other) const noexcept { return string_view{ *this } == string_view{ other }; }

    template<typename T>
    [[nodiscard]] constexpr bool 
    operator<(const T &other) const noexcept { return compare(other) < 0; }

    constexpr void swap(string &other) noexcept
    {
      COMPLEX_SWAP_MEMBERS(data_, other);
      COMPLEX_SWAP_MEMBERS(allocatorType_, other);
      COMPLEX_SWAP_MEMBERS(freeingDestructor_, other);
      COMPLEX_SWAP_MEMBERS(size_, other);
      COMPLEX_SWAP_MEMBERS(capacity_, other);
    }
  };

  // a type alias to signal that the destructor doesn't have to run
  // to be used in arena context where freeing is automatic
  using stringnd = string;

  inline usize
  floatToString(double value, char *string, usize maximumStringLength,
    usize maximumDecimalLength = 5, bool keepPlus = false)
  {
    COMPLEX_ASSERT(maximumStringLength > 0);
    const char *format = keepPlus ? "%+.*f" : "%.*f";

    usize size = (usize)::stbsp_snprintf(string, (int)maximumStringLength, format, (int)maximumDecimalLength, value);

    usize i;
    for (i = size - 1; i > 0; --i)
      if (string[i] != '0' && string[i] != ' ' && string[i] != '\0')
        break;

    size -= (size - 1) - i;
    if (string[size - 1] == '.')
      --size;
    string[size] = '\0';

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
}

extern thread_local utils::bumpArena *localScratch;
