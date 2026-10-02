//引入依赖
const grpc = require('@grpc/grpc-js');
const { v4: uuidv4 } = require('uuid');
const emailModule = require('./email');
const const_module = require('./const');
const message_proto = require('./proto');
const redis_module = require('./redis')


//async：声明这是异步函数，里面能用 await
//call：gRPC 传进来的请求上下文，call.request 就是客户端发来的 GetVarifyReq
//callback：你调它，gRPC 就把结果发回客户端
async function GetVarifyCode(call, callback) {
    
    console.log("email is ", call.request.email)

    try{
        let query_res = await redis_module.GetRedis(const_module.code_prefix+call.request.email);
        console.log("query_res is ", query_res)

        let uniqueId = query_res;
        if(!query_res){
            uniqueId = uuidv4();
            if (uniqueId.length > 4) {
                uniqueId = uniqueId.substring(0, 4);
            } 
            let bres = await redis_module.SetRedisExpire(const_module.code_prefix+call.request.email, uniqueId,180)
            if(!bres){
                callback(null, { email:  call.request.email,
                    error:const_module.Errors.RedisErr
                });
                return;
            }
        }

        console.log("uniqueId is ", uniqueId)
        let text_str =  '您的验证码为'+ uniqueId +'请三分钟内完成注册'
        //发送邮件
        let mailOptions = {
            from: '19178223162@163.com',
            to: call.request.email,
            subject: '验证码',
            text: text_str,
        };

        let send_res = await emailModule.SendMail(mailOptions);
        console.log("send res is ", send_res)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Success
        }); 


    }catch(error){
        console.log("catch error is ", error)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Exception
        }); 
    }

}

function main() {
    //创建一个 gRPC Server 对象
    var server = new grpc.Server()
    
    /*
    *第一个参数：从 .proto 加载的服务定义
    *第二个参数：定义的处理函数。key 是方法名，value 是函数
    *当客户端调 GetVarifyCode 时，调用定义的处理函数
    */
    server.addService(message_proto.VarifyService.service, { GetVarifyCode: GetVarifyCode })
    
    /*
    *bindAsync：绑定地址和端口，异步
    *'0.0.0.0:50051'：监听本机所有网卡的 50051 端口
    *createInsecure()：不加密（生产环境该用 TLS）
    *第三个参数是绑定成功后的回调
    */
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')        
    })
}

main()