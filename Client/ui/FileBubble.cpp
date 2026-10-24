#include "FileBubble.h"
#include <QLabel>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QApplication>
#include <QStyle>
#include <QEvent>
#include <QMouseEvent>
#include <QFontMetrics>

#define FILE_BUBBLE_WIDTH  210
#define FILE_BUBBLE_HEIGHT 56
#define FILE_ICON_SIZE     40

QString FileBubble::formatSize(qint64 bytes)
{
    if (bytes < 0) {
        return QString("未知大小");
    }
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    }
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

QPixmap FileBubble::iconForName(const QString &fileName)
{
    // QFileIconProvider 能按扩展名给出系统图标，即使文件在本地不存在
    // （接收方只有文件名，没有实体文件）
    QFileIconProvider provider;
    QIcon icon = provider.icon(QFileInfo(fileName));
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    }
    return icon.pixmap(FILE_ICON_SIZE, FILE_ICON_SIZE);
}

FileBubble::FileBubble(const QString &fileName, qint64 size, const QPixmap &icon,
                       ChatRole role, QWidget *parent)
    : BubbleFrame(role, parent), _file_name(fileName), _size(size)
{
    initUi(icon.isNull() ? iconForName(fileName) : icon, role);
}

FileBubble::FileBubble(const QString &url, const QString &fileName, qint64 size,
                       ChatRole role, QWidget *parent)
    : BubbleFrame(role, parent), _file_name(fileName), _url(url), _size(size)
{
    initUi(iconForName(fileName), role);
}

void FileBubble::setFileUrl(const QString &url)
{
    _url = url;
}

void FileBubble::initUi(const QPixmap &icon, ChatRole role)
{
    Q_UNUSED(role);
    _body = new QWidget();
    _body->setCursor(Qt::PointingHandCursor);
    _body->setToolTip(QString("点击：打开 / 另存为..."));   // 提示可点，否则看不出是个可交互控件
    _body->installEventFilter(this);

    _icon_lb = new QLabel(_body);
    _icon_lb->setFixedSize(FILE_ICON_SIZE, FILE_ICON_SIZE);
    _icon_lb->setAlignment(Qt::AlignCenter);
    if (!icon.isNull()) {
        _icon_lb->setPixmap(icon.scaled(FILE_ICON_SIZE, FILE_ICON_SIZE,
                                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        _icon_lb->setText("...");
    }

    // 右侧列可用宽度 = 总宽 - 图标 - 左右margin(6*2) - 图标与文字间距(10)
    const int name_w = FILE_BUBBLE_WIDTH - FILE_ICON_SIZE - 22;

    _name_lb = new QLabel(_body);
    QFont nameFont = _name_lb->font();
    nameFont.setPixelSize(13);
    _name_lb->setFont(nameFont);
    _name_lb->setStyleSheet("QLabel{color:#1f1f1f;background:transparent;}");
    _name_lb->setFixedWidth(name_w);
    // 文件名过长时中间省略（保留扩展名，比尾部省略有用）
    QFontMetrics fm(nameFont);
    _name_lb->setText(fm.elidedText(_file_name, Qt::ElideMiddle, name_w));

    _size_lb = new QLabel(_body);
    QFont sizeFont = _size_lb->font();
    sizeFont.setPixelSize(11);
    _size_lb->setFont(sizeFont);
    _size_lb->setStyleSheet("QLabel{color:#999999;background:transparent;}");
    _size_lb->setText(formatSize(_size));

    auto *right = new QVBoxLayout();
    right->setSpacing(2);
    right->setContentsMargins(0, 0, 0, 0);
    right->addWidget(_name_lb);
    right->addWidget(_size_lb);
    right->addStretch();

    auto *h = new QHBoxLayout(_body);
    h->setSpacing(10);
    h->setContentsMargins(6, 6, 6, 6);
    h->addWidget(_icon_lb);
    h->addLayout(right);

    this->setWidget(_body);

    int left_margin = this->layout()->contentsMargins().left();
    int right_margin = this->layout()->contentsMargins().right();
    int v_margin = this->layout()->contentsMargins().bottom();
    setFixedSize(FILE_BUBBLE_WIDTH + left_margin + right_margin,
                 FILE_BUBBLE_HEIGHT + v_margin * 2);
}

bool FileBubble::eventFilter(QObject *o, QEvent *e)
{
    if (o == _body && e->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::LeftButton) {
            emit sig_open_file(_url, _file_name);
            return true;   // 吞掉，避免冒泡触发外层（如消息选中）逻辑
        }
    }
    return BubbleFrame::eventFilter(o, e);
}
