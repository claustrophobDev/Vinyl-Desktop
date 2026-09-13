#pragma once

#include <string>
#include <vector>

// список запущенных программ, из него выбираем кому можно в туннель а кому нельзя
namespace processes {

struct Item {
    std::string exe;     // chrome.exe, в нижнем регистре
    std::wstring title;  // как показать человеку
};

// только имена, без системного мусора, отсортировано и без повторов
std::vector<Item> running();

} // namespace processes
