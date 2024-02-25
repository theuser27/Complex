//
// std::function replacement which does not ever allocate
// heap memory
// 

#pragma once

#include <functional>
#include <utility>

namespace clg 
{
  template <typename Signature, size_t MaxSize = 64>
  class small_function;

  namespace detail 
  {
    template <typename T> struct type_identity { using type = T; };

    //
    // Type trait to help disable the constructor/assignment taking
    // a forwarding reference when we want to do a copy/move instead
    //
    template<typename>
    struct is_small_function : std::false_type {};

    template<typename Signature, size_t MaxSize>
    struct is_small_function<small_function<Signature, MaxSize>> : std::true_type {};

    template <typename T>
    inline constexpr auto is_small_function_v = is_small_function<T>::value;

    //
    // The vtable!
    //
    template <typename R, typename... Args>
    struct vtable_t
    {
      using Copier = void (*)(void*, const void*);
      using Destroyer = void (*)(void*);
      using Invoker = R (*)(const void*, Args&&...);
      using Mover = void (*)(void*, void*);

      constexpr vtable_t() noexcept
        : copier{[](void*, const void*){}}
        , destroyer{[](void*){}}
        , invoker{&empty_invoke}
        , mover{[](void*, void*){}}
      {
      }

      template <typename FnT>
      constexpr vtable_t(detail::type_identity<FnT>) noexcept
        : copier{&copy<FnT>}
        , destroyer{&destroy<FnT>}
        , invoker{&invoke<FnT>}
        , mover{&move<FnT>}
      {
      }

      template <typename FnT>
      static auto copy(void* dest, const void* src) -> void
      {
        const auto& src_fn{*static_cast<const FnT*>(src)};

        ::new (dest) FnT(src_fn);
      }

      template <typename FnT>
      static auto destroy(void* src) -> void
      {
        auto& src_fn{*static_cast<FnT*>(src)};

        src_fn.~FnT();
      }

      template <typename FnT>
      static auto move(void* dest, void* src) -> void
      {
        auto& src_fn{*static_cast<FnT*>(src)};

        ::new (dest) FnT(std::move(src_fn));

        src_fn.~FnT();
      }

      template <typename FnT>
      static auto invoke(const void* data, Args&&... args) -> R
      {
        const auto& fn{*static_cast<const FnT*>(data)};

        return fn(std::forward<Args>(args)...);
      }

      static auto empty_invoke(const void*, Args&&...) -> R
      {
        throw std::bad_function_call();
      }

      Copier copier{};
      Destroyer destroyer{};
      Invoker invoker{};
      Mover mover{};
    };

    // Empty vtable which is used when small_function is empty
    template <typename R, typename... Args>
    inline constexpr vtable_t<R, Args...> empty_vtable{};

    template <std::size_t size, std::size_t alignment>
    struct aligned_storage 
    {
      alignas(alignment) std::byte data[size];
    };

  } // detail

  template <typename R, typename... Args, size_t MaxSize>
  class small_function<R(Args...), MaxSize>
  {
  private:
    static constexpr std::size_t alignment = 8;
    using vtable_t = detail::vtable_t<R, Args...>;

    static_assert((MaxSize - sizeof(vtable_t)) % alignment == 0);

  public:

    small_function() noexcept
      : vtable_{std::addressof(detail::empty_vtable<R, Args...>)}
    {
    }

    small_function(std::nullptr_t) noexcept
      : vtable_{std::addressof(detail::empty_vtable<R, Args...>)}
    {
    }

    small_function(const small_function& rhs)
      : vtable_{rhs.vtable_}
    {
      vtable_->copier(std::addressof(data_), std::addressof(rhs.data_));
    }

    small_function(small_function&& rhs) noexcept
      : vtable_{std::exchange(rhs.vtable_, std::addressof(detail::empty_vtable<R, Args...>))}
    {
      vtable_->mover(std::addressof(data_), std::addressof(rhs.data_));
    }

    //
    // This constructor is enabled only for function objects.
    // We make sure that is_small_function == false and that we can invoke it like R(Args...)
    //
    template<typename FnT,
      typename fn_t = std::decay_t<FnT>,
      typename = std::enable_if_t<!detail::is_small_function_v<fn_t> && std::is_invocable_r_v<R, fn_t&, Args...>>>
    small_function(FnT&& fn)
    {
      from_fn(std::forward<FnT>(fn));
    }

    ~small_function()
    {
      vtable_->destroyer(std::addressof(data_));
    }

    auto operator=(const small_function& rhs) -> small_function&
    {
      vtable_ = rhs.vtable_;
      vtable_->copier(std::addressof(data_), std::addressof(rhs.data_));

      return *this;
    }

    auto operator=(small_function&& rhs) noexcept -> small_function&
    {
      vtable_ = std::exchange(rhs.vtable_, std::addressof(detail::empty_vtable<R, Args...>));
      vtable_->mover(std::addressof(data_), std::addressof(rhs.data_));

      return *this;
    }

    auto operator=(std::nullptr_t) -> small_function&
    {
      vtable_->destroyer(std::addressof(data_));
      vtable_ = std::addressof(detail::empty_vtable<R, Args...>);

      return *this;
    }

    //
    // This is the same enable_if as above
    //
    template <typename FnT,
      typename fn_t = std::decay_t<FnT>,
      typename = std::enable_if_t<!detail::is_small_function_v<fn_t>&& std::is_invocable_r_v<R, fn_t&, Args...>>>
    auto operator=(FnT&& fn) -> small_function&
    {
      from_fn(std::forward<FnT>(fn));
      return *this;
    }

    template <typename FnT>
    auto operator=(std::reference_wrapper<FnT> fn) -> small_function&
    {
      from_fn(fn);
      return *this;
    }

    explicit operator bool() const noexcept
    {
      return vtable_ != std::addressof(detail::empty_vtable<R, Args...>);
    }

    auto operator()(Args... args) const -> R
    {
      return vtable_->invoker(std::addressof(data_), std::forward<Args>(args)...);
    }

  private:
    using storage_t = detail::aligned_storage<MaxSize - sizeof(vtable_t *), alignment>;

    template<typename FnT>
    auto from_fn(FnT&& fn) -> void
    {
      using fn_t = typename std::decay<FnT>::type;

      static_assert(std::is_copy_constructible<fn_t>::value,
        "clg::small_function cannot be constructed from a non-copyable type"
        );

      static_assert(sizeof(fn_t) <= sizeof(storage_t),
        "This object is too big to fit inside the clg::small_function"
        );

      static_assert(alignof(storage_t) % alignof(fn_t) == 0,
        "clg::small_function cannot be constructed from an object of this alignment"
        );

      static_assert(alignof(storage_t) >= alignof(fn_t),
        "clg::small_function does not support alignment higher the one defined in the class"
        );

      static const vtable_t vtable{detail::type_identity<fn_t>{}};

      ::new (std::addressof(data_)) fn_t(std::forward<FnT>(fn));

      vtable_ = std::addressof(vtable);
    }

    storage_t data_;
    const vtable_t* vtable_;
  };

} //clg
