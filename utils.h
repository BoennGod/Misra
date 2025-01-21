#ifndef UTILS_H
#define UTILS_H

namespace utils {


struct InitArgs {
    bool isInit;                 
    std::string nextAddress;      
    int nodePort;               
    int nextPort;               


    InitArgs()
        : isInit(false), nextAddress("127.0.0.1"), nodePort(8089), nextPort(8090) {}
};


int Abs(int x) {
    return (x < 0) ? -x : x;
}


InitArgs ReadArguments(int argc, char* argv[]) {
    InitArgs args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-initiator") {
            args.isInit = true;
        } else if (arg == "-node_port" && i + 1 < argc) {
            args.nodePort = std::stoi(argv[++i]);
        } else if (arg == "-next_ip" && i + 1 < argc) {
            args.nextAddress = argv[++i];
        } else if (arg == "-next_port" && i + 1 < argc) {
            args.nextPort = std::stoi(argv[++i]);
        }
    }

    // Validate required arguments
    if (args.nextAddress.empty() || args.nextPort == 0) {
        std::cerr << "Minimal usage: ./program -next_ip <ip> -next_port <port>\n";
        std::cerr << "With all flags: ./program -initiator -node_port <port> -next_ip <ip> -next_port <port>\n";
        exit(1);
    }

    return args;
}

} // namespace utils
#endif // UTILS_H