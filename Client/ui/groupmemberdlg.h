#ifndef GROUPMEMBERDLG_H
#define GROUPMEMBERDLG_H

#include <QDialog>
#include <vector>

class QListWidget;
class QPushButton;

// 群成员查看对话框：显示群名 + 成员列表（昵称 / uid），并可邀请好友入群。
// 纯代码构建，不生成 .ui —— .ui 有收尾顺序坑，能不用就不用。
// 成员昵称通过 UserMgr::GetFriendById 拿（好友才查得到）；非好友成员只显示 uid。
class GroupMemberDlg : public QDialog
{
    Q_OBJECT
public:
    explicit GroupMemberDlg(int thread_id, const QString& group_name,
                            const std::vector<int>& members, QWidget* parent = nullptr);
    ~GroupMemberDlg();

private:
    void buildUi(const QString& group_name);
    void fillMembers(const std::vector<int>& members);
    void sendAddMembersReq(const std::vector<int>& uids);

    QListWidget* _member_list = nullptr;
    QPushButton* _invite_btn = nullptr;
    int _thread_id = 0;
    std::vector<int> _members;
    QString _group_name;   // 群名缓存：发 1035 邀请请求时带上，供服务端回推群信息
    // 本次发出去的邀请 uid：服务端 1036 不回"加了谁"，成功后用它并进列表
    std::vector<int> _pending_added;
};

#endif // GROUPMEMBERDLG_H
