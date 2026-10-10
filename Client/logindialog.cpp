#include "logindialog.h"
#include "ui_logindialog.h"
#include <QDebug>
#include "httpmgr.h"
#include "tcpmgr.h"
#include <QRegularExpression>
#include <QPainter>
#include <QPainterPath>
#include "clickedlabel.h"
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
    if (!CryptProtectData(&in, L"HChat", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        qWarning() << "CryptProtectData failed, error =" << GetLastError();
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
        qWarning() << "CryptUnprotectData failed, error =" << GetLastError();
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
    connect(ui->reg_btn, &QPushButton::clicked, this, &LoginDialog::sig_switch_register);
    ui->forget_label->SetState("normal","hover","","selected","selected_hover","");
    ui->forget_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_label, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);

    // 密码显示/隐藏图标（与注册页 pass_visible 完全一致：同样 20x20、同一套图标）
    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->pass_visible->SetState("unvisible", "unvisible_hover", "",
                               "visible", "visible_hover", "");
    connect(ui->pass_visible, &ClickedLabel::clicked, this, [this]() {
        bool b_visible = (ui->pass_edit->echoMode() == QLineEdit::Normal);
        ui->pass_edit->setEchoMode(b_visible ? QLineEdit::Password : QLineEdit::Normal);
        qDebug() << "pass_visible clicked, echoMode =" << ui->pass_edit->echoMode();
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
            qDebug() << "auto login triggered";
            on_login_btn_clicked();
        });
    }
}

LoginDialog::~LoginDialog()
{
    qDebug()<<"destruct LoginDlg";
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
        qWarning() << "saved password can not be decrypted, drop it";
        st.remove("login/pwd_cipher");
        return;
    }
    ui->pass_edit->setText(pwd);
    ui->auto_login_chk->setChecked(st.value("login/auto_login", false).toBool());
}

void LoginDialog::SetAutoLogin(bool on)
{
    ui->remember_pwd_chk->setChecked(on);
    ui->auto_login_chk->setChecked(on);
    if (!on) {
        saveSettings();          // 立刻把勾选状态落盘，清掉旧的密码密文
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
    QPixmap originalPixmap(":/res/head_1.jpg");
      // 设置图片自动缩放
    qDebug()<< originalPixmap.size() << ui->head_label->size();
    originalPixmap = originalPixmap.scaled(ui->head_label->size(),
            Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 创建一个和原始图片相同大小的QPixmap，用于绘制圆角图片
    QPixmap roundedPixmap(originalPixmap.size());
    roundedPixmap.fill(Qt::transparent); // 用透明色填充

    QPainter painter(&roundedPixmap);
    painter.setRenderHint(QPainter::Antialiasing); // 设置抗锯齿，使圆角更平滑
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 使用QPainterPath设置圆角
    QPainterPath path;
    path.addRoundedRect(0, 0, originalPixmap.width(), originalPixmap.height(), 10, 10); // 最后两个参数分别是x和y方向的圆角半径
    painter.setClipPath(path);

    // 将原始图片绘制到roundedPixmap上
    painter.drawPixmap(0, 0, originalPixmap);

    // 设置绘制好的圆角图片到QLabel上
    ui->head_label->setPixmap(roundedPixmap);

}

void LoginDialog::initHttpHandlers()
{
    //注册获取登录回包逻辑
    _handlers.insert(ReqId::ID_LOGIN_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
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
        qDebug()<< "email is " << email << " uid is " << si.Uid <<" host is "
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
    qDebug()<<"LonginDialog [slot_forget_pwd]";
    emit sig_switch_reset();
}

bool LoginDialog::checkUserValid(){

    auto email = ui->email_edit->text();
    if(email.isEmpty()){
        qDebug() << "email empty " ;
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool LoginDialog::checkPwdValid(){
    auto pwd = ui->pass_edit->text();
    if(pwd.length() < 6 || pwd.length() > 15){
        qDebug() << "Pass length invalid";
        //提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
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
        return false;;
    }

    DelTipErr(TipErr::TIP_PWD_ERR);

    return true;
}

bool LoginDialog::enableBtn(bool enabled)
{
    ui->login_btn->setEnabled(enabled);
    ui->reg_btn->setEnabled(enabled);
    return true;
}

void LoginDialog::on_login_btn_clicked()
{
    qDebug()<<"login btn clicked";
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
        qDebug() << "no handler registered for req id " << id;
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
