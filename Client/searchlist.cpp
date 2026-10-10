#include "searchlist.h"
#include<QScrollBar>
#include "adduseritem.h"
#include "invaliditem.h"
#include "findsuccessdlg.h"
#include "tcpmgr.h"
#include "customizeedit.h"
#include "findfaildlg.h"
#include "loadingdlg.h"
#include "userdata.h"
#include "usermgr.h"

// ★ 初始化顺序必须跟 searchlist.h 里成员的【声明顺序】一致，
//   否则编译器会报 -Wreorder（并且实际初始化顺序是声明顺序，不是这里写的顺序）。
//   声明顺序是：_send_pending -> _find_dlg -> _search_edit -> _loadingDialog
SearchList::SearchList(QWidget *parent):QListWidget(parent),_send_pending(false), _find_dlg(nullptr), _search_edit(nullptr), _loadingDialog(nullptr)
{
    Q_UNUSED(parent);
     this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
     this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 安装事件过滤器
    this->viewport()->installEventFilter(this);
    //连接点击的信号和槽
    connect(this, &QListWidget::itemClicked, this, &SearchList::slot_item_clicked);
    //添加条目
    addTipItem();
    //连接搜索条目
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_user_search, this, &SearchList::slot_user_search);
}

void SearchList::CloseFindDlg()
{
    if(_find_dlg){
        _find_dlg->hide();
        _find_dlg = nullptr;
    }
}

void SearchList::SetSearchEdit(QWidget* edit) {
    _search_edit = edit;
}

void SearchList::waitPending(bool pending)
{
    if (pending) {
        // 已经在显示就别再new 一个 —— LoadingDlg 是模态的，
        // 叠多个只会挡住窗口且只有最后一个能被关掉。
        if (_loadingDialog) {
            return;
        }
        _loadingDialog = new LoadingDlg(this);
        _loadingDialog->setModal(true);
        _loadingDialog->show();
        _send_pending = pending;
    } else {
        // ★ 必须判空：SearchList 构造完到第一次搜索之间，
        //   waitPending(false) 可能被调到（比如收到一个空结果），
        //   原来会解引用未初始化/已 deleteLater 的指针 -> 崩。
        if (_loadingDialog) {
            _loadingDialog->hide();
            _loadingDialog->deleteLater();
            _loadingDialog = nullptr;      // 置空，避免悬垂指针
        }
        _send_pending = pending;
    }
}


void SearchList::addTipItem()
{
    auto *invalid_item = new QWidget();
    QListWidgetItem *item_tmp = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item_tmp->setSizeHint(QSize(250,10));
    this->addItem(item_tmp);
    invalid_item->setObjectName("invalid_item");
    this->setItemWidget(item_tmp, invalid_item);
    item_tmp->setFlags(item_tmp->flags() & ~Qt::ItemIsSelectable);


    auto *add_user_item = new AddUserItem();
    QListWidgetItem *item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(add_user_item->sizeHint());
    this->addItem(item);
    this->setItemWidget(item, add_user_item);
}

void SearchList::slot_item_clicked(QListWidgetItem *item)
{
    QWidget *widget = this->itemWidget(item); // 获取自定义widget对象
    if(!widget){
        qDebug()<< "slot item clicked widget is nullptr";
        return;
    }

    // 对自定义widget进行操作， 将item 转化为基类ListItemBase
    ListItemBase *customItem = qobject_cast<ListItemBase*>(widget);
    if(!customItem){
        qDebug()<< "slot item clicked widget is nullptr";
        return;
    }

    auto itemType = customItem->GetItemType();
    if(itemType == ListItemType::INVALID_ITEM){
        qDebug()<< "slot invalid item clicked ";
        return;
    }

   if(itemType == ListItemType::ADD_USER_TIP_ITEM){

       if (_send_pending) {
           return;
       }

           if (!_search_edit) {
               return;
           }

           auto search_edit = dynamic_cast<CustomizeEdit*>(_search_edit);
           if (search_edit == nullptr) {
               qDebug() << "search_edit is not a CustomizeEdit, give up";
               return;
           }

           // ★ 空输入就别发请求了。
           //   原先直接发 {"uid":""}，而服务端 isPureDigit("") 会把空串当成"纯数字"
           //   -> GetUserByUid("") -> std::stoi("") 抛 std::invalid_argument
           //   -> DealMsg 没有 try/catch -> ChatServer 整个进程崩
           //   -> 客户端收不到回包永远转圈 -> 心跳超时掉线。
           auto uid_str = search_edit->text().trimmed();
           if (uid_str.isEmpty()) {
               qDebug() << "search text is empty, do not send request";
               _find_dlg = std::make_shared<FindFailDlg>(this);
               _find_dlg->show();
               return;
           }

           waitPending(true);
           //此处发送请求给server
           QJsonObject jsonObj;
           jsonObj["uid"] = uid_str;

           QJsonDocument doc(jsonObj);
           QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

           //发送tcp请求给chat server
           emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_SEARCH_USER_REQ, jsonData);
       return;
   }

   //清除弹出框
    CloseFindDlg();
}

void SearchList::slot_user_search(std::shared_ptr<SearchInfo> si)
{
    waitPending(false);
    if (si == nullptr) {
        _find_dlg = std::make_shared<FindFailDlg>(this);
    }else{
        //如果是自己，暂且先直接返回，以后看逻辑扩充
        auto self_uid = UserMgr::GetInstance()->GetUid();
        if (si->_uid == self_uid) {
                 return;
        }
        //此处分两种情况，一种是搜多到已经是自己的朋友了，一种是未添加好友
        //查找是否已经是好友
        bool bExist = UserMgr::GetInstance()->CheckFriendById(si->_uid);
        if(bExist){
                //此处处理已经添加的好友，实现页面跳转
            //跳转到聊天界面指定的item中
            emit sig_jump_chat_item(si);
            return;
        }
        //此处先处理为添加的好友
        _find_dlg = std::make_shared<FindSuccessDlg>(this);
        dynamic_pointer_cast<FindSuccessDlg>(_find_dlg)->SetSearchInfo(si);

    }
    _find_dlg->show();
}
