#ifndef ANIMUTIL_H
#define ANIMUTIL_H

#include <QWidget>
#include <QStackedWidget>
#include <QColor>
#include <QLabel>
#include <QPropertyAnimation>
#include <functional>

// 一组轻量 UI 动效工具：窗口淡入、堆叠页交叉淡入、按钮 hover 背景色过渡、未读红点脉冲。
// 放在独立命名空间，避免污染全局；所有动画都用 DeleteWhenStopped /
// 显式 delete 自清理，不会泄漏。
namespace AnimUtil {

// 窗口构造时 150ms 透明度淡入 + 轻微缩放（默认从 0.95 放大到 1.0）。
// 缩放基于当前几何计算，仅做"中心略小 → 原尺寸"的放大，不抢窗口管理器的居中。
void fadeInWindow(QWidget* w, int duration = 150, double fromScale = 0.95);

// 堆叠部件交叉淡入：先淡出当前页(半程)，切换页面并改窗体尺寸，再淡入新页(半程)。
// applySize 在"淡出结束、切换瞬间"执行（此时透明度为 0，尺寸跳变不可见）。
void crossFadeStacked(QStackedWidget* stack, QWidget* next,
                      std::function<void()> applySize, int duration = 200);

// 给按钮安装 hover 背景色过渡（QVariantAnimation）：normal -> hover。
// staticStyle 为过渡期间需保留的静态样式（边框/圆角/文字色等），避免内联样式覆盖 qss 后变丑。
void installHoverColor(QWidget* w, const QColor& normal, const QColor& hover,
                       int duration = 150, const QString& staticStyle = QString());

// 按 role 安装（配合 setProperty("role",...) 使用）：primary/ghost 做背景过渡，link 不动。
// 颜色与 stylesheet.qss §9 保持一致；圆角按对象名匹配（login_btn=22px，其余主按钮=10px）。
void installHoverColorForRole(QWidget* w, const QString& role, int duration = 150);

// 未读红点脉冲：对红点 QLabel 的 geometry 做"原大小 -> 1.18 倍 -> 原大小"的
// 无限循环缩放（默认 900ms 一周期，InOutSine）。重复调用会先停掉旧动画；
// 停止时会恢复原始几何，避免停在放大状态。
void startRedDotPulse(QLabel* dot, int periodMs = 900);

// 停止红点脉冲并恢复原始几何（隐藏红点前调用）。
void stopRedDotPulse(QLabel* dot);

// 校验失败时输入框左右抖动：水平位移关键帧（默认 400ms），结束后自动归位。
// 用临时 pos 动画实现，不与布局长期冲突；连续触发会刷新基准位置避免累积偏移。
void shakeWidget(QWidget* w, int duration = 400);

// 给按钮安装"悬浮上浮"：Enter 时整体上移 liftPx 像素（带过渡回弹），Leave 时落回。
// 同样用临时 pos 动画，不影响布局最终状态。
void installHoverLift(QWidget* w, int liftPx = 2);

// 消息气泡入场：高度由 0 展开到自然高度（不使用 GraphicsEffect，规避与
// _main_widget 阴影 effect 的嵌套崩溃；尺寸变化由布局自然接管）。
// 适用于聊天列表里的 ChatItemBase 等被 _main_widget 阴影祖先包裹的控件。
void revealItem(QWidget* w, int duration = 220);

// 弹出层向上展开（表情面板等）：调用前调用方应已 move() 到目标位置，
// 本函数以"当前底边"为锚点，把高度从 0 动画到自然高度 —— 观感是
// 从按钮上方"长"出来。全程只改 geometry，不碰 GraphicsEffect
// （EmojiPanel 是 Qt::Popup 不透明窗口 + 圆角遮罩，挂特效会踩
//  UpdateLayeredWindowIndirect 失败的坑，见 emojipanel.cpp 注释）。
void unfoldUp(QWidget* w, int duration = 180);

// 点击"下压-回弹"：整体临时下移 dy 像素再回到基准位置（pos 动画，
// 时长很短，结束后落回布局原本的位置）。用于发送键这类必须给触感的按钮。
// 与 shakeWidget 同一套基准点修正逻辑，连续点击不会累积偏移。
void pressBounce(QWidget* w, int dy = 2, int duration = 160);

}

#endif // ANIMUTIL_H
