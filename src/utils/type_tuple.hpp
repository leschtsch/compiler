#pragma once

namespace utils {

template <typename... Ts>
struct TypeTuple {};

namespace details {

template <typename T>
constexpr bool kIsTypeTuple = false;

template <typename... Ts>
constexpr bool kIsTypeTuple<TypeTuple<Ts...>> = true;

}  // namespace details

template <typename T>
concept IsTypeTuple = details::kIsTypeTuple<T>;

namespace details {

template <IsTypeTuple T, IsTypeTuple U>
struct Concat2Helper;

template <typename... Ts, typename... Us>
struct Concat2Helper<TypeTuple<Ts...>, TypeTuple<Us...>> {
  using T = TypeTuple<Ts..., Us...>;
};

template <typename... Ts>
struct ConcatHelper;

template <>
struct ConcatHelper<> {
  using T = TypeTuple<>;
};

template <IsTypeTuple TT>
struct ConcatHelper<TT> {
  using T = TT;
};

template <IsTypeTuple Head, IsTypeTuple... Tail>
struct ConcatHelper<Head, Tail...> {
  using T = typename Concat2Helper<Head, typename ConcatHelper<Tail...>::T>::T;
};

template <typename... Ts>
struct ConcatHelper<TypeTuple<Ts...>> {
  using T = TypeTuple<Ts...>;
};

template <typename T, template <typename...> typename C>
struct ConvertToHelper;

template <typename... Ts, template <typename...> typename C>
struct ConvertToHelper<TypeTuple<Ts...>, C> {
  using T = C<Ts...>;
};

}  // namespace details

template <IsTypeTuple... TTs>
using Concat = typename details::ConcatHelper<TTs...>::T;

template <IsTypeTuple T, template <typename...> typename C>
using ConvertTo = typename details::ConvertToHelper<T, C>::T;

}  // namespace utils
