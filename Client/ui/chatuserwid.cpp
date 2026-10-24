#include "chatuserwid.h"
#include "ui_chatuserwid.h"
#include "usermgr.h"
#include "imagemanager.h"
#include "animutil.h"
#include "avatarutil.h"
#include <QPointer>
#include <QLabel>
#include <QDateTime>
#include <QFontMetrics>
#include "logcategory.h"

ChatUserWid::ChatUserWid(QWidget *parent) :
    ListItemBase(parent),
    ui(new Ui::ChatUserWid)
{
    ui->setupUi(this);
    SetItemType(ListItemType::CHAT_USER_ITEM);

    // ★ 未读数字徽标：运行时创建（不动 .ui），贴在头像右上角。
    //   红色圆角 + 白字，>99 显示 "99+"；位数变多时自动加宽。
    _unread_lb = new QLabel(this);
    _unread_lb->setObjectName("chat_unread_badge");
    _unread_lb->setAlignment(Qt::AlignCenter);
    _unread_lb->setStyleSheet(QString(
        "QLabel{background:#ff4d4f;color:#ffffff;border-radius:8px;"
        "font-size:11px;font-weight:bold;padding:0px 4px;}"));
    _unread_lb->hide();

    ui->red_point->raise();
    // ★ 必须放在 _unread_lb = new 之后：ShowRedPoint(false) -> clearUnread()
    //   里会 hide() 这个徽标，放在 new 之前就是对 nullptr 调 QWidget::hide()，
    //   登录加载会话列表（slot_load_chat_thread）建第一个 ChatUserWid 时必崩。
    ShowRedPoint(false);
}

ChatUserWid::~ChatUserWid()
{
    delete ui;
}

QSize ChatUserWid::sizeHint() const
{
    return QSize(250, 70); // 返回自定义的尺寸
}

void ChatUserWid::SetChatData(std::shared_ptr<ChatThreadData> chat_data) {
    _chat_data = chat_data;
    auto other_id = _chat_data->GetOtherId();
    auto other_info = UserMgr::GetInstance()->GetFriendById(other_id);
    // ★ 必须判空：GetFriendById 查不到会返回 nullptr，
    //   之前这里直接 other_info->_icon，库里一旦有 uid=0 的坏会话就会闪退。
    if (other_info == nullptr) {
        // ★ 群聊没有单一 peer（other_id 为 0），必然查不到好友条目。
        //   这里原来直接 return，结果会话列表里出现一行空白。
        //   改成用群名兜底，时间照常取最后一条消息。
        if (_chat_data->IsGroup()) {
            auto group_name = _chat_data->GetGroupName();
            const QString shown = group_name.isEmpty()
                ? QString("群聊 %1").arg(_chat_data->GetThreadId()) : group_name;
            ui->user_name_lb->setText(shown);
            // ★ 群聊没有头像 URL，用字母头像兜底（首字 + 按 thread_id 选色）。
            //   以前这里什么都不设，列表里就是 .ui 占位的那张空白图。
            ui->icon_lb->setPixmap(AvatarUtil::MakeLetterAvatar(
                shown, ui->icon_lb->width(), _chat_data->GetThreadId()));
            ui->icon_lb->setScaledContents(true);
            ui->user_chat_lb->setText(chat_data->GetLastMsg());
            const auto& g_map = chat_data->GetMsgMapRef();
            if (!g_map.isEmpty()) {
                auto g_last = g_map.end();
                --g_last;
                setLastTime(QDateTime::fromString(g_last.value()->GetChatTime(),
                                                  QString("yyyy-MM-dd hh:mm:ss")));
            } else {
                setLastTime(QDateTime());
            }
            return;
        }
        qCDebug(lcChat) << "SetChatData: friend info not found, other_id =" << other_id;
        return;
    }
    // _icon 是服务端 URL，QPixmap(url) 会当本地文件读 → 空白。异步拉取。
    if (!other_info->_icon.isEmpty()) {
        QPointer<ChatUserWid> guard(this);
        ImageManager::instance()->loadAvatar(
            other_info->_icon, [guard, this](bool ok, const QPixmap& pix, const QString& err) {
                if (!guard) { return; }
                if (ok && !pix.isNull()) {
                    ui->icon_lb->setPixmap(pix.scaled(ui->icon_lb->size(),
                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
                } else {
                    qCDebug(lcChat) << "ChatUserWid load avatar failed:" << err;
                }
            });
        ui->icon_lb->setScaledContents(true);
    }

    ui->user_name_lb->setText(other_info->GetDisplayName());

    ui->user_chat_lb->setText(chat_data->GetLastMsg());

    // ★ 右侧时间：取消息表里最后一条的时间（QMap 按 msg_id 升序，取末尾即可）。
    //   以前 .ui 里的 time_lb 只有占位文字 "12:00"，代码从来没赋值过。
    const auto& msg_map = chat_data->GetMsgMapRef();
    if (!msg_map.isEmpty()) {
        auto last = msg_map.end();
        --last;
        setLastTime(QDateTime::fromString(last.value()->GetChatTime(),
                                          QString("yyyy-MM-dd hh:mm:ss")));
    } else {
        setLastTime(QDateTime());
    }
}

QString ChatUserWid::listTimeText(const QDateTime& when)
{
    if (!when.isValid()) {
        return QString();
    }
    const QDate d = when.date();
    const QDate today = QDate::currentDate();
    if (d == today) {
        return when.toString(QString("hh:mm"));
    }
    if (d == today.addDays(-1)) {
        return QString("昨天");
    }
    if (d.year() == today.year()) {
        return d.toString(QString("M月d日"));
    }
    return d.toString(QString("yyyy年M月d日"));
}

void ChatUserWid::setLastTime(const QDateTime& when)
{
    ui->time_lb->setText(listTimeText(when));
}

void ChatUserWid::addUnread(int n)
{
    if (n <= 0 || _unread_lb == nullptr) {
        return;
    }
    _unread += n;
    const QString txt = _unread > 99 ? QString("99+") : QString::number(_unread);
    _unread_lb->setText(txt);
    // 数字变多要跟着加宽，否则会被裁成 "..."
    QFontMetrics fm(_unread_lb->font());
    const int w = qMax(18, fm.horizontalAdvance(txt) + 10);
    _unread_lb->setFixedSize(w, 16);
    // 头像在 (7,12,45,45)，徽标贴它右上角
    _unread_lb->move(7 + 45 - w + 6, 12 - 4);
    _unread_lb->raise();
    _unread_lb->show();
    // 数字徽标与脉冲红点二选一，避免两个标同时出现
    AnimUtil::stopRedDotPulse(ui->red_point);
    ui->red_point->hide();
}

void ChatUserWid::clearUnread()
{
    _unread = 0;
    // 数字徽标与脉冲红点二选一，两个都收起来，避免同时出现
    AnimUtil::stopRedDotPulse(ui->red_point);
    ui->red_point->hide();
    if (_unread_lb) {
        _unread_lb->hide();
    }
}

std::shared_ptr<ChatThreadData> ChatUserWid::GetChatData()
{
    return _chat_data;
}

void ChatUserWid::ShowRedPoint(bool bshow)
{
    if(bshow){
        addUnread(1);   // 兼容旧调用：有未读 → 数字徽标
    }else{
        clearUnread();
    }
}



void ChatUserWid::updateLastMsg(std::vector<std::shared_ptr<TextChatData>> msgs) {

    int last_msg_id = 0;
    QString last_msg = "";
    QDateTime last_time;
    for (auto& msg : msgs) {
        last_msg = msg->GetContent();
        last_msg_id = msg->GetMsgId();
        last_time = QDateTime::fromString(msg->GetChatTime(), QString("yyyy-MM-dd hh:mm:ss"));
        _chat_data->AddMsg(msg);
    }

    _chat_data->SetLastMsgId(last_msg_id);
    ui->user_chat_lb->setText(last_msg);
    // ★ 顺带刷新右侧时间（同一天不重复渲染也没关系，setText 幂等）
    setLastTime(last_time);
}


