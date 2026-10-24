#ifndef TEXTBUBBLE_H
#define TEXTBUBBLE_H

#include <QTextEdit>
#include "BubbleFrame.h"
#include <QHBoxLayout>

class TextBubble : public BubbleFrame
{
    Q_OBJECT
public:
    TextBubble(ChatRole role, const QString &text, QWidget *parent = nullptr);
    // 主动清掉本气泡内的文字选区（供"点击任意位置取消选中"使用）。
    // 只清光标选区、不碰 TextEdit 本身，不影响后续重新选中。
    void clearSelection();
protected:
    bool eventFilter(QObject *o, QEvent *e);
private:
    void adjustTextHeight();
    void setPlainText(const QString &text);
    void initStyleSheet(ChatRole role);
    void initSelectionPalette();
private:
    QTextEdit *m_pTextEdit;
};

#endif // TEXTBUBBLE_H
