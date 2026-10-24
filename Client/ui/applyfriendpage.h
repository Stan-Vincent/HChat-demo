#ifndef APPLYFRIENDPAGE_H
#define APPLYFRIENDPAGE_H

#include <QWidget>
#include "userdata.h"
#include <memory>
#include <QJsonArray>
#include <unordered_map>
#include "applyfrienditem.h"

namespace Ui {
class ApplyFriendPage;
}

class ApplyFriendPage : public QWidget
{
    Q_OBJECT

public:
    explicit ApplyFriendPage(QWidget *parent = nullptr);
    ~ApplyFriendPage();
    void AddNewApply(std::shared_ptr<AddFriendApply> apply);
    // Clear every rendered apply row + the uid->item map (called on logout,
    // and internally by loadApplyList to avoid stacking duplicates).
    void clearAll();
    // Reload from UserMgr::GetApplyList() (called after each successful login).
    void loadApplyList();
protected:
    void paintEvent(QPaintEvent *event);
private:
    Ui::ApplyFriendPage *ui;
    std::unordered_map<int, ApplyFriendItem*> _unauth_items;
public slots:
    void slot_auth_rsp(std::shared_ptr<AuthRsp> );
signals:
    void sig_show_search(bool);
};

#endif // APPLYFRIENDPAGE_H
