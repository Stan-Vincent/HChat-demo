#ifndef SELECTFRIENDDLG_H
#define SELECTFRIENDDLG_H

#include <QDialog>

class QListWidget;
class QListWidgetItem;

// 选好友对话框（多选）：用于"邀请好友入群"。
// 已在群里的好友会被过滤掉并给出提示，避免重复邀请。
class SelectFriendDlg : public QDialog
{
    Q_OBJECT
public:
    // exclude_uids: 需要排除的 uid（群内已有成员，含自己）
    explicit SelectFriendDlg(const std::vector<int>& exclude_uids,
                             QWidget* parent = nullptr);
    ~SelectFriendDlg();

    // 用户勾选的好友 uid
    std::vector<int> GetSelectedUids() const;

private:
    void fillFriends(const std::vector<int>& exclude_uids);

    QListWidget* _list = nullptr;
};

#endif // SELECTFRIENDDLG_H
