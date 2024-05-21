#include "vt100.hpp"

#include "char_stream.hpp"

#include <cassert>
#include <coroutine>
#include <iostream>
#include <variant>

struct tag_open {
  std::string_view name;
};
struct tag_close {
  std::string_view name;
  bool self_closing{false};
};
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

struct markup_open {};
struct header_open {};
struct header_close {};

struct config {
  bool emit_tag_open{true};
  bool emit_tag_close{true};
  bool emit_tag_attribute{true};
  bool emit_tag_content{true};
  bool emit_comments{false};

  enum class error_handling { stop };
  error_handling on_error{error_handling::stop};
};

namespace detail {
template <bool config::*Cond, class Container> struct pair;

template <config C, class V, class...> struct expand;
template <config C, class... Vs> struct expand<C, std::variant<Vs...>> {
  using type = std::variant<Vs...>;
};

template <config C, bool config::*B, class T, class... Vs, class... Rest>
struct expand<C, std::variant<Vs...>, pair<B, T>, Rest...> {
  using type = typename expand<
      C,
      std::conditional_t<(C.*B), std::variant<Vs..., T>, std::variant<Vs...>>,
      Rest...>::type;
};

template <config C, class... Vals>
  requires(sizeof...(Vals) > 0)
using expand_t = typename expand<C, std::variant<>, Vals...>::type;
} // namespace detail

template <config c, class toAdd, class V> struct add_to_variant;

template <config Config = config{}> struct configurable_xml_parser {
  constexpr static inline config configuration = Config;
  using event_type =
      detail::expand_t<configuration,
                       detail::pair<&config::emit_tag_open, tag_open>,
                       detail::pair<&config::emit_tag_close, tag_close>,
                       detail::pair<&config::emit_tag_attribute, tag_attribute>,
                       detail::pair<&config::emit_comments, comment>,
                       detail::pair<&config::emit_tag_content, tag_content>>;

  struct promise_type;
  using handle_type = std::coroutine_handle<promise_type>;
  struct promise_type {
    event_type current_event;

    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    configurable_xml_parser get_return_object() noexcept {
      return {handle_type::from_promise(*this)};
    }
    void unhandled_exception() noexcept { std::terminate(); }

    auto yield_value(tag_open t) noexcept {
      if constexpr (configuration.emit_tag_open) {
        current_event = t;
        return std::suspend_always{};
      } else {
        return std::suspend_never{};
      }
    }

    auto yield_value(comment t) noexcept {
      if constexpr (configuration.emit_comments) {
        current_event = t;
        return std::suspend_always{};
      } else {
        return std::suspend_never{};
      }
    }

    auto yield_value(tag_close t) noexcept {
      if constexpr (configuration.emit_tag_close) {
        current_event = t;
        return std::suspend_always{};
      } else {
        return std::suspend_never{};
      }
    }

    auto yield_value(tag_attribute t) noexcept {
      if constexpr (configuration.emit_tag_attribute) {
        current_event = t;
        return std::suspend_always{};
      } else {
        return std::suspend_never{};
      }
    }

    auto yield_value(tag_content t) noexcept {
      if constexpr (configuration.emit_tag_content) {
        current_event = t;
        return std::suspend_always{};
      } else {
        return std::suspend_never{};
      }
    }

    auto yield_value(std::same_as<configurable_xml_parser> auto &&p) noexcept {
      if (p && !p.done()) {
        current_event = p.handle_.promise().current_event;
        this->next_ = &p;
      }
      struct awaitable {
        bool should_keep_going;
        // inner promise
        std::coroutine_handle<promise_type> to_resume{nullptr};
        bool await_ready() noexcept { return should_keep_going; }
        void await_suspend(std::coroutine_handle<promise_type> awaiter) {
          to_resume = awaiter;
        }
        void await_resume() noexcept {
          if (to_resume != nullptr) {
            to_resume.promise().next_ = nullptr;
          }
        }
      };
      return awaitable{.should_keep_going = p.done()};
    }

    auto await_transform(std::same_as<configurable_xml_parser> auto &&) {
      return std::suspend_always{};
    };

    void return_void() noexcept {}

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

  inline operator bool() const noexcept { return !done(); }

  inline bool done() const noexcept {
    assert(handle_ != nullptr);
    return handle_.done();
  }

  inline event_type next_event() const noexcept {
    assert(handle_ != nullptr);
    assert(!handle_.done());
    return resume_();
  }

  ~configurable_xml_parser() noexcept {
    if (handle_ != nullptr) {
      handle_.destroy();
    }
  }

private:
  handle_type handle_;

  event_type resume_() const {
    if (handle_.promise().next_ != nullptr &&
        !handle_.promise().next_->done()) {
      return handle_.promise().next_->next_event();
    } else {
      auto event = handle_.promise().current_event;
      handle_.resume();
      return event;
    }
  }
};

using xml_parser = configurable_xml_parser<>;

bool find_opening_char(char_stream &stream) {
  return stream.seek(stream.find('<'));
}

template <config Config>
configurable_xml_parser<Config> parse_tag_name(char_stream &stream) {
  co_return;
}

template <config Config>
configurable_xml_parser<Config> parse_tag(char_stream &stream) {
  stream.advance();
  switch (stream.peek()) {
  case '/':
    break;
  default: {
    co_yield parse_tag_name<Config>(stream);
  }
  }
  co_return;
}

template <config Config>
configurable_xml_parser<Config> coro_parse_xml(char_stream &stream) {
  while (stream) {
    if (!find_opening_char(stream)) {
      std::cout << "no opening char" << std::endl;
      co_return;
    }

    co_yield parse_tag<Config>(stream);
  }
}

xml_parser coro_parse_xml(char_stream &stream) {
  return coro_parse_xml<config{}>(stream);
}

struct print_xml_t {
  void operator()(tag_open t) const {
    std::cout << "Tag open: " << t.name << std::endl;
  }
  void operator()(tag_attribute t) const {
    std::cout << "Attribute: " << t.key << " -> "
              << "value" << std::endl;
  }

  void operator()(tag_close t) const {
    std::cout << "Tag close: " << t.name << " (self closing? " << std::boolalpha
              << t.self_closing << ") " << std::endl;
  }
  void operator()(tag_content t) const {
    std::cout << "Content: " << t.content << std::endl;
  }
  void operator()(comment t) const {
    std::cout << "Comment: " << t.comment << std::endl;
  }
} constexpr static inline print_xml{};

template <config Config>
configurable_xml_parser<Config> parse_fake_tag(std::string str) {

  co_yield tag_open{str};
  co_yield tag_attribute{"attr1", "val1"};
  co_yield [&]() -> configurable_xml_parser<Config> {
    co_yield tag_open{str + ":nested"};
    co_yield tag_attribute{"attr2", "val2"};
    co_yield [&]() -> configurable_xml_parser<Config> {
      co_yield tag_open{str + ":nested:nested"};
      co_yield [&]() -> configurable_xml_parser<Config> { co_return; }();
      co_yield tag_close{str + ":nested:nested"};
    }();
    co_yield tag_close{str + ":nested"};
  }();
  co_yield tag_close{str};
}

template <config Config>
configurable_xml_parser<Config> parse_fake_xml(char_stream &stream) {
  co_yield parse_fake_tag<Config>("fake1");
  co_yield comment{"comment"};
  co_yield parse_fake_tag<Config>("fake2");
}
xml_parser parse_fake_xml(char_stream &stream) {
  return parse_fake_xml<config{}>(stream);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "usage: parser <FILE>" << std::endl;
    return EXIT_FAILURE;
  }
  auto f = slurp_file(argv[1]);

  auto parser = coro_parse_xml(f);
  while (parser) {
    auto ev = parser.next_event();
    std::visit(print_xml, ev);
  }

  return 0;
}
