#include "char_stream.hpp"

char_stream slurp_file(const char *filename) {
  constexpr auto BUFFER_SIZE = 1024;

  FILE *file = fopen(filename, "r");
  if (file == nullptr) {
    co_return false;
  }

  DEFER { fclose(file); };

  char buf[BUFFER_SIZE];
  size_t nread;
  while ((nread = fread(buf, 1, sizeof(buf), file))) {
    if (ferror(file)) {
      co_return false;
    }

    co_yield std::string_view{buf, nread};

    if (nread < BUFFER_SIZE) {
      co_return true;
    }
  }

  co_return true;
}
