
// Created: 2021-07-27 00:53:28

#pragma once

#include "satomi.hpp"
#include "stl_utils.hpp"
#include "constants.hpp"

namespace utils
{
  enum class MathOperations { Assign, Add, Multiply, FadeInAdd, FadeOutAdd, Interpolate };

  constexpr bool 
  closeToZero(double value) 
  { return value <= kEpsilon && value >= -kEpsilon; }

  constexpr bool 
  closeTo(double reference, double value) 
  { return closeToZero(reference - value); }

  template<bool UseAnd = false, typename T>
  constexpr T 
  circularDifference(T lower, T upper, T size)
  {
    if constexpr (UseAnd)
      return (size + upper - lower) & (size - 1);
    else
      return (size + upper - lower) % size;
  }

  strict_inline double 
  amplitudeToDb(double amplitude)
  { return 20.0 * ::log10(amplitude); }

  strict_inline double 
  dbToAmplitude(double decibels) 
  { return ::pow(10.0, decibels / 20.0); }

  strict_inline double 
  normalisedToDb(double normalised, double maxDb) 
  { return ::pow(maxDb + 1.0, normalised) - 1.0; }

  strict_inline double 
  dbToNormalised(double db, double maxDb) 
  { return ::log2(db + 1.0) / ::log2(maxDb + 1.0); }

  strict_inline double 
  normalisedToFrequency(double normalised, double sampleRate) 
  { return ::pow(sampleRate * 0.5 / kMinFrequency, normalised) * kMinFrequency; }

  strict_inline double 
  frequencyToNormalised(double frequency, double sampleRate) 
  { return ::log2(frequency / kMinFrequency) / ::log2(sampleRate * 0.5 / kMinFrequency); }

  // returns the proper bin which may also be nyquist, which is outside a power-of-2 
  strict_inline double 
  normalisedToBinUnsafe(double normalised, u32 FFTSize, double sampleRate) 
  { return ::round(normalisedToFrequency(normalised, sampleRate) / sampleRate * (double)FFTSize); }

  // always returns a bin < FFTSize, therefore cannot return nyquist
  strict_inline double 
  normalisedToBinSafe(double normalised, u32 FFTSize, double sampleRate) 
  { return min(normalisedToBinUnsafe(normalised, FFTSize, sampleRate), (double)FFTSize / 2.0 - 1.0); }

  strict_inline double 
  binToNormalised(double bin, u32 FFTSize, double sampleRate) 
  {
    // at 0 logarithm doesn't produce valid values
    if (bin == 0.0)
      return 0.0;
    return frequencyToNormalised(bin * sampleRate / (double)FFTSize, sampleRate);
  }

  strict_inline double 
  centsToRatio(double cents) 
  { return ::pow(2.0, cents / (double)kCentsPerOctave); }

  strict_inline double 
  midiCentsToFrequency(double cents) 
  { return kMidi0Frequency * centsToRatio(cents); }

  strict_inline double 
  midiNoteToFrequency(double note) 
  { return midiCentsToFrequency(note * kCentsPerNote); }

  strict_inline double 
  frequencyToMidiNote(double frequency) 
  { return (double)kNotesPerOctave * ::log(frequency / kMidi0Frequency) * kExpConversionMult; }

  strict_inline double 
  frequencyToMidiCents(double frequency) 
  { return kCentsPerNote * frequencyToMidiNote(frequency); }

  strict_inline float 
  nextPowerOfTwo(float value) 
  { return ::roundf(::powf(2.0f, ::ceilf(::logf(value) * kExpConversionMult))); }

  strict_inline i32 
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
      ::zeroset(object->data, size);

    return object;
  }

  class ScopedNoDenormals
  {
  public:
    ScopedNoDenormals();
    ~ScopedNoDenormals();

  private:
    u64 flags_;
  };

  class Dylib
  {
  public:
    Dylib(const char *fullPath);
    ~Dylib();
    void *handle;
  };

  // ghetto shared_ptr implementation
  template<typename T>
  class sp
  {
  protected:
    struct controlBlock
    {
      satomi::atomic<u64> refCount = 1;
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
    constexpr sp(sp &&other) { swap(other); }
    constexpr sp &operator=(const sp &other)
    {
      if (this != &other)
      {
        this->~sp();

        refCount_ = other.refCount_;
        refCount_->refCount.fetch_add(1, satomi::memory_order_relaxed);
      }

      return *this;
    }
    constexpr sp &operator=(sp &&other)
    {
      if (this != &other)
      {
        this->~sp();

        swap(other);
      }

      return *this;
    }
    constexpr ~sp()
    {
      auto refCount = refCount_;
      refCount_ = nullptr;
      if (!refCount)
        return;

      auto value = refCount->refCount.fetch_sub(1, satomi::memory_order_acq_rel);
      if (value > 1)
        return;

      refCount->object->~T();
      refCount->~controlBlock();
      if (utils::is_constant_evaluated())
        operator delete[](refCount, utils::align_val_t(alignof(T)));
      else
        utils::deallocate(refCount);
    }

    static constexpr sp<T> 
    create(auto &&... args)
    {
      constexpr auto alignment = alignof(T);
      constexpr auto headerSize = utils::roundUpToMultiple(sizeof(controlBlock), alignment);
      constexpr auto totalSize = headerSize + sizeof(T);

      byte *memory;
      if (utils::is_constant_evaluated())
      {
        // can't use operator new[] the usual way because of a longstanding bug in msvc...
        // https://developercommunity.visualstudio.com/t/using-c17-new-stdalign-val-tn-syntax-results-in-er/528320
        memory = (byte *)operator new[](totalSize, utils::align_val_t(alignment));
      }
      else
        memory = (byte *)utils::allocate(totalSize, alignment);
      
      sp shared{};
      shared.refCount_ = new(memory) controlBlock{};
      shared.refCount_->object = new(memory + headerSize) T{ COMPLEX_FWD(args)... };

      return shared;
    }

    constexpr void swap(sp &other)
    {
      COMPLEX_SWAP_MEMBERS(refCount_, other);
    }

    constexpr void reset() { sp{}.swap(*this); }

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

  inline constexpr usize heapAllocatedTag = usize(-1) >> 1;

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
      !requires{ typename Callable::fnTag; } // necessary to avoid fn & matches
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
      !requires{ typename Callable::fnTag; } // necessary to avoid fn & matches
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

    inline static thread_local id currentId = {};
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
    thread(thread &&other) noexcept : thread{}
    {
      COMPLEX_SWAP_MEMBERS(threadId, other);
    #ifdef COMPLEX_WINDOWS
      COMPLEX_SWAP_MEMBERS(handle_, other);
    #endif
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
  private:
  #ifdef COMPLEX_WINDOWS
    /*HANDLE*/ void *handle_{};
  #endif
  };
}

namespace Interface
{
  // adapted from https://github.com/thegabman/native_message_box/blob/master/include/NMB/NMB.h
  enum class MessageBoxType { Info, Warning, Error };
  bool showNativeMessageBox(const char *title, const char *message, MessageBoxType type);
}

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
  template <typename T>
  constexpr auto EPSILON = T(0.001);

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

  template<typename T>
  constexpr bool is_nan(const T x) { return x != x; }


  template<typename T>
  constexpr T quiet_nan()
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
      auto floor_int = [](T x, T x_whole)
      {
        return x_whole - static_cast<T>((x < T(0)) && (x < x_whole));
      };

      if constexpr (utils::is_same_v<T, float>)
        return (abs(x) >= 8388608.f) ? x : floor_int(x, (float)(int)x);
      else if constexpr (utils::is_same_v<T, double>)
        return (abs(x) >= 4503599627370496.) ? x : floor_int(x, (double)(long long)x);
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
      auto tan_cf_recur = [](const auto &self, T xx, int depth, int max_depth) -> T
      {
        // continued fraction calculation
        // https://math.stackexchange.com/a/433857
        T z = T(2 * depth - 1);
        if (depth < max_depth)
          z -= xx / self(self, xx, depth + 1, max_depth);

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
