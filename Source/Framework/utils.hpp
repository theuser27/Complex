
// Created: 2021-07-27 00:53:28

#pragma once

#include "satomi.hpp"
#include "stl_utils.hpp"
#include "constants.hpp"

namespace utils
{
  struct Pool;
}

// DLL-wide (used for all instances of a plugin !!!) temporary memory resource
// for the main thread, only gets deallocated when the DLL is unloaded
utils::Pool *&getLocalScratch();
// settable memory source, used when an external library wants to call malloc
// can be used if malloc/realloc/free are replaced with arena_(malloc/free/realloc)
// remember to always replace the pool pointer when you want to use it (undefined value)
utils::Pool *&getSettableMallocPool();

extern utils::Pool *globalPool;

namespace utils
{
  enum class MathOperations { Assign, Add, Multiply, FadeInAdd, FadeOutAdd, Interpolate };

  forceinline float
  amplitudeToDb(float amplitude)
  { return 20.0f * log10f(amplitude); }

  forceinline float
  dbToAmplitude(float decibels)
  { return powf(10.0f, decibels / 20.0f); }

  forceinline float
  normalisedToDb(float normalised, float maxDb)
  { return powf(maxDb + 1.0f, normalised) - 1.0f; }

  forceinline float
  dbToNormalised(float db, float maxDb)
  { return log2f(db + 1.0f) / log2f(maxDb + 1.0f); }

  forceinline float
  normalisedToFrequency(float normalised, float sampleRate)
  { return powf(sampleRate * 0.5f / kMinFrequency, normalised) * kMinFrequency; }

  forceinline float
  frequencyToNormalised(float frequency, float sampleRate)
  { return log2f(frequency / kMinFrequency) / log2f(sampleRate * 0.5f / kMinFrequency); }

  // returns the proper bin which may also be nyquist, which is outside a power-of-2
  forceinline float
  normalisedToBinUnsafe(float normalised, u32 FFTSize, float sampleRate)
  { return roundf(normalisedToFrequency(normalised, sampleRate) / sampleRate * (float)FFTSize); }

  // always returns a bin < FFTSize, therefore cannot return nyquist
  forceinline float
  normalisedToBinSafe(float normalised, u32 FFTSize, float sampleRate)
  { return min(normalisedToBinUnsafe(normalised, FFTSize, sampleRate), (float)FFTSize / 2.0f - 1.0f); }

  forceinline float
  binToNormalised(float bin, u32 FFTSize, float sampleRate)
  {
    // at 0 logarithm doesn't produce valid values
    if (bin == 0.0f)
      return 0.0f;
    return frequencyToNormalised(bin * sampleRate / (float)FFTSize, sampleRate);
  }

  forceinline float
  centsToRatio(float cents)
  { return powf(2.0f, cents / (float)kCentsPerOctave); }

  forceinline float
  midiCentsToFrequency(float cents)
  { return kMidi0Frequency * centsToRatio(cents); }

  forceinline float
  midiNoteToFrequency(float note)
  { return midiCentsToFrequency(note * kCentsPerNote); }

  forceinline float
  frequencyToMidiNote(float frequency)
  { return (float)kNotesPerOctave * logf(frequency / kMidi0Frequency) * kExpConversionMult; }

  forceinline float
  frequencyToMidiCents(float frequency)
  { return kCentsPerNote * frequencyToMidiNote(frequency); }

  forceinline float
  nextPowerOfTwo(float value)
  { return roundf(powf(2.0f, ceilf(logf(value) * kExpConversionMult))); }

  forceinline i32
  prbs32(i32 x)
  {
    // maximal length, taps 32 31 29 1, from wikipedia
    return (i32)(((u32)x >> 1U) ^ (-(x & 1) & 0xd0000001U));
  }

  template<typename Struct>
  constexpr Struct *
  createFAM(const auto &allocate, usize size, bool initialiseToZero = false)
  {
    using T = utils::remove_cvref_t<decltype(Struct::data)>;

    usize extra = utils::roundUpToMultiple(sizeof(Struct), alignof(T));
    usize totalSize = extra + size * sizeof(T);

    byte *memory = (byte *)allocate(totalSize, utils::max(alignof(Struct), alignof(T)));
    auto *object = new(memory) Struct{ .data = new(memory + extra) T[size] };

    if (initialiseToZero)
      zeroset(object->data, size);

    return object;
  }

  class ScopedNoDenormals
  {
    u64 flags_;
  public:
    ScopedNoDenormals();
    ~ScopedNoDenormals();
  };

  class Dylib
  {
  public:
    const char *path{};
    void *handle;

    Dylib(const char *fullPath);
    ~Dylib();

    void *getSymbol(utils::string_view decoratedName) const;
  };

  enum class AllocatorType : u8 { General, Pool };

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

  // a pool of memory for unifying lifetimes
  struct Pool
  {
    static constexpr auto type = AllocatorType::Pool;

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
        // next is shift to next free node relative to the pool
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
    u32 lastUsedNode;

    Pool *nextArena{};

    static usize getUsedSize(Pool *pool);
    static usize getUnusedSize(Pool *pool);
    static Pool *
    fromAllocation(const void *data)
    {
      auto *node = utils::launder((Pool::node *)((byte *)data - sizeof(Pool::node)));
      return utils::launder((Pool *)((byte *)node - node->offsetToArena));
    }
    static usize
    getAllocationSize(const void *data)
    {
      auto *node = utils::launder((Pool::node *)((byte *)data - sizeof(Pool::node)));
      return node->size - sizeof(Pool::node);
    }

    [[nodiscard]] static byte *insert(Pool *pool, usize size, usize alignment, bool clean = false);
    [[nodiscard]] static byte *
    insert(const void *data, usize size, usize alignment, bool clean = false)
    {
      return insert(fromAllocation(data), size, alignment, clean);
    }
    static void remove(const void *data);
    [[nodiscard]] static byte *resize(const void *data, usize newSize, bool cleanNewSpace = false);
    static void shrinkToFit(Pool *pool, bool shrinkToCommitted);

    [[nodiscard]] static Pool *
    create(usize reservedSize = COMPLEX_MB(64), usize commitSize = COMPLEX_KB(64))
    {
      reservedSize = utils::clamp(reservedSize, sizeof(Pool), usize(u32(-1)) + 1);
      commitSize = utils::clamp(commitSize, sizeof(Pool), reservedSize);

      byte *memory = reserveMemory(reservedSize);
      commitMemory(memory, commitSize);

      (void)new(memory + sizeof(Pool)) node{ .size = (u32)(commitSize - sizeof(Pool)) };

      return new(memory) Pool{ .reservedSize = (u32)(reservedSize - 1),
        .committedSize = (u32)(commitSize - 1), .freeNodeStart = (u32)sizeof(Pool),
        .lastUsedNode = (u32)sizeof(Pool) };
    }
    template<typename T>
    [[nodiscard]] static Pool *
    createNested(T *differentArena, usize allocateSize) requires requires { T::insert(differentArena, usize(0), usize(0)); }
    {
      allocateSize = utils::min(allocateSize + sizeof(Pool), usize(u32(-1)) + 1);

      byte *memory = T::insert(differentArena, allocateSize, alignof(Pool));
      (void)new(memory + sizeof(Pool)) node{ .size = (u32)(allocateSize - sizeof(Pool)) };

      u8 flags = (u8)T::type;

      // Pool is nested in an pool
      return new(memory) Pool{ .flags = flags, .reservedSize = (u32)(allocateSize - 1),
        .committedSize = (u32)(allocateSize - 1), .freeNodeStart = (u32)sizeof(Pool),
        .lastUsedNode = (u32)sizeof(Pool) };
    }

    static void clear(Pool *pool);

    static void destroy(Pool *pool);
  };

  // anew, the replacement to operator new (with allocators)
  // using placement new on memory resource, provided by an pool/arena implementation
  //
  // the macros have an evil use of decltype which *should* be well-defined
  // the reason for this, is because allocator gets pasted twice so expressions will also be evaluated twice

  #define anew(allocator, T, ...) (new(decltype(&(*allocator)){}->insert(allocator, sizealignof(T))) T __VA_ARGS__)
  #define arranew(allocator, T, count, ...) (new(decltype(&(*allocator)){}->insert(allocator, sizealignof(T, count))) T[count] __VA_ARGS__)

  struct Allocator
  {
    struct AllocatorVtable
    {
      byte *(*insert)(void *allocator, usize size, usize alignment, bool clean);
      void (*remove)(const void *allocation);
      Allocator (*fromAllocation)(const void *allocation);
      AllocatorType type;
    };

    SourceLocation location{};
    const AllocatorVtable *vtable{};
    void *allocator{};
    bool freeingDestructor = true;

    constexpr Allocator() = default;
    constexpr Allocator(AllocatorConcept auto *allocator, bool freeingDestructor = true,
      utils::SourceLocation location = utils::SourceLocation::current()) : location{ location },
      vtable{ fromType(allocator->type).vtable }, allocator{ allocator }, freeingDestructor{ freeingDestructor } { }
    constexpr Allocator(const AllocatorVtable *vtable, void *allocator, bool freeingDestructor = true,
      utils::SourceLocation location = utils::SourceLocation::current()) : location{ location },
      vtable{ vtable }, allocator{ allocator }, freeingDestructor{ freeingDestructor } { }

    [[nodiscard]] byte *
    insert(usize size, usize alignment, bool clean = true) const
    { return vtable->insert(allocator, size, alignment, clean); }
    void remove(const void *allocation) const { if (allocation) vtable->remove(allocation); }
    Allocator fromAllocation(const void *allocation) const { return vtable->fromAllocation(allocation); }
    AllocatorType type() const { return vtable->type; }

    static constexpr Allocator fromType(AllocatorType type);

    template<AllocatorConcept T>
    static constexpr AllocatorVtable
    createVtable()
    {
      return AllocatorVtable
      {
        .insert = [](void *allocator, usize size, usize alignment, bool clean)
        { return T::insert((T *)allocator, size, alignment, clean); },
        .remove = T::remove,
        .fromAllocation = +[](const void *allocation)
        {
          auto allocator = fromType(T::type);
          allocator.allocator = T::fromAllocation(allocation);
          return allocator;
        },
        .type = T::type
      };
    }

    explicit operator bool() const { return vtable; }
  };

  inline constexpr Allocator::AllocatorVtable allocatorTable[] =
  {
    Allocator::AllocatorVtable
    {
      .insert = [](void *, usize size, usize alignment, bool clean)
      {
        return utils::Pool::insert(globalPool, size, alignment, clean);
      },
      .remove = [](const void *allocation) { utils::Pool::remove(allocation); },
      .fromAllocation = +[](const void *) { return Allocator::fromType(AllocatorType::General); },
      .type = AllocatorType::General
    },
    Allocator::createVtable<utils::Pool>()
  };
  constexpr Allocator Allocator::fromType(AllocatorType type) { return { &allocatorTable[(usize)type], nullptr }; }


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
      memmove(destination, source, size * sizeof(T));
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
          valcpy(data, existingData, existingSize);
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
      if (dataSize)
        valcpy(existingData, dataToClone, dataSize);
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
      // have we cached the pool we want to allocate in?
      if (!capacity_ && data_)
      {
        oldAllocator = Allocator::fromType((AllocatorType)allocatorType_);
        oldAllocator.allocator = data_;
        data_ = nullptr;
      }
      else
        oldAllocator = Allocator::fromType((AllocatorType)allocatorType_).fromAllocation(data_);
      oldAllocator.freeingDestructor = freeingDestructor_;

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
    insert(usize index)
    {
      COMPLEX_HARD_ASSERT(index <= size());

      if (capacity_ == 0)
        reserve(kStartingCapacity);
      else if (size_ + 1 > capacity_)
        reserve(utils::max(2 * capacity_, size_ + 1));

      utils::contiguousMoveElements(data_ + index + 1, data_ + index, size_ - index);

      ++size_;
      return data_ + index;
    }
    void *insertAt(iterator iter) { return insert((usize)(iter - begin())); }

    T &emplace(usize index, auto &&... args) { return *new(insert(index)) T{ COMPLEX_FWD(args)... }; }
    T &emplaceAt(iterator iter, auto &&... args) { return *new(insertAt(iter)) T{ COMPLEX_FWD(args)... }; }

    constexpr T
    popFront()
    {
      if (!size_)
        return {};

      T ret = COMPLEX_MOVE(front());
      front().~T();
      --size_;
      utils::contiguousMoveElements(data_, data_ + 1, size_);

      return ret;
    }
    constexpr T
    popBack()
    {
      if (!size_)
        return {};

      T ret = COMPLEX_MOVE(back());
      back().~T();
      --size_;

      return ret;
    }

    void erase(usize index, usize count)
    {
      COMPLEX_HARD_ASSERT(index < size());

      size_ -= count;
      for (usize i = 0; i < count; ++i)
        data_[index + i].~T();

      utils::contiguousMoveElements(data_ + index, data_ + index + count, size_ - index);
    }
    void eraseAt(iterator iter, usize count) { erase((usize)(iter - begin()), count); }
    void erase(usize index) { erase(index, 1); }
    void eraseAt(iterator iter) { eraseAt(iter, 1); }

    void erase(const T &element) { eraseIf([&element](const auto &data) { return element == data; }); }
    void eraseIf(const auto &predicate)
    {
      for (usize i = size_; i > 0; --i)
      {
        if (!predicate(data_[i - 1]))
          continue;

        data_[i - 1].~T();

        if (size_ - i > 0)
          utils::contiguousMoveElements(data_ + i - 1, data_ + i, size_ - i);

        --size_;
      }
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

  template<typename Key, typename Value, template<typename> class Container = vectornd>
  class vectormapnd
  {
  public:
    Container<utils::pair<Key, Value>> data{};

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
        data.eraseAt(iter);
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
      auto [iter, wasFound] = binary_search(data, element,
        [](const auto &element, const auto &test) { return element == test; });
      return (wasFound) ? iter : data.end();
    }
    constexpr auto binary_search(const utils::pair<Key, Value> &element)
    {
      auto [iter, wasFound] = binary_search(data, element,
        [](const auto &element, const auto &test) { return element == test; });
      return (wasFound) ? iter : data.end();
    }

    constexpr auto get_first_of(const Key &key)
    {
      auto [iter, wasFound] = binary_search(data, key,
        [](const auto &element, const auto &test) { return element == test.first; });
      if (wasFound)
      {
        for (auto nextIter = iter - 1;
          nextIter != data.end() && nextIter->first == key;
          (iter = nextIter), (--nextIter)) { }

        if (iter->first == key)
          return iter;
      }
      return data.end();
    }

    constexpr auto get_last_of(const Key &key)
    {
      auto [iter, wasFound] = binary_search(data, key,
        [](const auto &element, const auto &test) { return element == test.first; });
      if (!wasFound)
        return data.end();

      for (auto nextIter = iter + 1;
        nextIter != data.end() && nextIter->first == key;
        (iter = nextIter), (++nextIter)) { }

      return iter;
    }

    // this will work correctly only if you ever use the ordered functions
    // returns iterator of the added element
    constexpr auto add_ordered(Key key, Value value)
    {
      // find AN occurance of specified key
      auto [iter, _] = binary_search(data, key,
        [](const auto &element, const auto &test) { return element == test.first; });
      while (iter != data.end() && iter->first == key)
        ++iter;

      auto index = iter - data.begin();
      data.emplace(index, COMPLEX_MOVE(key), COMPLEX_MOVE(value));
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
        data.eraseAt(iter);
    }
  };

  template<typename Key, typename Value>
  using vectormap = vectormapnd<Key, Value, vector>;

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
      ret.size_ = (size_type)stbsp_snprintf(nullptr, 0, format, COMPLEX_FWD(args)...);
      ret.reserve(allocator, ret.size_);
      stbsp_snprintf(ret.data_, (int)ret.capacity_ + 1, format, COMPLEX_FWD(args)...);
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
      if (!capacity_ && !capacity && allocator)
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
      // have we cached the pool we want to allocate in?
      if (!capacity_ && data_)
      {
        oldAllocator = Allocator::fromType((AllocatorType)allocatorType_);
        oldAllocator.allocator = data_;
        data_ = nullptr;
      }
      else
        oldAllocator = Allocator::fromType((AllocatorType)allocatorType_).fromAllocation(data_);
      oldAllocator.freeingDestructor = freeingDestructor_;

      allocator = (allocator) ? allocator : oldAllocator;
      utils::contiguousReserve(allocator, oldAllocator, capacity, data_, size_, capacity_);
      allocatorType_ = (usize)allocator.type();
      freeingDestructor_ = allocator.freeingDestructor;

      --capacity_;
      zeroset(data_ + size_, capacity_ + 1 - size_);
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
      COMPLEX_ASSERT(size_ >= index + length, "Range is outside of string in string::remove()");
      utils::contiguousMoveElements(data_ + index, data_ + index + length, size_ - index - length);
      size_ -= length;
      data_[size_] = '\0';
      return *this;
    }
    stringnd &
    removeFirst(size_type length)
    {
      COMPLEX_ASSERT(size_ >= length, "Length out of range in string::removeFirst()");
      size_ -= length;
      utils::contiguousMoveElements(data_, data_ + length, size_);
      return *this;
    }
    stringnd &
    removeLast(size_type length)
    {
      COMPLEX_ASSERT(size_ >= length, "Length out of range in string::removeLast()");
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
      removeLast(size_ - i);

      for (i = 0; i < size_; ++i)
      {
        if (!(data_[i - 1] == ' ' || data_[i - 1] == '\t' ||
          (data_[i - 1] >= '\n' && data_[i - 1] <= '\r')))
          break;
      }
      removeFirst(i);

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

  // ghetto unique_ptr implementation
  template<typename T>
  class up
  {
    static constexpr bool is_array = is_array_v<T>;
    using V = remove_extent_t<T>;
  public:
    constexpr up() noexcept = default;
    constexpr up(nullptr_t) noexcept {}
    constexpr up(T *pointer) : object_{ pointer } { }
    constexpr up(const up &other) noexcept = delete;
    constexpr up(up &&other) noexcept
    {
      object_ = other.object_;
      other.object_ = nullptr;
    }
    // allows up- AND downcasting, check types before doing this
    template<typename U> requires (!is_array) && (derived_from<U, T> || derived_from<T, U>)
    constexpr up(up<U> &&other) noexcept
    {
      object_ = static_cast<V *>(other.object_);
      other.object_ = nullptr;
    }
    constexpr up &operator=(const up &other) noexcept = delete;
    constexpr up &operator=(up &&other) noexcept { up<T>{ COMPLEX_MOVE(other) }.swap(*this); return *this; }
    // allows up- AND downcasting, check types before doing this
    template<typename U> requires utils::derived_from<U, T> || utils::derived_from<T, U>
    constexpr up &operator=(up<U> &&other) noexcept { up<T>{ COMPLEX_MOVE(other) }.swap(*this); return *this; }
    constexpr ~up() noexcept
    {
      if constexpr (is_array)
        delete[] object_;
      else
        delete object_;

      object_ = nullptr;
    }

    static constexpr up<T> create(usize size) requires is_array
    {
      up unique;
      unique.object_ = new V[size]{};
      return unique;
    }
    static constexpr up<T> create(auto &&... args) requires (!is_array)
    {
      up unique;
      unique.object_ = new T{ COMPLEX_FWD(args)... };
      return unique;
    }

    constexpr void swap(up &other) noexcept
    {
      COMPLEX_SWAP_MEMBERS(object_, other);
    }

    constexpr void reset() noexcept { up{}.swap(*this); }
    template<typename U> requires (!is_array) && (derived_from<U, T> || derived_from<T, U>)
    constexpr void reset(U *pointer) { up<T>{ pointer }.swap(*this); }

    constexpr V *release() noexcept
    {
      auto *pointer = object_;
      object_ = nullptr;
      return pointer;
    }

    constexpr V *get() const noexcept { return object_; }
    constexpr V *operator->() const noexcept { return object_; }
    constexpr V &operator*() const noexcept { return *object_; }
    constexpr V &operator[](usize index) const noexcept requires is_array { return object_[index]; }

    explicit constexpr operator bool() const noexcept { return object_; }

  private:
    V *object_ = nullptr;

    template<typename U> friend class up;
  };

  template<typename T> up(T *pointer) -> up<T>;

  template<typename T, typename U>
  constexpr bool operator==(const up<T> &one, const up<U> &two) noexcept { return one.get() == two.get(); }
  template<typename T>
  constexpr bool operator==(const up<T> &one, nullptr_t) noexcept { return one.get() == nullptr; }
  template<typename T>
  constexpr bool operator==(nullptr_t, const up<T> &two) noexcept { return two.get() == nullptr; }

  // ghetto shared_ptr implementation
  template<typename T>
  class sp
  {
  protected:
    struct controlBlock
    {
      const AllocatorType type{};
      satomi::atomic<u32> refCount = 1;
      T *object = nullptr;
    };

  public:
    constexpr sp() = default;
    constexpr sp(nullptr_t) { }
    constexpr sp(const sp &other) : refCount_{ other.refCount_ }
    {
      if (!refCount_)
        return;

      refCount_->refCount.fetch_add(1, satomi::memory_order_relaxed);
    }
    constexpr sp(sp &&other) noexcept { swap(other); }
    sp &operator=(const sp &other)
    {
      if (this != &other)
      {
        this->~sp();

        refCount_ = other.refCount_;
        refCount_->refCount.fetch_add(1, satomi::memory_order_relaxed);
      }

      return *this;
    }
    sp &operator=(sp &&other)
    {
      if (this != &other)
      {
        this->~sp();

        swap(other);
      }

      return *this;
    }
    ~sp()
    {
      auto refCount = refCount_;
      refCount_ = nullptr;
      if (!refCount)
        return;

      auto value = refCount->refCount.fetch_sub(1, satomi::memory_order_acq_rel);
      if (value > 1)
        return;

      refCount->object->~T();
      auto type = refCount->type;
      refCount->~controlBlock();
      Allocator::fromType(type).remove(refCount);
    }

    static sp<T>
    create(Allocator allocator, auto &&... args)
    {
      constexpr auto alignment = alignof(T);
      constexpr auto headerSize = utils::roundUpToMultiple(sizeof(controlBlock), alignment);
      constexpr auto totalSize = headerSize + sizeof(T);

      byte *memory = allocator.insert(totalSize, alignment, false);

      sp shared{};
      shared.refCount_ = new(memory) controlBlock{ allocator.type() };
      shared.refCount_->object = new(memory + headerSize) T{ COMPLEX_FWD(args)... };

      return shared;
    }

    constexpr void swap(sp &other)
    {
      COMPLEX_SWAP_MEMBERS(refCount_, other);
    }

    void reset() { sp{}.swap(*this); }

    constexpr T *get() const { return (refCount_) ? refCount_->object : nullptr; }
    constexpr T *operator->() const { return get(); }
    constexpr T &operator*() const { return *get(); }

    constexpr usize
    useCount() const
    { return (!refCount_) ? 0 : refCount_->refCount.load(satomi::memory_order_relaxed); }

    explicit constexpr operator bool() const { return get() != nullptr; }

  protected:
    controlBlock *refCount_ = nullptr;
  };

  template<typename T, typename U>
  constexpr bool operator==(const sp<T> &one, const sp<U> &two) { return one.get() == two.get(); }
  template<typename T>
  constexpr bool operator==(const sp<T> &one, nullptr_t) { return one.get() == nullptr; }
  template<typename T>
  constexpr bool operator==(nullptr_t, const sp<T> &two) { return two.get() == nullptr; }

  // msvc uses signed u64s as array sizes so you can't just do usize(-1) and
  // clang for some reason doesn't like sizes above usize(-1) >> 3
  inline constexpr usize heapAllocatedTag = usize(-1) >> 3;

  // ghetto std::any implementation that can also be stack allocated
  template<usize MaxSize, usize Alignment = sizeof(void *)>
  class whatever
  {
    static constexpr bool isOnHeap = MaxSize == heapAllocatedTag;

    enum Op { OpClone, OpDestroy, OpMoveOut, OpTransfer, OpTypeId, OpSizeAlignment };

    union InOutParams
    {
      void *object{};
      struct
      {
        usize size{};
        usize alignment{};
      } sizeAlignment;
      typeInfo typeId;
      whatever *whateverPtr;
    };

    using Storage = conditional_t<isOnHeap, void *, byte[MaxSize]>;

    void (*manager_)(Op, const whatever *, InOutParams *){};
    alignas(isOnHeap ? alignof(Storage) : Alignment) Storage data_{};

    template<typename T>
    static void ManageStorage(Op op, const whatever *instance, InOutParams *inOutParams)
    {
      // The contained object is in the pointer
      auto objectPtr = utils::launder((T *)instance->data_);
      switch (op)
      {
      case OpClone:
        if constexpr (isOnHeap)
        {
          inOutParams->whateverPtr->data_ = new(utils::allocate(sizealignof(T))) T{ *objectPtr };
          inOutParams->whateverPtr->manager_ = instance->manager_;
        }
        else
        {
          new(inOutParams->whateverPtr->data_) T{ *objectPtr };
          inOutParams->whateverPtr->manager_ = instance->manager_;
        }
        break;
      case OpDestroy:
        objectPtr->~T();
        break;
      case OpMoveOut:
        new(inOutParams->object) T{ COMPLEX_MOVE(*objectPtr) };
        objectPtr->~T();
        break;
      case OpTransfer:
        new(inOutParams->whateverPtr->data_) T{ COMPLEX_MOVE(*objectPtr) };
        objectPtr->~T();
        inOutParams->whateverPtr->manager_ = instance->manager_;
        const_cast<whatever *>(instance)->manager_ = {};
        break;
      case OpTypeId:
        inOutParams->typeId = typeId(T);
        break;
      case OpSizeAlignment:
        inOutParams->sizeAlignment = { .size = sizeof(T), .alignment = alignof(T) };
        break;
      }
    }

    void allocate(decltype(manager_) newManager)
    {
      InOutParams newParams;
      newManager(OpSizeAlignment, nullptr, &newParams);

      if constexpr (isOnHeap)
      {
        InOutParams params;

        if (manager_ && (manager_(OpSizeAlignment, nullptr, &params),
          (params.sizeAlignment.alignment >= newParams.sizeAlignment.alignment &&
           params.sizeAlignment.size >= newParams.sizeAlignment.size)))
        {
          COMPLEX_ASSERT(data_);
          // we already have enough memory, no allocation needed
          // however we might be losing track of the current amount of memory available
          // but we'd need to store this information dynamically otherwise ¯\_(ツ)_/¯
          manager_(OpDestroy, this, nullptr);
        }
        else
        {
          // we have (no) memory and it's not enough
          if (data_)
          {
            COMPLEX_ASSERT(manager_);
            utils::deallocate(data_);
          }

          data_ = utils::allocate(newParams.sizeAlignment.size, newParams.sizeAlignment.alignment);
        }
      }
      else
      {
        COMPLEX_ASSERT(Alignment >= newParams.sizeAlignment.alignment &&
          MaxSize >= newParams.sizeAlignment.size);
      }

      manager_ = newManager;
    }

  public:
    constexpr ~whatever()
    {
      if (manager_)
      {
        COMPLEX_ASSERT(data_);
        manager_(OpDestroy, this, nullptr);
        if constexpr (isOnHeap)
          if (data_)
            utils::deallocate(data_);
        manager_ = {};
      }
    }

    constexpr whatever() = default;
    whatever(const whatever &other)
    {
      if (other.hasValue())
      {
        InOutParams inOutParams;
        inOutParams.whateverPtr = this;
        other.manager_(OpClone, &other, &inOutParams);
      }
    }
    whatever(whatever &&other) noexcept
    {
      if (other.hasValue())
      {
        InOutParams inOutParams;
        inOutParams.whateverPtr = this;
        other.manager_(OpMoveOut, &other, &inOutParams);
      }
    }
    whatever &
    operator=(const whatever &other)
    {
      *this = whatever{ other };
      return *this;
    }
    whatever &
    operator=(whatever &&other) noexcept
    {
      if (this != &other)
      {
        this->~whatever();
        InOutParams inOutParams;
        inOutParams.whateverPtr = this;
        other.manager_(OpTransfer, &other, &inOutParams);
      }

      return *this;
    }

    template<typename T>
    whatever(T &&object)
    {
      using U = utils::remove_cvref_t<T>;
      static constexpr auto newManager = &ManageStorage<U>;

      allocate(newManager);
      (void)new(data_) T{ COMPLEX_FWD(object) };
    }
    template<typename T>
    static whatever
    create(auto &&... args)
    {
      whatever instance;
      instance.emplace<T>(COMPLEX_FWD(args)...);
      return instance;
    }
    template<typename T>
    T &
    emplace(auto &&... args)
    {
      static constexpr auto newManager = &ManageStorage<T>;
      allocate(newManager);
      auto *ret = new(data_) T{ COMPLEX_FWD(args)... };
      return *ret;
    }

    bool hasValue() const { return manager_ != nullptr; }

    typeInfo
    type() const
    {
      if (!hasValue())
        return typeId(void);
      InOutParams inOutParams;
      manager_(OpTypeId, this, &inOutParams);
      return inOutParams.typeId;
    }

    void swap(whatever &other)
    {
      if constexpr (isOnHeap)
      {
        COMPLEX_SWAP_MEMBERS(manager_, other);
        COMPLEX_SWAP_MEMBERS(data_, other);
      }
      else
      {
        if (hasValue() && other.hasValue())
        {
          if (this == &other)
            return;

          whatever temp;
          InOutParams inOutParams;

          inOutParams.whateverPtr = &temp;
          other.manager_(OpTransfer, &other, &inOutParams);
          inOutParams.whateverPtr = &other;

          manager_(OpTransfer, this, &inOutParams);
          inOutParams.whateverPtr = this;
          temp.manager_(OpTransfer, &temp, &inOutParams);
        }
        else if (hasValue() || other.hasValue())
        {
          whatever *empty = (hasValue()) ? &other : this;
          whatever *full = (hasValue()) ? this : &other;

          InOutParams inOutParams;
          inOutParams.whateverPtr = empty;
          full->manager_(OpTransfer, full, &inOutParams);
        }
      }
    }

    template<typename ... VisitorFunctions>
    usize
    visit(VisitorFunctions &&... visitors)
    {
      if (!hasValue())
        return usize(-1);

      usize index = 0;
      auto probe = [this, &index]<typename T>(T &&visitor)
      {
        using URef = typename detail::signature<T>::type;
        using UClean = utils::remove_cvref_t<URef>;
        static_assert(utils::is_lvalue_reference_v<URef>, "Parameter must an lvalue reference");

        if (manager_ == &ManageStorage<UClean>)
        {
          visitor(*utils::launder((T *)data_));
          return true;
        }
        ++index;
        return false;
      };

      bool isFound = (probe(COMPLEX_FWD(visitors)) || ...);
      if (!isFound)
        return usize(-1);
      return index;
    }

    template<typename ... Ts>
    bool
    isOneOf()
    {
      usize matches = 0;
      if (hasValue())
        matches = (usize(manager_ == &ManageStorage<Ts>) + ...);
      return matches > 0;
    }

    template<template<typename...> class VariantLike, typename ... Ts>
    usize
    tryExtract(VariantLike<Ts...> &variant)
    {
      if (!hasValue())
        return usize(-1);

      usize index = 0;
      auto probe = [&]<typename T>()
      {
        if (manager_ == &ManageStorage<utils::remove_cvref_t<T>>)
        {
          auto *object = utils::launder((T *)data_);
          variant = T{ COMPLEX_MOVE(*object) };
          object->~T();

          return true;
        }
        ++index;
        return false;
      };

      bool isFound = (probe.template operator()<Ts>() || ...);
      if (!isFound)
        return usize(-1);
      return index;
    }

    template<typename T>
    bool
    tryExtract(T &variable)
    {
      if (manager_ == &ManageStorage<utils::remove_cvref_t<T>>)
      {
        auto *object = utils::launder((T *)data_);
        variable = T{ COMPLEX_MOVE(*object) };
        object->~T();

        return true;
      }

      return false;
    }

    template<typename T>
    T *
    tryGet()
    {
      if (manager_ == &ManageStorage<utils::remove_cvref_t<T>>)
        return utils::launder((T *)data_);

      return nullptr;
    }

    template<typename T>
    const T *tryGet() const { return const_cast<whatever *>(this)->tryGet<T>(); }
  };


  namespace detail
  {
    template <typename R, typename... Args>
    struct vtable_t
    {
      using Copier = void (*)(void *, const void *);
      using Destroyer = void (*)(void *);
      using Invoker = R(*)(const void *, Args &&...);
      using Mover = void (*)(void *, void *);

      template<typename FnT>
      static constexpr vtable_t
      create()
      {
        return vtable_t{ sizeof(FnT), alignof(FnT), &copy<FnT>, &destroy<FnT>, &invoke<FnT>, &move<FnT> };
      }

      template<typename FnT>
      static constexpr void copy(void *dest, const void *src)
      {
        const auto &object{ *static_cast<const FnT *>(src) };
        (void)new (dest) FnT{ object };
      }

      template<typename FnT>
      static constexpr void destroy(void *src)
      {
        auto &object{ *static_cast<FnT *>(src) };
        object.~FnT();
      }

      template<typename FnT>
      static constexpr void move(void *dest, void *src)
      {
        auto &object{ *static_cast<FnT *>(src) };
        (void)new (dest) FnT{ COMPLEX_MOVE(object) };
      }

      template<typename FnT>
      static constexpr R
      invoke(const void *data, Args &&... args)
      {
        const auto &object{ *static_cast<const FnT *>(data) };
        return object(COMPLEX_FWD(args)...);
      }

      static constexpr R
      emptyInvoke(const void *, Args &&...)
      {
        // bad function call
        COMPLEX_TRAP();
      }

      // you can't just assign lambdas to the fn ptrs because micro soft can't fix their compiler
      static constexpr void emptyCopy([[maybe_unused]] void *, [[maybe_unused]] const void *) { }
      static constexpr void emptyDestroy([[maybe_unused]] void *) { }
      static constexpr void emptyMove([[maybe_unused]] void *, [[maybe_unused]] void *) { }

      usize size = 0;
      usize alignment = 0;
      Copier copier = &emptyCopy;
      Destroyer destroyer = &emptyDestroy;
      Invoker invoker = &emptyInvoke;
      Mover mover = &emptyMove;
    };

    // Empty vtable which is used when fn is empty
    template<typename R, typename ... Args>
    inline constexpr vtable_t<R, Args...> emptyVtable{};
  }


  // ghetto std::function implementation that can also be stack-allocated
  // and go back and forth between the heap and stack if needed
  // based on https://github.com/colugomusic/clog/blob/master/include/clog/small_function.hpp
  template<typename Signature, usize MaxSize = 16, usize Alignment = 8>
  class fn;

  template<typename R, typename ... Args, usize MaxSize, usize Alignment>
  class fn<R(Args...), MaxSize, Alignment>
  {
  public:
    using fnTag = void;
    static constexpr bool isOnHeap = MaxSize == heapAllocatedTag;

    constexpr ~fn()
    {
      if (vtable_ == &detail::emptyVtable<R, Args...>)
        return;

      vtable_->destroyer(data_);
      if constexpr (isOnHeap)
        if (data_)
          utils::deallocate(data_);
      vtable_ = &detail::emptyVtable<R, Args...>;
    }

    constexpr fn() = default;
    constexpr fn(nullptr_t) { }

    template<usize Size>
    fn(const fn<R(Args...), Size> &other)
    {
      allocate(other.vtable_);
      vtable_->copier(data_, other.data_);
    }

    template<usize Size>
    fn(fn<R(Args...), Size> &&other) noexcept
    {
      if constexpr (isOnHeap && fn<R(Args...), Size>::isOnHeap)
      {
        // optimisation: both on heap, exchange members
        swap(other);
      }
      else
      {
        allocate(other.vtable_);
        vtable_->mover(data_, other.data_);
        other.~fn<R(Args...), Size>();
      }
    }

    template<typename T, typename Callable = remove_cvref_t<T>>
      requires is_invocable_r_v<R, Callable &, Args...> &&
      (!requires{ typename Callable::fnTag; }) // necessary to avoid fn & matches
    fn(T &&lambda) { fromCallable(COMPLEX_FWD(lambda)); }



    template<usize Size>
    fn &
    operator=(const fn<R(Args...), Size> &other)
    {
      if (vtable_ && data_)
        vtable_->destroyer(data_);

      allocate(other.vtable_);
      vtable_->copier(data_, other.data_);

      return *this;
    }

    template<usize Size>
    fn &
    operator=(fn<R(Args...), Size> &&other) noexcept
    {
      if constexpr (isOnHeap && fn<R(Args...), Size>::isOnHeap)
      {
        // optimisation: both on heap, exchange pointers
        this->~fn();

        vtable_ = other.vtable_;
        data_ = other.data_;
        other.data_ = nullptr;
        other.vtable_ = &detail::emptyVtable<R, Args...>;
      }
      else
      {
        if (vtable_ && data_)
          vtable_->destroyer(data_);
        allocate(other.vtable_);
        vtable_->mover(data_, other.data_);

        other.~fn<R(Args...), Size>();
      }

      return *this;
    }

    constexpr fn &operator=(nullptr_t) { this->~fn(); return *this; }

    template<typename T, typename Callable = remove_cvref_t<T>>
      requires is_invocable_r_v<R, Callable &, Args...> &&
      (!requires{ typename Callable::fnTag; }) // necessary to avoid fn & matches
    fn &
    operator=(T &&lambda)
    {
      fromCallable(COMPLEX_FWD(lambda));
      return *this;
    }

    fn<R(Args...), heapAllocatedTag> toDynFn() const & { return *this; }
    fn<R(Args...), heapAllocatedTag> toDynFn() & { return *this; }
    fn<R(Args...), heapAllocatedTag> toDynFn() const && { return COMPLEX_MOVE(*this); }
    fn<R(Args...), heapAllocatedTag> toDynFn() && { return COMPLEX_MOVE(*this); }

    template<usize Size> fn<R(Args...), Size> toSmallFn() const & { return *this; }
    template<usize Size> fn<R(Args...), Size> toSmallFn() & { return *this; }
    template<usize Size> fn<R(Args...), Size> toSmallFn() const && { return COMPLEX_MOVE(*this); }
    template<usize Size> fn<R(Args...), Size> toSmallFn() && { return COMPLEX_MOVE(*this); }

    explicit constexpr operator bool() const { return vtable_ != &detail::emptyVtable<R, Args...>; }

    constexpr R operator()(Args ... args) const { return vtable_->invoker(data_, COMPLEX_FWD(args)...); }

    void swap(fn &other) requires isOnHeap
    {
      COMPLEX_SWAP_MEMBERS(vtable_, other);
      COMPLEX_SWAP_MEMBERS(data_, other);
    }

  private:
    using vtable_t = detail::vtable_t<R, Args...>;

    static_assert(((MaxSize - sizeof(vtable_t)) % Alignment == 0) || isOnHeap);

    using storage_t = conditional_t<isOnHeap, void *, byte[MaxSize]>;

    void allocate(const vtable_t *newVtable)
    {
      if constexpr (isOnHeap)
      {
        if (vtable_ && vtable_->alignment >= newVtable->alignment && vtable_->size >= newVtable->size)
        {
          // we already have enough memory, no allocation needed
          // however we might be losing track of the current amount of memory available
          // but we'd need to store this information dynamically otherwise ¯\_(ツ)_/¯
          vtable_->destroyer(data_);
        }
        else
        {
          // we have (no) memory and it's not enough
          if (data_)
          {
            COMPLEX_ASSERT(vtable_);
            utils::deallocate(data_);
          }

          data_ = utils::allocate(newVtable->size, newVtable->alignment);
        }
      }
      else
      {
        COMPLEX_ASSERT(Alignment >= newVtable->alignment && MaxSize >= newVtable->size);
      }

      vtable_ = newVtable;
    }

    template<typename FnT>
    void fromCallable(FnT &&lambda)
    {
      using fn_t = remove_cvref_t<FnT>;

      static_assert(is_copy_constructible_v<fn_t>,
        "fn cannot be constructed from a non-copyable type");

      static_assert(sizeof(fn_t) <= MaxSize || isOnHeap,
        "fn is too small to fit this object");

      static_assert((Alignment % alignof(fn_t) == 0) || isOnHeap,
        "fn cannot be constructed from an object of this alignment");

      static_assert(Alignment >= alignof(fn_t) || isOnHeap,
        "fn does not support alignment higher the one defined in the class");

      static constexpr auto vtable = vtable_t::template create<fn_t>();

      allocate(&vtable);

      (void)new (data_) fn_t{ COMPLEX_FWD(lambda) };

      vtable_ = &vtable;
    }

    alignas(isOnHeap ? alignof(storage_t) : Alignment) storage_t data_{};
    const vtable_t *vtable_ = &detail::emptyVtable<R, Args...>;

    template<typename, usize, usize>
    friend class fn;
  };

  template<typename Signature>
  using dynFn = fn<Signature, heapAllocatedTag>;

  template<typename Signature, usize MaxSize = 16, usize Alignment = 8>
  using smallFn = fn<Signature, MaxSize, Alignment>;

  // to be called at DLL/Process load time
  void atLoad();
  // to be called at DLL/Process unload time
  void atUnload();
  // will be called at the entry point of every thread
  // or (for the initial main thread) at DLL/Process load time
  //
  // needs to be implemented by the user and allocate a structure
  // that will hold all the thread-local data that's going to be used
  //
  // can return nullptr, in which case no thread-local storage will be used
  // can be replaced with setTls below at any point
  void *createTlsContext();
  void destroyTlsContext(void *context);
  void *getTls();
  void setTls(void *context);

  class thread
  {
    void createThread(utils::dynFn<int()> *procedure);
  public:
    struct id
    {
    #ifdef COMPLEX_WINDOWS
      /*DWORD*/ unsigned int nativeId{};
    #else
      /*pthread_t*/ unsigned long nativeId{};
    #endif

      bool operator==(const id &other) const;
      explicit operator bool() { return nativeId; }
    } threadId{};

    // the necessity for this function instead of just getting the value directly
    // is because the thread might have been started
    static id getCurrentId();
    [[noreturn]] static void exit(int result);

    ~thread();

    thread(auto &&function)
    {
      if constexpr (requires{ requires utils::is_same_v<decltype(function()), void>; })
      {
        createThread(new(utils::allocate(sizealignof(utils::dynFn<int()>)))
          utils::dynFn<int()>{ [fn = COMPLEX_MOVE(function)]() { fn(); return 0; } });
      }
      else
      {
        createThread(new(utils::allocate(sizealignof(utils::dynFn<int()>)))
          utils::dynFn<int()>{ [fn = COMPLEX_MOVE(function)]() { return (int)fn(); } });
      }
    }
    thread() = default;
    thread(const thread &) = delete;
    thread(thread &&other) noexcept
    {
      COMPLEX_SWAP_MEMBERS(threadId, other);
    }
    thread &operator=(const thread &other) = delete;
    thread &operator=(thread &&other) noexcept
    {
      if (&other != this)
      {
        this->~thread();
        COMPLEX_SWAP_MEMBERS(threadId, other);
      }

      return *this;
    }

    bool join(int *exitCode = nullptr);
    bool detach();
    void yield();

    bool operator==(const thread &other) const = default;
  };

  // intended relative comparisons in time
  u64 getTimestamp();
  void millisleep() noexcept;

  void setHighResolutionClock(bool isHighResolution);

  /// this timer is intended to be used inside a running loop
  /// for accurate dispatch at regular intervals
  /// sleep of <1ms is not guaranteed
  class Timer
  {
    static constexpr double kMicro = 1'000'000.0;

    /// static state
    //
    i32 sleepUs_ = 0;
    u64 correctionPeriod_; // corrects every second by default

    /// runtime state
    //
    i32 correctionUs_ = 500; // start at some reasonable correction
    u32 counter_ = 0;
    u64 correctionStart_ = 0;

  public:
    Timer();
    void setPeriod(double microseconds)
    {
      sleepUs_ = (i32)microseconds;
      correctionStart_ = 0;
      correctionUs_ = 500;
      counter_ = 0;
    }
    void setFrequency(double hz) { setPeriod(kMicro / hz); }

    /// how often to check correct sleep time to more closely match desired loop period
    /// can also be set to 0 and manually call correctSleep()
    /// @see Timer::correctSleep()
    void setCorrectionPeriod(double seconds);

    void sleep(bool correctAutomatically = true);

    /// corrects sleep time to more closely match desired loop period
    /// can be used without the provided automatic correction
    /// so long as setCorrectionPeriod(0.0)
    /// this is intended when user wants to meet a certain threshold of accuracy
    /// and then stop looking for a better sleep correction offset to minimise jitter
    void correctSleep();
  };

  // Spin - spinlock (use on realtime threads)
  // Wait - waits for signal from locking site (use for unknown sleep length)
  // Sleep - millisleep (use if you expect something reasonably soon)
  //
  // all notify variants signal waiting sites upon unlock (REALTIME-UNSAFE)
  enum class WaitMechanism : u32 { Spin = 0, Wait, Sleep, SpinNotify = 4, WaitNotify, SleepNotify };

  // git blame for deadlocks
  template<typename T>
  struct LockBlame
  {
    satomi::atomic<T> lock{};
    satomi::atomic<utils::thread::id> lastLockId{};
  };

  template<typename T>
  struct ReentrantLock : LockBlame<T> { };

  inline void lockAtomic(satomi::atomic<bool> &atomic, WaitMechanism mechanism, bool expected = false)
  {
    bool state = expected;
    while (!atomic.compare_exchange_weak(state, !expected, satomi::memory_order_acq_rel))
    {
      // protecting against spurious failures
      if (state == expected)
        continue;

      switch (mechanism)
      {
      case WaitMechanism::Spin:
      case WaitMechanism::SpinNotify:
        // taking advantage of the MESI protocol where we only want shared access to read until the value is changed,
        // instead of read/write with exclusive access thereby slowing down other threads trying to read as well
        while (atomic.load(satomi::memory_order_relaxed) != expected) { COMPLEX_PAUSE(); }
        break;
      case WaitMechanism::Wait:
      case WaitMechanism::WaitNotify:
        atomic.wait(state, satomi::memory_order_relaxed);
        break;
      case WaitMechanism::Sleep:
      case WaitMechanism::SleepNotify:
        // sleeping should take ideally around a milisecond, may change later
        millisleep();
        break;
      default:
        COMPLEX_ASSERT_FALSE("Invalid wait mechanism");
        break;
      }

      state = expected;
    }
  }
  inline void unlockAtomic(satomi::atomic<bool> &atomic, WaitMechanism mechanism, bool expected = false)
  {
    atomic.store(expected, satomi::memory_order_release);
    if ((u32)mechanism & (u32)WaitMechanism::SpinNotify)
      atomic.notify_all();
  }
  i32 lockAtomic(satomi::atomic<i32> &atomic, bool isExclusive, WaitMechanism mechanism) noexcept;
  inline i32
  lockAtomic(LockBlame<i32> &lock, bool isReentrant, bool isExclusive, WaitMechanism mechanism) noexcept
  {
    auto threadId = utils::thread::getCurrentId();
    if (lock.lock.load(satomi::memory_order_relaxed) < 0 &&
      lock.lastLockId.load(satomi::memory_order_relaxed) == threadId)
    {
      if (isReentrant)
        return -1;
      else
        COMPLEX_ASSERT_FALSE("Guess who forgot to unlock this atomic");
    }

    auto ret = lockAtomic(lock.lock, isExclusive, mechanism);
    if (isExclusive)
    {
      if (ret == 0)
        lock.lastLockId.store(threadId, satomi::memory_order_relaxed);
    }
    return ret;
  }
  void unlockAtomic(satomi::atomic<i32> &atomic, bool wasExclusive, WaitMechanism mechanism) noexcept;
  inline void unlockAtomic(LockBlame<i32> &lock, bool wasExclusive,
    WaitMechanism mechanism, i32 previousValue) noexcept
  {
    if (previousValue == -1)
      return;

    unlockAtomic(lock.lock, wasExclusive, mechanism);

    if (wasExclusive)
      lock.lastLockId.store({}, satomi::memory_order_relaxed);
  }

  class ScopedLock
  {
  public:
    ScopedLock() : type_{ Empty }, mechanism_{ WaitMechanism::Spin }, bool_{} { }

    ScopedLock(satomi::atomic<bool> &atomic, WaitMechanism mechanism, bool expected = false) noexcept :
      type_(BoolEnum), mechanism_(mechanism), bool_{ &atomic, expected }
    { lockAtomic(atomic, mechanism, expected); }

    ScopedLock(ReentrantLock<bool> &reentrantLock, WaitMechanism mechanism, bool expected = false) noexcept :
      type_(ReentrantBoolEnum), mechanism_(mechanism), reentrantBool_{ &reentrantLock, false, expected }
    {
      auto threadId = utils::thread::getCurrentId();
      reentrantBool_.wasLocked = threadId == reentrantLock.lastLockId.load(satomi::memory_order_relaxed);

      if (!reentrantBool_.wasLocked)
      {
        lockAtomic(reentrantLock.lock, mechanism, expected);
        reentrantLock.lastLockId.store(threadId, satomi::memory_order_relaxed);
      }
    }

    ScopedLock(satomi::atomic<i32> &atomic, bool isExclusive, WaitMechanism mechanism) noexcept :
      type_{ I32LockEnum }, mechanism_{ mechanism }, i32_{ &atomic, isExclusive }
    { lockAtomic(atomic, isExclusive, mechanism); }

    ScopedLock(ReentrantLock<i32> &reentrantLock, bool isExclusive, WaitMechanism mechanism) noexcept :
      type_{ I32LockEnum }, mechanism_{ mechanism }, i32Lock_{ &reentrantLock, isExclusive }
    { i32Lock_.previousValue = lockAtomic(reentrantLock, true, isExclusive, mechanism); }

    ScopedLock(LockBlame<i32> &lock, bool isExclusive, WaitMechanism mechanism) noexcept :
      type_{ I32LockEnum }, mechanism_{ mechanism }, i32Lock_{ &lock, isExclusive }
    { i32Lock_.previousValue = lockAtomic(lock, false, isExclusive, mechanism); }

    ScopedLock(ScopedLock &&other) noexcept = delete;
    ScopedLock &operator=(ScopedLock &&other) noexcept
    {
      valcpy(this, &other);
      other.type_ = Empty;
      return *this;
    }
    ScopedLock(const ScopedLock &) = delete;
    ScopedLock operator=(const ScopedLock &) = delete;

    ~ScopedLock() noexcept
    {
      if (type_ == Empty)
        return;

      if (type_ == BoolEnum)
        unlockAtomic(*bool_.atomic, mechanism_, bool_.expected);
      else if (type_ == I32Enum)
        unlockAtomic(*i32_.atomic, i32_.isExclusive, mechanism_);
      else if (type_ == I32LockEnum)
        unlockAtomic(*i32Lock_.atomic, i32Lock_.isExclusive, mechanism_, i32Lock_.previousValue);
      else if (type_ == ReentrantBoolEnum && !reentrantBool_.wasLocked)
      {
        unlockAtomic(reentrantBool_.atomic->lock, mechanism_, reentrantBool_.expected);
        reentrantBool_.atomic->lastLockId.store({}, satomi::memory_order_relaxed);
      }

      type_ = Empty;
    }

  private:
    enum { Empty, BoolEnum, I32Enum, I32LockEnum, ReentrantBoolEnum } type_;
    WaitMechanism mechanism_;
    struct BoolType { satomi::atomic<bool> *atomic; bool expected; };
    struct I32Type { satomi::atomic<i32> *atomic; bool isExclusive; };
    struct I32LockType { LockBlame<i32> *atomic; bool isExclusive; i32 previousValue; };
    struct ReentrantBoolType { ReentrantLock<bool> *atomic; bool wasLocked; bool expected; };
    union
    {
      BoolType bool_;
      I32Type i32_;
      I32LockType i32Lock_;
      ReentrantBoolType reentrantBool_;
    };
  };
}

// adapted from https://github.com/thegabman/native_message_box/blob/master/include/NMB/NMB.h
enum class MessageBoxType { Info, Warning, Error };
bool showNativeMessageBox(const char *title, const char *message, MessageBoxType type);

// https://github.com/colugomusic/snd/blob/master/include/snd/const_math.hpp
// MIT License
//
// Copyright(c) 2020 colugomusic
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
namespace const_math
{
  template<typename T>
  inline constexpr auto EPSILON = T(0.0001);

  template<typename T>
  constexpr T
  abs(T x)
  {
    if (x == T(0))
      return T(0);
    return (x < T(0)) ? -x : x;
  }

  template<typename T>
  constexpr T
  min()
  {
    if constexpr (utils::is_same_v<T, float>)
      return 1.175494351e-38F;
    else if constexpr (utils::is_same_v<T, double>)
      return 2.2250738585072014e-308;
    else
    {
      // we do not handle other types here
    }
  }

  template<typename T>
  constexpr T
  max()
  {
    if constexpr (utils::is_same_v<T, float>)
      return 3.402823466e+38F;
    else if constexpr (utils::is_same_v<T, double>)
      return 1.7976931348623158e+308;
    else
    {
      // we do not handle other types here
    }
  }

  template<typename T>
  constexpr T
  infinity()
  {
    if constexpr (utils::is_same_v<T, float>)
      return __builtin_huge_valf();
    else if constexpr (utils::is_same_v<T, double>)
      return __builtin_huge_val();
    else
    {
      // we do not handle other types here
    }
  }

  template<typename T>
  constexpr bool is_nan(const T x) { return x != x; }


  template<typename T>
  constexpr T
  quiet_nan()
  {
    if constexpr (utils::is_same_v<T, float>)
      return __builtin_nanf("0");
    else if constexpr (utils::is_same_v<T, double>)
      return __builtin_nan("0");
    else
    {
      // we do not handle other types here
    }
  }

  template<typename T>
  constexpr T
  sqrt(T x)
  {
    auto g = T(1);
    while (abs(g - x / g) >= EPSILON<T>)
      g = (g + x / g) / 2;

    return g;
  }

  template<typename T>
  constexpr T
  sin(T x)
  {
    x = (x < 0) ? -x + kPi : x;

    usize n = 0;
    while (x >= EPSILON<T>)
    {
      x /= 3;
      ++n;
    }

    for (usize i = 0; i < n; ++i)
      x = 3 * x - 4 * (x * x * x);

    return x;
  }

  template<typename T>
  constexpr T cos(T x) { return sin(kPi / 2 - x); }

  template<typename T>
  constexpr T
  pow(T base, int exponent)
  {
    bool isNegative = exponent < 0;
    if (isNegative)
      exponent = -exponent;

    T y = 1;
    for (int i = 0; i < exponent; ++i)
      y *= base;

    return (isNegative) ? T(1) / y : y;
  }

  template<typename T>
  constexpr T
  nearest(T x)
  {
    return (int)(x - T(0.5)) > (int)(x) ?
      (T)((int)(x + T(0.5))) : (T)((int)(x));
  }

  template<typename T>
  constexpr T
  fraction(T x)
  {
    return (int)(x - T(0.5)) > (int)(x) ?
      -(((T)(int)(x + T(0.5))) - x) :
      x - ((T)(int)(x));
  }

  template<typename T>
  constexpr T
  exp(T x)
  {
    T y = fraction(x);
    y = 1 + y + pow(y, 2) / 2 + pow(y, 3) / 6 + pow(y, 4) / 24 +
      pow(y, 5) / 120 + pow(y, 6) / 720 + pow(y, 7) / 5040;

    return pow((T)kExp, (int)nearest(x)) * y;
  }

  template <typename T>
  constexpr T
  log(T x)
  {
    if (x == 0)
      return -infinity<T>();
    if (x < 0)
      return quiet_nan<T>();

    int exponent = 0;
    while (true)
    {
      if (x >= 10)
      {
        x /= 10;
        ++exponent;
      }
      else if (x < 1)
      {
        x *= 10;
        --exponent;
      }
      else
        break;
    }

    x = sqrt(x);
    x = (x - 1) / (x + 1);
    x = T(2) * (x + pow(x, 3) / 3 + pow(x, 5) / 5 + pow(x, 7) / 7 + pow(x, 9) / 9 + pow(x, 11) / 11);

    return T(2) * x + T(2.3025851) * T(exponent);
  }

  template <typename T>
  constexpr T log2(T x) { return log(x) / log(T(2)); }

  template<typename T>
  constexpr T
  floor(T x)
  {
    if (is_nan(x))
      return quiet_nan<T>();
    // +/- infinite and signed-zero cases
    else if (abs(x) == infinity<T>() || min<T>() > abs(x))
      return x;
    else
    {
      auto floorInt = [](T x, T x_whole)
      {
        return x_whole - static_cast<T>((x < T(0)) && (x < x_whole));
      };

      if constexpr (utils::is_same_v<T, float>)
        return (abs(x) >= 8388608.f) ? x : floorInt(x, (float)(int)x);
      else if constexpr (utils::is_same_v<T, double>)
        return (abs(x) >= 4503599627370496.) ? x : floorInt(x, (double)(long long)x);
      else
      {
        // we do not handle other types here
      }
    }
  }

  /*
   * Compile-time tangent function
   *
   * @param x a real-valued input.
   * @return the tangent function using
   * \f[ \tan(x) = \dfrac{x}{1 - \dfrac{x^2}{3 - \dfrac{x^2}{5 - \ddots}}} \f]
   * To deal with a singularity at \f$ \pi / 2 \f$, the following expansion is employed:
   * \f[ \tan(x) = - \frac{1}{x-\pi/2} - \sum_{k=1}^\infty \frac{(-1)^k 2^{2k} B_{2k}}{(2k)!} (x - \pi/2)^{2k - 1} \f]
   * where \f$ B_n \f$ is the n-th Bernoulli number.
   */
  template<typename T>
  constexpr T
  tan(T x)
  {
    if (min<T>() > abs(x))
      return 0;

    bool isNegative = x < T(0);
    x = (isNegative) ? -x : x;

    // tan(x) = tan(x + pi)
    x -= T(kPi) * floor(x / T(kPi));
    if (x > T(kPi))
      return quiet_nan<T>();

    // main calculation
    T y{};

    // deals with a singularity at tan(pi/2)
    if (x > T(1.55) && x < T(1.60))
    {
      if (min<T>() > abs(x - T(kPi / 2)))
        y = T(1.633124e+16);
      else
      {
        x -= T(kPi / 2);
        // this is based on a fourth-order expansion of tan(z) using Bernoulli numbers
        y = -1 / x + (x / 3 + (pow(x, 3) / 45 + (2 * pow(x, 5) / 945 + pow(x, 7) / 4725)));
      }
    }
    else
    {
      auto tan_cf_recur = [](const auto &self, T x2, int depth, int max_depth) -> T
      {
        // continued fraction calculation
        // https://math.stackexchange.com/a/433857
        T z = T(2 * depth - 1);
        if (depth < max_depth)
          z -= x2 / self(self, x2, depth + 1, max_depth);

        return z;
      };

      if (x > T(1.4))
        y = x / tan_cf_recur(tan_cf_recur, x * x, 1, 45);
      else if (x > T(1))
        y = x / tan_cf_recur(tan_cf_recur, x * x, 1, 35);
      else
        y = x / tan_cf_recur(tan_cf_recur, x * x, 1, 25);
    }

    return (isNegative) ? -y : y;
  }
}
