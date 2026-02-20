
// Created: 2025-07-10 15:58:26

#pragma once

#define COMPLEX_INTERNAL_PARENS ()

#define COMPLEX_INTERNAL_VANCOMPLEX_INTERNAL_ISH
#define COMPLEX_INTERNAL_ESC_(...) COMPLEX_INTERNAL_VAN ## __VA_ARGS__
#define COMPLEX_INTERNAL_ESC(...) COMPLEX_INTERNAL_ESC_(__VA_ARGS__)
#define COMPLEX_INTERNAL_ISH(...) COMPLEX_INTERNAL_ISH __VA_ARGS__
#define COMPLEX_DEPAREN(...) COMPLEX_INTERNAL_ESC(COMPLEX_INTERNAL_ISH __VA_ARGS__)

#define COMPLEX_IGNORE(...)
#define COMPLEX_DEFAULT_OR(def, ...) COMPLEX_DEPAREN(__VA_OPT__(COMPLEX_IGNORE) (def)) __VA_ARGS__
#define COMPLEX_PAREN(...) (__VA_ARGS__)
#define COMPLEX_IDENTITY(...) __VA_ARGS__

#define COMPLEX_INTERNAL_REPEAT1(...) __VA_ARGS__
#define COMPLEX_INTERNAL_REPEAT2(...) COMPLEX_INTERNAL_REPEAT1(COMPLEX_INTERNAL_REPEAT1(__VA_ARGS__))
#define COMPLEX_INTERNAL_REPEAT3(...) COMPLEX_INTERNAL_REPEAT2(COMPLEX_INTERNAL_REPEAT2(__VA_ARGS__))
#define COMPLEX_REPEAT(...)           COMPLEX_INTERNAL_REPEAT3(COMPLEX_INTERNAL_REPEAT3(__VA_ARGS__))

#define COMPLEX_FOR_EACH(helper, macro, parameters, ...) __VA_OPT__(COMPLEX_REPEAT(helper COMPLEX_PAREN(macro, COMPLEX_DEPAREN(parameters), __VA_ARGS__)))

#define COMPLEX_INTERNAL_ITERATE_EXCLUSIVE(macro, prefix, suffix, a1, ...)                                                   \
    COMPLEX_DEPAREN(prefix) macro COMPLEX_PAREN(COMPLEX_DEPAREN(a1)) __VA_OPT__(COMPLEX_DEPAREN(suffix))              \
    __VA_OPT__(COMPLEX_INTERNAL_ITERATE_EXCLUSIVE_AGAIN COMPLEX_INTERNAL_PARENS (macro, prefix, suffix, __VA_ARGS__))
#define COMPLEX_INTERNAL_ITERATE_EXCLUSIVE_AGAIN() COMPLEX_INTERNAL_ITERATE_EXCLUSIVE

#define COMPLEX_INTERNAL_ITERATE(macro, prefix, suffix, a1, ...)                                                   \
    COMPLEX_DEPAREN(prefix) macro COMPLEX_PAREN(COMPLEX_DEPAREN(a1)) COMPLEX_DEPAREN(suffix)                \
    __VA_OPT__(COMPLEX_INTERNAL_ITERATE_AGAIN COMPLEX_INTERNAL_PARENS (macro, prefix, suffix, __VA_ARGS__))
#define COMPLEX_INTERNAL_ITERATE_AGAIN() COMPLEX_INTERNAL_ITERATE

#if defined(__GNUC__)
#define COMPLEX_DEFER(begin, end) _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-value\"") _Pragma("GCC diagnostic ignored \"-Wshadow\"") for(int _i___ = ((begin), 0); !_i___; (end), (_i___ += 1)) _Pragma("GCC diagnostic pop")
#else
#define COMPLEX_DEFER(begin, end) _Pragma("warning(suppress : 4456)") for(int _i___ = ((begin), 0); !_i___; (end), (_i___ += 1))
#endif

#define COMPLEX_INTERNAL_GET_1(x, ...) x
#define COMPLEX_INTERNAL_GET_2(x, y, ...) y
#define COMPLEX_INTERNAL_GET_3(x, y, z, ...) z

#define DEPENDENT_REQUIRES(type, ...) []<typename T = type>() { return requires __VA_ARGS__; }()

#define COMPLEX_FOREACH_AND_REVERSE_SLL(iteratorName, begin, ... /*nextName*/) \
  for (decltype(begin) iteratorName = begin, nextList__ = nullptr, temp__ = nullptr; iteratorName; \
    (temp__ = nextList__), (nextList__ = iteratorName), \
    (iteratorName = iteratorName->COMPLEX_DEFAULT_OR(next, __VA_ARGS__)), \
    (nextList__->COMPLEX_DEFAULT_OR(next, __VA_ARGS__) = temp__))

#define COMPLEX_DEFINE_ENUM_OPERATION(T, operation, underlying) \
  constexpr underlying operator operation (T a, auto b) { return (underlying)(a) operation (underlying)(b); }\
  constexpr T &operator operation##=(T &a, auto b) { a = (T)((underlying)(a) operation (underlying)(b)); return a; }

// enabling portable out of order designated initialisers
#define with(T, ...)\
  ([&]{ T temp________{}; COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_IDENTITY, (temp________, ;), __VA_ARGS__); return temp________; }())
#define with_val(object, ...) \
  ([&](auto &temp________) -> decltype(auto) { COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_IDENTITY, (temp________, ;), __VA_ARGS__); return temp________; }(object))

#define test_enum(variable, flag) (((variable) & (flag)) == (flag))
