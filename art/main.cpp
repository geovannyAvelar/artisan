#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Token {};

std::vector<Token> tokenize(std::string &script);
std::string read_file(std::string &filepath);

int main(int argc, char *argv[]) {

  if (argc <= 1) {
    std::cerr << "Error: input file is absent" << std::endl;
    return 1;
  }

  std::string filepath(argv[1]);
  std::string content = read_file(filepath);

  return 0;
}

std::vector<Token> tokenize(std::string &script) {
  auto tokens = std::vector<Token>();
  std::int32_t cursor;
  auto size = script.size();

  while (cursor < size) {
    char c = script[cursor];

    cursor++;
  }

  return tokens;
}

std::string read_file(std::string &filepath) {
  if (!std::filesystem::exists(filepath)) {
    throw std::runtime_error("File do not exists");
  }

  std::ifstream file(filepath);

  if (!file.is_open()) {
    throw std::runtime_error("Error: Could not open the file.");
  }

  std::ostringstream sstr;
  sstr << file.rdbuf();

  file.close();

  return sstr.str();
}
