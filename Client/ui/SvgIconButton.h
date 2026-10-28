#ifndef SVGICONBUTTON_H
#define SVGICONBUTTON_H

#include <QLabel>
#include <QVariantAnimation>
#include "global.h"   // ClickLbState

/*
 * SvgIconButton —— P2 §3「真 SVG 图标」落地控件。
 *
 * 设计要点（替代原来的 ClickedLabel + 六态 border-image）：
 *   - 只持有一张（最多两张）SVG，按 normal/hover/press 切换【描边色】，不再需要
 *     *_hover / *_press / *_select_* 一堆冗余 svg 文件；
 *   - 支持 checked 第二张图（如密码可见的「睁眼」）与 selected 底色圆（侧边栏选中）；
 *   - 高 DPI：按 devicePixelRatio 渲染，缩放清晰不糊；
 *   - 信号 clicked(const QString&, ClickLbState) 与 ClickedLabel 同签名，可原地替换。
 *   - P3 动效：normal/hover/press 切换时描边色做 ~120ms 交叉淡化（图标 morph 的取色过渡）。
 */
class SvgIconButton : public QLabel
{
    Q_OBJECT
public:
    explicit SvgIconButton(QWidget *parent = nullptr);

    // 默认图标（qrc 路径，如 ":/res/smile.svg"）。加载后捕获 SVG 源，
    // 之后只靠改描边色切换状态，不再依赖多文件。
    void setSvg(const QString &pathInQrc);

    // 可选：勾选态显示的另一张 SVG（如密码可见的「睁眼」）。不设置则与默认同图。
    void setCheckedSvg(const QString &pathInQrc);

    // 三态描边色：普通 / 悬停 / 按下（默认走设计 token：灰 → 靛蓝）。
    void setColors(const QString &normal,
                   const QString &hover,
                   const QString &press);

    // 勾选态（密码可见切睁眼 / 通用 toggle）
    void setChecked(bool on);
    bool isChecked() const { return _checked; }

    // 选中底色圆（侧边栏等）：开启后绘制圆角矩形底色
    void setSelected(bool on);
    bool isSelected() const { return _selected; }
    void setSelectedColor(const QString &color) { _selectedColor = color; }

    // 图标绘制尺寸；缺省取控件当前尺寸
    void setIconSize(const QSize &sz) { _iconSize = sz; rebuild(); }
    QSize sizeHint() const override;

signals:
    // 与 ClickedLabel::clicked 同签名，便于原地替换
    void clicked(const QString &text, ClickLbState state);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // 选择当前该用的描边色（依 normal/hover/press 状态）
    QColor stateColor() const;
    // 依当前状态重绘（选 svg / 选中底色 / 用 _displayColor 描边）
    void rebuild();
    // 用指定描边色渲染 pixmap（不含状态判定，供动画逐帧调用）
    void render(const QColor &color);
    // 计算目标色并做 ~120ms 交叉淡化到该色
    void updateColor();

    static QString loadSvg(const QString &path);       // 从 qrc 读 SVG 源
    static QString recolor(const QString &svg,
                           const QString &color);      // 替换 stroke / fill 颜色

    QString _svgNormal;
    QString _svgChecked;
    QString _colorNormal = "#6B6F76";
    QString _colorHover   = "#4C5FD7";
    QString _colorPress   = "#4C5FD7";
    QString _selectedColor = "#ECEDEF";
    QSize   _iconSize;
    bool    _hovered = false;
    bool    _pressed = false;
    bool    _checked = false;
    bool    _selected = false;

    QColor _displayColor;                 // 当前显示中的描边色（动画过程中为中间值）
    QVariantAnimation* _colorAnim = nullptr;
};

#endif // SVGICONBUTTON_H
