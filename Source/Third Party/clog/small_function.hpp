//
// std::function replacement which does not ever allocate
// heap memory
// 

#pragma once

#include <cstddef>
#include <cstdlib>
#include <type_traits>

namespace clg 
{
  template <typename Signature, size_t MaxSize = 32>
  class small_fn;

  #define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)
  #define MOV(...) static_cast<std::remove_reference_t<decltype(__VA_ARGS__)>&&>(__VA_ARGS__)

  namespace detail 
  {
    //
    // Type trait to help disable the constructor/assignment taking
    // a forwarding reference when we want to do a copy/move instead
    //
    template<typename>
    inline constexpr bool is_small_fn_v = false;
    template<typename Signature, size_t MaxSize>
    inline constexpr bool is_small_fn_v<small_fn<Signature, MaxSize>> = true;
    
    //
    // The vtable!
    //
    template <typename R, typename... Args>
    struct vtable_t
    {
      using Copier = void (*)(void*, const void*);
      using Destroyer = void (*)(void*);
      using Invoker = auto (*)(const void*, Args&&...) -> R;
      using Mover = void (*)(void*, void*);

      template <typename FnT>
      static constexpr vtable_t create() noexcept
      {
        return vtable_t{ &copy<FnT>, &destroy<FnT>, &invoke<FnT>, &move<FnT> };
      }

      template <typename FnT>
      static constexpr void copy(void* dest, const void* src)
      {
        const auto& src_fn{ *static_cast<const FnT*>(src) };

        ::new (dest) FnT{ src_fn };
      }

      template <typename FnT>
      static constexpr void destroy(void* src)
      {
        auto& src_fn{ *static_cast<FnT*>(src) };

        src_fn.~FnT();
      }

      template <typename FnT>
      static constexpr void move(void* dest, void* src)
      {
        auto& src_fn{ *static_cast<FnT*>(src) };

        ::new (dest) FnT{ MOV(src_fn) };

        src_fn.~FnT();
      }

      template <typename FnT>
      static constexpr auto invoke(const void* data, Args&&... args) -> R
      {
        const auto& fn{ *static_cast<const FnT*>(data) };

        return fn(FWD(args)...);
      }

      static constexpr auto empty_invoke(const void*, Args&&...) -> R
      {
        // bad function call
        std::abort();
      }

      Copier copier = [](void*, const void*) { };
      Destroyer destroyer = [](void*) { };
      Invoker invoker = &empty_invoke;
      Mover mover = [](void*, void*) { };
    };

    // Empty vtable which is used when small_fn is empty
    template <typename R, typename... Args>
    inline constexpr vtable_t<R, Args...> empty_vtable{};
  }

  template <typename R, typename... Args, size_t MaxSize>
  class small_fn<auto(Args...) -> R, MaxSize>
  {
  private:
    static constexpr std::size_t alignment = 8;
    using vtable_t = detail::vtable_t<R, Args...>;

    static_assert((MaxSize - sizeof(vtable_t)) % alignment == 0);

  public:

    constexpr small_fn() noexcept : vtable_{ &detail::empty_vtable<R, Args...> } { }
    constexpr small_fn(std::nullptr_t) noexcept : small_fn{} { }

    constexpr small_fn(const small_fn& rhs) : vtable_{ rhs.vtable_ }
    { vtable_->copier(&data_, &rhs.data_); }

    constexpr small_fn(small_fn&& rhs) noexcept : vtable_{ rhs.vtable_ }
    {
      rhs.vtable_ = &detail::empty_vtable<R, Args...>;
      vtable_->mover(&data_, &rhs.data_);
    }

    //
    // This constructor is enabled only for function objects.
    // We make sure that is_small_function == false and that we can invoke it like R(Args...)
    //
    template<typename FnT, typename fn_t = std::decay_t<FnT>> 
      requires (!detail::is_small_fn_v<fn_t> && std::is_invocable_r_v<R, fn_t &, Args...>)
    constexpr small_fn(FnT&& fn) { from_fn(FWD(fn)); }

    constexpr ~small_fn() { vtable_->destroyer(&data_); }

    constexpr auto operator=(const small_fn& rhs) -> small_fn&
    {
      vtable_ = rhs.vtable_;
      vtable_->copier(&data_, &rhs.data_);

      return *this;
    }

    constexpr auto operator=(small_fn&& rhs) noexcept -> small_fn&
    {
      vtable_ = rhs.vtable_;
      rhs.vtable_ = &detail::empty_vtable<R, Args...>;
      vtable_->mover(&data_, &rhs.data_);

      return *this;
    }

    constexpr auto operator=(std::nullptr_t) -> small_fn&
    {
      vtable_->destroyer(&data_);
      vtable_ = &detail::empty_vtable<R, Args...>;

      return *this;
    }

    template<typename FnT, typename fn_t = std::decay_t<FnT>>
      requires (!detail::is_small_fn_v<fn_t> && std::is_invocable_r_v<R, fn_t &, Args...>)
    constexpr auto operator=(FnT&& fn) -> small_fn&
    {
      from_fn(FWD(fn));
      return *this;
    }

    explicit constexpr operator bool() const noexcept
    {
      return vtable_ != &detail::empty_vtable<R, Args...>;
    }

    constexpr auto operator()(Args... args) const -> R
    {
      return vtable_->invoker(&data_, FWD(args)...);
    }

  private:
    using storage_t = unsigned char[MaxSize - sizeof(vtable_t *)];

    template<typename FnT>
    auto from_fn(FnT&& fn) -> void
    {
      using fn_t = std::decay_t<FnT>;

      static_assert(std::is_copy_constructible_v<fn_t>,
        "clg::small_fn cannot be constructed from a non-copyable type");

      static_assert(sizeof(fn_t) <= sizeof(storage_t),
        "This object is too big to fit inside the clg::small_fn");

      static_assert(alignment % alignof(fn_t) == 0,
        "clg::small_fn cannot be constructed from an object of this alignment");

      static_assert(alignment >= alignof(fn_t),
        "clg::small_fn does not support alignment higher the one defined in the class");

      static constexpr auto vtable = vtable_t::template create<fn_t>();

      ::new (&data_) fn_t{ FWD(fn) };

      vtable_ = &vtable;
    }

    alignas(alignment) storage_t data_;
    const vtable_t* vtable_;
  };

  #undef FWD
  #undef MOV

} //clg
