#include "MessageTextEdit.h"
#include <QDebug>
#include <QMessageBox>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QVariant>


MessageTextEdit::MessageTextEdit(QWidget *parent)
    : QTextEdit(parent)
{

    //this->setStyleSheet("border: none;");
    this->setMaximumHeight(60);

//    connect(this,SIGNAL(textChanged()),this,SLOT(textEditChanged()));

}

MessageTextEdit::~MessageTextEdit()
{

}

QVector<MsgInfo> MessageTextEdit::getMsgList()
{
    mGetMsgList.clear();

    QString doc = this->document()->toPlainText();
    QString text="";//存储文本信息
    int indexUrl = 0;
    //这个是存储的富文本信息，包括图片的url以及文件的url
    int count = mMsgList.size();

    for(int index=0; index<doc.size(); index++)
    {
        //如果遇到替换符，说明后面的是图片或者文件的url
        if(doc[index]==QChar::ObjectReplacementCharacter)
        {
            if(!text.isEmpty())
            {
                QPixmap pix;
                insertMsgList(mGetMsgList,"text",text,pix);
                text.clear();
            }
            while(indexUrl<count)
            {
                MsgInfo msg =  mMsgList[indexUrl];
                if(this->document()->toHtml().contains(msg.content,Qt::CaseSensitive))
                {
                    indexUrl++;
                    mGetMsgList.append(msg);
                    break;
                }
                indexUrl++;
            }
        }
        else
        {
            //追加字符到文本消息中
            text.append(doc[index]);
        }
    }
    if(!text.isEmpty())
    {
        QPixmap pix;
        insertMsgList(mGetMsgList,"text",text,pix);
        text.clear();
    }
    mMsgList.clear();
    this->clear();
    return mGetMsgList;
}

void MessageTextEdit::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->source()==this)
        event->ignore();
    else
        event->accept();
}

void MessageTextEdit::dropEvent(QDropEvent *event)
{
    insertFromMimeData(event->mimeData());
    event->accept();
}

void MessageTextEdit::keyPressEvent(QKeyEvent *e)
{
    if((e->key()==Qt::Key_Enter||e->key()==Qt::Key_Return)&& !(e->modifiers() & Qt::ShiftModifier))
    {
        emit send();
        return;
    }
    QTextEdit::keyPressEvent(e);
}

void MessageTextEdit::insertFileFromUrl(const QStringList &urls)
{
    if(urls.isEmpty())
        return;

    foreach (QString url, urls){
         if(isImage(url))
             insertImages(url);
         else
             insertTextFile(url);
    }
}

void MessageTextEdit::insertImages(const QString &url)
{
    QImage image(url);
    // ★ 必须判空：QImage 构造失败不抛异常，只是一个 null image。
    //   原来不判的话，下面 image.width() 是 0、不进缩放分支，
    //   于是 cursor.insertImage(null) 插一个空白图元，
    //   mMsgList 里还记了一条 "image" —— 用户点发送会得到一个空图片消息，
    //   而且界面上看不出哪里出了问题。拖一个不支持的格式进来就会这样。
    if (image.isNull()) {
        QMessageBox::warning(this, tr("提示"),
                             tr("无法加载这张图片：\n%1\n\n请确认文件没损坏，且格式受支持（png/jpg/bmp/webp）。").arg(url));
        return;
    }
    //按比例缩放图片
    if(image.width()>120||image.height()>80)
    {
        if(image.width()>image.height())
        {
          image =  image.scaledToWidth(120,Qt::SmoothTransformation);
        }
        else
            image = image.scaledToHeight(80,Qt::SmoothTransformation);
    }
    QTextCursor cursor = this->textCursor();
    // QTextDocument *document = this->document();
    // document->addResource(QTextDocument::ImageResource, QUrl(url), QVariant(image));
    cursor.insertImage(image,url);

    insertMsgList(mMsgList,"image",url,QPixmap::fromImage(image));
}

void MessageTextEdit::insertTextFile(const QString &url)
{
    QFileInfo fileInfo(url);
    if(fileInfo.isDir())
    {
        QMessageBox::information(this,"提示","只允许拖拽单个文件!");
        return;
    }

    // ★ 20MB：与 UploadManager::MAX_FILE_SIZE 以及服务端 MAX_UPLOAD_BYTES 保持一致，
    //   否则会出现"客户端放行了、服务端却拒收"的割裂体验。
    if(fileInfo.size()>20*1024*1024)
    {
        QMessageBox::information(this,"提示","发送的文件大小不能大于20M");
        return;
    }

    QPixmap pix = getFileIconPixmap(url);
    QTextCursor cursor = this->textCursor();
    cursor.insertImage(pix.toImage(),url);
    insertMsgList(mMsgList,"file",url,pix);
}

bool MessageTextEdit::canInsertFromMimeData(const QMimeData *source) const
{
    return QTextEdit::canInsertFromMimeData(source);
}

void MessageTextEdit::insertFromMimeData(const QMimeData *source)
{
    // ★ 之前这里只在"识别到文件 URL"时才做事，否则直接 return，
    //   把基类 Paste 调用也吞掉了 —— 导致 CTRL+V 粘贴纯文字完全没反应（输入框"粘贴不了"）。
    //   正确做法：文件/图片走项目自己的插入逻辑；其余（纯文本/HTML/图片剪贴板）
    //   一律交给 QTextEdit 基类，保证普通粘贴正常。
    //   用 source->hasUrls() 判断"带文件"，比旧代码在 source->text() 里找 "///" 的 hack
    //   更可靠，且能正确覆盖"从资源管理器 Ctrl+V 粘贴文件"的场景。
    if (source->hasUrls()) {
        QStringList localUrls;
        for (const QUrl &u : source->urls()) {
            if (u.isLocalFile()) {
                localUrls.append(u.toLocalFile());
            }
        }
        if (!localUrls.isEmpty()) {
            insertFileFromUrl(localUrls);   // 内部按图片/文件分别处理
            return;
        }
    }
    // ★ 剪贴板里的图片（截图工具 / 复制的图片）：基类 QTextEdit 会把它直接画进文档，
    //   但不会登记进 mMsgList —— getMsgList() 只看到一个占位符、拿不到内容，
    //   结果"粘贴的图片发不出去"。这里先落盘成临时 PNG，再走和拖入图片完全相同的链路。
    //   注意要放在 hasUrls 分支【之后】：从资源管理器复制的图片文件同时带 url，
    //   那种情况已经有真实路径了，没必要再落一份临时文件。
    if (!source->hasUrls() && source->hasImage()) {
        QVariant imgVar = source->imageData();
        QImage img;
        if (imgVar.canConvert<QImage>()) {
            img = qvariant_cast<QImage>(imgVar);
        } else if (imgVar.canConvert<QPixmap>()) {
            img = qvariant_cast<QPixmap>(imgVar).toImage();
        }
        if (!img.isNull()) {
            QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            QDir().mkpath(dir);
            QString path = dir + QDir::separator()
                         + QString("paste_%1.png").arg(QDateTime::currentMSecsSinceEpoch());
            if (img.save(path, "PNG")) {
                insertImages(path);
                return;
            }
        }
    }
    // 纯文本 / HTML / 图片剪贴板等：交还基类标准粘贴
    QTextEdit::insertFromMimeData(source);
}

bool MessageTextEdit::isImage(QString url)
{
    QString imageFormat = "bmp,jpg,png,tif,gif,pcx,tga,exif,fpx,svg,psd,cdr,pcd,dxf,ufo,eps,ai,raw,wmf,webp";
    QStringList imageFormatList = imageFormat.split(",");
    QFileInfo fileInfo(url);
    QString suffix = fileInfo.suffix();
    if(imageFormatList.contains(suffix,Qt::CaseInsensitive)){
        return true;
    }
    return false;
}

void MessageTextEdit::insertMsgList(QVector<MsgInfo> &list, QString flag, QString text, QPixmap pix)
{
    MsgInfo msg;
    msg.msgFlag=flag;
    msg.content = text;
    msg.pixmap = pix;
    list.append(msg);
}

QPixmap MessageTextEdit::getFileIconPixmap(const QString &url)
{
    QFileIconProvider provder;
    QFileInfo fileinfo(url);
    QIcon icon = provder.icon(fileinfo);

    QString strFileSize = getFileSize(fileinfo.size());
    //qDebug() << "FileSize=" << fileinfo.size();

    QFont font(QString("宋体"),10,QFont::Normal,false);
    QFontMetrics fontMetrics(font);
    QSize textSize = fontMetrics.size(Qt::TextSingleLine, fileinfo.fileName());

    QSize FileSize = fontMetrics.size(Qt::TextSingleLine, strFileSize);
    int maxWidth = textSize.width() > FileSize.width() ? textSize.width() :FileSize.width();
    QPixmap pix(50 + maxWidth + 10, 50);
    pix.fill();

    QPainter painter;
   // painter.setRenderHint(QPainter::Antialiasing, true);
    //painter.setFont(font);
    painter.begin(&pix);
    // 文件图标
    QRect rect(0, 0, 50, 50);
    painter.drawPixmap(rect, icon.pixmap(40,40));
    painter.setPen(Qt::black);
    // 文件名称
    QRect rectText(50+10, 3, textSize.width(), textSize.height());
    painter.drawText(rectText, fileinfo.fileName());
    // 文件大小
    QRect rectFile(50+10, textSize.height()+5, FileSize.width(), FileSize.height());
    painter.drawText(rectFile, strFileSize);
    painter.end();
    return pix;
}

QString MessageTextEdit::getFileSize(qint64 size)
{
    QString Unit;
    double num;
    if(size < 1024){
        num = size;
        Unit = "B";
    }
    else if(size < 1024 * 1224){
        num = size / 1024.0;
        Unit = "KB";
    }
    else if(size <  1024 * 1024 * 1024){
        num = size / 1024.0 / 1024.0;
        Unit = "MB";
    }
    else{
        num = size / 1024.0 / 1024.0/ 1024.0;
        Unit = "GB";
    }
    return QString::number(num,'f',2) + " " + Unit;
}

void MessageTextEdit::textEditChanged()
{
    //qDebug() << "text changed!" << endl;
}
