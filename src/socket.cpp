#include "socket.h"


extern "C" Value *c_socket(Parser_Struct *parser_struct, Function *TheFunction,
                 std::string Callee, Data_Tree data_type, std::vector<Data_Tree> &args_type,
                 Value *scope_struct, std::vector<std::unique_ptr<ExprAST>> &Args, std::vector<Value*> &ArgsV) {
    
    // Cast args to 32-bit integers (standard for C ints)
    Value *domain = Builder->CreateIntCast(ArgsV[0], Builder->getInt32Ty(), true);
    Value *type = Builder->CreateIntCast(ArgsV[1], Builder->getInt32Ty(), true);
    Value *protocol = Builder->CreateIntCast(ArgsV[2], Builder->getInt32Ty(), true);
    
    return callret("socket", {domain, type, protocol});
}


extern "C" Value *c_listen(Parser_Struct *parser_struct, Function *TheFunction,
                 std::string Callee, Data_Tree data_type, std::vector<Data_Tree> &args_type,
                 Value *scope_struct, std::vector<std::unique_ptr<ExprAST>> &Args, std::vector<Value*> &ArgsV) {
    return callret("listen", {ArgsV[0], const_int(3)});
}



// send(int sockfd, const void *buf, size_t len, int flags)
extern "C" Value *c_send(Parser_Struct *parser_struct, Function *TheFunction,
                 std::string Callee, Data_Tree data_type, std::vector<Data_Tree> &args_type,
                 Value *scope_struct, std::vector<std::unique_ptr<ExprAST>> &Args, std::vector<Value*> &ArgsV) {
    Value *str = ArgsV[1];
    Value *msg = Builder->CreateExtractValue(str, {0});
    Value *msg_size = Builder->CreateExtractValue(str, {1});
    return callret("send", {ArgsV[0], msg, msg_size, const_int(0)});
}

// close(int fd)
extern "C" Value *neve_close(Parser_Struct *parser_struct, Function *TheFunction,
                 std::string Callee, Data_Tree data_type, std::vector<Data_Tree> &args_type,
                 Value *scope_struct, std::vector<std::unique_ptr<ExprAST>> &Args, std::vector<Value*> &ArgsV) {
    return callret("close", {ArgsV[0]});
}






extern "C" Value *reuseaddr(Parser_Struct *parser_struct, Function *TheFunction,
                 std::string Callee, Data_Tree data_type, std::vector<Data_Tree> &args_type,
                 Value *scope_struct, std::vector<std::unique_ptr<ExprAST>> &Args, std::vector<Value*> &ArgsV) {
    // ArgsV[0] = socket file descriptor
    Value *sockfd = Builder->CreateIntCast(ArgsV[0], intTy, true);
    
    // level = SOL_SOCKET (usually 1 on Linux)
    Value *level = const_int(1);
    
    // optname = SO_REUSEADDR (usually 2 on Linux)
    Value *optname = const_int(2);
    
    // optval = 1 (we allocate an integer, set it to 1, and cast to void*)
    AllocaInst *OptValAlloca = Builder->CreateAlloca(intTy, nullptr, "optval");
    Builder->CreateStore(Builder->getInt32(1), OptValAlloca);
    Value *optval_ptr = Builder->CreatePointerCast(OptValAlloca, int8PtrTy);
    
    // optlen = sizeof(int) (which is 4 bytes)
    Value *optlen = const_int(4);
    
    // Call setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))
    return callret("setsockopt", {sockfd, level, optname, optval_ptr, optlen});
}



extern "C" Value *bind_ipv4(Parser_Struct *parser_struct, Function *TheFunction,
                 std::string Callee, Data_Tree data_type, std::vector<Data_Tree> &args_type,
                 Value *scope_struct, std::vector<std::unique_ptr<ExprAST>> &Args, std::vector<Value*> &ArgsV) {
    
    // ArgsV[0] = fd
    Value *sockfd = Builder->CreateIntCast(ArgsV[0], intTy, true);
    
    // ArgsV[1] = ip string (Neve struct)
    // Extract the char pointer (msg) from the Neve string struct
    Value *ip_str_struct = ArgsV[1];
    Value *ip_ptr = Builder->CreateExtractValue(ip_str_struct, {0});
    
    // ArgsV[2] = port
    Value *port_val = Builder->CreateIntCast(ArgsV[2], int16Ty, false);

    // 1. Call htons() on the port
    Function *htons_func = TheFunction->getParent()->getFunction("htons");
    if (!htons_func) {
        FunctionType *htonsType = FunctionType::get(int16Ty, {int16Ty}, false);
        htons_func = Function::Create(htonsType, Function::ExternalLinkage, "htons", TheFunction->getParent());
    }
    Value *network_port = Builder->CreateCall(htons_func, {port_val});

    // 1.5. Call inet_addr() to convert string IP to i32 network byte order integer
    Function *inet_addr_func = TheFunction->getParent()->getFunction("inet_addr");
    if (!inet_addr_func) {
        // inet_addr takes an i8* and returns an i32 (in_addr_t)
        FunctionType *inetAddrType = FunctionType::get(intTy, {int8PtrTy}, false);
        inet_addr_func = Function::Create(inetAddrType, Function::ExternalLinkage, "inet_addr", TheFunction->getParent());
    }
    Value *network_ip = Builder->CreateCall(inet_addr_func, {ip_ptr});

    // 2. Define the LLVM Struct Type for sockaddr_in
    StructType *SockAddrInTy = StructType::getTypeByName(Builder->getContext(), "struct.sockaddr_in");
    if (!SockAddrInTy) {
        SockAddrInTy = StructType::create(Builder->getContext(), "struct.sockaddr_in");
        SockAddrInTy->setBody({
            int16Ty,                                // sin_family
            int16Ty,                                // sin_port
            intTy,                                  // sin_addr.s_addr
            ArrayType::get(Builder->getInt8Ty(), 8) // sin_zero padding
        }, false);
    }

    AllocaInst *AddrAlloca = Builder->CreateAlloca(SockAddrInTy, nullptr, "address");

    // address.sin_family = AF_INET (usually 2)
    Value *FamilyPtr = Builder->CreateStructGEP(SockAddrInTy, AddrAlloca, 0, "sin_family");
    Builder->CreateStore(Builder->getInt16(2), FamilyPtr);

    // address.sin_port = htons(port)
    Value *PortPtr = Builder->CreateStructGEP(SockAddrInTy, AddrAlloca, 1, "sin_port");
    Builder->CreateStore(network_port, PortPtr);

    // address.sin_addr.s_addr = inet_addr(ip_ptr)
    Value *AddrPtr = Builder->CreateStructGEP(SockAddrInTy, AddrAlloca, 2, "sin_addr");
    Builder->CreateStore(network_ip, AddrPtr); // Store the converted IP here!

    // bind(sockfd, (struct sockaddr *)&address, sizeof(address))
    Value *CastAddr = Builder->CreatePointerCast(AddrAlloca, int8PtrTy);
    Value *AddrLen = Builder->getInt32(16); 

    call("bind", {sockfd, CastAddr, AddrLen});
        
    return CastAddr;
}




extern "C" Value *net_accept(Parser_Struct *parser_struct, Function *TheFunction,
                 std::string Callee, Data_Tree data_type, std::vector<Data_Tree> &args_type,
                 Value *scope_struct, std::vector<std::unique_ptr<ExprAST>> &Args, std::vector<Value*> &ArgsV) {
    
    Value *sockfd = Builder->CreateIntCast(ArgsV[0], intTy, true);
    Value *addr_ptr = Builder->CreatePointerCast(ArgsV[1], int8PtrTy);
    
    // 1. Allocate an integer on the stack for `addrlen`
    // This is the equivalent of: int addrlen;
    AllocaInst *AddrLenAlloca = Builder->CreateAlloca(intTy, nullptr, "addrlen");
    
    // 2. Initialize it to 16 (sizeof(struct sockaddr_in))
    // This is the equivalent of: addrlen = sizeof(address);
    Builder->CreateStore(Builder->getInt32(16), AddrLenAlloca);
    
    // 3. Call the system accept(server_fd, &address, &addrlen)
    // accept returns the new socket file descriptor as an i32
    return callret("accept", {sockfd, addr_ptr, AddrLenAlloca});
}
