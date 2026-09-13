#pragma once

#include <string>

// простой лог в data/vinyl.log рядом с exe, чтобы потом было понятно что пошло не так
namespace applog {

void open();
void line(const std::string& text);
void close();

} // namespace applog
