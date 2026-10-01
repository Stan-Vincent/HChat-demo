#pragma once
#include "const.h"

//封装config.ini的子map
struct SectionInfo {
    SectionInfo();
    ~SectionInfo();

    //复制构造
    SectionInfo(const SectionInfo& src);

    SectionInfo& operator = (const SectionInfo& src);

    std::map<std::string, std::string> _section_datas;

    std::string  operator[](const std::string& key);
};

//封装封装config.ini的主map
class ConfigMgr
{
public:
    ~ConfigMgr();

    SectionInfo operator[](const std::string& section);

    //懒汉式单例
    static ConfigMgr& Inst();


    ConfigMgr& operator=(const ConfigMgr& src) = delete;


    ConfigMgr(const ConfigMgr& src) = delete;
    
private:
    //默认构造函数(.cpp)
    ConfigMgr();

    // 存储section和key-value对的map  
    std::map<std::string, SectionInfo> _config_map;
};

