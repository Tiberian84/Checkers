#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
  public:
    Config()    // Конструктор по умолчанию, инициализирует конфигурацию загрузкой настроек
    {
        reload();
    }

    void reload()   // Перезагружает конфигурацию из файла settings.json, обновляя данные в объекте config
    {
        std::ifstream fin(project_path + "settings.json");
        fin >> config;
        fin.close();
    }

    auto operator()(const string &setting_dir, const string &setting_name) const    // Перегрузка оператора () для удобного доступа к значениям конфигурации по разделу и имени настройки (например, config("Bot", "IsWhiteBot"))
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;    // Объект JSON для хранения конфигурационных данных
};
