#include "char_stream.hpp"
#include <iostream>


int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: parser <FILE>" << std::endl;
    return EXIT_FAILURE;
  }

  auto f = slurp_file(argv[1]);

  while (f) {
    std::cout << "↣" << f.read_char();
  }

  std::cout << std::endl;

  f.read_from(0);

  while (f) {
    auto line = f.readline();
    if (line.back() == '\n') {
     std::cout << line.substr(0, line.size() - 1) << "↩\n";
    } else {
      std::cout << line << "↯";
    }
  }

  std::cout << std::endl;
}

