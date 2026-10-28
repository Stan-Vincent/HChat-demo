#ifndef STATEWIDGET_H
#define STATEWIDGET_H
#include <QWidget>
#include "global.h"
#include <QLabel>
#include "SvgIconButton.h"

/*
 * StateWidget —— 侧边栏「聊天 / 联系人 / 设置」图标容器（P2 §3 批次③迁移）。
 *
 * 原为 QWidget + 六态 border-image（state 属性切多张 SVG）+ 红点；
 * 现内部持有 SvgIconButton 自绘单 SVG，按 normal/hover/press 切换描边色，
 * 选中态用 setSelected 画浅灰底色圆 + 靛蓝描边，不再需要
 * *_hover / *_press / *_select_* 一堆冗余 SVG 文件。
 *
 * 对外接口（SetSelected / ClearState / AddRedPoint / ShowRedPoint /
 * clicked(void) / GetCurState）与迁移前保持一致，调用方无需改动。
 */
class StateWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StateWidget(QWidget *parent = nullptr);

    // 配置图标：默认 SVG + 图标尺寸 + 三态描边色（普通 / 悬停 / 按下）
    void SetIcon(const QString &svgPath,
                 const QSize &iconSize = QSize(30, 30),
                 const QString &normal = "#D5D8E0",
                 const QString &hover  = "#4C5FD7",
                 const QString &press  = "#4C5FD7");

    ClickLbState GetCurState();
    void ClearState();

    void SetSelected(bool bselected);
    void AddRedPoint();
    void ShowRedPoint(bool show = true);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    // 依据 _curstate 应用描边色 + 选中底色圆
    void applyColors();

    SvgIconButton* _iconBtn;
    ClickLbState   _curstate;
    QLabel*        _red_point;

    QString _colorNormal = "#D5D8E0";
    QString _colorHover  = "#4C5FD7";
    QString _colorPress  = "#4C5FD7";

signals:
    void clicked(void);
};

#endif // STATEWIDGET_H
