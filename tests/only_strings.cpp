#include "parsers.hpp"
#include "char_stream.hpp"

#include <iostream>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: parser <FILE>" << std::endl;
    return EXIT_FAILURE;
  }
  auto f = slurp_file(argv[1]);

  std::optional<std::string_view> word;
  int i = 0;
  while (f && (word = next_string(f)).has_value()) {
    std::cout << ++i << '\'' << word.value() << "'\n" << std::flush;
  }
}
