#ifndef FILEBUBBLE_H
#define FILEBUBBLE_H

#include "BubbleFrame.h"
#include <QPixmap>
#include <QString>

class QLabel;

/**
 * 文件气泡
 *
 * 【和图片气泡的区别】图片气泡要异步下完图才能显示内容；文件气泡不需要下载就能
 * 完整显示——因为文件名和大小都随消息内容一起带过来了（msg_type=2 的 content
 * 是一个 JSON：{url, name, size}）。真正需要下载的是【点击之后】打开文件本体。
 *
 * 【为什么 content 要塞 JSON】服务端只存一个 url（如 upload/1728..._ab12.pdf），
 * 接收方单看 url 拿不到"用户原本的文件名"和"大小"（服务端的 unique_name 是
 * 时间戳+hash，不是原文件名）。所以发送方把这三项一起打包进 content。
 *
 * 【点击行为】只发 sig_open_file 信号，不自己下载。下载/保存/打开交给 ChatPage，
 * 好处是气泡不需要持有网络对象，切会话销毁时不会出现回调到已死对象的问题。
 */
class FileBubble : public BubbleFrame
{
    Q_OBJECT
public:
    // 本地发送：name/size/icon 已知，url 要等上传成功，之后由 setFileUrl() 回填
    FileBubble(const QString &fileName, qint64 size, const QPixmap &icon,
               ChatRole role, QWidget *parent = nullptr);

    // 接收：url/name/size 都从消息 content 解析出来，图标按扩展名取
    FileBubble(const QString &url, const QString &fileName, qint64 size,
               ChatRole role, QWidget *parent = nullptr);

    // 上传成功后回填服务端地址，之后点击才能下载
    void setFileUrl(const QString &url);

    const QString& fileName() const { return _file_name; }
    const QString& fileUrl()  const { return _url; }
    qint64 fileSize() const { return _size; }

    // 把字节数显示成 B/KB/MB
    static QString formatSize(qint64 bytes);
    // 按文件名（主要看扩展名）取系统图标；取不到就退回通用文件图标
    static QPixmap iconForName(const QString &fileName);

signals:
    // 点击气泡：请求下载并打开。url 为空表示还没上传完，ChatPage 会提示稍等。
    void sig_open_file(const QString &url, const QString &fileName);

private:
    void initUi(const QPixmap &icon, ChatRole role);
    bool eventFilter(QObject *o, QEvent *e) override;

    QWidget *_body = nullptr;
    QLabel  *_icon_lb = nullptr;
    QLabel  *_name_lb = nullptr;
    QLabel  *_size_lb = nullptr;
    QString  _file_name;
    QString  _url;
    qint64   _size = 0;
};

#endif // FILEBUBBLE_H
