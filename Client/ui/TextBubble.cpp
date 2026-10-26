#include "TextBubble.h"
#include <QFontMetricsF>
#include <QDebug>
#include <QFont>
#include <QPalette>
#include "global.h"
#include <QTimer>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QFont>

TextBubble::TextBubble(ChatRole role, const QString &text, QWidget *parent)
    :BubbleFrame(role, parent)
{
    m_pTextEdit = new QTextEdit();
    m_pTextEdit->setReadOnly(true);
    m_pTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTextEdit->installEventFilter(this);
    QFont font("Microsoft YaHei");
    font.setPointSize(12);
    m_pTextEdit->setFont(font);
    setPlainText(text);
    setWidget(m_pTextEdit);
    initStyleSheet(role);
    // ★ 必须在 setStyleSheet 之后调：QSS polish 会合并调色板，
    //   顺序反了可能把选中色冲掉。
    initSelectionPalette();
}

bool TextBubble::eventFilter(QObject *o, QEvent *e)
{
    if(m_pTextEdit == o && e->type() == QEvent::Paint)
    {
        adjustTextHeight(); //PaintEvent中设置
    }
    return BubbleFrame::eventFilter(o, e);
}

void TextBubble::setPlainText(const QString &text)
{
    m_pTextEdit->setPlainText(text);
    //m_pTextEdit->setHtml(text);
    //找到段落中最大宽度
    qreal doc_margin = m_pTextEdit->document()->documentMargin();
    int margin_left = this->layout()->contentsMargins().left();
    int margin_right = this->layout()->contentsMargins().right();
    QFontMetricsF fm(m_pTextEdit->font());
    QTextDocument *doc = m_pTextEdit->document();
    int max_width = 0;
    //遍历每一段找到 最宽的那一段
    for (QTextBlock it = doc->begin(); it != doc->end(); it = it.next())    //字体总长
    {
        int txtW = int(fm.horizontalAdvance(it.text()));
        max_width = max_width < txtW ? txtW : max_width;                 //找到最长的那段
    }
    //设置这个气泡的最大宽度 只需要设置一次
    setMaximumWidth(max_width + doc_margin * 2 + (margin_left + margin_right));        //设置最大宽度
}

void TextBubble::adjustTextHeight()
{
    qreal doc_margin = m_pTextEdit->document()->documentMargin();    //字体到边框的距离默认为4
    QTextDocument *doc = m_pTextEdit->document();
    qreal text_height = 0;
    //把每一段的高度相加=文本高
    for (QTextBlock it = doc->begin(); it != doc->end(); it = it.next())
    {
        QTextLayout *pLayout = it.layout();
        QRectF text_rect = pLayout->boundingRect();                             //这段的rect
        text_height += text_rect.height();
    }
    int vMargin = this->layout()->contentsMargins().top();
    //设置这个气泡需要的高度 文本高+文本边距+TextEdit边框到气泡边框的距离
    setFixedHeight(text_height + doc_margin *2 + vMargin*2 );
}

void TextBubble::initStyleSheet(ChatRole role)
{
    // ★ 必须给 QTextEdit 不透明底色（与 BubbleFrame 画的气泡同色）：
    //   原来用 background:transparent，文字直接画在透明背景上再与气泡底色
    //   合成——局部重绘（如 hover 时间胶囊显示/隐藏触发条目重绘）时，
    //   抗锯齿边缘会与"透明"混合，文字整体发灰发虚，且不会自愈。
    //   底色与气泡同色 + 同圆角(9px)，视觉上完全无缝。
    //   Self=珊瑚 #EEF0FB（与 BubbleFrame 一致），Other=白。
    const QString bg = (role == ChatRole::Self) ? "#EEF0FB" : "#ffffff";
    m_pTextEdit->setStyleSheet(
        QString("QTextEdit{background:%1;border:none;border-radius:10px;}").arg(bg));
}

void TextBubble::clearSelection()
{
    // ★ 折叠光标即可抹掉选区高亮（无论当前是 Active 还是 Inactive 配色，
    //   没有选中文字就不会再渲染橙色/浅橙底色）。不动 focus、不动交互标志，
    //   用户随后照样能重新拖选。空选区时 setTextCursor 是无害的 no-op。
    QTextCursor cur = m_pTextEdit->textCursor();
    if (cur.hasSelection()) {
        cur.clearSelection();
        m_pTextEdit->setTextCursor(cur);
    }
}

void TextBubble::initSelectionPalette()
{
    // ★ 选中态配色必须三组（Active/Inactive/Disabled）都显式指定。
    //   只靠默认调色板时：QTextEdit 失焦后（典型操作：右键菜单"全选"→
    //   点一下别处关闭菜单），Qt 用 Inactive 组的默认灰色 Highlight 渲染
    //   残留选区——表现为"选中的文字变灰"。
    //   三组统一主色系：主动态 #4C5FD7，失焦/禁用态 #6B7CE8（只变浅不变灰），
    //   选中文字一律白色。选中功能保留（复制消息文字是基本操作）。
    QPalette pal = m_pTextEdit->palette();
    const QColor kSelActive(76, 95, 215);    // #4C5FD7 主色
    const QColor kSelInactive(107, 124, 232); // #6B7CE8 主色浅调
    const QColor kSelText(255, 255, 255);     // 选中文字：白
    pal.setColor(QPalette::Active,   QPalette::Highlight,       kSelActive);
    pal.setColor(QPalette::Active,   QPalette::HighlightedText, kSelText);
    pal.setColor(QPalette::Inactive, QPalette::Highlight,       kSelInactive);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, kSelText);
    pal.setColor(QPalette::Disabled, QPalette::Highlight,       kSelInactive);
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, kSelText);
    m_pTextEdit->setPalette(pal);
}
