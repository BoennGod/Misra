#include "utils.h"
#include <atomic>
#include <thread>
#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <csignal>

class Node {
public:
    Node(utils::InitArgs args) : client(args.nodePort, args.nextAddress, args.nextPort, args.prob),
                                 ping(1), pong(-1), m(0),
                                 HAS_PING(false) {

        client.setReceiveCallback([this](int token) {
            std::lock_guard<std::mutex> lock(tokenMutex);
            tokenQueue.push(token);
            tokenCondition.notify_one();
        });

        if (args.isInit) {
            printf("Node is initiator, sending initial tokens...\n");
            ping = 1;
            pong = -1;
            m = 0;
            sendToken(ping);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            sendToken(pong);
        }

        setupSignalHandler();
    }

    void start() {
        std::thread(&Node::listen, this).detach();
        std::thread(&Node::processTokens, this).detach();
    }

private:
    NetworkClient client;
    std::atomic<bool> HAS_PING;
    std::atomic<bool> HAS_PONG;
    int ping;
    int pong;
    int m;
    std::queue<int> tokenQueue;
    std::mutex tokenMutex;
    std::condition_variable tokenCondition;

    void listen() {
        client.listen();
    }

    void processTokens() {
        std::thread(&Node::processPingTokens, this).detach();
        std::thread(&Node::processPongTokens, this).detach();
    }

    void processPingTokens() {
        while (true) {
            std::unique_lock<std::mutex> lock(tokenMutex);
            tokenCondition.wait(lock, [this]() { return !tokenQueue.empty(); });

            if (tokenQueue.front() > 0) {  // Process ping
                int token = tokenQueue.front();
                tokenQueue.pop();
                lock.unlock();
                handlePing(token);
            } else {
                tokenCondition.notify_one();
            }
        }
    }

    void processPongTokens() {
        while (true) {
            std::unique_lock<std::mutex> lock(tokenMutex);
            tokenCondition.wait(lock, [this]() { return !tokenQueue.empty(); });

            if (tokenQueue.front() < 0) {  // Process pong
                int token = tokenQueue.front();
                tokenQueue.pop();
                lock.unlock();
                handlePong(token);
            } else {
                tokenCondition.notify_one();
            }
        }
}

    void handlePing(int token) {
        std::thread([this, token]() {
            if (std::abs(token) < std::abs(m)) {
                printf("Received old token, ignoring\n");
                return;
            }
            HAS_PING = true;
            ping = token;


            if (HAS_PING && HAS_PONG){
                incarnate(token);
            }
            if (token == m && !HAS_PONG){
                printf("Pong lost\n");
                printf("ping: %d, pong: %d, m: %d \n", ping, pong, m);
                regenerate(ping);
                sendToken(pong);
            }

            printf("\n|||Entering critical section\n\n");
            std::this_thread::sleep_for(std::chrono::seconds(10));
            printf("\n|||Leaving critical section\n\n");
            
            sendToken(ping); // Send the current value of ping
        }).detach();
    }

    void handlePong(int token) {
        
        std::thread([this, token]() {
            if (std::abs(token) < std::abs(m)) {
            printf("Received old token, ignoring\n");
            return;
            }
            HAS_PONG = true;
            pong = token;

            if (HAS_PING && HAS_PONG){
                incarnate(token);
            }

            if (token == m && !HAS_PING){
                printf("Ping lost\n");
                printf("ping: %d, pong: %d, m: %d \n", ping, pong, m);
                regenerate(pong);
                sendToken(ping);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            sendToken(pong); // Send the current value of pong
        }).detach();
         
    }

    void sendToken(int token) {
        if (token < 0){
            HAS_PONG = false;
        } else if (token > 0){
            HAS_PING = false;
        }
        m = token;
        if (!client.send(token)) {
            printf("Error sending token: %d\n", token);
            disconnect();
        } else {
            printf("Token: %d sent successfully                           m value: %d \n", token, m);
        }
    }

    void incarnate(int value) {
        ping = utils::Abs(value) + 1;
        pong = -ping;
        printf("Incarnated! ping: %d, pong: %d\n", ping, pong);
    }

    void regenerate(int value) {
        ping = utils::Abs(value);
        pong = -ping;
        printf("Regenerated! ping: %d, pong: %d\n", ping, pong);
    }

    void disconnect() {
        if (!client.closeConnection()) {
            printf("Error closing client connection.\n");
        }
        std::exit(1);
    }

    void setupSignalHandler() {
    std::signal(SIGINT, [](int) {
        printf("\nGracefully shutting down...\n");
        // client.closeConnection();
        std::exit(0);
    });
}
};
