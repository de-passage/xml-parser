#pragma once

#include "char_stream.hpp"
#include <optional>
#include <string>
#include <vector>

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
} // namespace xml

std::optional<xml::tag> parse_xml(char_stream &stream);
