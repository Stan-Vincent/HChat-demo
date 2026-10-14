#ifndef EMOJIPANEL_H
#define EMOJIPANEL_H

#include <QWidget>
#include <QVector>
#include <QPair>
#include <QString>

class QTabWidget;

// 表情选择面板：锚定在输入框表情按钮上方的弹出层。
// 用 Qt::Popup 标志 —— 点击面板以外区域会自动关闭（原生行为），
// 不需要自己做全局事件过滤器。选中的 emoji 作为文本插入输入框，
// 走现有 TEXT 消息链路，无需任何后端/资源改动。
class EmojiPanel : public QWidget
{
    Q_OBJECT
public:
    explicit EmojiPanel(QWidget *parent = nullptr);
signals:
    // 用户点了一个 emoji（可能是单个字符，也可能是带 VS16 的彩色序列）
    void sig_emoji_selected(const QString &emoji);
    // 面板被隐藏（点击外部 / 再次点按钮 / Esc），供 ChatPage 复位按钮状态
    void sig_panel_hidden();
protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
private:
    void buildUi();
    QWidget* buildCategoryPage(const QVector<QString> &emojis);
    // 用 code point 构造 QString，彻底规避源码编码问题；
    // 需要彩色表现的字符补 VS16(0xFE0F)（如 ❤️ ✈️ 等默认文本表现的）。
    static QString emo(char32_t cp);
    static QString emo(char32_t cp, char32_t vs);
};

#endif // EMOJIPANEL_H
