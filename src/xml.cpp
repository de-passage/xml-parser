#include "xml.hpp"
#include "parsers.hpp"

#include <stack>

namespace xml {

#define fail_if_impl_(expr, func, line)                                        \
  if ((expr)) {                                                                \
    /* std::clog << "ERROR: " << func << ':' << line << ':' << #expr <<        \
     * '\n'; // do some actual error handling here maybe */                    \
    return std::nullopt;                                                       \
  }

#define fail_if_pp_bs_(...) fail_if_impl_(__VA_ARGS__)
#define fail_if(...) fail_if_pp_bs_((__VA_ARGS__), __FUNCTION__, __LINE__)

#define break_if(...)                                                          \
  if ((__VA_ARGS__)) {                                                         \
    break;                                                                     \
  }

#define advance_to(...) fail_if(!stream.seek(__VA_ARGS__))

std::optional<std::string_view> next_string_or_word(char_stream &stream) {
  advance_to(std::not_fn(isspace));

  char c = stream.peek();
  if (c == '"') {
    return syntax::parse_to(stream, xml_string_end);
  } else if (xml_tag_head(c)) {
    return syntax::parse_to(stream, xml_word_end);
  }
  return std::nullopt;
}

namespace tree {
template <size_t S>
size_t tag_end(char_stream &stream, const char (&pattern)[S]) {
  const char *current = pattern;
  auto beg = stream.cursor();
  size_t result = std::string::npos;

  while (stream) {
    auto c = stream.read_char();

    break_if(c == '"' && !stream.seek(xml_string_end(stream)));

    if (c == *current) {
      current++;
      if (current == pattern + S - 1) {
        result = stream.cursor() + 1;
        break;
      }
    } else {
      current = pattern;
    }
  }

  stream.seek(beg);
  return result;
}

struct opening_tag {
  std::vector<xml::attribute> attributes;
  bool self_closing;
};

std::optional<opening_tag> parse_attributes(char_stream &stream) {
  std::vector<xml::attribute> attrs;
  while (stream) {
    auto c = stream.peek();

    if (c == '/') {
      stream.advance();
      fail_if(stream.read_char() != '>');
      return {{
          .attributes = std::move(attrs),
          .self_closing = true,
      }};
    }
    if (c == '>') {
      stream.advance();
      return {{
          .attributes = std::move(attrs),
          .self_closing = false,
      }};
    }

    if (xml_tag_head(c)) {
      syntax::parse_to(stream, xml_word_end)
          .transform([&attrs](std::string_view attr_name_end) {
            attrs.emplace_back(xml::attribute{
                .name = std::string{attr_name_end},
                .value = "",
            });
            return 0;
          });
    } else if (c == '=') {
      fail_if(attrs.size() == 0);
      stream.advance();
      auto attr_value = next_string_or_word(stream);
      fail_if(!attr_value.has_value());
      attrs.back().value = std::string{attr_value.value()};
    } else {
      stream.advance();
    }
  }
  return std::nullopt;
}

std::optional<xml::tag> parse_current_tag_body(char_stream &stream, xml::tag);
std::optional<xml::tag> parse_tag(char_stream &stream) {
  return next_xml_word(stream).and_then(
      [&](std::string_view name) -> std::optional<xml::tag> {
        return parse_attributes(stream).and_then(
            [&](opening_tag attrs) -> std::optional<xml::tag> {
              xml::tag xml{
                  .name = std::string{name},
                  .attributes = std::move(attrs).attributes,
              };
              if (attrs.self_closing) {
                return std::move(xml);
              } else {
                return parse_current_tag_body(stream, std::move(xml));
              }
            });
      });
}

std::optional<xml::tag> next_tag(char_stream &stream) {

  while (stream) {
    advance_to(char_eq('<'));
    stream.advance();
    advance_to(std::not_fn(isspace));

    switch (stream.peek()) {

    case '?': {
      advance_to(tag_end(stream, "?>"));
      break;
    }

    case '!': {
      stream.advance();
      if (stream.peek() != '-') {
        fail_if(next_xml_word(stream) != "DOCTYPE");
        advance_to(tag_end(stream, ">"));
      } else {
        stream.advance();
        advance_to(tag_end(stream, "-->"));
      }
      break;
    }

    default:
      goto loop_exit;
    }
  }
loop_exit:

  return parse_tag(stream);
}

// Parse the body of a tag (content and children). The attributes must have
// already been processed
std::optional<xml::tag> parse_current_tag_body(char_stream &stream,
                                               xml::tag tag) {
  int buffered_space = 0;
  advance_to(std::not_fn(isspace));
  while (stream) {
    char c = stream.peek();
    if (c == '<') {
      stream.advance();
      advance_to(std::not_fn(isspace));

      if (stream.peek() == '/') {
        stream.advance();
        auto word = next_xml_word(stream);
        fail_if(!word || word != tag.name);
        advance_to(std::not_fn(isspace));
        fail_if(!stream || stream.read_char() != '>');
        return std::move(tag);
      } else if (stream.peek() == '!') {
        stream.advance();
        fail_if(stream.read_char() != '-');
        fail_if(stream.read_char() != '-');
        advance_to(tag_end(stream, "-->"));
      } else {
        auto xml = parse_tag(stream);
        fail_if(!xml.has_value());
        tag.children.emplace_back(std::move(xml).value());
      }
      buffered_space = 0;
    } else if (isspace(c)) {
      buffered_space++;
      stream.advance();
    } else {
      if (buffered_space != 0) {
        tag.content += ' ';
        buffered_space = 0;
      }
      tag.content += stream.read_char();
    }
  }
  return std::nullopt;
}

} // namespace tree
} // namespace xml

std::optional<xml::tag> build_xml_doc(char_stream &stream) {
  xml::tag root{};
  std::vector<xml::tag> tags;
  xml::tag *current_tag = &root;
  std::stack<xml::tag *> tag_stack;
  tag_stack.push(current_tag);

  while (stream) {
    if (auto maybe_tag = xml::tree::next_tag(stream); maybe_tag.has_value()) {
      root.children.emplace_back(std::move(maybe_tag).value());
    } else {
      break;
    }
  }
  return root;
}

namespace xml {

} // namespace xml
