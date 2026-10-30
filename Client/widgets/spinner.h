#ifndef SPINNER_H
#define SPINNER_H

#include <QWidget>
#include <QColor>

class QPropertyAnimation;

// Spinner —— 轻量"加载中"指示器（P3 微交互：加载中点位）。
// 用 QPainter 自绘 8 点环形旋转，替代原来的 loading.gif：
//   · 分辨率无关，任意尺寸都清晰（gif 放大后发虚）；
//   · 颜色跟主题走（默认静靛 #4C5FD7），不再是灰阶；
//   · 不占额外资源文件，也不需要 QMovie 解码线程。
// 动画用 QPropertyAnimation 驱动 angle(0->360)，仅在可见时运行
// （showEvent 启动 / hideEvent 停止），避免隐藏后白烧 CPU。
class Spinner : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int angle READ angle WRITE setAngle)
public:
    explicit Spinner(QWidget* parent = nullptr, int diameter = 56);

    // 换主色（默认取主题静靛）
    void setColor(const QColor& c);
    // 点数（默认 8；4~12 观感都不错）
    void setDotCount(int n);

    void start();
    void stop();
    bool isRunning() const { return _running; }

    int angle() const { return _angle; }
    void setAngle(int a);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    int _angle = 0;
    int _dots = 8;
    bool _running = false;
    QColor _color;
    QPropertyAnimation* _anim = nullptr;
};

#endif // SPINNER_H
