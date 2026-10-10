#include "chatpage.h"
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

ChatPage::ChatPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatPage)
{
    ui->setupUi(this);
    //设置按钮样式
    ui->receive_btn->SetState("normal","hover","press");
    ui->send_btn->SetState("normal","hover","press");

    //设置图标样式
    ui->emo_lb->SetState("normal","hover","press","normal","hover","press");
    ui->file_lb->SetState("normal","hover","press","normal","hover","press");

}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::SetChatData(std::shared_ptr<ChatThreadData> chat_data) {
    _chat_data = chat_data;
    auto other_id = _chat_data->GetOtherId();
    if(other_id == 0) {
        //说明是群聊
        ui->title_lb->setText(_chat_data->GetGroupName());
        //todo...加载群聊信息和成员信息
        return;
    }

    //私聊
    auto friend_info = UserMgr::GetInstance()->GetFriendById(other_id);
    if (friend_info == nullptr) {
        return;
    }
    ui->title_lb->setText(friend_info->_name);
    ui->chat_data_list->removeAllItem();
    _unrsp_item_map.clear();
    for(auto & msg : chat_data->GetMsgMapRef()){
        AppendChatMsg(msg);
    }

    for (auto& msg : chat_data->GetMsgUnRspRef()) {
        AppendChatMsg(msg);
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
            qDebug() << "makeBubble: PIC message with empty content";
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
                        qDebug() << "load bubble image failed:" << err;
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
    // 文件类型暂时不显示
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
        qDebug() << "AppendChatMsg: self info is null";
        return;
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
        userName = self_info->_name;
        userIcon = self_info->_icon;
    }
    else {
        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->GetSendUid());
        if (friend_info == nullptr) {
            // ★ 对方还没进好友列表（比如消息先于 AuthFriend 回包到达）。
            //   这里必须 return：继续走下去 pChatItem 的用户名/头像都是空的。
            qDebug() << "AppendChatMsg: friend not found, uid =" << msg->GetSendUid();
            return;
        }
        userName = friend_info->_name;
        userIcon = friend_info->_icon;
    }

    // ★ 统一走工厂：文本和图片都会返回非空，FILE 才返回 nullptr。
    //   原来两个分支各写一遍 if(TEXT)，非文本时 pBubble 是 nullptr，
    //   setWidget(nullptr) 之后 setFixedSize 用空指针 —— 必崩。
    QWidget* pBubble = makeBubble(msg->GetMsgType(), msg->GetMsgContent(), role);
    if (pBubble == nullptr) {
        return;
    }

    ChatItemBase* pChatItem = new ChatItemBase(role);
    pChatItem->setUserName(userName);
    pChatItem->setUserIcon(QPixmap(userIcon));
    pChatItem->setWidget(pBubble);
    auto status = msg->GetStatus();
    pChatItem->setStatus(status);
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
        qDebug() << "on_send_btn_clicked: chat_data is empty";
        return;
    }

    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    // ★ 登录回包没到之前 user_info 是空的，原来直接 user_info->_name 直接崩
    if (user_info == nullptr) {
        qDebug() << "on_send_btn_clicked: user info is null";
        return;
    }
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Self;
    QString userName = user_info->_name;
    QString userIcon = user_info->_icon;

    // ★ 原来是文件级全局变量：多个 ChatPage 实例共享一份，
    //   A 页收集的图片可能被 B 页的发送流程带走。改成函数内局部变量。
    QVector<PendingImage> pending_images;

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
        pChatItem->setUserIcon(QPixmap(userIcon));
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
        }
        else if(type == "file")
        {

        }
        //发送消息
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            pChatItem->setStatus(0);
            ui->chat_data_list->appendChatItem(pChatItem);
            // ★ 文本和图片都有回执，都要登记，才能把状态从"发送中"更新为"已送达"。
            //   FILE 目前没实现，不登记（否则永远等一个不来的回执）。
            if (type == "text" || type == "image") {
                _unrsp_item_map[uuidString] = pChatItem;
            }
        }

    }

    qDebug() << "textArray is " << textArray ;
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
        qDebug() << "no text to send, skip empty text request";
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
                    qDebug() << "upload image failed:" << errMsg;
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
                qDebug() << "image msg sent, url =" << urlPath;
            });
    }
}

void ChatPage::on_receive_btn_clicked()
{
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Other;
    auto friend_info = UserMgr::GetInstance()->GetFriendById(_chat_data->GetOtherId());
    QString userName = friend_info->_name;
    QString userIcon = friend_info->_icon;

    const QVector<MsgInfo>& msgList = pTextEdit->getMsgList();
    for(int i=0; i<msgList.size(); ++i)
    {
        QString type = msgList[i].msgFlag;
        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        pChatItem->setUserIcon(QPixmap(userIcon));
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

        }
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            pChatItem->setStatus(2);
            ui->chat_data_list->appendChatItem(pChatItem);
        }
    }
}

void ChatPage::clearItems()
{
    ui->chat_data_list->removeAllItem();
}
