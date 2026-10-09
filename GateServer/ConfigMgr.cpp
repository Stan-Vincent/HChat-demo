#include "ConfigMgr.h"

ConfigMgr::~ConfigMgr()
{
    _config_map.clear();
}

//C++11 之后，static 局部变量初始化是线程安全的，多个线程同时调用 Inst() 也只会构造一次
ConfigMgr& ConfigMgr::Inst()
{
    static ConfigMgr cfg_mgr;
    return cfg_mgr;
}

SectionInfo ConfigMgr::operator[](const std::string& section) {
    if (_config_map.find(section) == _config_map.end()) {
        return SectionInfo();
    }
    return _config_map[section];
}

ConfigMgr::ConfigMgr() {

    // 按顺序尝试多个位置：当前工作目录 -> 上两级（x64/Debug -> 工程目录） -> 上一级
    // 这样无论从工程目录运行（VS 的默认工作目录）还是从 x64/Debug 直接运行都能找到配置
    boost::filesystem::path base = boost::filesystem::current_path();
    boost::filesystem::path candidates[3] = {
        base / "config.ini",
        base / ".." / ".." / "config.ini",
        base / ".." / "config.ini"
    };
    
    boost::filesystem::path config_path;
    boost::property_tree::ptree pt;
    bool b_loaded = false;
    for (auto& candidate : candidates) {
        if (!boost::filesystem::exists(candidate)) {
            continue;
        }
        try {
            boost::property_tree::read_ini(candidate.string(), pt);
            config_path = candidate;
            b_loaded = true;
            break;
        } catch (const std::exception& parse_err) {
            std::cerr << "Failed to parse " << candidate << ": " << parse_err.what() << std::endl;
        }
    }
    
    if (!b_loaded) {
        std::cerr << "FATAL: config.ini not found. Tried these paths:" << std::endl;
        for (auto& candidate : candidates) {
            std::cerr << "    " << candidate << std::endl;
        }
        throw std::runtime_error("config.ini not found");
    }
    
    std::cout << "Config path: " << config_path << std::endl;


    // 遍历INI文件中的所有section  
    for (const auto& section_pair : pt) {
        const std::string& section_name = section_pair.first;
        const boost::property_tree::ptree& section_tree = section_pair.second;

        // 对于每个section，遍历其所有的key-value对  
        std::map<std::string, std::string> section_config;
        for (const auto& key_value_pair : section_tree) {
            const std::string& key = key_value_pair.first;
            const std::string& value = key_value_pair.second.get_value<std::string>();
            section_config[key] = value;
        }
        SectionInfo sectionInfo;
        sectionInfo._section_datas = section_config;
        // 将section的key-value对保存到config_map中  
        _config_map[section_name] = sectionInfo;
    }

    // 输出所有的section和key-value对  
    for (const auto& section_entry : _config_map) {
        const std::string& section_name = section_entry.first;
        SectionInfo section_config = section_entry.second;
        std::cout << "[" << section_name << "]" << std::endl;
        for (const auto& key_value_pair : section_config._section_datas) {
            std::cout << key_value_pair.first << "=" << key_value_pair.second << std::endl;
        }
    }
    

}

SectionInfo::SectionInfo()
{
}

SectionInfo::~SectionInfo()
{
    _section_datas.clear();
}

SectionInfo::SectionInfo(const SectionInfo& src)
{
    _section_datas = src._section_datas;
}

SectionInfo& SectionInfo::operator=(const SectionInfo& src)
{
    if (&src == this) {
        return *this;
    }

    this->_section_datas = src._section_datas;
    return *this;
}

std::string  SectionInfo::operator[](const std::string& key) {
    if (_section_datas.find(key) == _section_datas.end()) {
        return "";
    }
    // 这里可以添加一些边界检查  
    return _section_datas[key];
}


