# -*- coding: utf-8 -*-
"""端到端验证 GateServer 的三个新接口：上传图片 / 下载图片 / 更新资料。
顺带验证路径穿越防护。只用标准库，不需要requests。"""
import json
import base64
import urllib.request
import sys

BASE = "http://127.0.0.1:8080"
PASS, FAIL = 0, 0


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
    import subprocess
    mysql = r"D:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe"
    try:
        out = subprocess.run(
            [mysql, "-h127.0.0.1", "-P3308", "-uroot", "-p123456", "xxxl", "-N", "-B",
             "-e", "SELECT uid FROM user ORDER BY id LIMIT 1;"],
            capture_output=True, timeout=15)
        v = out.stdout.decode("utf-8", "replace").strip().splitlines()
        if v and v[0].strip().isdigit():
            return int(v[0].strip())
    except Exception as e:
        print("  (查库失败(%s)，退回 uid=2)" % e)
    return 2

UID = pick_uid()
print("  (使用 uid=%d)" % UID)

# ---- 先把原始资料读下来，测完好还原 ----
def mysql_query(sql):
    import subprocess
    mysql = r"D:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe"
    out = subprocess.run(
        [mysql, "-h127.0.0.1", "-P3308", "-uroot", "-p123456", "xxxl", "-N", "-B", "-e", sql],
        capture_output=True, timeout=15)
    return out.stdout.decode("utf-8", "replace").strip()

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

# ---- 1x. 上传 ----
# 造一个最小的合法 PNG（1x1 像素）
PNG_1PX = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmM"
    "IQAAAABJRU5ErkJggg==")
b64 = base64.b64encode(PNG_1PX).decode()

st, body = post_json("/upload_image",
                     {"uid": UID, "file_name": "e2e_test.png", "data": b64})
obj = json.loads(body) if st == 200 else {}
report("upload_image 返回 200", st == 200, "status=%d" % st)
report("upload_image error==0", obj.get("error") == 0, str(obj.get("msg", "")))
url = obj.get("url", "")
report("upload_image 返回 url", bool(url), url)

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

# ---- 6x. 空 data 上传要报错，不能崩 ----
st, body = post_json("/upload_image",
                     {"uid": UID, "file_name": "x.png", "data": ""})
obj = json.loads(body) if st == 200 else {}
report("空 data 返回错误而非崩溃", obj.get("error") != 0,
       "error=%s" % obj.get("error"))

# ---- 6b. 坏 base64 不能崩 ----
st, body = post_json("/upload_image",
                     {"uid": UID, "file_name": "x.png", "data": "!!!not-base64!!!"})
obj = json.loads(body) if st == 200 else {}
report("坏 base64 返回错误而非崩溃", obj.get("error") != 0,
       "error=%s" % obj.get("error"))

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

sys.exit(FAIL)