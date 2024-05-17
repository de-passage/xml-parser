#pragma once

#include <unordered_map>
#include <string_view>
#include <string>

struct string_hash {
  using hash_type = std::hash<std::string_view>;
  using is_transparent = void;
  std::size_t operator()(const char *str) const noexcept {
    return hash_type{}(str);
  }
  std::size_t operator()(std::string_view str) const noexcept {
    return hash_type{}(str);
  }
  std::size_t operator()(const std::string &str) const noexcept {
    return hash_type{}(str);
  }

};
template<class T>
concept StringLike = std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> || std::is_same_v<std::remove_const_t<std::remove_pointer_t<T>>*, char*>;
template<class S, class T>
using string_map = std::unordered_map<S, T, string_hash, std::equal_to<>>;
