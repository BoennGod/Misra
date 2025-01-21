#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <sstream> 
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <map>
#include <atomic>
#include <chrono>
#include <csignal>
#include <future> 
#include <cstring>       
#include <arpa/inet.h>   
#include <unistd.h>      
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <queue>

#include "utils.h"
#include "network.h"
#include "node.h"



int main(int argc, char* argv[]) {
    utils::InitArgs args = utils::ReadArguments(argc, argv);

    Node node(args);

    node.start();

    std::this_thread::sleep_for(std::chrono::hours(24));
    
    return 0;
}