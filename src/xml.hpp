#pragma once

#include "parsers.hpp"

#include "char_stream.hpp"

#include <functional>
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

#include <variant>
#include <cassert>
#include <coroutine>

namespace xml {
struct tag_open {
  std::string_view name;
};
struct tag_close {
  std::string_view name;
};
struct tag_self_close {};
struct tag_attribute {
  std::string_view key;
  std::string_view value;
};
struct tag_content {
  std::string_view content;
};

struct comment {
  std::string_view comment;
};

struct markup_open {
  std::string_view name;
};

struct processing_instruction_begin {
  std::string_view name;
};

struct processing_instruction_end {};

struct config {
  bool emit_tag_open{true};
  bool emit_tag_close{true};
  bool emit_tag_self_close{true};
  bool emit_tag_attribute{true};
  bool emit_tag_content{true};
  bool emit_processing_instruction_begin{true};
  bool emit_processing_instruction_end{true};
  bool emit_comments{false};

  enum class error_handling { stop };
  error_handling on_error{error_handling::stop};
};

namespace detail {
template <bool config::*Cond, class Container> struct pair;
template <class... Rs> struct tuple {};

template <config C, class V, class T> struct expand_event_type_impl;
template <config C, class... Vs>
struct expand_event_type_impl<C, std::variant<Vs...>, tuple<>> {
  using type = std::variant<Vs...>;
};

template <config C, bool config::*B, class T, class... Vs, class... Rest>
struct expand_event_type_impl<C, std::variant<Vs...>,
                              tuple<pair<B, T>, Rest...>> {
  using type = typename expand_event_type_impl<
      C,
      std::conditional_t<(C.*B), std::variant<Vs..., T>, std::variant<Vs...>>,
      tuple<Rest...>>::type;
};

template <config C, class T>
using expand_event_type =
    typename expand_event_type_impl<C, std::variant<>, T>::type;

template <class Self, bool B, class R> struct promise_base {
  auto yield_value(R t) noexcept { return std::suspend_never{}; }
};
template <class Self, class R> struct promise_base<Self, true, R> {
  auto yield_value(R t) noexcept {
    static_cast<Self *>(this)->current_event = t;
    static_cast<Self *>(this)->value_pending = true;
    return std::suspend_always{};
  }
};
template <class S, config C, class P> struct expand_base_impl;
template <class S, config C, bool config::*B, class R>
struct expand_base_impl<S, C, pair<B, R>> {
  using type = promise_base<S, (C.*B), R>;
};

template <class S, config C, class T> struct expand_base;
template <class S, config C, class... Ts>
struct expand_base<S, C, tuple<Ts...>> : expand_base_impl<S, C, Ts>::type... {
  using expand_base_impl<S, C, Ts>::type::yield_value...;
};

} // namespace detail

template <config c, class toAdd, class V> struct add_to_variant;

template <config Config = config{}> struct configurable_xml_parser {

  using supported_events =
      detail::tuple<detail::pair<&config::emit_tag_open, tag_open>,
                    detail::pair<&config::emit_tag_close, tag_close>,
                    detail::pair<&config::emit_tag_self_close, tag_self_close>,
                    detail::pair<&config::emit_tag_attribute, tag_attribute>,
                    detail::pair<&config::emit_processing_instruction_begin,
                                 processing_instruction_begin>,
                    detail::pair<&config::emit_processing_instruction_end,
                                 processing_instruction_end>,
                    detail::pair<&config::emit_comments, comment>,
                    detail::pair<&config::emit_tag_content, tag_content>>;

  constexpr static inline config configuration = Config;
  using event_type = detail::expand_event_type<configuration, supported_events>;

  struct promise_type;
  using handle_type = std::coroutine_handle<promise_type>;
  struct promise_type
      : detail::expand_base<promise_type, configuration, supported_events> {
    using detail::expand_base<promise_type, configuration,
                              supported_events>::yield_value;

    friend detail::expand_base<promise_type, configuration, supported_events>;
    event_type current_event;
    bool value_pending = false;

    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    configurable_xml_parser get_return_object() noexcept {
      return {handle_type::from_promise(*this)};
    }
    void unhandled_exception() noexcept { std::terminate(); }
    auto yield_value(std::same_as<configurable_xml_parser> auto &&p) noexcept {

      bool has_value = p.has_value();
      if (has_value) {
        current_event = p.event();
        value_pending = true;
        this->next_ = &p;
      }

      struct awaitable {
        bool should_keep_going;
        bool await_ready() noexcept { return should_keep_going; }
        void await_suspend(std::coroutine_handle<promise_type> awaiter) {}
        void await_resume() noexcept {}
      };
      return awaitable{.should_keep_going = !has_value};
    }

    auto await_transform(std::same_as<configurable_xml_parser> auto &&) {
      return std::suspend_always{};
    };

    void return_void() noexcept {}

    bool resume_next() noexcept {
      assert(next_ != nullptr);
      next_->resume_();
      if (next_->has_value_pending_()) {
        current_event = next_->event();
        value_pending = true;
        return true;
      }
      return false;
    }

    event_type &&get_event() noexcept {
      assert(value_pending);
      value_pending = false;
      return std::move(current_event);
    }

    configurable_xml_parser *next_{nullptr};
  };

  explicit(false) configurable_xml_parser(handle_type handle)
      : handle_{handle} {}
  configurable_xml_parser(const configurable_xml_parser &other) = delete;
  configurable_xml_parser(configurable_xml_parser &&other) noexcept
      : handle_(std::exchange(other.handle_, nullptr)) {}
  configurable_xml_parser &
  operator=(const configurable_xml_parser &other) = delete;
  configurable_xml_parser &operator=(configurable_xml_parser &&other) noexcept {
    this->handle_ = std::exchange(other.handle_, nullptr);
    return *this;
  }

  inline operator bool() noexcept { return has_value(); }

  inline bool has_value() noexcept {
    assert(handle_ != nullptr);
    return resume_();
  }

  inline event_type event() noexcept {
    assert(handle_ != nullptr);
    resume_();
    return promise().get_event();
  }

  ~configurable_xml_parser() noexcept {
    if (handle_ != nullptr) {
      handle_.destroy();
    }
  }

private:
  handle_type handle_;

  // returns has value pending
  bool resume_() {
    if (has_value_pending_()) {
      return true;
    }
    if (next() != nullptr) {
      promise().resume_next();
      if (next()->handle_.done()) {
        handle_.promise().next_ = nullptr;
        return resume_();
      }
      return has_value_pending_();
    } else {
      handle_.resume();
      return has_value_pending_();
    }
  }

  bool has_value_pending_() { return promise().value_pending; }

  auto &promise() { return handle_.promise(); }
  const auto &promise() const { return handle_.promise(); }

  const configurable_xml_parser *next() const { return promise().next_; }

  configurable_xml_parser *next() { return promise().next_; }
};
using xml_parser = configurable_xml_parser<>;

#define fail_if(cond)                                                          \
  if (cond) {                                                                  \
    co_return;                                                                 \
  }

#define advance_to(...) fail_if(!stream.seek(__VA_ARGS__))

namespace syntax {
inline bool find_opening_char(char_stream &stream) {
  return stream.seek(stream.find('<'));
}
std::optional<std::string_view> parse_to(char_stream &stream, auto &&func) {
  auto end = std::forward<decltype(func)>(func)(stream);
  if (end == std::string::npos) {
    return std::nullopt;
  }
  return stream.consume_to(end);
}


template <config Config>
configurable_xml_parser<Config> parse_tag_name(char_stream &stream) {
  auto end = xml::xml_word_end(stream);
  fail_if(end == std::string::npos);
  auto name = stream.consume_to(end);
  co_yield tag_open{name};
  co_return;
}
template <config Config>
configurable_xml_parser<Config> parse_tag_close(char_stream &stream) {
  advance_to(xml::xml_tag_head);
  auto name = parse_to(stream, xml::xml_word_end);
  fail_if(!name);
  fail_if(stream.at_eos() ||
          stream.read_char() != '>') co_yield tag_close{*name};
  co_return;
}

template <config Config>
configurable_xml_parser<Config>
parse_single_tag_attribute(char_stream &stream) {
  auto key = parse_to(stream, xml::xml_word_end);
  fail_if(!key);
  advance_to(std::not_fn(isspace));
  if (stream.peek() != '=') {
    co_yield tag_attribute{*key, ""};
    co_return;
  }
  stream.advance();
  advance_to(std::not_fn(isspace));
  fail_if(stream.peek() != '"');
  stream.advance();
  auto string_end = xml::xml_string_end(stream);
  fail_if(string_end == std::string::npos);
  auto value = stream.consume_to(string_end - 1);
  advance_to(string_end);
  co_yield tag_attribute{*key, value};
}

template <config Config>
configurable_xml_parser<Config> parse_tag_attributes(char_stream &stream) {
  while (stream) {
    advance_to(std::not_fn(isspace));
    if (stream.peek() == '>') {
      stream.advance();
      co_return;
    }
    if (stream.peek() == '/') {
      stream.advance();
      fail_if(stream.at_eos() ||
              stream.read_char() != '>') co_yield tag_self_close{};
      co_return;
    }
    co_yield parse_single_tag_attribute<Config>(stream);
  }
  co_return;
}

template <size_t S>
size_t tag_end(char_stream &stream, const char (&pattern)[S]) {
  const char *current = pattern;
  auto beg = stream.cursor();
  size_t result = std::string::npos;

  while (stream) {
    auto c = stream.read_char();

    if (c == '"' && !stream.seek(xml::xml_string_end(stream))) {
      break;
    }

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

template <size_t S>
size_t find_seq(char_stream &stream, const char (&pattern)[S]) {
  const char *current = pattern;
  auto beg = stream.cursor();

  while (stream) {
    auto c = stream.read_char();

    if (c == '"' && !stream.seek(xml::xml_string_end(stream))) {
      break;
    }

    if (c == *current) {
      current++;
      if (current == pattern + S - 1) {
        auto result = stream.cursor();
        stream.seek(beg);
        return result - S + 1;
      }
    } else {
      current = pattern;
    }
  }

  return std::string::npos;
}

template <config Config>
configurable_xml_parser<Config> parse_tag(char_stream &stream);
template <config Config>
configurable_xml_parser<Config> parse_tag_content(char_stream &stream) {
  auto cursor = stream.cursor();
  bool only_space = true;
  while (stream) {
    auto c = stream.peek();
    if (c == '<') {
      if (!only_space) {
        auto content = stream.substring(cursor, stream.cursor() - cursor);
        co_yield tag_content{content};
        only_space = true;
      }

      stream.advance();
      fail_if(stream.at_eos());
      auto n = stream.peek();

      if (n == '/') {
        co_yield parse_tag_close<Config>(stream);
        co_return;
      } else {
        stream.seek(stream.cursor() - 1);
        co_yield parse_tag<Config>(stream);
      }
    } else {
      if (!isspace(c)) {
        only_space = false;
      }
      stream.advance();
    }
  }
}
template <config Config>
configurable_xml_parser<Config>
parse_processing_instruction(char_stream &stream) {
  while (stream) {
    advance_to(std::not_fn(isspace));
    fail_if(stream.peek() == '>');
    if (stream.peek() == '?') {
      stream.advance();
      fail_if(stream.at_eos() ||
              stream.read_char() != '>') co_yield processing_instruction_end{};
      co_return;
    }
    co_yield parse_single_tag_attribute<Config>(stream);
  }
  co_return;
}

template <config Config>
configurable_xml_parser<Config> parse_tag(char_stream &stream) {
  stream.advance();
  fail_if(stream.at_eos());
  switch (stream.peek()) {
  case '?': {
    advance_to(xml::xml_tag_head);
    auto name_end = xml::xml_word_end(stream);
    fail_if(name_end == std::string::npos);
    co_yield processing_instruction_begin{stream.consume_to(name_end)};
    co_yield parse_processing_instruction<Config>(stream);
    break;
  }
  case '!': {
    stream.advance();
    fail_if(stream.at_eos());
    if (stream.peek() == '-') {
      stream.advance();
      fail_if(stream.read_char() != '-');
      auto end = find_seq(stream, "-->");
      fail_if(end == std::string::npos);
      co_yield comment{stream.consume_to(end)};
      break;
    }

    auto end = tag_end(stream, ">");
    fail_if(end == std::string::npos);
    break;
  }
  default: {
    co_yield parse_tag_name<Config>(stream);
    co_yield parse_tag_attributes<Config>(stream);
    co_yield parse_tag_content<Config>(stream);
  }
  }
  co_return;
}
}//namespace syntax

template <config Config>
configurable_xml_parser<Config> parse_xml(char_stream &stream) {
  while (stream) {
    if (!syntax::find_opening_char(stream)) {
      co_return;
    }

    co_yield syntax::parse_tag<Config>(stream);
  }
}

inline xml_parser parse_xml(char_stream &stream) {
  return parse_xml<config{}>(stream);
}

#undef fail_if
#undef advance_to
} // namespace xml
