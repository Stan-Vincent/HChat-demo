# -*- coding: utf-8 -*-
"""验证服务端新加的"批量查群成员"SQL 逻辑（不等 VS 重编，先拿真库验一遍）。"""
import sys
sys.path.insert(0, 'test')
import dbconf

def rows(sql):
    out = dbconf.query(sql)
    if not out:
        return []
    # Windows 下 mysql.exe 输出 CRLF，必须去掉行尾 \r，否则值会带上回车
    return [[c.strip() for c in l.rstrip('\r').split('\t')] for l in out.split('\n')]

uid = dbconf.pick_uid(default=2)
groups = rows(
    "SELECT m.thread_id, IFNULL(g.name,'') AS name "
    "FROM group_chat_member m "
    "LEFT JOIN group_chat g ON g.thread_id = m.thread_id "
    "WHERE m.user_id = %s ORDER BY m.thread_id;" % uid)

print('uid=%s 所在群: %d 个' % (uid, len(groups)))
ok = True
all_ids = []
for g in groups:
    tid, name = g[0], g[1]
    members = [r[0] for r in rows(
        "SELECT user_id FROM group_chat_member WHERE thread_id = %s;" % tid)]
    print('  thread %-5s name=%-20s members=%s' % (tid, name, members))
    if str(uid) not in members:
        print('    !! 自己不在成员里，GetUserThreads 会漏这个群'); ok = False
    all_ids.append(tid)

if all_ids:
    in_sql = ("SELECT thread_id, user_id FROM group_chat_member "
              "WHERE thread_id IN (%s) ORDER BY thread_id, user_id;"
              % ','.join(all_ids))
    batch = rows(in_sql)
    print('批量 IN 查询（服务端实际写法）返回 %d 行，覆盖 %d 个群'
          % (len(batch), len(all_ids)))
    # 逐群查一遍对比，确认批量没漏
    one = []
    for t in all_ids:
        one += [[t, r[0]] for r in rows(
            "SELECT user_id FROM group_chat_member WHERE thread_id = %s;" % t)]
    if len(batch) != len(one):
        print('    !! 批量与逐条不一致: %d vs %d' % (len(batch), len(one))); ok = False
    else:
        print('批量与逐条结果一致')

print('OK：SQL 逻辑验证通过' if ok else 'FAIL')
sys.exit(0 if ok else 1)
