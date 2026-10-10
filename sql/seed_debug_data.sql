-- ============================================================
-- HChat 调试用种子数据（可重复执行：先清空再重建）
-- 生成时间: 2026-10-10 15:16:43
-- 用户 6 个，两两互加好友 -> 15 对 / 30 条 friend 记录
-- ============================================================
SET NAMES utf8mb4;

-- ---------- 1. 清空业务数据（保留 user / user_id）----------
DELETE FROM chat_message;
DELETE FROM private_chat;
DELETE FROM chat_thread;
DELETE FROM friend;
DELETE FROM friend_apply;

-- ---------- 2. 补全缺失的昵称（NULL 会让资料页显示空白）----------
UPDATE user SET nick = name WHERE nick IS NULL OR TRIM(nick) = '';

-- ---------- 3. 两两互加好友（双向各一条）----------
INSERT INTO friend (self_id, friend_id, back) VALUES
  (2, 3, 'xxxl'),
  (3, 2, 'testuser'),
  (2, 5, 'test1999'),
  (5, 2, 'testuser'),
  (2, 6, 'test1998'),
  (6, 2, 'testuser'),
  (2, 7, 'test1997'),
  (7, 2, 'testuser'),
  (2, 8, 'text1996'),
  (8, 2, 'testuser'),
  (3, 5, 'test1999'),
  (5, 3, 'xxxl'),
  (3, 6, 'test1998'),
  (6, 3, 'xxxl'),
  (3, 7, 'test1997'),
  (7, 3, 'xxxl'),
  (3, 8, 'text1996'),
  (8, 3, 'xxxl'),
  (5, 6, 'test1998'),
  (6, 5, 'test1999'),
  (5, 7, 'test1997'),
  (7, 5, 'test1999'),
  (5, 8, 'text1996'),
  (8, 5, 'test1999'),
  (6, 7, 'test1997'),
  (7, 6, 'test1998'),
  (6, 8, 'text1996'),
  (8, 6, 'test1998'),
  (7, 8, 'text1996'),
  (8, 7, 'test1997');

-- ---------- 4. 每对用户一个私聊会话 ----------
INSERT INTO chat_thread (thread_id, type, created_at) VALUES
  (1, 'private', NOW()),
  (2, 'private', NOW()),
  (3, 'private', NOW()),
  (4, 'private', NOW()),
  (5, 'private', NOW()),
  (6, 'private', NOW()),
  (7, 'private', NOW()),
  (8, 'private', NOW()),
  (9, 'private', NOW()),
  (10, 'private', NOW()),
  (11, 'private', NOW()),
  (12, 'private', NOW()),
  (13, 'private', NOW()),
  (14, 'private', NOW()),
  (15, 'private', NOW());

INSERT INTO private_chat (thread_id, user1_id, user2_id, created_at) VALUES
  (1, 2, 3, NOW()),
  (2, 2, 5, NOW()),
  (3, 2, 6, NOW()),
  (4, 2, 7, NOW()),
  (5, 2, 8, NOW()),
  (6, 3, 5, NOW()),
  (7, 3, 6, NOW()),
  (8, 3, 7, NOW()),
  (9, 3, 8, NOW()),
  (10, 5, 6, NOW()),
  (11, 5, 7, NOW()),
  (12, 5, 8, NOW()),
  (13, 6, 7, NOW()),
  (14, 6, 8, NOW()),
  (15, 7, 8, NOW());

ALTER TABLE chat_thread AUTO_INCREMENT = 16;

-- ---------- 5. 聊天记录（每对 4 条，含长文本 / 不同 status）----------
-- status: 0=发送中未回执  1=已送达未读  2=已读
INSERT INTO chat_message (message_id, thread_id, sender_id, recv_id, content, created_at, updated_at, status) VALUES
  (1, 1, 2, 3, '你好呀，这是第 1-1 条测试消息', NOW() - INTERVAL 450 MINUTE, NOW() - INTERVAL 450 MINUTE, 0),
  (2, 1, 3, 2, '收到！消息 1-2 已送达', NOW() - INTERVAL 447 MINUTE, NOW() - INTERVAL 447 MINUTE, 2),
  (3, 1, 2, 3, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 1-3', NOW() - INTERVAL 444 MINUTE, NOW() - INTERVAL 444 MINUTE, 2),
  (4, 1, 3, 2, '好的，收到 [msg-1-4]', NOW() - INTERVAL 441 MINUTE, NOW() - INTERVAL 441 MINUTE, 2),
  (5, 2, 2, 5, '你好呀，这是第 2-1 条测试消息', NOW() - INTERVAL 420 MINUTE, NOW() - INTERVAL 420 MINUTE, 0),
  (6, 2, 5, 2, '收到！消息 2-2 已送达', NOW() - INTERVAL 417 MINUTE, NOW() - INTERVAL 417 MINUTE, 2),
  (7, 2, 2, 5, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 2-3', NOW() - INTERVAL 414 MINUTE, NOW() - INTERVAL 414 MINUTE, 2),
  (8, 2, 5, 2, '好的，收到 [msg-2-4]', NOW() - INTERVAL 411 MINUTE, NOW() - INTERVAL 411 MINUTE, 2),
  (9, 3, 2, 6, '你好呀，这是第 3-1 条测试消息', NOW() - INTERVAL 390 MINUTE, NOW() - INTERVAL 390 MINUTE, 0),
  (10, 3, 6, 2, '收到！消息 3-2 已送达', NOW() - INTERVAL 387 MINUTE, NOW() - INTERVAL 387 MINUTE, 2),
  (11, 3, 2, 6, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 3-3', NOW() - INTERVAL 384 MINUTE, NOW() - INTERVAL 384 MINUTE, 2),
  (12, 3, 6, 2, '好的，收到 [msg-3-4]', NOW() - INTERVAL 381 MINUTE, NOW() - INTERVAL 381 MINUTE, 2),
  (13, 4, 2, 7, '你好呀，这是第 4-1 条测试消息', NOW() - INTERVAL 360 MINUTE, NOW() - INTERVAL 360 MINUTE, 0),
  (14, 4, 7, 2, '收到！消息 4-2 已送达', NOW() - INTERVAL 357 MINUTE, NOW() - INTERVAL 357 MINUTE, 2),
  (15, 4, 2, 7, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 4-3', NOW() - INTERVAL 354 MINUTE, NOW() - INTERVAL 354 MINUTE, 2),
  (16, 4, 7, 2, '好的，收到 [msg-4-4]', NOW() - INTERVAL 351 MINUTE, NOW() - INTERVAL 351 MINUTE, 2),
  (17, 5, 2, 8, '你好呀，这是第 5-1 条测试消息', NOW() - INTERVAL 330 MINUTE, NOW() - INTERVAL 330 MINUTE, 0),
  (18, 5, 8, 2, '收到！消息 5-2 已送达', NOW() - INTERVAL 327 MINUTE, NOW() - INTERVAL 327 MINUTE, 2),
  (19, 5, 2, 8, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 5-3', NOW() - INTERVAL 324 MINUTE, NOW() - INTERVAL 324 MINUTE, 2),
  (20, 5, 8, 2, '好的，收到 [msg-5-4]', NOW() - INTERVAL 321 MINUTE, NOW() - INTERVAL 321 MINUTE, 2),
  (21, 6, 3, 5, '你好呀，这是第 6-1 条测试消息', NOW() - INTERVAL 300 MINUTE, NOW() - INTERVAL 300 MINUTE, 0),
  (22, 6, 5, 3, '收到！消息 6-2 已送达', NOW() - INTERVAL 297 MINUTE, NOW() - INTERVAL 297 MINUTE, 2),
  (23, 6, 3, 5, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 6-3', NOW() - INTERVAL 294 MINUTE, NOW() - INTERVAL 294 MINUTE, 2),
  (24, 6, 5, 3, '好的，收到 [msg-6-4]', NOW() - INTERVAL 291 MINUTE, NOW() - INTERVAL 291 MINUTE, 2),
  (25, 7, 3, 6, '你好呀，这是第 7-1 条测试消息', NOW() - INTERVAL 270 MINUTE, NOW() - INTERVAL 270 MINUTE, 0),
  (26, 7, 6, 3, '收到！消息 7-2 已送达', NOW() - INTERVAL 267 MINUTE, NOW() - INTERVAL 267 MINUTE, 2),
  (27, 7, 3, 6, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 7-3', NOW() - INTERVAL 264 MINUTE, NOW() - INTERVAL 264 MINUTE, 2),
  (28, 7, 6, 3, '好的，收到 [msg-7-4]', NOW() - INTERVAL 261 MINUTE, NOW() - INTERVAL 261 MINUTE, 2),
  (29, 8, 3, 7, '你好呀，这是第 8-1 条测试消息', NOW() - INTERVAL 240 MINUTE, NOW() - INTERVAL 240 MINUTE, 0),
  (30, 8, 7, 3, '收到！消息 8-2 已送达', NOW() - INTERVAL 237 MINUTE, NOW() - INTERVAL 237 MINUTE, 2),
  (31, 8, 3, 7, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 8-3', NOW() - INTERVAL 234 MINUTE, NOW() - INTERVAL 234 MINUTE, 2),
  (32, 8, 7, 3, '好的，收到 [msg-8-4]', NOW() - INTERVAL 231 MINUTE, NOW() - INTERVAL 231 MINUTE, 2),
  (33, 9, 3, 8, '你好呀，这是第 9-1 条测试消息', NOW() - INTERVAL 210 MINUTE, NOW() - INTERVAL 210 MINUTE, 0),
  (34, 9, 8, 3, '收到！消息 9-2 已送达', NOW() - INTERVAL 207 MINUTE, NOW() - INTERVAL 207 MINUTE, 2),
  (35, 9, 3, 8, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 9-3', NOW() - INTERVAL 204 MINUTE, NOW() - INTERVAL 204 MINUTE, 2),
  (36, 9, 8, 3, '好的，收到 [msg-9-4]', NOW() - INTERVAL 201 MINUTE, NOW() - INTERVAL 201 MINUTE, 2),
  (37, 10, 5, 6, '你好呀，这是第 10-1 条测试消息', NOW() - INTERVAL 180 MINUTE, NOW() - INTERVAL 180 MINUTE, 0),
  (38, 10, 6, 5, '收到！消息 10-2 已送达', NOW() - INTERVAL 177 MINUTE, NOW() - INTERVAL 177 MINUTE, 2),
  (39, 10, 5, 6, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 10-3', NOW() - INTERVAL 174 MINUTE, NOW() - INTERVAL 174 MINUTE, 2),
  (40, 10, 6, 5, '好的，收到 [msg-10-4]', NOW() - INTERVAL 171 MINUTE, NOW() - INTERVAL 171 MINUTE, 2),
  (41, 11, 5, 7, '你好呀，这是第 11-1 条测试消息', NOW() - INTERVAL 150 MINUTE, NOW() - INTERVAL 150 MINUTE, 0),
  (42, 11, 7, 5, '收到！消息 11-2 已送达', NOW() - INTERVAL 147 MINUTE, NOW() - INTERVAL 147 MINUTE, 2),
  (43, 11, 5, 7, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 11-3', NOW() - INTERVAL 144 MINUTE, NOW() - INTERVAL 144 MINUTE, 2),
  (44, 11, 7, 5, '好的，收到 [msg-11-4]', NOW() - INTERVAL 141 MINUTE, NOW() - INTERVAL 141 MINUTE, 2),
  (45, 12, 5, 8, '你好呀，这是第 12-1 条测试消息', NOW() - INTERVAL 120 MINUTE, NOW() - INTERVAL 120 MINUTE, 0),
  (46, 12, 8, 5, '收到！消息 12-2 已送达', NOW() - INTERVAL 117 MINUTE, NOW() - INTERVAL 117 MINUTE, 2),
  (47, 12, 5, 8, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 12-3', NOW() - INTERVAL 114 MINUTE, NOW() - INTERVAL 114 MINUTE, 2),
  (48, 12, 8, 5, '好的，收到 [msg-12-4]', NOW() - INTERVAL 111 MINUTE, NOW() - INTERVAL 111 MINUTE, 2),
  (49, 13, 6, 7, '你好呀，这是第 13-1 条测试消息', NOW() - INTERVAL 90 MINUTE, NOW() - INTERVAL 90 MINUTE, 0),
  (50, 13, 7, 6, '收到！消息 13-2 已送达', NOW() - INTERVAL 87 MINUTE, NOW() - INTERVAL 87 MINUTE, 2),
  (51, 13, 6, 7, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 13-3', NOW() - INTERVAL 84 MINUTE, NOW() - INTERVAL 84 MINUTE, 2),
  (52, 13, 7, 6, '好的，收到 [msg-13-4]', NOW() - INTERVAL 81 MINUTE, NOW() - INTERVAL 81 MINUTE, 2),
  (53, 14, 6, 8, '你好呀，这是第 14-1 条测试消息', NOW() - INTERVAL 60 MINUTE, NOW() - INTERVAL 60 MINUTE, 0),
  (54, 14, 8, 6, '收到！消息 14-2 已送达', NOW() - INTERVAL 57 MINUTE, NOW() - INTERVAL 57 MINUTE, 2),
  (55, 14, 6, 8, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 14-3', NOW() - INTERVAL 54 MINUTE, NOW() - INTERVAL 54 MINUTE, 2),
  (56, 14, 8, 6, '好的，收到 [msg-14-4]', NOW() - INTERVAL 51 MINUTE, NOW() - INTERVAL 51 MINUTE, 2),
  (57, 15, 7, 8, '你好呀，这是第 15-1 条测试消息', NOW() - INTERVAL 30 MINUTE, NOW() - INTERVAL 30 MINUTE, 0),
  (58, 15, 8, 7, '收到！消息 15-2 已送达', NOW() - INTERVAL 27 MINUTE, NOW() - INTERVAL 27 MINUTE, 2),
  (59, 15, 7, 8, '这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 15-3', NOW() - INTERVAL 24 MINUTE, NOW() - INTERVAL 24 MINUTE, 2),
  (60, 15, 8, 7, '好的，收到 [msg-15-4]', NOW() - INTERVAL 21 MINUTE, NOW() - INTERVAL 21 MINUTE, 2);
ALTER TABLE chat_message AUTO_INCREMENT = 61;

-- ---------- 6. 好友申请 ----------
-- status: 0=待处理  1=已同意  2=已拒绝
-- 前两条是【待处理】，用来测"通过/拒绝"流程（这几对已经是好友，
--   重复通过不会产生重复数据：friend 用 INSERT IGNORE，会话会复用已有的 thread）
INSERT INTO friend_apply (from_uid, to_uid, descs, back_name, status, created_at) VALUES
  (2, 3, '我是 testuser，交个朋友吧~', 'xxxl', 0, NOW() - INTERVAL 20 MINUTE),
  (5, 6, '在吗？想加你好友', 'test1998', 0, NOW() - INTERVAL 5 MINUTE),
  (7, 8, '已经加过了', 'text1996', 1, NOW() - INTERVAL 3 DAY);

-- ---------- 7. 校验 ----------
SELECT 'friend' t, COUNT(*) n FROM friend
UNION ALL SELECT 'chat_thread', COUNT(*) FROM chat_thread
UNION ALL SELECT 'private_chat', COUNT(*) FROM private_chat
UNION ALL SELECT 'chat_message', COUNT(*) FROM chat_message
UNION ALL SELECT 'friend_apply', COUNT(*) FROM friend_apply;
