# -*- coding: utf-8 -*-
"""端到端验证 GateServer 的三个新接口：上传图片 / 下载图片 / 更新资料。
顺带验证路径穿越防护。只用标准库，不需要requests。"""
import json
import base64
import os
import sys
import urllib.request
from urllib.parse import quote

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dbconf   # MySQL 连接信息统一从 GateServer/config.ini 读

BASE = "http://127.0.0.1:8080"
PASS, FAIL = 0, 0

# 上传落到 GateServer/upload 下，测完删掉，别在仓库里堆垃圾
SERVER_FILES = []


def report(name, ok, detail=""):
    global PASS, FAIL
    if ok:
        PASS += 1
        print("  [PASS] %s %s" % (name, detail))
    else:
        FAIL += 1
        print("  [FAIL] %s %s" % (name, detail))


def post_json(path, obj):
    data = json.dumps(obj).encode("utf-8")
    req = urllib.request.Request(BASE + path, data=data,
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()
    except Exception as e:
        return -1, str(e).encode()


def post_raw(path, data, ctype="application/octet-stream", timeout=30):
    """发 body 为【原始字节】的 POST —— 2026-10-17 起 /upload_* 只认这个协议。

    老协议是 JSON 里塞 base64 的 data 字段，已被服务端明确拒绝
    （error=1001, msg="expected a raw binary body, not application/json"）。
    服务端只挡 Content-Type 里含 json 的，其它类型都放行。"""
    req = urllib.request.Request(BASE + path, data=data,
                                 headers={"Content-Type": ctype})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()
    except Exception as e:
        return -1, str(e).encode()


def get_bytes(path):
    try:
        with urllib.request.urlopen(BASE + path, timeout=10) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()
    except Exception as e:
        return -1, str(e).encode()


# ★ 用库里真实存在的 uid。
#   注意：库里第一个用户是 uid=2（不是 1），写死 uid=1 会让
#   UPDATE 匹配不到行 -> rows 0 -> "update failed"，那是测试数据错不是接口 bug。
#   这里直接问 MySQL 要一个真实 uid，避免以后数据变了又踩。
def pick_uid():
    """问库要一个真实 uid。

    ★ 口令绝不能写死：这里原来写死 -p123456（真实口令见 dbconf / config.ini），
      连不上库时**不报错**，只是静默退回 uid=2 —— 测试照样"通过"，
      但验的是不是你想的那个人、退出时的资料还原有没有生效，全都是问号。
      现在统一走 dbconf。"""
    v = dbconf.query("SELECT uid FROM user ORDER BY id LIMIT 1;")
    if v.splitlines() and v.splitlines()[0].strip().isdigit():
        return int(v.splitlines()[0].strip())
    print("  (查库失败，退回 uid=2 —— 检查 dbconf.MYSQL_EXE 与 config.ini [Mysql])")
    return 2

UID = pick_uid()
print("  (使用 uid=%d)" % UID)

# ---- 先把原始资料读下来，测完好还原 ----
def mysql_query(sql):
    return dbconf.query(sql)

RESTORE = None
try:
    row = mysql_query("SELECT nick, `desc`, sex, IFNULL(icon,'') FROM user WHERE uid=%d;" % UID)
    parts = row.split("\t")
    if len(parts) == 4:
        RESTORE = {"uid": UID, "nick": parts[0], "desc": parts[1],
                   "sex": int(parts[2] or 0), "icon": parts[3]}
        print("  (已备份原资料: nick=%r sex=%s)" % (RESTORE["nick"], RESTORE["sex"]))
    else:
        print("  (备份原资料失败，测试后请手动检查 uid=%d)" % UID)
except Exception as e:
    print("  (备份原资料异常 %s)" % e)

# ---- 1x. 上传（流式原始字节协议）----
# 造一个最小的合法 PNG（1x1 像素）
PNG_1PX = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmM"
    "IQAAAABJRU5ErkJggg==")


def upload(name, data, ep="/upload_image"):
    """按新协议上传：query 带 uid/file_name，body 就是文件原始字节。"""
    st, body = post_raw("%s?uid=%d&file_name=%s" % (ep, UID, quote(name)), data)
    obj = json.loads(body) if st == 200 else {}
    if obj.get("url"):
        SERVER_FILES.append(obj["url"])
    return st, obj


st, obj = upload("e2e_test.png", PNG_1PX)
report("upload_image 返回 200", st == 200, "status=%d" % st)
report("upload_image error==0", obj.get("error") == 0, str(obj.get("msg", "")))
url = obj.get("url", "")
report("upload_image 返回 url", bool(url), url)
report("上传的 .png 扩展名保留", url.endswith(".png"), url)

# ---- 2x. 下载刚上传的 ----
if url:
    # ★ 服务端返回的 url 没有前导斜杠（"upload/xxx.png"），
    #   客户端 ImageManager::toFullUrl 会补"/"。这里模拟客户端行为。
    dl_path = url if url.startswith("/") else "/" + url
    st, body = get_bytes(dl_path)
    report("下载图片 status==200", st == 200, "status=%d len=%d" % (st, len(body)))
    report("下载内容是同一张PNG", body == PNG_1PX,
           "expect=%d got=%d" % (len(PNG_1PX), len(body)))

    # 重复下载应命中磁盘、结果一致（客户端有缓存，这里只验服务端幂等）
    st2, body2 = get_bytes(dl_path)
    report("重复下载结果一致", st2 == 200 and body2 == PNG_1PX)

# ---- 3x. 更新资料 ----
st, body = post_json("/update_userinfo",
                     {"uid": UID, "nick": "e2e_nick_test", "desc": "e2e_desc",
                      "sex": 2, "icon": url})
obj = json.loads(body) if st == 200 else {}
report("update_userinfo 200", st == 200, "status=%d" % st)
report("update_userinfo error==0", obj.get("error") == 0, str(obj.get("msg", "")))
report("回包带回 nick", obj.get("nick") == "e2e_nick_test", str(obj.get("nick")))

# ---- 4x. 幂等：再提交一次同样内容，应该还是成功 ----
st, body = post_json("/update_userinfo",
                     {"uid": UID, "nick": "e2e_nick_test", "desc": "e2e_desc",
                      "sex": 2, "icon": url})
obj = json.loads(body) if st == 200 else {}
report("update_userinfo 幂等重试", st == 200 and obj.get("error") == 0)

# ---- 4b. 超长字段要能被服务端截断，而不是报错 ----
st, body = post_json("/update_userinfo",
                     {"uid": UID, "nick": "N" * 100, "desc": "D" * 500,
                      "sex": 1, "icon": url})
obj = json.loads(body) if st == 200 else {}
report("超长 nick 被截断到 21", obj.get("error") == 0 and len(obj.get("nick", "")) == 21,
       "len=%d" % len(obj.get("nick", "")))
report("超长 desc 被截断到 200", len(obj.get("desc", "")) == 200,
       "len=%d" % len(obj.get("desc", "")))

# ---- 5x. 非法 uid 要被拒 ----
st, body = post_json("/update_userinfo",
                     {"uid": 0, "nick": "x", "desc": "", "sex": 0, "icon": ""})
obj = json.loads(body) if st == 200 else {}
report("uid=0 被拒绝", obj.get("error") != 0, "error=%s" % obj.get("error"))

# ---- 6x. 空文件要报错，不能崩 ----
st, obj = upload("x.png", b"")
report("空文件返回错误而非崩溃", obj.get("error") != 0,
       "error=%s msg=%s" % (obj.get("error"), obj.get("msg")))

# ---- 6b. 可执行文件扩展名必须被中和成 .bin ----
# 否则 GET /upload/xxx.exe 能直接把可运行文件喂回去，是个真漏洞。
st, obj = upload("evil.exe", b"MZ\x90\x00", ep="/upload_file")
u = obj.get("url", "")
report("exe 扩展名被中和成 .bin", obj.get("error") == 0 and u.endswith(".bin"),
       u or str(obj.get("msg")))

# ---- 6c2. 老协议（base64 塞 JSON）必须被明确拒绝，不能被当成文件内容写盘 ----
st, body = post_json("/upload_image",
                     {"uid": UID, "file_name": "old.png",
                      "data": base64.b64encode(PNG_1PX).decode()})
obj = json.loads(body) if st == 200 else {}
report("老 base64/JSON 协议被拒", obj.get("error") != 0,
       "error=%s msg=%s" % (obj.get("error"), obj.get("msg")))

# ---- 6c. 非法 JSON 不能崩 ----
req = urllib.request.Request(BASE + "/upload_image", data=b"{not json",
                             headers={"Content-Type": "application/json"})
try:
    with urllib.request.urlopen(req, timeout=10) as r:
        obj = json.loads(r.read())
    report("非法 JSON 返回错误而非崩溃", obj.get("error") != 0)
except Exception as e:
    report("非法 JSON 返回错误而非崩溃", False, str(e))

# ---- 7x. 路径穿越防护 ----
for evil in ["/upload/../../config.ini", "/upload/../config.ini",
             "/upload/..%2f..%2fconfig.ini", "/upload/"]:
    st, body = get_bytes(evil)
    leaked = b"Passwd" in body or b"Schema" in body
    report("路径穿越被挡住: %s" % evil, not leaked,
           "status=%d len=%d" % (st, len(body)))

print("\n  ----")
print("  e2e: %d 通过, %d 失败" % (PASS, FAIL))

# 测试把 uid 的资料改成了 e2e_* ，退出前还原，别把用户的演示数据搞脏。
if RESTORE:
    st, body = post_json("/update_userinfo", RESTORE)
    obj = json.loads(body) if st == 200 else {}
    if obj.get("error") == 0:
        print("  [还原] 已把 uid=%d 的资料改回原值" % UID)
    else:
        print("  [警告] 还原失败，请手动检查 uid=%d 的 nick/desc/icon/sex" % UID)

# ---- 清理本次上传落到服务端的文件 ----
updir = os.path.join(dbconf.ROOT, "GateServer", "upload")
removed = 0
for u in SERVER_FILES:
    p = os.path.join(updir, os.path.basename(u))
    if os.path.exists(p):
        try:
            os.remove(p)
            removed += 1
        except OSError as e:
            print("  [警告] 删不掉 %s: %s" % (p, e))
print("  [清理] 服务端测试文件 %d 个" % removed)

sys.exit(FAIL)