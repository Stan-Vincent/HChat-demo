# -*- coding: utf-8 -*-
"""修 GateServer 的 update_userinfo 重复提交报失败的问题。

【症状】同一个用户连续提交两次完全相同的资料，第二次返回
        error=1007 "update failed"。

【根因】MysqlDao::UpdateUserInfo 用 `return updateCount > 0` 判断成败。
        但 MySQL 的 UPDATE 在【新值与旧值完全相同】时 affected rows = 0
        （除非连接开启 CLIENT_FOUND_ROWS）。
        所以「资料没改任何东西」= rows 0= 被当成失败 —— 而这恰恰是最正常的情况。

【修法】rows > 0 直接成功；rows == 0 时再 SELECT 一次确认这个 uid 存在：
        存在 -> 说明值确实没变化，算成功；不存在 -> 才算失败。

源文件是 GBK，保留 CRLF / TAB。
"""
p = r"D:\QTcode\HChat\GateServer\MysqlDao.cpp"
raw = open(p, "rb").read().decode("gbk")

# ★ 注意：这个文件的换行是 "\r\r\n"（双CR），不是常规的 "\r\n"。
#   按实际字节来写锚点，否则匹配不到。
EOL = "\r\r\n"

old = (
    "\t\tint updateCount = pstmt->executeUpdate();" + EOL +
    "\t\tstd::cout << \"UpdateUserInfo rows: \" << updateCount << std::endl;" + EOL +
    "\t\treturn updateCount > 0;" + EOL +
    "\t}" + EOL
)

new = (
    "\t\tint updateCount = pstmt->executeUpdate();" + EOL +
    "\t\tstd::cout << \"UpdateUserInfo rows: \" << updateCount << std::endl;" + EOL +
    "\t\tif (updateCount > 0) {" + EOL +
    "\t\t\treturn true;" + EOL +
    "\t\t}" + EOL +
    EOL +
    "\t\t// ★ rows == 0 有两种可能：" + EOL +
    "\t\t//   a) uid 不存在；" + EOL +
    "\t\t//   b) uid 存在，但新值和旧值【完全一样】—— MySQL 在没开" + EOL +
    "\t\t//      CLIENT_FOUND_ROWS 时就返回 0，这是标准语义，不是错误。" + EOL +
    "\t\t//   原来直接 `return updateCount > 0`，把 (b) 也判成失败，" + EOL +
    "\t\t//   于是「重复提交同一份资料」必然报错。" + EOL +
    "\t\t//   这里再 SELECT 一次区分：查得到就是 (b)，按成功处理。" + EOL +
    "\t\tstd::unique_ptr<sql::PreparedStatement> chk(con->_con->prepareStatement(" + EOL +
    "\t\t\t\"SELECT uid FROM user WHERE uid = ?\"));" + EOL +
    "\t\tchk->setInt(1, uid);" + EOL +
    "\t\tstd::unique_ptr<sql::ResultSet> rs(chk->executeQuery());" + EOL +
    "\t\tif (rs->next()) {" + EOL +
    "\t\t\tstd::cout << \"UpdateUserInfo: uid=\" << uid" + EOL +
    "\t\t\t          << \" exists, values unchanged -> success\" << std::endl;" + EOL +
    "\t\t\treturn true;" + EOL +
    "\t\t}" + EOL +
    "\t\treturn false;" + EOL +
    "\t}" + EOL
)

cnt = raw.count(old)
if cnt != 1:
    raise SystemExit("[FAIL] anchor count = %d" % cnt)

raw = raw.replace(old, new, 1)
open(p, "wb").write(raw.encode("gbk"))
print("[OK] patched MysqlDao::UpdateUserInfo")