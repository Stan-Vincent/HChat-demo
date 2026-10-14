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

}

#endif // ANIMUTIL_H
