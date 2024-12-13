/*
  ==============================================================================

    utils.hpp
    Created: 27 Jul 2021 12:53:28am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <math.h>
#include <new>
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

  template<typename T>
  constexpr strict_inline const T &min(const T &one, const T &two) noexcept { return (one < two) ? one : two; }
  template<typename T>
  constexpr strict_inline const T &max(const T &one, const T &two) noexcept { return (one >= two) ? one : two; }
  template<typename T>
  constexpr strict_inline const T &clamp(const T &value, const T &min, const T &max) noexcept
  { return (value > max) ? max : (value < min) ? min : value; }

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

  // rounds up a structure size so that the next element has the specified alignment
  constexpr auto roundUpToAlignment(usize size, usize alignment)
  { return ((size + alignment - 1) / alignment) * alignment; }

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

  // ghetto unique_ptr implementation
  template<typename T>
  class up
  {
  public:
    constexpr up() noexcept = default;
    constexpr up(std::nullptr_t) noexcept {}
    constexpr up(const up &other) noexcept = delete;
    constexpr up(up &&other) noexcept
    {
      object_ = other.object_;
      other.object_ = nullptr;
    }
    // allows up- AND downcasting, check types before doing this
    template<typename U> requires utils::is_convertible_v<U *, T *> || utils::is_convertible_v<T *, U *>
    constexpr up(up<U> &&other) noexcept
    {
      object_ = static_cast<T *>(other.object_);
      other.object_ = nullptr;
    }
    constexpr up &operator=(const up &other) noexcept = delete;
    constexpr up &operator=(up &&other) noexcept { up<T>{ COMPLEX_MOV(other) }.swap(*this); return *this; }
    // allows up- AND downcasting, check types before doing this
    template<typename U> requires utils::is_convertible_v<U *, T *> || utils::is_convertible_v<T *, U *>
    constexpr up &operator=(up<U> &&other) noexcept { up<T>{ COMPLEX_MOV(other) }.swap(*this); return *this; }
    constexpr ~up() noexcept { delete object_; }

    static constexpr up<T> create(auto &&... args)
    {
      up unique;
      unique.object_ = new T{ COMPLEX_FWD(args)... };
      return unique;
    }

    constexpr void swap(up &other) noexcept
    {
      auto *otherObject = other.object_;
      other.object_ = object_; 
      object_ = otherObject;
    }

    constexpr void reset() noexcept { up{}.swap(*this); }
    template<typename U> requires utils::is_convertible_v<U *, T *>
    constexpr void reset(U *pointer) { up<T>{ pointer }.swap(*this); }

    constexpr T *release() noexcept
    {
      T *pointer = object_;
      object_ = nullptr;
      return pointer;
    }

    constexpr T *get() const noexcept { return object_; }
    constexpr T *operator->() const noexcept { return get(); }
    constexpr T &operator*() const noexcept { return *get(); }

    explicit constexpr operator bool() const noexcept { return get(); }

  private:
    T *object_ = nullptr;

    template<typename U> friend class up;
  };

  template<typename U> up(U *pointer) -> up<U>;

  template<typename T, typename U>
  constexpr bool operator==(const up<T> &one, const up<U> &two) noexcept { return one.get() == two.get(); }
  template<typename T>
  constexpr bool operator==(const up<T> &one, std::nullptr_t) noexcept { return one.get() == nullptr; }
  template<typename T>
  constexpr bool operator==(std::nullptr_t, const up<T> &two) noexcept { return two.get() == nullptr; }

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
    constexpr sp &operator=(sp &&other) noexcept { sp<T>{ COMPLEX_MOV(other) }.swap(*this); return *this; }
    template<typename U> requires utils::is_convertible_v<U *, T *>
    constexpr sp &operator=(sp<U, RefCountT> &&other) noexcept { sp<T>{ COMPLEX_MOV(other) }.swap(*this); return *this; }
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

  // because c++ const is shit i need to use this
  template<typename T>
  struct DeferredConstant
  {
    constexpr DeferredConstant() : dummy_(0), isActive_(false) { }
    constexpr DeferredConstant(T value) : value_(COMPLEX_MOV(value)), isActive_(true) { }
    constexpr DeferredConstant(const DeferredConstant &other) = default;
    constexpr DeferredConstant(DeferredConstant &&other) = default;

    // aborts here should be taken to mean a compiler error
    constexpr DeferredConstant &operator=(T value)
    {
      if (!isActive_)
      {
        isActive_ = true;
        value_ = COMPLEX_MOV(value);
      }
      else
      {
        COMPLEX_ASSERT_FALSE("Value is already assigned");
        std::abort();
      }

      return *this;
    }
    constexpr DeferredConstant &operator=(const DeferredConstant &other)
    {
      if (!isActive_)
      {
        if (!other.isActive_)
          return *this;

        isActive_ = true;
        value_ = other.value_;
      }
      else
      {
        COMPLEX_ASSERT_FALSE("Value is already assigned");
        std::abort();
      }

      return *this;
    }
    constexpr DeferredConstant &operator=(DeferredConstant &&other)
    {
      if (!isActive_)
      {
        if (!other.isActive_)
          return *this;

        isActive_ = true;
        value_ = COMPLEX_MOV(other.value_);
      }
      else
      {
        COMPLEX_ASSERT_FALSE("Value is already assigned");
        std::abort();
      }

      return *this;
    }

    constexpr explicit operator bool() const noexcept { return this->isActive_; }
    constexpr bool has_value() const noexcept { return isActive_; }

    // aborts here should be taken to mean a compiler error
    constexpr auto value() const
    {
      if (!isActive_)
      {
        COMPLEX_ASSERT_FALSE("Value was not assigned");
        std::abort();
      }
      return value_;
    }
    constexpr T operator*() const { return value(); }
    constexpr operator T() const { return value(); }
    constexpr T operator->() const requires utils::is_pointer_v<T> { return value(); }

  private:
    union { char dummy_; T value_; };
    bool isActive_;
  };

}