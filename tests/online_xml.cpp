#include "vt100.hpp"

#include "char_stream.hpp"
#include "xml.hpp"

#include <iostream>
#include <variant>

using namespace dpsg::vt100;

using namespace xml;

struct print_xml_t {
  void operator()(tag_open t) const {
    std::cout << (bold | blue) << "Tag open: " << t.name << reset
              << std::endl;
  }

  void operator()(tag_attribute t) const {
    std::cout << (bold | cyan) << "Attribute: " << t.key << " -> " << t.value
              << reset << std::endl;
  }

  void operator()(tag_close t) const {
    std::cout << (bold | blue) << "Tag close: " << t.name << reset
              << std::endl;
  }

  void operator()(tag_self_close t) const {
    std::cout << (blue | bold) << "Tag self-closes" << reset << std::endl;
  }

  void operator()(tag_content t) const {
    std::cout << (bold | green) << "Content: " << t.content << reset
              << std::endl;
  }

  void operator()(comment t) const {
    std::cout << (faint) << "Comment: " << t.comment << reset
              << std::endl;
  }
  void operator()(processing_instruction_begin t) const {
    std::cout << (bold | reverse) << "Processing instruction begin: " << t.name << reset
              << std::endl;
  }
  void operator()(processing_instruction_end t) const {
    std::cout << (bold | reverse) << "Processing instruction end" << reset
              << std::endl;
  }
} constexpr static inline print_xml{};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "usage: parser <FILE>" << std::endl;
    return EXIT_FAILURE;
  }
  auto f = slurp_file(argv[1]);

  auto parser =
      parse_xml<config{.emit_tag_close = false,.emit_tag_content = false, .emit_comments = false, }>(
          f);
  while (parser) {
    auto ev = parser.event();
    std::visit(print_xml, ev);
  }

  return 0;
}
