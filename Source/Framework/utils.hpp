/*
  ==============================================================================

    utils.hpp
    Created: 27 Jul 2021 12:53:28am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <math.h>
#ifdef COMPLEX_MSVC
  #include <vcruntime_new.h>
#else
  #include <new>
#endif
#include <atomic>

#include "constants.hpp"
#include "stl_utils.hpp"

namespace utils
{
  enum class MathOperations { Assign, Add, Multiply, FadeInAdd, FadeOutAdd, Interpolate };

  // get you the N-th element of a parameter pack
  template <auto N, auto I = 0, typename T>
  decltype(auto) getNthElement(T &&arg, [[maybe_unused]] auto &&... args)
  {
    if constexpr (I == N)
      return COMPLEX_FWD(arg);
    else if constexpr (sizeof...(args) > 0)
      return getNthElement<N, I + 1>(COMPLEX_FWD(args)...);
  }

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
  { return utils::min(normalisedToBinUnsafe(normalised, FFTSize, sampleRate), (double)FFTSize / 2.0 - 1.0); }

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


  strict_inline u32 log2(u32 value) noexcept
  {
  #if defined(COMPLEX_GCC) || defined(COMPLEX_CLANG)
    constexpr u32 kMaxBitIndex = sizeof(u32) * 8 - 1;
    return kMaxBitIndex - __builtin_clz(max(value, 1));
  #elif defined(COMPLEX_MSVC)
    unsigned long result = 0;
    _BitScanReverse(&result, value);
    return result;
  #else
    u32 num = 0;
    while (value >>= 1)
      num++;
    return num;
  #endif
  }

  constexpr strict_inline bool isPowerOfTwo(utils::integral auto value) noexcept
  { return (value & (value - 1)) == 0; }

  strict_inline float nextPowerOfTwo(float value) noexcept
  { return ::roundf(::powf(2.0f, ::ceilf(::logf(value) * kExpConversionMult))); }

  template<utils::integral T>
  constexpr auto roundUpToMultiple(T i, T factor)
  { return ((i + factor - 1) / factor) * factor; }

  strict_inline float rms(const float *buffer, u32 num) noexcept
  {
    float squared_total = 0.0f;
    for (u32 i = 0; i < num; i++)
      squared_total += buffer[i] * buffer[i];

    return sqrtf(squared_total / (float)num);
  }

  constexpr strict_inline i32 prbs32(i32 x) noexcept
  {
    // maximal length, taps 32 31 29 1, from wikipedia
    return (i32)(((u32)x >> 1U) ^ (-(x & 1) & 0xd0000001U));
  }

  template <typename T> 
  constexpr strict_inline int signum(T val) noexcept
  { return (T(0) < val) - (val < T(0)); }

  // returns the starting position of the centered element relative to container
  template<typename T>
  constexpr T centerAxis(T elementRange, T containerRange) noexcept
  { return (containerRange - elementRange) / T(2); }

  template<utils::floating_point T>
  constexpr strict_inline T unsignFloat(T &value) noexcept
  {
    int sign = signum(value);
    value *= (T)sign;
    return (T)sign;
  }

  template<utils::signed_integral T>
  constexpr strict_inline int unsignInt(T &value) noexcept
  {
    int sign = signum(value);
    value *= sign;
    return (T)sign;
  }

  template<utils::floating_point T>
  constexpr strict_inline T smoothStep(T value) noexcept
  {
    T sqr = value * value;
    return (T(3) * sqr) - (T(2) * sqr * value);
  }

  template<typename T>
  strict_inline T *as(auto *pointer)
  {
  #if COMPLEX_DEBUG
    auto *castPointer = dynamic_cast<T *>(pointer);
    COMPLEX_ASSERT(castPointer, "Unsuccessful cast");
    return castPointer;
  #else
    return static_cast<T *>(pointer);
  #endif
  }

  // if you want to augment the control block, derive from this type (non-virtually)
  struct sp_ref_base
  {
    static constexpr u64 isCombinedMask = u64(1) << 63;
    static constexpr u64 refCountMask = ~isCombinedMask;

    std::atomic<u64> refCount = 1;
  };

  // ghetto shared_ptr implementation with changable control block
  template<typename T, typename RefCountT = sp_ref_base> 
    requires utils::is_base_of_v<sp_ref_base, RefCountT>
  class sp
  {
  protected:
    struct combined : public RefCountT
    {
      constexpr combined() : RefCountT{ .refCount = RefCountT::isCombinedMask + 1 } { }
      T *create_object(auto &&... args) { return ::new(object_) T{ COMPLEX_FWD(args)... }; }
      alignas(alignof(T)) unsigned char object_[sizeof(T)]{};
    };
  public:
    constexpr sp() noexcept = default;
    constexpr sp(std::nullptr_t) noexcept {}
    template<typename U> requires utils::is_convertible_v<U *, T *>
    explicit constexpr sp(U *instance)
    {
      object_ = instance;
      refCount_ = new RefCountT{};
    }
    constexpr sp(const sp &other) noexcept
    {
      if (!other.refCount_)
        return;

      object_ = other.object_;
      refCount_ = other.refCount_;
      refCount_->refCount.fetch_add(1, std::memory_order_relaxed);
    }
    template<typename U> requires utils::is_convertible_v<U *, T *>
    constexpr sp(const sp<U, RefCountT> &other) noexcept
    {
      if (!other.refCount_)
        return;

      object_ = other.object_;
      refCount_ = other.refCount_;
      refCount_->refCount.fetch_add(1, std::memory_order_relaxed);
    }
    constexpr sp(sp &&other) noexcept
    {
      object_ = other.object_;
      refCount_ = other.refCount_;
      other.object_ = nullptr;
      other.refCount_ = nullptr;
    }
    template<typename U> requires utils::is_convertible_v<U *, T *>
    constexpr sp(sp<U, RefCountT> &&other) noexcept
    {
      object_ = other.object_;
      refCount_ = other.refCount_;
      other.object_ = nullptr;
      other.refCount_ = nullptr;
    }
    constexpr sp &operator=(const sp &other) noexcept { sp<T>{ other }.swap(*this); return *this; }
    template<typename U> requires utils::is_convertible_v<U *, T *>
    constexpr sp &operator=(const sp<U, RefCountT> &other) noexcept { sp<T>{ other }.swap(*this); return *this; }
    constexpr sp &operator=(sp &&other) noexcept { sp<T>{ COMPLEX_MOVE(other) }.swap(*this); return *this; }
    template<typename U> requires utils::is_convertible_v<U *, T *>
    constexpr sp &operator=(sp<U, RefCountT> &&other) noexcept { sp<T>{ COMPLEX_MOVE(other) }.swap(*this); return *this; }
    constexpr ~sp() noexcept
    {
      if (!refCount_)
        return;

      auto *refCount = refCount_;
      refCount_ = nullptr;
      auto value = refCount->refCount.fetch_sub(1, std::memory_order_acq_rel);
      if ((value & RefCountT::refCountMask) > 1)
        return;

      if (value & RefCountT::isCombinedMask)
      {
        object_->~T();
        delete static_cast<combined *>(refCount);
      }
      else
      {
        delete object_;
        delete refCount;
      }
    }

    static constexpr sp<T> create(auto &&... args)
    {
      sp shared;
      auto *refCount = new combined{};
      shared.object_ = refCount->create_object(COMPLEX_FWD(args)...);
      shared.refCount_ = refCount;
      return shared;
    }

    constexpr void swap(sp &other) noexcept
    {
      auto *otherObject = other.object_;
      auto *otherRefCount = other.refCount_;
      other.object_ = object_; 
      other.refCount_ = refCount_;
      object_ = otherObject;
      refCount_ = otherRefCount;
    }

    constexpr void reset() noexcept { sp{}.swap(*this); }
    template<typename U> requires utils::is_convertible_v<U *, T *>
    constexpr void reset(U *pointer) { sp<T>{ pointer }.swap(*this); }

    constexpr T *get() const noexcept { return object_; }
    constexpr T *operator->() const noexcept { return get(); }
    constexpr T &operator*() const noexcept { return *get(); }

    constexpr usize use_count() const noexcept
    { return (!refCount_) ? 0 : (refCount_->refCount.load(std::memory_order_relaxed) & RefCountT::refCountMask); }

    explicit constexpr operator bool() const noexcept { return get() != nullptr; }

  protected:
    RefCountT *refCount_ = nullptr;
    T *object_ = nullptr;
  };

  template<typename T, typename U>
  constexpr bool operator==(const sp<T> &one, const sp<U> &two) noexcept { return one.get() == two.get(); }
  template<typename T>
  constexpr bool operator==(const sp<T> &one, std::nullptr_t) noexcept { return one.get() == nullptr; }
  template<typename T>
  constexpr bool operator==(std::nullptr_t, const sp<T> &two) noexcept { return two.get() == nullptr; }

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
      unsigned char buffer[sizeof(pointer)];
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
        auto objectPtr = utils::launder(reinterpret_cast<const T *>(&instance->storage.buffer));
        switch (op)
        {
        case OpAccess:
          inOutParams->object = const_cast<T *>(objectPtr);
          break;
        case OpClone:
          ::new(&inOutParams->whateverPtr->storage.buffer) T(*objectPtr);
          inOutParams->whateverPtr->manager = instance->manager;
          break;
        case OpDestroy:
          objectPtr->~T();
          break;
        case OpMoveOut:
          ::new(inOutParams->object) T{ COMPLEX_MOVE(*const_cast<T *>(objectPtr)) };
          objectPtr->~T();
          break;
        case OpTransfer:
          ::new(&inOutParams->whateverPtr->storage.buffer) T{ COMPLEX_MOVE(*const_cast<T *>(objectPtr)) };
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
        ::new (address) T{ COMPLEX_FWD(args)... };
        return *static_cast<T *>(address);
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
          ::new(inOutParams->object) T{ COMPLEX_MOVE(*const_cast<T *>(objectPtr)) };
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

    template<typename Sig>
    struct signature;

    template<typename Ret, typename T>
    struct signature<Ret(T)> { using type = T; };

    template<typename Ret, typename Obj, typename T>
    struct signature<Ret(Obj:: *)(T)> { using type = T; };

    template<typename Ret, typename Obj, typename T>
    struct signature<Ret(Obj:: *)(T) const> { using type = T; };

  public:
    constexpr whatever() noexcept : manager{ nullptr } { }
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
      auto probe = [this, &index]<typename T>(T && visitor)
      {
        using URef = typename signature<T>::type;
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
      auto matches = (((manager == &get_manager<Ts>::ManageStorage) ? usize(1) : usize(0)) + ...);
      return matches > 0;
    }

    template<template <typename...> class VariantLike, typename ... Us>
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

      bool isFound = (probe.template operator() < Us > () || ...);
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
      std::atomic<bool> exists = true;
      std::atomic<usize> refCount = 1;
    } *controlBlock_ = nullptr;

    void construct(ControlBlock *controlBlock)
    {
      controlBlock_ = controlBlock;
      controlBlock_->refCount.fetch_add(1, std::memory_order_relaxed);
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
        controlBlock->exists.store(false, std::memory_order_relaxed);
        auto value = controlBlock->refCount.fetch_sub(1, std::memory_order_relaxed);
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
      auto value = controlBlock->refCount.fetch_sub(1, std::memory_order_relaxed);
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
        controlBlock_->exists.load(std::memory_order_relaxed);
    }
  };

#define COMPLEX_MAKE_LIVENESS_CHECKED                     \
    ::utils::LivenessChecker::Master livenessIndicator_{};\
    friend ::utils::LivenessChecker

}