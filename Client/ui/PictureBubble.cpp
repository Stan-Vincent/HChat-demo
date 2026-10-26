#include "PictureBubble.h"
#include <QLabel>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <QPropertyAnimation>
#include <QEasingCurve>

namespace {

// 图片灯箱：全屏半透明黑底，点击或 ESC 关闭，带淡入淡出。
class ImageViewer : public QWidget
{
public:
    ImageViewer(const QPixmap &pix, QWidget *parent = nullptr)
        : QWidget(parent), _pix(pix)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setStyleSheet("background-color:rgba(0,0,0,0.92);");
        setCursor(Qt::PointingHandCursor);

        QScreen *screen = QApplication::primaryScreen();
        QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1024, 768);
        setGeometry(avail);

        _label = new QLabel(this);
        _label->setAlignment(Qt::AlignCenter);
        _label->setStyleSheet("background:transparent;");
        updateImage();

        // 淡入
        setWindowOpacity(0.0);
        auto *in = new QPropertyAnimation(this, "windowOpacity", this);
        in->setDuration(180);
        in->setStartValue(0.0);
        in->setEndValue(1.0);
        in->setEasingCurve(QEasingCurve::OutCubic);
        in->start(QAbstractAnimation::DeleteWhenStopped);
    }

protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape) closeWithFade();
    }
    void mousePressEvent(QMouseEvent *) override { closeWithFade(); }
    void resizeEvent(QResizeEvent *) override { updateImage(); }

private:
    void updateImage() {
        if (_pix.isNull()) return;
        QSize avail = size();
        QPixmap scaled = _pix.scaled(avail, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        _label->setPixmap(scaled);
        _label->setFixedSize(scaled.size());
        _label->move((avail.width() - scaled.width()) / 2,
                     (avail.height() - scaled.height()) / 2);
    }
    void closeWithFade() {
        auto *out = new QPropertyAnimation(this, "windowOpacity", this);
        out->setDuration(150);
        out->setStartValue(1.0);
        out->setEndValue(0.0);
        out->setEasingCurve(QEasingCurve::InCubic);
        connect(out, &QPropertyAnimation::finished, this, &QObject::deleteLater);
        out->start(QAbstractAnimation::DeleteWhenStopped);
    }
    QPixmap _pix;
    QLabel *_label;
};

void openImageLightbox(const QPixmap &pix, QWidget *parent)
{
    if (pix.isNull()) return;
    auto *viewer = new ImageViewer(pix, parent);
    viewer->show();
    viewer->raise();
    viewer->activateWindow();
}

} // namespace

#define PIC_MAX_WIDTH 160
#define PIC_MAX_HEIGHT 90

QHash<QString, QPixmap> PictureBubble::_pixmap_cache;

PictureBubble::PictureBubble(const QPixmap &picture, ChatRole role, QWidget *parent)
    :BubbleFrame(role, parent)
{
    initEmpty(role);
    applyPixmap(picture);
}

PictureBubble::PictureBubble(const QString &url, ChatRole role, QWidget *parent)
    :BubbleFrame(role, parent)
{
    initEmpty(role);
    // 命中缓存就直接填，省掉一次网络往返
    auto it = _pixmap_cache.find(url);
    if (it != _pixmap_cache.end()) {
        applyPixmap(it.value());
    }
}

void PictureBubble::initEmpty(ChatRole role)
{
    Q_UNUSED(role);
    _lb = new QLabel();
    _lb->setScaledContents(true);
    _lb->setCursor(Qt::PointingHandCursor);
    _lb->installEventFilter(this);
    // 占位：一块固定大小的灰底，避免气泡在图片到达前高度为 0 造成列表跳动。
    // qss 圆角只影响 label 自身背景，不影响 setPixmap 的内容，所以真正的图片圆角
    // 在 applyPixmap() 里对 pixmap 做裁剪（见下）。
    _lb->setStyleSheet("QLabel{background:#f0f0f0;border-radius:8px;color:#999999;}");
    _lb->setFixedSize(PIC_MAX_WIDTH, PIC_MAX_HEIGHT);
    _lb->setAlignment(Qt::AlignCenter);
    _lb->setText("...");
    this->setWidget(_lb);

    int left_margin = this->layout()->contentsMargins().left();
    int right_margin = this->layout()->contentsMargins().right();
    int v_margin = this->layout()->contentsMargins().bottom();
    setFixedSize(PIC_MAX_WIDTH + left_margin + right_margin,
                 PIC_MAX_HEIGHT + v_margin * 2);
}

void PictureBubble::SetPixmap(const QPixmap &pix)
{
    applyPixmap(pix);
}

void PictureBubble::applyPixmap(const QPixmap &pix)
{
    if (_lb == nullptr) {
        return;
    }
    _full_pix = pix;   // 保留原图，用于灯箱全屏预览
    if (pix.isNull()) {
        // 下载失败：占位改成失败提示，别留一个空白方块让人以为还在加载
        _lb->setText("[图片加载失败]");
        return;
    }
    _lb->setText("");
    QPixmap scaled = pix.scaled(QSize(PIC_MAX_WIDTH, PIC_MAX_HEIGHT),
                                Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // ★ 必须同步重设 _lb 的尺寸，否则 initEmpty 里 setFixedSize(160,90) 的 label
    //   会按 scaledContents=true 把缩放后的图片再抻回 160×90，而气泡却被缩到
    //   scaled 尺寸（比 label 小）→ 气泡只看得见 label 顶部/左侧的一小块，
    //   表现为"图片显示不全"。
    _lb->setFixedSize(scaled.size());
    // 图片已经过 QPainter 缩放到目标尺寸，不需要 QLabel 再缩放；关掉 scaledContents
    // 避免任何二次拉伸/走样。
    _lb->setScaledContents(false);
    // 去掉占位灰底：圆角 pixmap 的透明角应该露出气泡底色（自己#EEF0FB/对方白），
    // 而不是占位灰 #f0f0f0。
    _lb->setStyleSheet("QLabel{background:transparent;border-radius:8px;}");

    // 圆角裁剪：QLabel 的 qss border-radius 裁不了 setPixmap 的内容，
    // 必须把 pixmap 本身画进带圆角裁剪路径的透明 pixmap 里。
    // 半径与气泡圆角(9px)呼应，视觉上图片像"嵌"在气泡里。
    QPixmap rounded(scaled.size());
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, scaled.width(), scaled.height()), 8, 8);
    painter.setClipPath(clip);
    painter.drawPixmap(0, 0, scaled);
    painter.end();
    _lb->setPixmap(rounded);

    int left_margin = this->layout()->contentsMargins().left();
    int right_margin = this->layout()->contentsMargins().right();
    int v_margin = this->layout()->contentsMargins().bottom();
    setFixedSize(scaled.width() + left_margin + right_margin,
                 scaled.height() + v_margin * 2);
}

bool PictureBubble::eventFilter(QObject *o, QEvent *e)
{
    if (o == _lb && e->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::LeftButton && !_full_pix.isNull()) {
            openImageLightbox(_full_pix, this->window());
        }
    }
    return BubbleFrame::eventFilter(o, e);
}