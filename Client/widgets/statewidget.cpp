#include "statewidget.h"
#include <QResizeEvent>

StateWidget::StateWidget(QWidget *parent)
    : QWidget(parent), _curstate(ClickLbState::Normal),
      _iconBtn(nullptr), _red_point(nullptr)
{
    setCursor(Qt::PointingHandCursor);

    // 图标自绘：SvgIconButton 接手 hover/press 换色 + 点击信号。
    // 鼠标事件直接落在它身上（它是最上层子控件），StateWidget 不再处理，
    // 点击经由 SvgIconButton::clicked 转发为 StateWidget::clicked(void)。
    _iconBtn = new SvgIconButton(this);
    // 选中底色圆：深色侧边栏上用浅灰圆区分选中（原 selected_* 态语义）
    _iconBtn->setSelectedColor("#ECEDEF");
    connect(_iconBtn, &SvgIconButton::clicked, this, [this] { emit clicked(); });

    AddRedPoint();
}

void StateWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (_iconBtn) {
        _iconBtn->setGeometry(0, 0, width(), height());
    }
    if (_red_point) {
        const int s = qMin(14, width());                // 红点直径
        _red_point->setGeometry(width() - s, 0, s, s);   // 右上角未读提示
    }
}

// 依据当前态应用图标描边色 + 选中底色圆：
//   Normal   —— 普通灰描边，无圆
//   Selected —— 靛蓝描边 + 浅灰圆（原 chat_icon_hover.svg 语义）
void StateWidget::applyColors()
{
    if (_curstate == ClickLbState::Selected) {
        _iconBtn->setColors(_colorHover, _colorHover, _colorPress);
        _iconBtn->setSelected(true);
    } else {
        _iconBtn->setColors(_colorNormal, _colorHover, _colorPress);
        _iconBtn->setSelected(false);
    }
}

void StateWidget::SetIcon(const QString &svgPath, const QSize &iconSize,
                          const QString &normal, const QString &hover, const QString &press)
{
    _colorNormal = normal;
    _colorHover  = hover;
    _colorPress  = press;

    _iconBtn->setSvg(svgPath);
    _iconBtn->setIconSize(iconSize);
    applyColors();   // 初始 Normal 态
}

ClickLbState StateWidget::GetCurState()
{
    return _curstate;
}

void StateWidget::ClearState()
{
    _curstate = ClickLbState::Normal;
    applyColors();
}

void StateWidget::SetSelected(bool bselected)
{
    _curstate = bselected ? ClickLbState::Selected : ClickLbState::Normal;
    applyColors();
}

void StateWidget::AddRedPoint()
{
    _red_point = new QLabel(this);
    _red_point->setObjectName("red_point");
    // 鼠标穿透：点击红点区域也当作点击图标（红点只是装饰提示）
    _red_point->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    _red_point->setVisible(false);
}

void StateWidget::ShowRedPoint(bool show)
{
    if (_red_point) _red_point->setVisible(show);
}
