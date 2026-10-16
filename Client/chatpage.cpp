#include "chatpage.h"
#include <QDateTime>
#include <QTimer>
#include "ui_chatpage.h"
#include <QStyleOption>
#include <QPainter>
#include "ChatItemBase.h"
#include "TextBubble.h"
#include "PictureBubble.h"
#include "imagemanager.h"
#include "applyfrienditem.h"
#include "usermgr.h"
#include <QJsonArray>
#include <QJsonObject>
#include "tcpmgr.h"
#include <QUuid>
#include <QPointer>
#include "logcategory.h"
#include "toast.h"
#include "clickedlabel.h"
#include "FileBubble.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMessageBox>
#include <QMenu>
#include <QCursor>
#include <QProgressDialog>
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QClipboard>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QHBoxLayout>
#include <QDateTime>
#include "PictureBubble.h"

namespace {

// ★ 图片气泡自己内部已有灯箱（PictureBubble::eventFilter → openImageLightbox），
//   这里【不要】再挂一层左键点击，否则点一下会弹两个窗口。

} // namespace

ChatPage::ChatPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatPage)
{
    ui->setupUi(this);
    //设置按钮样式
    // ★ 「接收」是 llfc 教程遗留的【调试按钮】：把输入框里的内容就地渲染成
    //   "对方发来的消息"（左侧气泡），全程不走网络、不落库。真实收发链路已通，
    //   留着只会让用户误触把假消息混进会话，所以隐藏 + 禁用双重保险。
    //   调试时把下面两行注释掉（或临时改 true）即可恢复。
    ui->receive_btn->SetState("normal","hover","press");
    ui->receive_btn->setVisible(false);
    ui->receive_btn->setEnabled(false);
    ui->send_btn->SetState("normal","hover","press");

    //设置图标样式
    ui->emo_lb->SetState("normal","hover","press","normal","hover","press");
    ui->file_lb->SetState("normal","hover","press","normal","hover","press");

    // 表情功能：把已有的表情按钮接上 EmojiPanel 弹出层
    _emo_panel = new EmojiPanel(this);
    _emo_panel->hide();
    connect(ui->emo_lb, &ClickedLabel::clicked, this, &ChatPage::slot_toggle_emoji_panel);
    connect(_emo_panel, &EmojiPanel::sig_emoji_selected, this, &ChatPage::slot_insert_emoji);
    connect(_emo_panel, &EmojiPanel::sig_panel_hidden, this, &ChatPage::slot_emo_panel_hidden);

    // 文件功能：文件夹图标 → 打开文件选择框（多选），选中的文件插入输入框
    connect(ui->file_lb, &ClickedLabel::clicked, this, &ChatPage::slot_pick_file);
    _nam = new QNetworkAccessManager(this);   // 文件消息下载用

    // ★ 回车发送。MessageTextEdit::keyPressEvent 里其实早就实现了：
    //     Enter / Ctrl+Enter → emit send()
    //     Shift+Enter        → 不拦截，交回 QTextEdit 基类插入换行
    //   但之前这个 send() 信号【没接任何槽】，所以按回车看着像"没反应"。
    connect(ui->chatEdit, &MessageTextEdit::send, this, &ChatPage::on_send_btn_clicked);

    // ★ 人性化：输入框占位提示，把快捷键和"可拖文件"这件事直接写在框里
    ui->chatEdit->setPlaceholderText(QString("输入消息，Enter 发送 / Shift+Enter 换行（可拖入文件或 Ctrl+V 粘贴）"));
}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::slot_toggle_emoji_panel(QString, ClickLbState)
{
    if (_emo_panel->isVisible()) {
        _emo_panel->hide();
        return;
    }
    // 锚定在表情按钮正上方居中对齐
    QPoint globalPos = ui->emo_lb->mapToGlobal(QPoint(0, 0));
    int x = globalPos.x() - (_emo_panel->width() - ui->emo_lb->width()) / 2;
    if (x < 4) x = 4;
    int y = globalPos.y() - _emo_panel->height() - 6;
    if (y < 4) y = globalPos.y() + ui->emo_lb->height() + 6; // 上方不够则翻到下方
    _emo_panel->move(x, y);
    _emo_panel->show();
}

void ChatPage::slot_insert_emoji(const QString &emoji)
{
    // emoji 作为普通文本插入光标处，走现有 TEXT 消息链路（getMsgList 会把它并入文本段）
    // ★ 不要在这里 setFocus() 到 chatEdit：那会把焦点移出 Qt::Popup，触发 popup
    //   失焦自动关闭，导致"点一个表情面板就消失、无法连续插入"。insertPlainText 不依赖焦点。
    ui->chatEdit->insertPlainText(emoji);
    // 面板保持打开，方便连续插入多个表情；点击面板外会自动关闭。
}

void ChatPage::slot_emo_panel_hidden()
{
    // 面板因点击外部/Esc/再次点击按钮而关闭时，把表情按钮状态复位成 Normal
    ui->emo_lb->ResetNormalState();
}

void ChatPage::slot_pick_file(QString, ClickLbState)
{
    // 多选。insertFileFromUrl() 内部会按扩展名分流：
    //   图片 → insertImages（缩放后内联显示）
    //   其它 → insertTextFile（判目录、判大小上限、取系统图标、登记为 "file"）
    // 所以点图标 / 拖拽 / Ctrl+V 三条路最终走的是同一套逻辑。
    QStringList paths = QFileDialog::getOpenFileNames(this, QString("选择要发送的文件"));
    if (paths.isEmpty()) {
        return;
    }
    ui->chatEdit->insertFileFromUrl(paths);
    // 这里可以 setFocus：不是 popup，选完文件本来就该回到输入框
    ui->chatEdit->setFocus();
}

// ★ 点击文件气泡：不再"默默下载到下载目录再打开"，改成先弹菜单让用户选。
//   原来直接下载 + 直接打开有两个问题：①用户不知道文件被存到哪了；
//   ②.exe/脚本之类直接打开有风险。现在：
//     - 打开     → 下载到系统下载目录（同名自动 (1)(2)）后用系统默认程序打开
//     - 另存为… → 弹出保存对话框自己选位置，存完再问是否打开
void ChatPage::slot_open_file(const QString& url, const QString& fileName)
{
    if (url.isEmpty()) {
        // 自己刚发的、还在上传中的文件：服务端地址还没回来，无可下载的目标
        QMessageBox::information(this, QString("提示"), QString("文件还在上传中，请稍后再点击"));
        return;
    }

    QMenu menu(this);
    QAction* actOpen = menu.addAction(QString("打开"));
    QAction* actSave = menu.addAction(QString("另存为..."));
    QAction* chosen = menu.exec(QCursor::pos());
    if (chosen == nullptr) {
        return;   // 点了菜单外 / Esc
    }

    if (chosen == actSave) {
        QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (defaultDir.isEmpty()) {
            defaultDir = QDir::homePath();
        }
        QString defaultPath = defaultDir + QDir::separator() + fileName;
        QString savePath = QFileDialog::getSaveFileName(this, QString("保存文件"), defaultPath);
        if (savePath.isEmpty()) {
            return;
        }
        // 存完再问一句是否打开，省得用户还要自己去目录里翻
        downloadFile(url, savePath, /*openAfter=*/false, /*askOpen=*/true);
        return;
    }

    if (chosen == actOpen) {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (dir.isEmpty()) {
            dir = QDir::homePath();
        }
        QDir().mkpath(dir);

        // 同名文件追加 (1)(2)...，避免静默覆盖已有文件
        QString savePath = dir + QDir::separator() + fileName;
        QFileInfo fi(savePath);
        int n = 1;
        while (QFile::exists(savePath)) {
            savePath = dir + QDir::separator() + fi.completeBaseName()
                     + QString("(%1)").arg(n++)
                     + (fi.suffix().isEmpty() ? QString() : QString(".") + fi.suffix());
        }
        downloadFile(url, savePath, /*openAfter=*/true, /*askOpen=*/false);
    }
}

// 下载到指定路径。openAfter=直接打开，askOpen=存完弹窗询问是否打开。
void ChatPage::downloadFile(const QString& url, const QString& savePath,
                            bool openAfter, bool askOpen)
{
    // url 是服务端返回的相对路径（如 upload/1728..._ab12.pdf），拼上网关前缀
    QUrl full(QString("%1/%2").arg(gate_url_prefix, url));
    QNetworkReply* reply = _nam->get(QNetworkRequest(full));
    QPointer<ChatPage> self(this);

    // ★ 人性化：20MB 的文件下载要走网络，没有进度反馈的话界面像"卡住了"。
    QProgressDialog* dlg = new QProgressDialog(
        QString("正在下载 %1...").arg(QFileInfo(savePath).fileName()),
        QString("取消"), 0, 100, this);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setAutoClose(true);
    dlg->setAutoReset(true);
    dlg->setMinimumDuration(300);   // 300ms 内下完就不弹出来，避免闪一下
    dlg->setValue(0);
    connect(dlg, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::downloadProgress, dlg,
            [dlg](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            dlg->setValue(int(bytesReceived * 100 / bytesTotal));
        }
    });

    connect(reply, &QNetworkReply::finished, this,
            [self, reply, savePath, openAfter, askOpen, dlg]() {
        reply->deleteLater();
        dlg->deleteLater();
        if (!self) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(self, QString("提示"),
                                 QString("下载失败：%1").arg(reply->errorString()));
            return;
        }
        QByteArray data = reply->readAll();
        QFile f(savePath);
        if (!f.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(self, QString("提示"),
                                 QString("无法写入文件：%1").arg(savePath));
            return;
        }
        f.write(data);
        f.close();
        qCDebug(lcChat) << "file downloaded:" << savePath << data.size() << "bytes";

        bool doOpen = openAfter;
        if (askOpen) {
            doOpen = (QMessageBox::question(self, QString("提示"),
                        QString("已保存到：\n%1\n\n是否现在打开文件？").arg(savePath),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                      == QMessageBox::Yes);
        }
        if (doOpen) {
            // 用系统默认程序打开
            QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
        }
    });
}

void ChatPage::SetChatData(std::shared_ptr<ChatThreadData> chat_data) {
    // ★ 草稿：切走之前把输入框内容按【会话】存起来，切回来再还原。
    //   原来输入框内容跨会话共用 —— 在 A 会话打一半切到 B，字会跟着跑到 B，
    //   再切回 A 也没了。只存纯文本：图片/文件是临时资产，不做草稿。
    if (_chat_data) {
        saveDraft(_chat_data->GetThreadId());
    }

    _chat_data = chat_data;
    _last_section_date = QDate();   // 时间分隔条状态随会话切换重置
    auto other_id = _chat_data->GetOtherId();
    if(other_id == 0) {
        //说明是群聊
        ui->title_lb->setText(_chat_data->GetGroupName());
        //todo...加载群聊信息和成员信息
        return;
    }

    // 还原这个会话的草稿（没有草稿就清空输入框）
    restoreDraft(_chat_data->GetThreadId());

    //私聊
    auto friend_info = UserMgr::GetInstance()->GetFriendById(other_id);
    if (friend_info == nullptr) {
        return;
    }
    ui->title_lb->setText(friend_info->GetDisplayName());
    ui->chat_data_list->removeAllItem();
    _unrsp_item_map.clear();
    _resend_ctx.clear();   // 切换会话：作废上一段会话的可重发上下文（旧气泡已被销毁）
    _rendered_ids.clear(); // 去重表随列表一起作废
    for(auto & msg : chat_data->GetMsgMapRef()){
        AppendChatMsg(msg);
    }

    for (auto& msg : chat_data->GetMsgUnRspRef()) {
        AppendChatMsg(msg);
    }

    // ★ 人性化：切到某个会话后光标直接落在输入框，不用再点一下才能打字。
    //   只在页面可见时抢焦点，避免加载阶段的隐藏调用干扰别的输入控件。
    if (isVisible()) {
        ui->chatEdit->setFocus();
    }
}

//按消息类型构造气泡。
//文本：直接new TextBubble。
//图片：new PictureBubble(url) 先占位，再异步下载回填。
//  回调用 QPointer 兜底 —— 下载是异步的，回包到达时气泡/页面可能已经被销毁，
//  无保护的话就是一次 use-after-free。
QWidget* ChatPage::makeBubble(ChatMsgType type, const QString& content, ChatRole role)
{
    if (type == ChatMsgType::TEXT) {
        return new TextBubble(role, content);
    }
    if (type == ChatMsgType::PIC) {
        if (content.isEmpty()) {
            qCDebug(lcChat) << "makeBubble: PIC message with empty content";
            return nullptr;
        }
        auto* bubble = new PictureBubble(content, role);
        // 构造函数内部已经查过 _pixmap_cache，命中就直接填好了，不用再发请求
        if (!PictureBubble::_pixmap_cache.contains(content)) {
            QPointer<PictureBubble> guard(bubble);
            ImageManager::instance()->downloadImage(
                content, [guard, content](bool ok, const QPixmap& pix, const QString& err) {
                    if (!guard) {
                        return;         // 气泡已被销毁
                    }
                    if (!ok) {
                        qCDebug(lcChat) << "load bubble image failed:" << err;
                    }
                    // 存进内存缓存：同一张图常被多条消息引用（自己发的+对方发的+历史），
                    // 不缓存的话每条消息都会重新下一次
                    if (ok && !pix.isNull()) {
                        PictureBubble::_pixmap_cache.insert(content, pix);
                    }
                    guard->SetPixmap(pix);   // 失败时 pix 为空，SetPixmap 内部会显示失败提示
                });
        }
        return bubble;
    }
    if (type == ChatMsgType::FILE) {
        // content 是发送方打包的 JSON：{url, name, size}
        QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
        if (doc.isNull() || !doc.isObject()) {
            qCDebug(lcChat) << "makeBubble: FILE content is not JSON:" << content;
            return nullptr;
        }
        QJsonObject o = doc.object();
        QString url = o.value("url").toString();
        if (url.isEmpty()) {
            qCDebug(lcChat) << "makeBubble: FILE content has no url:" << content;
            return nullptr;
        }
        QString name = o.value("name").toString();
        qint64 size = (qint64)o.value("size").toDouble();
        // 兜底：老数据/异常数据没有 name 时，用 url 最后一段当文件名
        if (name.isEmpty()) {
            name = url.section('/', -1);
        }
        return new FileBubble(url, name, size, role);
    }
    // 其它未知类型暂不显示
    return nullptr;
}

void ChatPage::AppendChatMsg(std::shared_ptr<ChatDataBase> msg)
{
    // ★ msg 与 self_info 都要判空：切会话时 GetMsgUnRspRef() 里可能残留空指针，
    //   GetUserInfo() 在登录回包到达之前也是空的。
    if (msg == nullptr) {
        return;
    }
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    if (self_info == nullptr) {
        qCDebug(lcChat) << "AppendChatMsg: self info is null";
        return;
    }
    // ★ 空/纯空白的文本消息不渲染：库里存在历史脏数据（空串、空格），
    //   渲染出来就是截图里那种"空白气泡"，还占一行位置。
    if (msg->GetMsgType() == ChatMsgType::TEXT && msg->GetMsgContent().trimmed().isEmpty()) {
        qCDebug(lcChat) << "AppendChatMsg: skip empty text msg, from uid =" << msg->GetSendUid();
        return;
    }
    // ★ 去重：同一条消息（unique_id 相同）只渲染一次。
    //   历史消息表和未回执表可能同时持有同一条，不去重界面上就会出两条一模一样的气泡。
    const QString unique_key = msg->GetUniqueId();
    if (!unique_key.isEmpty()) {
        if (_rendered_ids.contains(unique_key)) {
            return;
        }
        _rendered_ids.insert(unique_key);
    }
    ChatRole role;
    if (msg->GetSendUid() == self_info->_uid) {
        role = ChatRole::Self;
    }
    else {
        role = ChatRole::Other;
    }

    QString userName;
    QString userIcon;
    if (role == ChatRole::Self) {
        userName = self_info->GetDisplayName();
        userIcon = self_info->_icon;
    }
    else {
        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->GetSendUid());
        if (friend_info == nullptr) {
            // ★ 对方还没进好友列表（比如消息先于 AuthFriend 回包到达）。
            //   这里必须 return：继续走下去 pChatItem 的用户名/头像都是空的。
            qCDebug(lcChat) << "AppendChatMsg: friend not found, uid =" << msg->GetSendUid();
            return;
        }
        userName = friend_info->GetDisplayName();
        userIcon = friend_info->_icon;
    }

    // ★ 统一走工厂：文本和图片都会返回非空，FILE 才返回 nullptr。
    //   原来两个分支各写一遍 if(TEXT)，非文本时 pBubble 是 nullptr，
    //   setWidget(nullptr) 之后 setFixedSize 用空指针 —— 必崩。
    QWidget* pBubble = makeBubble(msg->GetMsgType(), msg->GetMsgContent(), role);
    if (pBubble == nullptr) {
        return;
    }
    // 文件气泡：点击 → 下载并打开。信号只能在【这里】连，
    // 因为 makeBubble 是 static，拿不到 this。
    if (auto* fb = qobject_cast<FileBubble*>(pBubble)) {
        connect(fb, &FileBubble::sig_open_file, this, &ChatPage::slot_open_file);
    }
    // ★ 人性化：右键菜单（复制文本 / 图片另存为 / 文件打开·另存为）+ 图片点击看大图
    attachBubbleActions(pBubble, msg);

    ChatItemBase* pChatItem = new ChatItemBase(role);
    pChatItem->setUserName(userName);
    // ★ userIcon 是服务端 URL，不能 QPixmap(url) 当本地文件读（必空白）。
    //   走 ImageManager 异步拉，QPointer 兜底防 use-after-free。
    if (!userIcon.isEmpty()) {
        QPointer<ChatItemBase> guard(pChatItem);
        ImageManager::instance()->loadAvatar(
            userIcon, [guard](bool ok, const QPixmap& pix, const QString& err) {
                if (!guard) { return; }
                if (ok && !pix.isNull()) { guard->setUserIcon(pix); }
                else { qCDebug(lcChat) << "ChatPage load msg avatar failed:" << err; }
            });
    }
    pChatItem->setWidget(pBubble);
    auto status = msg->GetStatus();
    pChatItem->setStatus(status);

    // 服务端时间格式 yyyy-MM-dd hh:mm:ss，解析失败按当前时间
    QDateTime when = QDateTime::fromString(msg->GetChatTime(), QString("yyyy-MM-dd hh:mm:ss"));
    if (!when.isValid()) {
        when = QDateTime::currentDateTime();
    }
    // hover 时间胶囊：人性化格式（今天 22:05 / 昨天 14:30 / 10月12日 09:00 ...），
    // 原来直接把服务端原始串 "2026-10-14 22:05:23" 摆上去
    pChatItem->setTime(friendlyTime(when));

    // ★ 时间分隔条
    ensureDateSeparator(when);

    ui->chat_data_list->appendChatItem(pChatItem);
    if (status == 0) {
        _unrsp_item_map[msg->GetUniqueId()] = pChatItem;
    }
}

void ChatPage::UpdateChatStatus(QString unique_id, int status)
{
    auto iter = _unrsp_item_map.find(unique_id);
    if (iter != _unrsp_item_map.end()) {
        iter.value()->setStatus(status);
        _unrsp_item_map.erase(iter);
        // 成功收到回执，这条消息不需要再重发了
        _resend_ctx.remove(unique_id);
    }
}

// 为某条已发出但未收到回执的消息启动超时计时：超过 8s 仍无回执则标记为发送失败
void ChatPage::armResendTimer(const QString& uuid)
{
    QPointer<ChatPage> guard(this);
    QTimer::singleShot(8000, this, [guard, uuid]() {
        if (!guard) {
            return;                                   // 页面已销毁
        }
        if (!guard->_resend_ctx.contains(uuid)) {
            return;                                   // 已成功（被 UpdateChatStatus 清掉）或已被清理
        }
        auto it = guard->_unrsp_item_map.find(uuid);
        if (it != guard->_unrsp_item_map.end()) {
            it.value()->setStatus(MsgStatus::SEND_FAILED);
        }
    });
}

// 点击气泡上的"发送失败"图标触发：把消息按原样（文本直接发 / 图片重新上传）再发一次
void ChatPage::slot_resend(const QString& unique_id)
{
    auto ctxIt = _resend_ctx.find(unique_id);
    if (ctxIt == _resend_ctx.end()) {
        return;
    }
    const ResendContext ctx = ctxIt.value();

    ChatItemBase* item = _unrsp_item_map.value(unique_id, nullptr);
    if (item == nullptr) {
        return;
    }
    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    if (user_info == nullptr) {
        return;
    }

    // 先把气泡状态改回"发送中"，并确保在未回复表里（防止被意外清掉）
    item->setStatus(0);
    _unrsp_item_map[unique_id] = item;

    // 实际发包：content 对文本是文本内容，对图片是服务端 URL
    auto sendReq = [this, unique_id, ctx, uid = user_info->_uid](const QString& content) {
        QJsonObject obj;
        obj["content"] = content;
        obj["unique_id"] = unique_id;
        if (ctx.type == ChatMsgType::PIC) {
            obj["msg_type"] = 1;
        }
        QJsonArray arr;
        arr.append(obj);
        QJsonObject req;
        req["fromuid"] = uid;
        req["touid"] = ctx.other_id;
        req["thread_id"] = ctx.thread_id;
        req["text_array"] = arr;
        QByteArray data = QJsonDocument(req).toJson(QJsonDocument::Compact);
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, data);
        armResendTimer(unique_id);
    };

    if (ctx.type == ChatMsgType::PIC) {
        // 图片重发：先重新上传本地文件，拿到服务端地址再发包
        QPointer<ChatPage> self(this);
        UploadManager::instance()->uploadImage(
            UserMgr::GetInstance()->GetUid(), ctx.content,
            [self, unique_id, ctx, sendReq](bool ok, const QString& urlPath, const QString& errMsg) {
                if (!self) {
                    return;
                }
                if (!ok) {
                    qCDebug(lcChat) << "resend upload image failed:" << errMsg;
                    // 上传失败：维持失败态，等用户再次点击
                    auto it = self->_unrsp_item_map.find(unique_id);
                    if (it != self->_unrsp_item_map.end()) {
                        it.value()->setStatus(MsgStatus::SEND_FAILED);
                    }
                    return;
                }
                if (!ctx.preview.isNull()) {
                    PictureBubble::_pixmap_cache.insert(urlPath, ctx.preview);
                }
                sendReq(urlPath);
            });
    } else {
        sendReq(ctx.content);
    }
}

// 服务端整体回包失败时（err != SUCCESS），把本次会话仍在等待回执的消息全部标记成发送失败
void ChatPage::FailAllPending()
{
    for (auto it = _resend_ctx.begin(); it != _resend_ctx.end(); ++it) {
        auto mapIt = _unrsp_item_map.find(it.key());
        if (mapIt != _unrsp_item_map.end()) {
            mapIt.value()->setStatus(MsgStatus::SEND_FAILED);
        }
    }
}

void ChatPage::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

// 待上传的图片。上传是异步的（走 GateServer 的 /upload_image），
// 所以不能在下面的同步循环里直接发包。
struct PendingImage {
    QString local_path;   // 本地文件路径
    QString uuid;         // 与服务端约定的唯一标识
    QPixmap  preview;     // 本地缩略图
    int other_id = 0;     // 接收方 uid
    int thread_id = 0;
};

void ChatPage::on_send_btn_clicked()
{
    if (_chat_data == nullptr) {
        qCDebug(lcChat) << "on_send_btn_clicked: chat_data is empty";
        return;
    }

    // ★ 断线中（含自动重连的窗口期）直接拦下来：
    //   这时候 _socket.write() 会静默成功但服务端永远收不到，
    //   气泡显示"已发送"却对端看不到，比报错更难排查。
    if (!TcpMgr::GetInstance()->IsConnected()) {
        Toast::showToast(this, "网络连接已断开，消息暂时无法发送", Toast::Warning);
        ui->chatEdit->setFocus();
        return;
    }

    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    // ★ 登录回包没到之前 user_info 是空的，原来直接 user_info->_name 直接崩
    if (user_info == nullptr) {
        qCDebug(lcChat) << "on_send_btn_clicked: user info is null";
        return;
    }
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Self;

    // ★ 空输入直接忽略：什么都不写（或只有空格/换行）点发送，
    //   原来会真发出一条空白消息，渲染出来就是刚清理过的那种"空白气泡"。
    //   注意要在 getMsgList() 【之前】判断 —— 那个函数会顺手清空输入框，
    //   提前 return 才能保住用户敲的内容。
    QString plain = pTextEdit->toPlainText();
    bool hasAttachment = plain.contains(QChar::ObjectReplacementCharacter); // 图片/文件占位符
    if (plain.trimmed().isEmpty() && !hasAttachment) {
        pTextEdit->setFocus();
        return;
    }

    QString userName = user_info->GetDisplayName();
    QString userIcon = user_info->_icon;

    // ★ 原来是文件级全局变量：多个 ChatPage 实例共享一份，
    //   A 页收集的图片可能被 B 页的发送流程带走。改成函数内局部变量。
    QVector<PendingImage> pending_images;
    QVector<PendingFile>  pending_files;

    const QVector<MsgInfo>& msgList = pTextEdit->getMsgList();
    QJsonObject textObj;
    QJsonArray textArray;
    int txt_size = 0;
    auto thread_id = _chat_data->GetThreadId();
    for(int i=0; i<msgList.size(); ++i)
    {
        QString type = msgList[i].msgFlag;

        //★ 长度限制只对【文本】生效。
        //   原来对所有类型都查content.length() > 1024，
        //   但图片的 content 是本地路径，一旦用户把图存在路径很深的目录下
        //   （或目录名本身很长）就会被静默跳过 —— 气泡都不显示，用户以为没点到。
        if (type == "text" && msgList[i].content.length() > 1024) {
            continue;
        }

        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        // ★ userIcon 是服务端 URL，不能 QPixmap(url) 当文件读（空白）。异步拉取。
        if (!userIcon.isEmpty()) {
            QPointer<ChatItemBase> guard(pChatItem);
            ImageManager::instance()->loadAvatar(
                userIcon, [guard](bool ok, const QPixmap& pix, const QString& err) {
                    if (!guard) { return; }
                    if (ok && !pix.isNull()) { guard->setUserIcon(pix); }
                    else { qCDebug(lcChat) << "ChatPage load self msg avatar failed:" << err; }
                });
        }
        QWidget *pBubble = nullptr;
        //生成唯一id
        QUuid uuid = QUuid::createUuid();
        //转为字符串
        QString uuidString = uuid.toString();
        if(type == "text")
        {
            pBubble = new TextBubble(role, msgList[i].content);
            if(txt_size + msgList[i].content.length()> 1024){
                textObj["fromuid"] = user_info->_uid;
                textObj["touid"] = _chat_data->GetOtherId();
                textObj["thread_id"] = thread_id;
                textObj["text_array"] = textArray;
                QJsonDocument doc(textObj);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
                //发送并清空之前累计的文本列表
                txt_size = 0;
                textArray = QJsonArray();
                textObj = QJsonObject();
                //发送tcp请求给chat server
                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
            }

            //将bubble和uid绑定，以后可以等网络返回消息后设置是否送达
            //_bubble_map[uuidString] = pBubble;
            txt_size += msgList[i].content.length();
            QJsonObject obj;
            QByteArray utf8Message = msgList[i].content.toUtf8();
            auto content = QString::fromUtf8(utf8Message);
            obj["content"] = content;
            obj["unique_id"] = uuidString;
            textArray.append(obj);
            //todo... 注意，此处先按私聊处理
            auto txt_msg = std::make_shared<TextChatData>(uuidString, thread_id, ChatFormType::PRIVATE,
                ChatMsgType::TEXT, content, user_info->_uid, 0);
            //将未回复的消息加入到未回复列表中，以便后续处理
            _chat_data->AppendUnRspMsg(uuidString,txt_msg);
        }
        else if(type == "image")
        {
            // ★ 图片消息：本地立刻显示气泡（不等上传），同时收集起来，
            //   循环结束后异步上传，拿到服务端地址再发 msg_type=1 的消息。
            pBubble = new PictureBubble(msgList[i].pixmap, role);
            PendingImage img;
            img.local_path = msgList[i].content;
            img.uuid = uuidString;
            img.preview = msgList[i].pixmap;
            img.other_id = _chat_data->GetOtherId();
            img.thread_id = thread_id;
            pending_images.push_back(img);
            // ★ 图片也登记进未回复表（在下面的统一处理里）：
            //   上传成功后是以 msg_type=1 发给服务端的，
            //   服务端会正常回 ID_TEXT_CHAT_MSG_RSP（带 unique_id），
            //   所以图片是有回执的，能把气泡从"发送中"改成"已送达"。
            // ★ 记录图片重发上下文（local_path + preview）。图片真正发包在上传成功后，
            //   超时计时也在那时启动（见 on_send_btn_clicked 末尾的上传回调）。
            ResendContext img_ctx;
            img_ctx.type = ChatMsgType::PIC;
            img_ctx.content = img.local_path;
            img_ctx.preview = img.preview;
            img_ctx.other_id = img.other_id;
            img_ctx.thread_id = img.thread_id;
            _resend_ctx[uuidString] = img_ctx;
        }
        else if(type == "file")
        {
            QFileInfo fi(msgList[i].content);
            if (!fi.exists() || !fi.isFile()) {
                qCDebug(lcChat) << "file msg: local file missing:" << msgList[i].content;
                continue;
            }
            PendingFile pf;
            pf.local_path = msgList[i].content;
            pf.file_name = fi.fileName();
            pf.size = fi.size();
            pf.icon = msgList[i].pixmap;
            pf.uuid = uuidString;
            pf.other_id = _chat_data->GetOtherId();
            pf.thread_id = thread_id;
            // 先落一个气泡（本地图标 + 文件名 + 大小），服务端 url 等上传成功再回填
            pBubble = new FileBubble(pf.file_name, pf.size, pf.icon, role);
            pf.bubble = qobject_cast<FileBubble*>(pBubble);
            if (pf.bubble) {
                connect(pf.bubble, &FileBubble::sig_open_file,
                        this, &ChatPage::slot_open_file);
            }
            pending_files.push_back(pf);
        }
        //发送消息
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            pChatItem->setStatus(0);
            // 自己刚发出的消息：服务端还没回时间，先用本地时间（人性化格式）
            QDateTime now = QDateTime::currentDateTime();
            pChatItem->setTime(friendlyTime(now));
            // ★ 时间分隔条：自己发的消息也可能跨天（深夜挂机场景）
            ensureDateSeparator(now);
            ui->chat_data_list->appendChatItem(pChatItem);
            // ★ 文本/图片/文件都【有回执】（服务端都会回 ID_TEXT_CHAT_MSG_RSP 带 unique_id），
            //   所以都要登记，才能把状态从"发送中"更新为"已送达"。
            if (type == "text" || type == "image" || type == "file") {
                _unrsp_item_map[uuidString] = pChatItem;
                // —— 发送失败重发相关：绑定 unique_id + 失败重发信号 ——
                //   （_resend_ctx 在各自分支里填好：文本在此处、图片在上面的 image 分支）
                pChatItem->setUniqueId(uuidString);
                connect(pChatItem, &ChatItemBase::sig_resend, this, &ChatPage::slot_resend);
                if (type == "text") {
                    // 文本发包是同步的，直接记录上下文并启动超时计时
                    ResendContext ctx;
                    ctx.type = ChatMsgType::TEXT;
                    ctx.content = msgList[i].content;
                    ctx.other_id = _chat_data->GetOtherId();
                    ctx.thread_id = thread_id;
                    _resend_ctx[uuidString] = ctx;
                    armResendTimer(uuidString);
                }
                // 图片的 _resend_ctx 已在 image 分支填好，超时计时在上传成功发包时启动。
                // 文件（A档）故意不填 _resend_ctx：上传失败直接标成失败态，暂不支持点击重发
                // （slot_resend 找不到 ctx 会安全返回，不会误发包）。
            }
        }

    }

    qCDebug(lcChat) << "textArray is " << textArray ;
    // ★ 只有真的攒到了文本才发文本包。
    //   原来这里是无条件 emit，结果拖图片/文件时循环走的是 image/file 分支，
    //   textArray 始终是空的 —— 每点一次发送就往服务器发一个 {"text_array":[]} 空包，
    //   服务端回空包，而本地图片气泡却被塞进 _unrsp_item_map（在等一个永远不会来的回执）。
    //
    //   ★★ 这里【不能 return】：只发图片时 textArray 本来就是空的，
    //   直接 return 会让下面的图片上传代码永远执行不到 —— 图片只在本地显示，
    //   根本发不出��。所以改成「跳过发文本包」，继续走图片上传。
    if (!textArray.isEmpty()) {
        //发送给服务器
        textObj["text_array"] = textArray;
        textObj["fromuid"] = user_info->_uid;
        textObj["touid"] = _chat_data->GetOtherId();
        textObj["thread_id"] = thread_id;
        QJsonDocument doc(textObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        //发送并清空之前累计的文本列表
        txt_size = 0;
        textArray = QJsonArray();
        textObj = QJsonObject();
        //发送tcp请求给chat server
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
    } else {
        qCDebug(lcChat) << "no text to send, skip empty text request";
    }

    // ---------- 图片：上传成功后再把消息发给服务端 ----------
    // 用 QPointer 兜底：上传是异步的，回包到达时这个页面可能已被销毁。
    QPointer<ChatPage> self(this);
    for (const auto& img : pending_images) {
        UploadManager::instance()->uploadImage(
            UserMgr::GetInstance()->GetUid(), img.local_path,
            [self, img](bool ok, const QString& urlPath, const QString& errMsg) {
                if (!self) {
                    return;                      // 页面已经没了
                }
                if (!ok) {
                    qCDebug(lcChat) << "upload image failed:" << errMsg;
                    return;
                }
                // ★ 把自己刚上传的这张图塞进内存缓存：
                //   本地已经用 preview 显示过了，气泡不需要重新下一遍。
                //   切会话后重新渲染时就能直接命中缓存。
                if (!img.preview.isNull()) {
                    PictureBubble::_pixmap_cache.insert(urlPath, img.preview);
                }

                QJsonObject item;
                item["content"] = urlPath;       // 服务端上的相对路径
                item["unique_id"] = img.uuid;
                item["msg_type"] = 1;            // 1 = 图片
                QJsonArray arr;
                arr.append(item);

                QJsonObject req;
                req["fromuid"] = UserMgr::GetInstance()->GetUid();
                req["touid"] = img.other_id;
                req["thread_id"] = img.thread_id;
                req["text_array"] = arr;
                QByteArray imgData = QJsonDocument(req).toJson(QJsonDocument::Compact);
                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, imgData);
                qCDebug(lcChat) << "image msg sent, url =" << urlPath;
                // ★ 图片是"上传成功后才真正发包"，超时计时必须在发包这一刻启动，
                //   否则上传耗时会被算进超时，导致还没发出去就被误判失败。
                self->armResendTimer(img.uuid);
            });
    }

    // ---------- 文件：上传成功后再把消息发给服务端 ----------
    // 与图片同构：先异步上传拿到服务端地址，再发 msg_type=2 的消息包。
    for (const auto& pf : pending_files) {
        UploadManager::instance()->uploadFile(
            UserMgr::GetInstance()->GetUid(), pf.local_path,
            [self, pf](bool ok, const QString& urlPath, const QString& errMsg) {
                if (!self) {
                    return;
                }
                if (!ok) {
                    qCDebug(lcChat) << "upload file failed:" << errMsg;
                    // 上传失败：把气泡标成失败态，别让它一直停在"发送中"
                    auto it = self->_unrsp_item_map.find(pf.uuid);
                    if (it != self->_unrsp_item_map.end()) {
                        it.value()->setStatus(MsgStatus::SEND_FAILED);
                    }
                    return;
                }
                // 回填服务端地址，之后点击气泡才能下载
                if (pf.bubble) {
                    pf.bubble->setFileUrl(urlPath);
                }

                // ★ content 打包成 JSON {url, name, size}：
                //   服务端只存一个 url，接收方单看 url 拿不到"用户原本的文件名"和
                //   大小（服务端 unique_name 是时间戳+hash），所以三项一起带过去。
                QJsonObject fc;
                fc["url"] = urlPath;
                fc["name"] = pf.file_name;
                fc["size"] = (double)pf.size;
                QString content = QString::fromUtf8(
                    QJsonDocument(fc).toJson(QJsonDocument::Compact));

                QJsonObject item;
                item["content"] = content;
                item["unique_id"] = pf.uuid;
                item["msg_type"] = 2;            // 2 = 文件
                QJsonArray arr;
                arr.append(item);

                QJsonObject req;
                req["fromuid"] = UserMgr::GetInstance()->GetUid();
                req["touid"] = pf.other_id;
                req["thread_id"] = pf.thread_id;
                req["text_array"] = arr;
                QByteArray data = QJsonDocument(req).toJson(QJsonDocument::Compact);
                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, data);
                qCDebug(lcChat) << "file msg sent, url =" << urlPath;
            });
    }

    // ★ 人性化：发送完把焦点还给输入框。点「发送」按钮时焦点会停在按钮上，
    //   接着打字打不进去；回车发送本来就有焦点，setFocus 是幂等的，无副作用。
    ui->chatEdit->setFocus();
}

void ChatPage::on_receive_btn_clicked()
{
    // ★ 双重保险：按钮已 setEnabled(false)，正常情况下点不到这里；
    //   万一以后调试时重新启用，也要避免在没选会话 / 取不到好友信息时解引用空指针。
    if (!ui->receive_btn->isEnabled() || !_chat_data) {
        return;
    }
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Other;
    auto friend_info = UserMgr::GetInstance()->GetFriendById(_chat_data->GetOtherId());
    if (!friend_info) {
        return;
    }
    QString userName = friend_info->GetDisplayName();
    QString userIcon = friend_info->_icon;

    const QVector<MsgInfo>& msgList = pTextEdit->getMsgList();
    for(int i=0; i<msgList.size(); ++i)
    {
        QString type = msgList[i].msgFlag;
        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        // ★ userIcon 是服务端 URL，不能 QPixmap(url) 当文件读（空白）。异步拉取。
        if (!userIcon.isEmpty()) {
            QPointer<ChatItemBase> guard(pChatItem);
            ImageManager::instance()->loadAvatar(
                userIcon, [guard](bool ok, const QPixmap& pix, const QString& err) {
                    if (!guard) { return; }
                    if (ok && !pix.isNull()) { guard->setUserIcon(pix); }
                    else { qCDebug(lcChat) << "ChatPage load other msg avatar failed:" << err; }
                });
        }
        QWidget *pBubble = nullptr;
        if(type == "text")
        {
            pBubble = new TextBubble(role, msgList[i].content);
        }
        else if(type == "image")
        {
            pBubble = new PictureBubble(QPixmap(msgList[i].content) , role);
        }
        else if(type == "file")
        {
            // 调试用：把输入框里的文件段按文件气泡渲染（content 是本地路径）
            QFileInfo fi(msgList[i].content);
            pBubble = new FileBubble(fi.fileName(), fi.size(), msgList[i].pixmap, role);
        }
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            pChatItem->setStatus(2);
            pChatItem->setTime(friendlyTime(QDateTime::currentDateTime()));
            ui->chat_data_list->appendChatItem(pChatItem);
        }
    }
}

void ChatPage::attachBubbleActions(QWidget* bubble, const std::shared_ptr<ChatDataBase>& msg)
{
    if (bubble == nullptr || msg == nullptr) {
        return;
    }
    const ChatMsgType type = msg->GetMsgType();
    const QString content  = msg->GetMsgContent();
    QPointer<QWidget>  bubbleGuard(bubble);
    QPointer<ChatPage> self(this);

    bubble->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(bubble, &QWidget::customContextMenuRequested, this,
            [self, bubbleGuard, type, content](const QPoint& pos) {
        if (!self || !bubbleGuard) {
            return;
        }
        QMenu menu;
        if (type == ChatMsgType::TEXT) {
            QAction* actCopy = menu.addAction(QString("复制"));
            if (menu.exec(bubbleGuard->mapToGlobal(pos)) == actCopy) {
                QApplication::clipboard()->setText(content);
            }
            return;
        }

        if (type == ChatMsgType::PIC) {
            QAction* actSave = menu.addAction(QString("另存为..."));
            QAction* actCopy = menu.addAction(QString("复制图片"));
            QAction* chosen  = menu.exec(bubbleGuard->mapToGlobal(pos));
            if (chosen == nullptr) {
                return;
            }
            QPixmap pix;
            if (auto* pb = qobject_cast<PictureBubble*>(bubbleGuard.data())) {
                pix = pb->fullPixmap();
            }
            if (pix.isNull()) {
                pix = bubbleGuard->grab();   // 还在下载/占位时退回"截当前渲染"
            }
            if (chosen == actCopy) {
                if (!pix.isNull()) { QApplication::clipboard()->setPixmap(pix); }
                return;
            }
            QString path = QFileDialog::getSaveFileName(self, QString("保存图片"),
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
                    + QDir::separator() + QString("image.png"));
            if (path.isEmpty()) {
                return;
            }
            if (!pix.save(path)) {
                QMessageBox::warning(self, QString("提示"),
                                     QString("保存失败：%1").arg(path));
            }
            return;
        }

        if (type == ChatMsgType::FILE) {
            // content 是 JSON {"url":..., "name":..., "size":...}
            QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
            if (!doc.isObject()) {
                return;
            }
            QString url  = doc.object().value("url").toString();
            QString name = doc.object().value("name").toString();
            if (name.isEmpty()) {
                name = QString("file");
            }
            QAction* actOpen = menu.addAction(QString("打开"));
            QAction* actSave = menu.addAction(QString("另存为..."));
            QAction* chosen  = menu.exec(bubbleGuard->mapToGlobal(pos));
            if (chosen == nullptr) {
                return;
            }
            if (chosen == actOpen) {
                self->slot_open_file(url, name);
                return;
            }
            QString defaultPath =
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
                + QDir::separator() + name;
            QString savePath = QFileDialog::getSaveFileName(self, QString("保存文件"), defaultPath);
            if (savePath.isEmpty()) {
                return;
            }
            self->downloadFile(url, savePath, /*openAfter=*/false, /*askOpen=*/true);
        }
    });

    // 图片：鼠标手型 + 提示可点（灯箱由 PictureBubble 自己的 eventFilter 打开）
    if (type == ChatMsgType::PIC) {
        bubble->setCursor(Qt::PointingHandCursor);
        bubble->setToolTip(QString("点击查看大图，右键可另存为"));
    }
}

// ---- 草稿：按 thread_id 存取输入框纯文本 ----
void ChatPage::saveDraft(int thread_id)
{
    if (thread_id <= 0) {
        return;
    }
    QString text = ui->chatEdit->toPlainText();
    if (text.trimmed().isEmpty()) {
        _drafts.remove(thread_id);   // 空的就不占草稿
        return;
    }
    _drafts[thread_id] = text;
}

void ChatPage::restoreDraft(int thread_id)
{
    ui->chatEdit->clear();
    auto it = _drafts.find(thread_id);
    if (it != _drafts.end()) {
        ui->chatEdit->setPlainText(it.value());
        // 光标移到末尾，接着打
        QTextCursor c = ui->chatEdit->textCursor();
        c.movePosition(QTextCursor::End);
        ui->chatEdit->setTextCursor(c);
    }
}

// ---- 时间分隔条：日期变化时在消息流里插一条"今天 / 昨天 / yyyy年M月d日" ----
QWidget* ChatPage::makeDateSeparator(const QString& text)
{
    auto* lb = new QLabel(text);
    lb->setAlignment(Qt::AlignCenter);
    // 灰色小胶囊，QQ 风格
    lb->setStyleSheet(QString("QLabel{color:#999999;background:#ececec;border-radius:9px;"
                              "padding:3px 10px;font-size:12px;}"));
    lb->adjustSize();
    auto* w = new QWidget();
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 6, 0, 6);
    h->addStretch();
    h->addWidget(lb);
    h->addStretch();
    // 不拦鼠标：点它等同点背景，不会挡住消息区的"清除选区"等逻辑
    w->setAttribute(Qt::WA_TransparentForMouseEvents);
    return w;
}

QString ChatPage::friendlyTime(const QDateTime& when)
{
    if (!when.isValid()) {
        return QString();
    }
    const QDate d = when.date();
    const QDate today = QDate::currentDate();
    if (d == today) {
        return QString("今天 %1").arg(when.toString(QString("hh:mm")));
    }
    if (d == today.addDays(-1)) {
        return QString("昨天 %1").arg(when.toString(QString("hh:mm")));
    }
    if (d.year() == today.year()) {
        return when.toString(QString("M月d日 hh:mm"));
    }
    return when.toString(QString("yyyy年M月d日 hh:mm"));
}

void ChatPage::ensureDateSeparator(const QDateTime& when)
{
    QDate d = when.date();
    if (d == _last_section_date) {
        return;   // 同一天，不插
    }
    _last_section_date = d;

    QDate today = QDate::currentDate();
    QString title;
    if (d == today) {
        title = QString("今天");
    } else if (d == today.addDays(-1)) {
        title = QString("昨天");
    } else {
        title = d.toString(QString("yyyy年M月d日"));
    }
    ui->chat_data_list->appendChatItem(makeDateSeparator(title));
}

void ChatPage::clearItems()
{
    ui->chat_data_list->removeAllItem();
}

// ★ 登出清理：消息气泡 + 未回执表 + 重发上下文 + 当前会话数据绑定 + 输入框。
//   SetChatData 只在点击会话时才重渲染，不主动清的话，
//   换号登录后聊天页停留的是上一个账号的最后一屏消息。
//   定时器无需处理：armResendTimer 的 singleShot 回调先查 _resend_ctx，
//   这里清空后即使触发也是安全 no-op。
void ChatPage::resetForLogout()
{
    clearItems();
    _unrsp_item_map.clear();
    _resend_ctx.clear();
    _rendered_ids.clear();
    _drafts.clear();
    _last_section_date = QDate();
    _chat_data.reset();
    ui->chatEdit->clear();
    ui->title_lb->setText(QString());
}
