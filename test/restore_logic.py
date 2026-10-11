# -*- coding: utf-8 -*-
"""恢复被脚本 bug 清空的 GateServer/LogicSystem.cpp（从 git HEAD 的 std::cout 版本
一步到位生成中文日志）。读用 utf-8（HEAD 版本是 UTF-8 无 BOM），写统一 GBK。"""
import re

txt = open('GateServer/LogicSystem.cpp', 'rb').read().decode('utf-8')

# 加 logger.h include
txt = txt.replace('#include "LogicSystem.h"\n',
                  '#include "LogicSystem.h"\n#include "logger.h"\n', 1)

# 删 3 处 receive body（敏感信息，之前审计已删）
txt = re.sub(r'[ \t]*std::cout << "receive body is " << body_str << std::endl;\n', '', txt)

# 单行 std::cout -> 中文 LOG（缩进保留）
repls = [
    ('std::cout << "Failed to parse JSON data!" << std::endl;', 'LOG_ERROR("JSON 解析失败");'),
    ('std::cout << "email is " << email << std::endl;', 'LOG_INFO("邮箱: {}", email);'),
    ('std::cout << "password err " << std::endl;', 'LOG_ERROR("密码错误");'),
    ('std::cout << " get varify code expired" << std::endl;', 'LOG_ERROR("验证码已过期");'),
    ('std::cout << " varify code error" << std::endl;', 'LOG_ERROR("验证码错误");'),
    ('std::cout << " reg user failed, mysql error" << std::endl;', 'LOG_ERROR("注册失败：数据库错误");'),
    ('std::cout << " user or email exist" << std::endl;', 'LOG_INFO("用户名或邮箱已存在");'),
    ('std::cout << " register success, uid = " << uid << std::endl;', 'LOG_INFO("注册成功，uid: {}", uid);'),
    ('std::cout << "reset_pwd request, user = " << name << ", email = " << email << std::endl;',
     'LOG_INFO("重置密码请求: 用户名={}, 邮箱={}", name, email);'),
    ('std::cout << " user email not match" << std::endl;', 'LOG_ERROR("邮箱不匹配");'),
    ('std::cout << " update pwd failed" << std::endl;', 'LOG_ERROR("更新密码失败");'),
    ('std::cout << "succeed to update password, user = " << name << std::endl;', 'LOG_INFO("更新密码成功，用户名: {}", name);'),
    ('std::cout << " user pwd not match" << std::endl;', 'LOG_INFO("密码不匹配");'),
    ('std::cout << " grpc get chat server failed, error is " << reply.error() << std::endl;',
     'LOG_ERROR("gRPC 获取聊天服务器失败: {}", reply.error());'),
    ('std::cout << "succeed to load userinfo uid is " << userInfo.uid << std::endl;',
     'LOG_INFO("加载用户信息成功，uid: {}", userInfo.uid);'),
    ('std::cout << "update_userinfo ok, uid=" << uid << std::endl;', 'LOG_INFO("更新用户信息成功，uid: {}", uid);'),
]
for idx, (old, new) in enumerate(repls):
    txt = txt.replace(old, new)
    if '\ufffd' in new:
        print('repls[%d] new contains U+FFFD' % idx)

# upload_image 多行 std::cout -> 单行中文 LOG
txt = re.sub(r'std::cout << "upload_image ok: " << unique_name\n[ \t]*<< " size=" << raw\.size\(\) << std::endl;',
             'LOG_INFO("上传图片成功: 文件名={}, 大小={}", unique_name, raw.size());', txt)

# 诊断：定位 \ufffd
import re as _re
_positions = [m.start() for m in _re.finditer('\ufffd', txt)]
print('U+FFFD count:', len(_positions))

open('GateServer/LogicSystem.cpp', 'wb').write(txt.encode('gbk', errors='replace'))
print('remaining std::cout:', txt.count('std::cout'))
print('LOG_ count:', txt.count('LOG_'))
print('receive body count:', txt.count('receive body'))
print('replacement char:', txt.count('\ufffd'))
