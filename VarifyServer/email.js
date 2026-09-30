const nodemailer = require('nodemailer');//第三方邮件库
const config_module = require("./config")//自己的配置模块

// 创建发送邮件的代理
let transport = nodemailer.createTransport({
    host: 'smtp.163.com',   //网易 163 邮箱的 SMTP 服务器地址和端口
    port: 465,
    secure: true,   //表示用 SSL 加密
    auth: {
        user: config_module.email_user, // 发送方邮箱地址
        pass: config_module.email_pass // 邮箱授权码或者密码
    }
});

/**
 * 发送邮件的函数
 * @param {*} mailOptions_ 发送邮件的参数
 * @returns 
 */
function SendMail(mailOptions_){
    return new Promise(function(resolve, reject){
        transport.sendMail(mailOptions_, function(error, info){
            if (error) {
                console.log(error);
                reject(error);
            } else {
                console.log('邮件已成功发送：' + info.response);
                resolve(info.response)
            }
        });
    })

}

module.exports.SendMail = SendMail