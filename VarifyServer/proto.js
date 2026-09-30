/*
proto.js 在程序启动时读 message.proto，
变成能用的 JS 对象，然后导出。
*/

const path = require('path') // Node 自带的路径工具
const grpc = require('@grpc/grpc-js') // gRPC 库
const protoLoader = require('@grpc/proto-loader')// 加载 .proto 的工具

//__dirname = 当前文件所在目录的绝对路径。
//path.join 把它和 message.proto 拼起来，得到完整路径。
const PROTO_PATH = path.join(__dirname, 'message.proto')

const packageDefinition = protoLoader.loadSync(PROTO_PATH, { 
    keepCase: true, 
    longs: String, 
    enums: String, 
    defaults: true, 
    oneofs: true 
})
const protoDescriptor = grpc.loadPackageDefinition(packageDefinition)

const message_proto = protoDescriptor.message

module.exports = message_proto