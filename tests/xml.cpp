#include "xml.hpp"
#include <iostream>

struct indent_t {
  size_t n{0};
  char chr{' '};
  indent_t &operator++() {
    n++;
    return *this;
  }
  indent_t operator++(int) {
    indent_t old = *this;
    n++;
    return old;
  }
  indent_t &operator--() {
    n--;
    return *this;
  }
  indent_t operator--(int) {
    indent_t old = *this;
    n--;
    return old;
  }
};

std::ostream &operator<<(std::ostream &out, indent_t i) {
  for (size_t n = i.n; n > 0; --n) {
    out.put(i.chr);
  }
  return out;
}

void print_xml(xml::tag &xml, indent_t i = {.n = 0}) {
  std::cout << i << '<' << xml.name << ">:\n";
  i++;
  std::cout << i << '[';
  if (xml.attributes.size() > 0) {
    std::cout << '\n';
  } else {
    std::cout << ' ';
  }
  i++;
  for (auto &a : xml.attributes) {
    std::cout << i << a.name << '=' << a.value << std::endl;
  }
  i--;
  if (xml.attributes.size() > 0) {
    std::cout << i;
  }
  std::cout << "]," << std::endl;
  std::cout << i << "{";
  if (xml.children.size() > 0) {
    std::cout << '\n';
  } else {
    std::cout << ' ';
  }
  i++;
  for (auto &child : xml.children) {
    print_xml(child, i);
  }
  --i;
  if (xml.children.size() > 0)
    std::cout << i;
  std::cout << '}' << std::endl;
  i--;
  if (!xml.content.empty())
    std::cout << i << "$\"" << xml.content << "\"\n";
  std::cout << i << "</" << xml.name << ">" << std::endl;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: parser <FILE>" << std::endl;
    return EXIT_FAILURE;
  }

  auto f = slurp_file(argv[1]);

  auto xml = parse_xml(f);
  if (!xml) {
    return 1;
  }
  print_xml(xml.value());
}
