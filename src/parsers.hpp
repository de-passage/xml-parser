#pragma once

#include "char_stream.hpp"

#include <cctype>
#include <functional>
#include <optional>
#include <string_view>

inline bool word_head_character(char c) { return isalpha(c) || c == '_'; }

inline bool word_body_character(char c) {
  return isalnum(c) || c == '_' || c == '-';
}

template <std::invocable<char> WHF = bool(*)(char),
          std::invocable<char> WBF = bool(*)(char)>
  requires requires(WHF &&wh, WBF &&wb) {
    { std::forward<WHF>(wh)('\0') } -> std::convertible_to<bool>;
    { std::forward<WBF>(wb)('\0') } -> std::convertible_to<bool>;
  }
inline std::optional<std::string_view>
next_word(char_stream &stream, WHF &&word_head = word_head_character,
          WBF &&word_body = word_body_character) {
  auto start = stream.find(word_head);
  if (start == std::string::npos) {
    return std::nullopt;
  }
  auto end = stream.find(std::not_fn(std::forward<WBF>(word_body)), start);
  auto s = stream.substring(start, end - start);
  stream.seek(end);
  return s;
}

inline size_t find_string_end(char_stream &stream, char delim = '"',
                              char escape = '\\') {
  auto beg = stream.cursor();
  bool escaped = false;
  while (stream) {
    char c = stream.read_char();
    if (c == escape) {
      escaped = !escaped;
    } else if (c == delim && !escaped) {
      auto p = stream.cursor();
      stream.seek(beg);
      return p;
    } else {
      escaped = false;
    }
  }
  stream.seek(beg);
  return std::string::npos;
}

inline size_t find_string_end(char_stream &stream, size_t pos, char delim = '"',
                              char escape = '\\') {
  auto beg = stream.cursor();
  stream.seek(pos);
  auto end = find_string_end(stream, delim, escape);
  stream.seek(beg);
  return end;
}

inline std::optional<std::string_view>
next_string(char_stream &stream, char delim = '"', char escape = '\\') {
  auto beg = stream.find(delim);
  if (beg == std::string::npos) {
    return std::nullopt;
  }
  auto end = find_string_end(stream, beg + 1, delim, escape);
  if (end == std::string::npos) {
    return std::nullopt;
  }
  auto t = stream.substring(beg, end - beg);
  stream.seek(end);
  return t;
}

inline auto char_eq(auto &&right) noexcept {
  return [&](auto &&left) { return left == right; };
}
