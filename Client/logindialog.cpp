#include "logindialog.h"
#include "ui_logindialog.h"
#include <QDebug>
#include "httpmgr.h"
#include "tcpmgr.h"
#include "usermgr.h"
#include <QRegularExpression>
#include <QPainter>
#include <QPainterPath>
#include "clickedlabel.h"
#include "animutil.h"
#include <QLineEdit>
#include <QCheckBox>
#include <QSettings>
#include <QTimer>
#ifdef _WIN32
// ★ 必须先定这两个宏再包含 windows.h：
//   windows.h 会连带引入 rpcndr.h，里面 `typedef byte cs_byte;`
//   与 QtCore 的 byte 撞名，报 "reference to 'byte' is ambiguous"。
//   WIN32_LEAN_AND_MEAN 让它跳过 RPC/ Winsock 那一堆用不上的头。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include "logcategory.h"
#endif


// ============================================================
//  「记住密码 / 自动登录」的存储实现
//
//  密码不明文落盘：用 Windows DPAPI（CryptProtectData）加密后再写进 login.ini。
//  DPAPI 以【当前登录的 Windows 用户凭据】为密钥，密文只有本机同一账户能解开；
//  换机器、换用户、把 login.ini 拷给别人都解不开。这是 Windows 桌面程序
//  保存密码的标准做法，比 base64 之类的自欺欺人强得多。
// ============================================================
static QString dpapiProtect(const QString& plain)
{
#ifdef _WIN32
    if (plain.isEmpty()) return QString();
    QByteArray utf8 = plain.toUtf8();
    DATA_BLOB in;
    in.pbData = (BYTE*)utf8.constData();
    in.cbData = (DWORD)utf8.size();

    DATA_BLOB out = { 0 };
    if (!CryptProtectData(&in, L"Hcat", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        qCWarning(lcUser) << "CryptProtectData failed, error =" << GetLastError();
        return QString();
    }
    QByteArray cipher((char*)out.pbData, (int)out.cbData);
    LocalFree(out.pbData);
    return QString::fromLatin1(cipher.toBase64());
#else
    return plain;
#endif
}

static QString dpapiUnprotect(const QString& b64)
{
#ifdef _WIN32
    if (b64.isEmpty()) return QString();
    QByteArray cipher = QByteArray::fromBase64(b64.toLatin1());
    if (cipher.isEmpty()) return QString();

    DATA_BLOB in;
    in.pbData = (BYTE*)cipher.constData();
    in.cbData = (DWORD)cipher.size();

    DATA_BLOB out = { 0 };
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        // 最常见原因：login.ini 被复制到了别的机器 / 别的 Windows 用户下
        qCWarning(lcUser) << "CryptUnprotectData failed, error =" << GetLastError();
        return QString();
    }
    QByteArray plain((char*)out.pbData, (int)out.cbData);
    LocalFree(out.pbData);
    return QString::fromUtf8(plain);
#else
    return b64;
#endif
}

// 配置放在 exe 同目录，和 config.ini 放一起，方便整体拷贝或删除
static QString loginSettingsPath()
{
    return QCoreApplication::applicationDirPath() + "/login.ini";
}

LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);

    // 按钮体系：用 role 属性 + reload_qss 统一三类按钮样式（见 stylesheet.qss §9）
    ui->login_btn->setProperty("role", "primary");
    reload_qss(ui->login_btn);
    ui->reg_btn->setProperty("role", "link");
    reload_qss(ui->reg_btn);
    // 登录主按钮：悬浮上浮（轻微上移 + 过渡回弹），见 AnimUtil::installHoverLift
    AnimUtil::installHoverLift(ui->login_btn, 2);

    // 把眼睛图标叠进密码框右侧，避免密码行因 eye 占宽而比邮箱行短一截（显歪）
    ui->pass_visible->setParent(ui->pass_edit);
    ui->pass_edit->setTextMargins(0, 0, 32, 0);
    ui->pass_edit->installEventFilter(this);
    QTimer::singleShot(0, this, [this]() {
        ui->pass_visible->setGeometry(ui->pass_edit->width() - 28,
                                      (ui->pass_edit->height() - 24) / 2,
                                      24, 24);
    });

    connect(ui->reg_btn, &QPushButton::clicked, this, &LoginDialog::sig_switch_register);
    ui->forget_label->SetState("normal","hover","","selected","selected_hover","");
    ui->forget_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_label, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);

    // 密码显示/隐藏图标（与注册页 pass_visible 完全一致：同样 20x20、同一套图标）
    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->pass_visible->SetState("unvisible", "unvisible_hover", "",
                               "visible", "visible_hover", "");
    connect(ui->pass_visible, &ClickedLabel::clicked, this, [this]() {
        // 双保险：自动填入状态下即使信号到达也不切换明文
        if (!ui->pass_visible->isEnabled()) {
            return;
        }
        bool b_visible = (ui->pass_edit->echoMode() == QLineEdit::Normal);
        ui->pass_edit->setEchoMode(b_visible ? QLineEdit::Password : QLineEdit::Normal);
        qCDebug(lcUser) << "pass_visible clicked, echoMode =" << ui->pass_edit->echoMode();
    });

    // 用户一旦手动编辑密码（textEdited 只在人工输入时触发，setText 不触发），
    // 就恢复眼睛按钮：手动输入的场景有"核对是否输错"的正当需求。
    connect(ui->pass_edit, &QLineEdit::textEdited, this, [this]() {
        ui->pass_visible->setEnabled(true);
    });

    // 取消「记住密码」时连带取消「自动登录」，否则下次会带着空密码自动登录
    connect(ui->remember_pwd_chk, &QCheckBox::toggled, this, [this](bool on) {
        if (!on) {
            ui->auto_login_chk->setChecked(false);
        }
    });

    loadSettings();
    initHttpHandlers();
    //连接登录回包信号
    connect(HttpMgr::GetInstance().get(), &HttpMgr::sig_login_mod_finish, this,
            &LoginDialog::slot_login_mod_finish);

    //连接tcp连接请求的信号和槽函数
    connect(this, &LoginDialog::sig_connect_tcp, TcpMgr::GetInstance().get(), &TcpMgr::slot_tcp_connect);
    //连接tcp管理者发出的连接成功信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_con_success, this, &LoginDialog::slot_tcp_con_finish);
    //连接tcp管理者发出的登陆失败信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_login_failed, this, &LoginDialog::slot_login_failed);

    initHead();

    // 回填完成后再自动登录：延时等界面真正显示出来，避免控件还没准备好就发请求
    if (ui->auto_login_chk->isChecked()
        && !ui->email_edit->text().isEmpty()
        && !ui->pass_edit->text().isEmpty()) {
        QTimer::singleShot(300, this, [this]() {
            qCDebug(lcUser) << "auto login triggered";
            on_login_btn_clicked();
        });
    }
}

LoginDialog::~LoginDialog()
{
    qCDebug(lcUser) <<"destruct LoginDlg";
    saveSettings();
    delete ui;
}

// ============================================================
//  读 / 写 login.ini
// ============================================================
void LoginDialog::loadSettings()
{
    QSettings st(loginSettingsPath(), QSettings::IniFormat);

    const QString email = st.value("login/email").toString();
    if (!email.isEmpty()) {
        ui->email_edit->setText(email);
    }

    const bool remember = st.value("login/remember", false).toBool();
    ui->remember_pwd_chk->setChecked(remember);
    if (!remember) {
        return;                       // 没勾「记住密码」就不去碰密文
    }

    const QString cipher = st.value("login/pwd_cipher").toString();
    if (cipher.isEmpty()) {
        return;
    }
    const QString pwd = dpapiUnprotect(cipher);
    if (pwd.isEmpty()) {
        // DPAPI 解不开（换机器 / 换 Windows 用户），把这份失效密文清掉
        qCWarning(lcUser) << "saved password can not be decrypted, drop it";
        st.remove("login/pwd_cipher");
        return;
    }
    ui->pass_edit->setText(pwd);
    ui->auto_login_chk->setChecked(st.value("login/auto_login", false).toBool());
    // 刚自动填入完，先按"自动填入"状态禁用眼睛（防止刚回登录页就点出明文）
    updateEyeButtonState();
}

void LoginDialog::SetAutoLogin(bool on)
{
    ui->remember_pwd_chk->setChecked(on);
    ui->auto_login_chk->setChecked(on);
    if (!on) {
        saveSettings();          // 立刻把勾选状态落盘，清掉旧的密码密文
    }
}

void LoginDialog::resetUIState()
{
    // 登录成功后按钮被禁用、提示停在"正在登录..."；回到登录页时复位，
    // 否则界面卡在"正在登录"且按钮灰色、点不动。
    enableBtn(true);
    _tip_errs.clear();
    ui->err_tip->clear();
    // 退出登录回到登录页时，密码框里往往是"记住密码"自动填入的内容，
    // 这时要重新禁用眼睛按钮（登录过程中用户可能手动输入过，眼睛被放开）。
    updateEyeButtonState();
}

void LoginDialog::updateEyeButtonState()
{
    // 判断当前密码框内容是否就是"记住密码"存的那份（即自动填入，而非用户输入）。
    // 是则禁用眼睛：自动填入没有"核对输错"的场景，点眼睛纯展示，
    // 只增加泄露面（共享屏幕 / 旁人窥视 / 截屏录屏）。
    const QString cur = ui->pass_edit->text();
    bool autofilled = false;
    if (!cur.isEmpty()) {
        QSettings st(loginSettingsPath(), QSettings::IniFormat);
        const QString cipher = st.value("login/pwd_cipher").toString();
        if (!cipher.isEmpty()) {
            autofilled = (cur == dpapiUnprotect(cipher));
        }
    }
    ui->pass_visible->setEnabled(!autofilled);
    // 禁用状态下如果残留明文显示，一并收回（防御：切回登录页前用户开着眼睛）
    if (autofilled && ui->pass_edit->echoMode() == QLineEdit::Normal) {
        ui->pass_edit->setEchoMode(QLineEdit::Password);
    }
}

void LoginDialog::saveSettings()
{
    QSettings st(loginSettingsPath(), QSettings::IniFormat);

    const QString email = ui->email_edit->text().trimmed();
    if (email.isEmpty()) {
        st.remove("login");
        return;
    }
    st.setValue("login/email", email);

    if (ui->remember_pwd_chk->isChecked()) {
        st.setValue("login/remember", true);
        st.setValue("login/auto_login", ui->auto_login_chk->isChecked());
        // 注意存的是【原始密码】。on_login_btn_clicked 里发出去的是 xorString 后的密文，
        // 那样存的话 DPAPI 解出来会是密文，登录会失败。
        st.setValue("login/pwd_cipher", dpapiProtect(ui->pass_edit->text()));
    } else {
        st.remove("login/pwd_cipher");
        st.remove("login/remember");
        st.remove("login/auto_login");
    }
}

void LoginDialog::initHead()
{
    // 加载图片
    QPixmap originalPixmap(":/res/Icon.png");
      // 设置图片自动缩放
    qCDebug(lcUser) << originalPixmap.size() << ui->head_label->size();
    originalPixmap = originalPixmap.scaled(ui->head_label->size(),
            Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 创建一个和原始图片相同大小的QPixmap，用于绘制圆角图片
    QPixmap roundedPixmap(originalPixmap.size());
    roundedPixmap.fill(Qt::transparent); // 用透明色填充

    QPainter painter(&roundedPixmap);
    painter.setRenderHint(QPainter::Antialiasing); // 设置抗锯齿，使圆角更平滑
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 使用QPainterPath设置正圆（圆角半径取宽高一半 -> 完整圆形，替代原 10px 圆角方形）
    QPainterPath path;
    int rc = qMin(originalPixmap.width(), originalPixmap.height()) / 2;
    path.addRoundedRect(0, 0, originalPixmap.width(), originalPixmap.height(), rc, rc);
    painter.setClipPath(path);

    // 将原始图片绘制到roundedPixmap上
    painter.drawPixmap(0, 0, originalPixmap);

    // 设置绘制好的圆形图片到QLabel上
    ui->head_label->setPixmap(roundedPixmap);

    // 圆形 logo 下方加一个居中的品牌标题（与无边框风格统一）
    if (QLabel *old = this->findChild<QLabel *>("app_title")) {
        delete old;
    }
    QLabel *title = new QLabel("HCat", this);
    title->setObjectName("app_title");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("QLabel#app_title{color:#ff7a45;font-size:20px;"
                         "font-weight:bold;background:transparent;border:none;}");
    ui->verticalLayout_2->insertWidget(2, title);

}

void LoginDialog::initHttpHandlers()
{
    //注册获取登录回包逻辑
    _handlers.insert(ReqId::ID_LOGIN_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            // 服务端对"邮箱不存在 / 密码错误"统一返回 PasswdInvalid(1009)，提示通用的"邮箱或密码不正确"
            if(error == ErrorCodes::PasswdInvalid){
                showTip(tr("邮箱或密码不正确"), false);
            } else {
                showTip(tr("参数错误"), false);
            }
            enableBtn(true);
            return;
        }
        auto email = jsonObj["email"].toString();

        //发送信号通知tcpMgr发送长链接
        ServerInfo si;
        si.Uid = jsonObj["uid"].toInt();
        si.Host = jsonObj["host"].toString();
        si.Port = jsonObj["port"].toString();
        si.Token = jsonObj["token"].toString();

        _uid = si.Uid;
        _token = si.Token;
        // ★ token 的真正来源就是这里（GateServer 的 HTTP 登录回包）。
        //   必须顺手存进 UserMgr：断线自动重连要靠它判断"是否登录过"，
        //   并且重连后要拿它重放聊天登录握手。
        //   （ChatServer 的 CHAT_LOGIN_RSP 并不回 token，别指望从那边拿。）
        UserMgr::GetInstance()->SetToken(si.Token);
        qCDebug(lcUser) << "email is " << email << " uid is " << si.Uid <<" host is "
                << si.Host << " Port is " << si.Port << " Token is " << si.Token;
        emit sig_connect_tcp(si);
    });
}

void LoginDialog::showTip(QString str, bool b_ok)
{
    if(b_ok){
         ui->err_tip->setProperty("state","normal");
    }else{
        ui->err_tip->setProperty("state","err");
    }

    ui->err_tip->setText(str);

    reload_qss(ui->err_tip);
}

void LoginDialog::slot_forget_pwd()
{
    qCDebug(lcUser) <<"LonginDialog [slot_forget_pwd]";
    emit sig_switch_reset();
}

bool LoginDialog::checkUserValid(){

    auto email = ui->email_edit->text();
    if(email.isEmpty()){
        qCDebug(lcUser) << "email empty " ;
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱不能为空"));
        AnimUtil::shakeWidget(ui->email_edit);
        return false;
    }
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool LoginDialog::checkPwdValid(){
    auto pwd = ui->pass_edit->text();
    if(pwd.length() < 6 || pwd.length() > 15){
        qCDebug(lcUser) << "Pass length invalid";
        //提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        AnimUtil::shakeWidget(ui->pass_edit);
        return false;
    }

    // 创建一个正则表达式对象，按照上述密码要求
    // 这个正则表达式解释：
    // ^[a-zA-Z0-9!@#$%^&*]{6,15}$ 密码长度至少6，可以是字母、数字和特定的特殊字符
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*.]{6,15}$");
    bool match = regExp.match(pwd).hasMatch();
    if(!match){
        //提示字符非法
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符且长度为(6~15)"));
        AnimUtil::shakeWidget(ui->pass_edit);
        return false;;
    }

    DelTipErr(TipErr::TIP_PWD_ERR);

    return true;
}

bool LoginDialog::enableBtn(bool enabled)
{
    ui->login_btn->setEnabled(enabled);
    ui->reg_btn->setEnabled(enabled);
    // 登录按钮禁用期间显示"登录中..."，恢复时回到"登录"（enableBtn 只在登录流程里切换状态）
    ui->login_btn->setText(enabled ? tr("登录") : tr("登录中..."));
    return true;
}

void LoginDialog::on_login_btn_clicked()
{
    qCDebug(lcUser) <<"login btn clicked";
    if(checkUserValid() == false){
        return;
    }

    if(checkPwdValid() == false){
        return ;
    }

    enableBtn(false);
    auto email = ui->email_edit->text();
    auto pwd = ui->pass_edit->text();
    //发送http请求登录
    QJsonObject json_obj;
    json_obj["email"] = email;
    json_obj["passwd"] = xorString(pwd);
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_login"),
                                        json_obj, ReqId::ID_LOGIN_USER,Modules::LOGINMOD);
}

void LoginDialog::slot_login_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"),false);
        return;
    }

    // 解析 JSON 字符串,res需转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    //json解析错误
    if(jsonDoc.isNull()){
        showTip(tr("json解析错误"),false);
        return;
    }

    //json解析错误
    if(!jsonDoc.isObject()){
        showTip(tr("json解析错误"),false);
        return;
    }


    // ★ 必须先 contains 再 []：QMap::operator[] 会为不存在的 key 插入一个空
    //   std::function，调用它直接抛 std::bad_function_call 崩掉。
    //   registerdialog / resetdialog 都有这个判断，只有这里漏了。
    if (!_handlers.contains(id)) {
        qCDebug(lcUser) << "no handler registered for req id " << id;
        return;
    }

    //调用对应的逻辑,根据id回调。
    _handlers[id](jsonDoc.object());

    return;
}

void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{

   if(bsuccess){
      showTip(tr("聊天服务连接成功，正在登录..."),true);
      QJsonObject jsonObj;
      jsonObj["uid"] = _uid;
      jsonObj["token"] = _token;

      QJsonDocument doc(jsonObj);
      QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

      //发送tcp请求给chat server
     emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CHAT_LOGIN, jsonData);

   }else{
      showTip(tr("网络异常"),false);
      enableBtn(true);
   }

}

void LoginDialog::slot_login_failed(int err)
{
    QString result = QString("登录失败, err is %1")
                             .arg(err);
    showTip(result,false);
    enableBtn(true);
}

void LoginDialog::AddTipErr(TipErr te,QString tips){
    _tip_errs[te] = tips;
    showTip(tips, false);
}
void LoginDialog::DelTipErr(TipErr te){
    _tip_errs.remove(te);
    if(_tip_errs.empty()){
      ui->err_tip->clear();
      return;
    }

    showTip(_tip_errs.first(), false);
}

bool LoginDialog::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == ui->pass_edit && ev->type() == QEvent::Resize) {
        ui->pass_visible->setGeometry(ui->pass_edit->width() - 28,
                                      (ui->pass_edit->height() - 24) / 2,
                                      24, 24);
    }
    return QDialog::eventFilter(obj, ev);
}
