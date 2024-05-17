#include "parsers.hpp"
#include <iostream>

struct xml_word_body_t {
  bool operator()(char c) const {
    return word_body_character(c) || c == '.';
  }
} constexpr static inline xml_word_body;


int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: parser <FILE>" << std::endl;
    return EXIT_FAILURE;
  }
  auto f = slurp_file(argv[1]);

  std::optional<std::string_view> word;
  int i = 0;
  while (f && (word = next_word(f, word_head_character, xml_word_body)).has_value()) {
    std::cout << ++i << '\'' << word.value() << "'\n" << std::flush;
  }
}
