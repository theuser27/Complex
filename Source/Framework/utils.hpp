/*
  ==============================================================================

    utils.hpp
    Created: 27 Jul 2021 00:53:28
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "satomi.hpp"
#include "stl_utils.hpp"
#include "constants.hpp"

namespace utils
{
  enum class MathOperations { Assign, Add, Multiply, FadeInAdd, FadeOutAdd, Interpolate };

  constexpr strict_inline bool closeToZero(double value) noexcept
  { return value <= kEpsilon && value >= -kEpsilon; }

  constexpr strict_inline bool closeTo(double reference, double value) noexcept
  {	return closeToZero(reference - value); }

  template<bool UseAnd = false, typename T>
  constexpr strict_inline T circularDifference(T lower, T upper, T size) noexcept
  {
    if constexpr (UseAnd)
      return (size + upper - lower) & (size - 1);
    else
      return (size + upper - lower) % size;
  }

  strict_inline double amplitudeToDb(double amplitude) noexcept
  { return 20.0 * ::log10(amplitude); }

  strict_inline double dbToAmplitude(double decibels) noexcept
  { return ::pow(10.0, decibels / 20.0); }

  strict_inline double normalisedToDb(double normalised, double maxDb) noexcept
  { return ::pow(maxDb + 1.0, normalised) - 1.0; }

  strict_inline double dbToNormalised(double db, double maxDb) noexcept
  { return ::log2(db + 1.0) / ::log2(maxDb + 1.0); }

  strict_inline double normalisedToFrequency(double normalised, double sampleRate) noexcept
  { return ::pow(sampleRate * 0.5 / kMinFrequency, normalised) * kMinFrequency; }

  strict_inline double frequencyToNormalised(double frequency, double sampleRate) noexcept
  { return ::log2(frequency / kMinFrequency) / ::log2(sampleRate * 0.5 / kMinFrequency); }

  // returns the proper bin which may also be nyquist, which is outside a power-of-2 
  strict_inline double normalisedToBinUnsafe(double normalised, u32 FFTSize, double sampleRate) noexcept
  { return ::round(normalisedToFrequency(normalised, sampleRate) / sampleRate * (double)FFTSize); }

  // always returns a bin < FFTSize, therefore cannot return nyquist
  strict_inline double normalisedToBinSafe(double normalised, u32 FFTSize, double sampleRate) noexcept
  { return min(normalisedToBinUnsafe(normalised, FFTSize, sampleRate), (double)FFTSize / 2.0 - 1.0); }

  strict_inline double binToNormalised(double bin, u32 FFTSize, double sampleRate) noexcept
  {
    // at 0 logarithm doesn't produce valid values
    if (bin == 0.0)
      return 0.0;
    return frequencyToNormalised(bin * sampleRate / (double)FFTSize, sampleRate);
  }

  strict_inline double centsToRatio(double cents) noexcept
  { return ::pow(2.0, cents / (double)kCentsPerOctave); }

  strict_inline double midiCentsToFrequency(double cents) noexcept
  { return kMidi0Frequency * centsToRatio(cents); }

  strict_inline double midiNoteToFrequency(double note) noexcept
  { return midiCentsToFrequency(note * kCentsPerNote); }

  strict_inline double frequencyToMidiNote(double frequency) noexcept
  { return (double)kNotesPerOctave * ::log(frequency / kMidi0Frequency) * kExpConversionMult; }

  strict_inline double frequencyToMidiCents(double frequency) noexcept
  { return kCentsPerNote * frequencyToMidiNote(frequency); }






  strict_inline float nextPowerOfTwo(float value) noexcept
  { return ::roundf(::powf(2.0f, ::ceilf(::logf(value) * kExpConversionMult))); }

  constexpr strict_inline i32 prbs32(i32 x) noexcept
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
    ScopedNoDenormals() noexcept;
    ~ScopedNoDenormals() noexcept;

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
    struct control_block
    {
      satomi::atomic<u64> refCount = 1;
      T *object_ = nullptr;
    };

  public:
    constexpr sp() noexcept = default;
    constexpr sp(nullptr_t) noexcept { }
    constexpr sp(const sp &other) noexcept : refCount_{ other.refCount_ }
    {
      if (!refCount_)
        return;

      refCount_->refCount.fetch_add(1, satomi::memory_order_relaxed);
    }
    constexpr sp(sp &&other) noexcept { swap(other); }
    constexpr sp &operator=(const sp &other) noexcept
    {
      if (this != &other)
      {
        this->~sp();

        refCount_ = other.refCount_;
        refCount_->refCount.fetch_add(1, satomi::memory_order_relaxed);
      }

      return *this;
    }
    constexpr sp &operator=(sp &&other) noexcept
    {
      if (this != &other)
      {
        this->~sp();

        swap(other);
      }

      return *this;
    }
    constexpr ~sp() noexcept
    {
      auto refCount = refCount_;
      refCount_ = nullptr;
      if (!refCount)
        return;

      auto value = refCount->refCount.fetch_sub(1, satomi::memory_order_acq_rel);
      if (value > 1)
        return;

      refCount->object_->~T();
      refCount->~control_block();
      if (utils::is_constant_evaluated())
        operator delete[](refCount, utils::align_val_t(alignof(T)));
      else
        utils::deallocate(refCount);
    }

    static constexpr sp<T> create(auto &&... args)
    {
      constexpr auto alignment = alignof(T);
      constexpr auto headerSize = utils::roundUpToMultiple(sizeof(control_block), alignment);
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
      shared.refCount_ = new(memory) control_block{};
      shared.refCount_->object_ = new(memory + headerSize) T{ COMPLEX_FWD(args)... };

      return shared;
    }

    constexpr void swap(sp &other) noexcept
    {
      COMPLEX_SWAP_MEMBERS(refCount_, other);
    }

    constexpr void reset() noexcept { sp{}.swap(*this); }

    constexpr T *get() const noexcept { return (refCount_) ? refCount_->object_ : nullptr; }
    constexpr T *operator->() const noexcept { return get(); }
    constexpr T &operator*() const noexcept { return *get(); }

    constexpr usize use_count() const noexcept
    { return (!refCount_) ? 0 : refCount_->refCount.load(satomi::memory_order_relaxed); }

    explicit constexpr operator bool() const noexcept { return get() != nullptr; }

  protected:
    control_block *refCount_ = nullptr;
  };

  template<typename T, typename U>
  constexpr bool operator==(const sp<T> &one, const sp<U> &two) noexcept { return one.get() == two.get(); }
  template<typename T>
  constexpr bool operator==(const sp<T> &one, nullptr_t) noexcept { return one.get() == nullptr; }
  template<typename T>
  constexpr bool operator==(nullptr_t, const sp<T> &two) noexcept { return two.get() == nullptr; }

  // ghetto std::any implementation
  class whatever
  {
  private:
    static constexpr auto data_size = 8;
    template <typename T>
    static constexpr bool is_small = alignof(T) <= alignof(void *) && sizeof(T) <= data_size;

    union Storage
    {
      constexpr Storage() : pointer{ nullptr } { }

      // Prevent trivial copies of this type, buffer might hold a non-POD.
      Storage(const Storage &) = delete;
      Storage &operator=(const Storage &) = delete;

      void *pointer;
      byte buffer[sizeof(pointer)];
    };

    enum Op { OpAccess, OpClone, OpDestroy, OpMoveOut, OpTransfer, OpTypeId };

    union InOutParams
    {
      void *object;
      type_id_t typeId;
      whatever *whateverPtr;
    };

    void (*manager)(Op, const whatever *, InOutParams *) = nullptr;
    Storage storage{};

    template<typename T>
    struct ManagerInternal
    {
      static void ManageStorage(Op op, const whatever *instance, InOutParams *inOutParams)
      {
        // The contained object is in storage.buffer
        auto objectPtr = utils::launder((const T *)&instance->storage.buffer);
        switch (op)
        {
        case OpAccess:
          inOutParams->object = const_cast<T *>(objectPtr);
          break;
        case OpClone:
          new(&inOutParams->whateverPtr->storage.buffer) T(*objectPtr);
          inOutParams->whateverPtr->manager = instance->manager;
          break;
        case OpDestroy:
          objectPtr->~T();
          break;
        case OpMoveOut:
          new(inOutParams->object) T{ COMPLEX_MOVE(*const_cast<T *>(objectPtr)) };
          objectPtr->~T();
          break;
        case OpTransfer:
          new(&inOutParams->whateverPtr->storage.buffer) T{ COMPLEX_MOVE(*const_cast<T *>(objectPtr)) };
          objectPtr->~T();
          inOutParams->whateverPtr->manager = instance->manager;
          const_cast<whatever *>(instance)->manager = nullptr;
          break;
        case OpTypeId:
          inOutParams->typeId = type_id<T>;
          break;
        }
      }

      template<typename... Args>
      static T &CreateStorage(Storage &storage, Args&&... args)
      {
        void *address = &storage.buffer;
        return *new (address) T{ COMPLEX_FWD(args)... };
      }
    };

    // Manage external contained object.
    template<typename T>
    struct ManagerExternal
    {
      static void ManageStorage(Op op, const whatever *instance, InOutParams *inOutParams)
      {
        // The contained object is *storage.ptr
        auto objectPtr = static_cast<const T *>(instance->storage.pointer);
        switch (op)
        {
        case OpAccess:
          inOutParams->object = const_cast<T *>(objectPtr);
          break;
        case OpClone:
          inOutParams->whateverPtr->storage.pointer = new T(*objectPtr);
          inOutParams->whateverPtr->manager = instance->manager;
          break;
        case OpDestroy:
          delete objectPtr;
          break;
        case OpMoveOut:
          (void)new(inOutParams->object) T{ COMPLEX_MOVE(*const_cast<T *>(objectPtr)) };
          objectPtr->~T();
          break;
        case OpTransfer:
          inOutParams->whateverPtr->storage.pointer = instance->storage.pointer;
          inOutParams->whateverPtr->manager = instance->manager;
          const_cast<whatever *>(instance)->manager = nullptr;
          break;
        case OpTypeId:
          inOutParams->typeId = type_id<T>;
          break;
        }
      }

      template<typename... Args>
      static constexpr T &CreateStorage(Storage &storage, Args&&... args)
      {
        auto *object = new T{ COMPLEX_FWD(args)... };
        storage.pointer = object;
        return *object;
      }
    };

    template<typename T>
    using get_manager = utils::conditional_t<is_small<T>, ManagerInternal<T>, ManagerExternal<T>>;

  public:
    constexpr whatever() noexcept = default;
    ~whatever() noexcept { reset(); }
    whatever(const whatever &other)
    {
      if (!other.has_value())
        manager = nullptr;
      else
      {
        InOutParams inOutParams;
        inOutParams.whateverPtr = this;
        other.manager(OpClone, &other, &inOutParams);
      }
    }
    whatever(whatever &&other) noexcept
    {
      if (!other.has_value())
        manager = nullptr;
      else
      {
        InOutParams inOutParams;
        inOutParams.whateverPtr = this;
        other.manager(OpTransfer, &other, &inOutParams);
      }
    }
    whatever &operator=(const whatever &other)
    {
      *this = whatever{ other };
      return *this;
    }
    whatever &operator=(whatever &&other) noexcept
    {
      if (!other.has_value())
        reset();
      else if (this != &other)
      {
        reset();
        InOutParams inOutParams;
        inOutParams.whateverPtr = this;
        other.manager(OpTransfer, &other, &inOutParams);
      }
      return *this;
    }

    template<typename T>
    whatever(T &&object)
    {
      using Manager = get_manager<utils::remove_cvref_t<T>>;
      manager = &Manager::ManageStorage;
      (void)Manager::CreateStorage(storage, COMPLEX_FWD(object));
    }
    template<typename T>
    static whatever create(auto &&... args)
    {
      whatever instance;
      instance.emplace<T>(COMPLEX_FWD(args)...);
      return instance;
    }
    template<typename T>
    T &emplace(auto &&... args)
    {
      auto &ret = get_manager<T>::CreateStorage(storage, COMPLEX_FWD(args)...);
      manager = &get_manager<T>::ManageStorage;
      return ret;
    }

    bool has_value() const noexcept { return manager != nullptr; }

    void reset() noexcept
    {
      if (has_value())
      {
        manager(OpDestroy, this, nullptr);
        manager = nullptr;
      }
    }

    type_id_t type() const noexcept
    {
      if (!has_value())
        return type_id<void>;
      InOutParams inOutParams;
      manager(OpTypeId, this, &inOutParams);
      return inOutParams.typeId;
    }

    void swap(whatever &other) noexcept
    {
      if (!has_value() && !other.has_value())
        return;

      if (has_value() && other.has_value())
      {
        if (this == &other)
          return;

        whatever temp;
        InOutParams inOutParams;

        inOutParams.whateverPtr = &temp;
        other.manager(OpTransfer, &other, &inOutParams);
        inOutParams.whateverPtr = &other;

        manager(OpTransfer, this, &inOutParams);
        inOutParams.whateverPtr = this;
        temp.manager(OpTransfer, &temp, &inOutParams);
      }
      else
      {
        whatever *empty = !has_value() ? this : &other;
        whatever *full = !has_value() ? &other : this;

        InOutParams inOutParams;
        inOutParams.whateverPtr = empty;
        full->manager(OpTransfer, full, &inOutParams);
      }
    }

    template<typename ... VisitorFunctions>
    usize visit(VisitorFunctions &&... visitors)
    {
      usize index = 0;
      auto probe = [this, &index]<typename T>(T &&visitor)
      {
        using URef = typename detail::signature<T>::type;
        using UClean = utils::remove_cvref_t<URef>;
        static_assert(utils::is_lvalue_reference_v<URef>, "Parameter must an lvalue reference");

        if (manager == &get_manager<UClean>::ManageStorage)
        {
          InOutParams outParam;
          manager(OpAccess, this, &outParam);

          visitor(*static_cast<UClean *>(outParam.object));

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
    bool is_one_of()
    {
      auto matches = (usize(manager == &get_manager<Ts>::ManageStorage) + ...);
      return matches > 0;
    }

    template<template<typename...> class VariantLike, typename ... Us>
    usize try_extract(VariantLike<Us...> &variant)
    {
      if (!has_value())
        return usize(-1);

      usize index = 0;
      auto probe = [&]<typename T>()
      {
        if (manager == &get_manager<utils::remove_cvref_t<T>>::ManageStorage)
        {
          InOutParams outParam;
          manager(OpAccess, this, &outParam);

          auto *object = static_cast<T *>(outParam.object);
          variant = T{ COMPLEX_MOVE(*object) };
          object->~T();

          return true;
        }
        ++index;
        return false;
      };

      bool isFound = (probe.template operator()<Us>() || ...);
      if (!isFound)
        return usize(-1);
      return index;
    }

    template<typename T>
    T *try_get()
    {
      if (manager != &get_manager<utils::remove_cvref_t<T>>::ManageStorage)
        return nullptr;

      InOutParams outParam;
      manager(OpAccess, this, &outParam);
      return static_cast<T *>(outParam.object);
    }

    template<typename T>
    const T *try_get() const { return const_cast<whatever *>(this)->try_get<T>(); }
  };

  struct LivenessChecker
  {
  private:
    struct ControlBlock
    {
      satomi::atomic<bool> exists = true;
      satomi::atomic<usize> refCount = 1;
    } *controlBlock_ = nullptr;

    void construct(ControlBlock *controlBlock)
    {
      controlBlock_ = controlBlock;
      controlBlock_->refCount.fetch_add(1, satomi::memory_order_relaxed);
    }
  public:
    struct Master
    {
      ~Master() noexcept
      {
        if (!controlBlock_)
          return;

        auto *controlBlock = controlBlock_;
        controlBlock_ = nullptr;
        controlBlock->exists.store(false, satomi::memory_order_relaxed);
        auto value = controlBlock->refCount.fetch_sub(1, satomi::memory_order_relaxed);
        COMPLEX_ASSERT(value != 0);
        if (value > 1)
          return;

        delete controlBlock;
      }

      ControlBlock *getControlBlock() const
      {
        if (!controlBlock_)
          controlBlock_ = new ControlBlock{};
        
        return controlBlock_;
      }
    private:
      mutable ControlBlock *controlBlock_ = nullptr;
    };

    LivenessChecker() = default;
    ~LivenessChecker() noexcept
    {
      if (!controlBlock_)
        return;
      
      auto *controlBlock = controlBlock_;
      controlBlock_ = nullptr;
      auto value = controlBlock->refCount.fetch_sub(1, satomi::memory_order_relaxed);
      COMPLEX_ASSERT(value != 0);
      if (value > 1)
        return;

      delete controlBlock;
    }
    LivenessChecker &operator=(const auto *object)
    {
      this->~LivenessChecker();
      if (object != nullptr)
        construct(object->livenessIndicator_.getControlBlock());

      return *this;
    }
    LivenessChecker &operator=(const auto &object)
    {
      this->~LivenessChecker();
      construct(object.livenessIndicator_.getControlBlock());
      return *this;
    }

    bool isObjectAlive() const noexcept
    {
      return controlBlock_ != nullptr &&
        controlBlock_->exists.load(satomi::memory_order_relaxed);
    }
  };

#define COMPLEX_MAKE_LIVENESS_CHECKED                     \
    ::utils::LivenessChecker::Master livenessIndicator_{};\
    friend ::utils::LivenessChecker

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
      static constexpr vtable_t create() noexcept
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
      static constexpr R invoke(const void *data, Args &&... args)
      {
        const auto &object{ *static_cast<const FnT *>(data) };
        return object(COMPLEX_FWD(args)...);
      }

      static constexpr R empty_invoke(const void *, Args &&...)
      {
        // bad function call
        COMPLEX_TRAP();
      }

      // you can't just assign lambdas to the fn ptrs because micro soft can't fix their compiler
      static constexpr void empty_copy([[maybe_unused]] void *, [[maybe_unused]] const void *) { }
      static constexpr void empty_destroy([[maybe_unused]] void *) { }
      static constexpr void empty_move([[maybe_unused]] void *, [[maybe_unused]] void *) { }

      usize size = 0;
      usize alignment = 0;
      Copier copier = &empty_copy;
      Destroyer destroyer = &empty_destroy;
      Invoker invoker = &empty_invoke;
      Mover mover = &empty_move;
    };

    // Empty vtable which is used when fn is empty
    template<typename R, typename ... Args>
    inline constexpr vtable_t<R, Args...> empty_vtable{};
  }

  inline constexpr usize heap_allocated_tag = usize(-1) >> 1;

  // ghetto std::function implementation that can also be stack-allocated
  // and go back and forth between the heap and stack if needed
  // based on https://github.com/colugomusic/clog/blob/master/include/clog/small_function.hpp
  template<typename Signature, usize MaxSize = 32>
  class fn;

  template<typename R, typename ... Args, usize MaxSize>
  class fn<R(Args...), MaxSize>
  {
  public:
    static constexpr bool is_on_heap = MaxSize == heap_allocated_tag;

    constexpr fn() noexcept { }
    constexpr fn(nullptr_t) noexcept { }

    template<usize Size>
    constexpr fn(const fn<R(Args...), Size> &rhs)
    {
      allocate(rhs.vtable_);
      vtable_->copier(data_, rhs.data_);
    }

    // needed because fn & go to the lambda constructor otherwise
    // and cannot forward to proper copy constructor because uhhh ¯\_(ツ)_/¯
    template<usize Size>
    constexpr fn(fn<R(Args...), Size> &rhs)
    {
      allocate(rhs.vtable_);
      vtable_->copier(data_, rhs.data_);
    }

    template<usize Size>
    constexpr fn(fn<R(Args...), Size> &&rhs) noexcept
    {
      if constexpr (is_on_heap && fn<R(Args...), Size>::is_on_heap)
      {
        // optimisation: both on heap, exchange pointers
        vtable_ = rhs.vtable_;
        data_ = rhs.data_;
        rhs.data_ = nullptr;
        rhs.vtable_ = &detail::empty_vtable<R, Args...>;
      }
      else
      {
        allocate(rhs.vtable_);
        vtable_->mover(data_, rhs.data_);
        rhs.~fn<R(Args...), Size>();
      }
    }

    template<typename FnT, typename fn_t = remove_cvref_t<FnT>>
      requires is_invocable_r_v<R, fn_t &, Args...>
    constexpr fn(FnT &&lambda) { from_fn(COMPLEX_FWD(lambda)); }

    constexpr ~fn()
    {
      if (vtable_ == &detail::empty_vtable<R, Args...>)
        return;

      vtable_->destroyer(data_);
      if constexpr (is_on_heap)
        operator delete[](data_, utils::align_val_t(vtable_->alignment));
      vtable_ = &detail::empty_vtable<R, Args...>;
    }

    template<usize Size>
    constexpr fn &operator=(const fn<R(Args...), Size> &rhs)
    {
      if (vtable_ && data_)
        vtable_->destroyer(data_);

      allocate(rhs.vtable_);
      vtable_->copier(data_, rhs.data_);

      return *this;
    }

    template<usize Size>
    constexpr fn &operator=(fn<R(Args...), Size> &&rhs) noexcept
    {
      if constexpr (is_on_heap && fn<R(Args...), Size>::is_on_heap)
      {
        // optimisation: both on heap, exchange pointers
        this->~fn();

        vtable_ = rhs.vtable_;
        data_ = rhs.data_;
        rhs.data_ = nullptr;
        rhs.vtable_ = &detail::empty_vtable<R, Args...>;
      }
      else
      {
        if (vtable_ && data_)
          vtable_->destroyer(data_);
        allocate(rhs.vtable_);
        vtable_->mover(data_, rhs.data_);

        rhs.~fn<R(Args...), Size>();
      }

      return *this;
    }

    constexpr fn &operator=(nullptr_t)
    {
      this->~fn();
      return *this;
    }

    template<typename FnT, typename fn_t = remove_cvref_t<FnT>>
      requires is_invocable_r_v<R, fn_t &, Args...>
    constexpr auto operator=(FnT &&lambda) -> fn &
    {
      from_fn(COMPLEX_FWD(lambda));
      return *this;
    }

    constexpr fn<R(Args...), heap_allocated_tag> to_dyn_fn() const & { return { *this }; }
    constexpr fn<R(Args...), heap_allocated_tag> to_dyn_fn() & { return { *this }; }
    constexpr fn<R(Args...), heap_allocated_tag> to_dyn_fn() const && { return COMPLEX_MOVE(*this); }
    constexpr fn<R(Args...), heap_allocated_tag> to_dyn_fn() && { return COMPLEX_MOVE(*this); }

    template<usize Size> fn<R(Args...), Size> constexpr to_small_fn() const & { return { *this }; }
    template<usize Size> fn<R(Args...), Size> constexpr to_small_fn() & { return { *this }; }
    template<usize Size> fn<R(Args...), Size> constexpr to_small_fn() const && { return COMPLEX_MOVE(*this); }
    template<usize Size> fn<R(Args...), Size> constexpr to_small_fn() && { return COMPLEX_MOVE(*this); }

    explicit constexpr operator bool() const noexcept
    {
      return vtable_ != &detail::empty_vtable<R, Args...>;
    }

    constexpr R operator()(Args ... args) const
    {
      return vtable_->invoker(data_, COMPLEX_FWD(args)...);
    }

  private:
    static constexpr usize alignment = 8;
    using vtable_t = detail::vtable_t<R, Args...>;

    static_assert(((MaxSize - sizeof(vtable_t)) % alignment == 0) || is_on_heap);

    using storage_t = conditional_t<is_on_heap, void *, byte[MaxSize - sizeof(vtable_t *)]>;

    constexpr void allocate(const vtable_t *newVtable)
    {
      if constexpr (is_on_heap)
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
            operator delete[](data_, utils::align_val_t(vtable_->alignment));
          }

          // can't use operator new[] the usual way because of a longstanding bug in msvc...
          // https://developercommunity.visualstudio.com/t/using-c17-new-stdalign-val-tn-syntax-results-in-er/528320
          data_ = operator new[](newVtable->size, utils::align_val_t(newVtable->alignment));
        }
      }
      else
      {
        COMPLEX_ASSERT(alignment >= newVtable->alignment && sizeof(storage_t) >= newVtable->size);
      }

      vtable_ = newVtable;
    }

    template<typename FnT>
    constexpr void from_fn(FnT &&lambda)
    {
      using fn_t = remove_cvref_t<FnT>;

      static_assert(is_copy_constructible_v<fn_t>,
        "fn cannot be constructed from a non-copyable type");

      static_assert(sizeof(fn_t) <= sizeof(storage_t) || is_on_heap,
        "fn is too small to fit this object");

      static_assert((alignment % alignof(fn_t) == 0) || is_on_heap,
        "fn cannot be constructed from an object of this alignment");

      static_assert(alignment >= alignof(fn_t) || is_on_heap,
        "fn does not support alignment higher the one defined in the class");

      static constexpr auto vtable = vtable_t::template create<fn_t>();

      allocate(&vtable);

      (void)new (data_) fn_t{ COMPLEX_FWD(lambda) };

      vtable_ = &vtable;
    }

    alignas(is_on_heap ? alignof(storage_t) : alignment) storage_t data_{};
    const vtable_t *vtable_ = &detail::empty_vtable<R, Args...>;

    template<typename, usize>
    friend class fn;
  };

  template<typename Signature>
  using dynFn = fn<Signature, heap_allocated_tag>;

  template<typename Signature, usize MaxSize = 32>
  using smallFn = fn<Signature, MaxSize>;


  class thread
  {
    void createThread(utils::dynFn<int()> *procedure);
  public:
    struct id
    {
    #ifdef COMPLEX_WINDOWS
      /*DWORD*/ unsigned int native_id{};
    #else
      /*pthread_t*/ unsigned long native_id{};
    #endif

      bool operator==(const id &other) const;
      explicit operator bool() { return native_id; }
    } thread_id{};

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
    thread(thread &&other) noexcept : thread{}
    {
      COMPLEX_SWAP_MEMBERS(thread_id, other);
      COMPLEX_SWAP_MEMBERS(handle_, other);
    }
    thread &operator=(const thread &other) = delete;
    thread &operator=(thread &&other) noexcept
    {
      if (&other != this)
      {
        this->~thread();
        COMPLEX_SWAP_MEMBERS(thread_id, other);
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
