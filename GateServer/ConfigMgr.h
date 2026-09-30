#pragma once
#include "const.h"

//封装config.ini的子map
struct SectionInfo {
    SectionInfo() {
    }
    ~SectionInfo() {
        _section_datas.clear();
    }

    //复制构造
    SectionInfo(const SectionInfo& src) {
        _section_datas = src._section_datas;
    }
    SectionInfo& operator = (const SectionInfo& src) {
        if (&src == this) {
            return *this;
        }

        this->_section_datas = src._section_datas;
    }

    std::map<std::string, std::string> _section_datas;

    std::string  operator[](const std::string& key) {
        if (_section_datas.find(key) == _section_datas.end()) {
            return "";
        }
        // 这里可以添加一些边界检查  
        return _section_datas[key];
    }
};

//封装封装config.ini的主map
class ConfigMgr
{
public:
    ~ConfigMgr() {
        _config_map.clear();
    }

    SectionInfo operator[](const std::string& section) {
        if (_config_map.find(section) == _config_map.end()) {
            return SectionInfo();
        }
        return _config_map[section];
    }


    ConfigMgr& operator=(const ConfigMgr& src) {
        if (&src == this) {
            return *this;
        }

        this->_config_map = src._config_map;
    };

    ConfigMgr(const ConfigMgr& src) {
        this->_config_map = src._config_map;
    }

    //默认构造函数(.cpp)
    ConfigMgr();
private:

    // 存储section和key-value对的map  
    std::map<std::string, SectionInfo> _config_map;
};

