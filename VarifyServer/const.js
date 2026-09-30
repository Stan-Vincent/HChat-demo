//字符串常量
let code_prefix = "code_";

//错误码
const Errors = {
    Success : 0,
    RedisErr : 1,
    Exception : 2,
};

//导出常量，其他文件require('./const') 之后能拿到它们
//./ 表示“当前目录”，const 是文件名，.js 可以省略
module.exports = {code_prefix,Errors}