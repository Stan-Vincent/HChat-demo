#include "PictureBubble.h"
#include <QLabel>
#include <QHash>


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
    // 占位：一块固定大小的灰底，避免气泡在图片到达前高度为 0 造成列表跳动
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
    if (pix.isNull()) {
        // 下载失败：占位改成失败提示，别留一个空白方块让人以为还在加载
        _lb->setText("[图片加载失败]");
        return;
    }
    _lb->setText("");
    QPixmap scaled = pix.scaled(QSize(PIC_MAX_WIDTH, PIC_MAX_HEIGHT),
                                Qt::KeepAspectRatio, Qt::SmoothTransformation);
    _lb->setPixmap(scaled);

    int left_margin = this->layout()->contentsMargins().left();
    int right_margin = this->layout()->contentsMargins().right();
    int v_margin = this->layout()->contentsMargins().bottom();
    setFixedSize(scaled.width() + left_margin + right_margin,
                 scaled.height() + v_margin * 2);
}