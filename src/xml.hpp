#pragma once

#include "parsers.hpp"

#include "char_stream.hpp"

#include <optional>
#include <string>
#include <vector>
#include <functional>

namespace xml {
struct attribute {
  std::string name;
  std::string value;
};

struct tag {
  std::string name;
  std::vector<attribute> attributes;
  std::vector<tag> children;
  std::string content;
};

inline bool xml_tag_head(char c) { return isalpha(c) || c == '_'; }

inline bool xml_tag_body(char c) {
  return isalnum(c) || c == '_' || c == '-' || c == '.' || c == ':';
}

// Find string end when at opening quote
inline size_t xml_string_end(char_stream &stream) {
  return find_string_end(stream, stream.cursor() + 1);
}

// Find word end when at head
inline size_t xml_word_end(char_stream &stream) {
  return stream.find(std::not_fn(xml_tag_body), stream.cursor() + 1);
}

inline std::optional<std::string_view> next_xml_word(char_stream &stream) {
  return next_word(stream, xml_tag_head, xml_tag_body);
}

std::optional<std::string_view> next_string_or_word(char_stream &stream);
} // namespace xml

std::optional<xml::tag> build_xml_doc(char_stream &stream);
