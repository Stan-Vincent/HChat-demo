#ifndef TOAST_H
#define TOAST_H

#include <QWidget>
#include <QString>
#include <QPointer>
#include <QList>

// 右上角轻量 Toast：自动消失、不阻塞、与圆角无边框风格统一。
//
// 用法：Toast::showToast(parent, "文本", Toast::Warning, 3000);
//   parent 一般传 MainWindow（this）；Toast 作为其子控件浮在最上层，
//   不会因为切页 / 弹登录页而消失（它不是堆叠页的一部分）。
class Toast : public QWidget
{
    Q_OBJECT
public:
    enum Type { Info = 0, Success = 1, Warning = 2, Error = 3 };

    // 在 parent 右上角弹出一个自动消失、不阻塞的圆角 Toast。
    // durationMs 为停留时长（毫秒），到点后淡出并自销毁。
    static void showToast(QWidget *parent, const QString &text,
                          Type type = Info, int durationMs = 3000);

private slots:
    // 停留结束后：从活跃列表移除、重排其余、淡出并删除自身
    void startDismiss();

private:
    Toast(QWidget *parent, const QString &text, Type type, int durationMs);
    // 重新排布所有活跃 Toast 的位置（新消息往下堆叠，旧消息离开后其余上移）
    static void relayout();

    static QList<QPointer<Toast>> s_active;   // 正在显示的 Toast（用于堆叠定位）
    int _durationMs;
};

#endif // TOAST_H
