#include "utils.h"

enum class NodeState {
    NO_TOKEN,
    PING_TOKEN,
    PONG_TOKEN,
    BOTH_TOKENS
};

enum class TokenType {
    PING,
    PONG
};

class Node {
public:
    Node(utils::InitArgs args) : client(args.nodePort, args.nextAddress, args.nextPort),
        m(0), ping(0), pong(0),
        state(NodeState::NO_TOKEN) {


        client.setReceiveCallback([this](int token) {
            std::lock_guard<std::mutex> lock(tokenMutex);
            tokenQueue.push(token);
            tokenCondition.notify_one();
        });

        if (args.isInit) {
            printf("Node is initiator, sending initial tokens...\n");
            send(TokenType::PING);
            send(TokenType::PONG);
            state = NodeState::BOTH_TOKENS;
        }
    }

    void start() {
        std::thread(&Node::listen, this).detach();
        std::thread(&Node::handleState, this).detach();
        std::thread(&Node::processTokens, this).detach();
    }

private:
    NetworkClient client;
    int m;
    int ping;
    int pong;
    NodeState state;
    std::queue<int> tokenQueue;
    std::mutex tokenMutex;
    std::condition_variable tokenCondition;

    void listen() {
        client.listen();
    }

    void handleState() {
        while (true) {
            switch (state) {
            case NodeState::NO_TOKEN:
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                break;
            case NodeState::PING_TOKEN:  
                std::this_thread::sleep_for(std::chrono::seconds(1));
                send(TokenType::PING);
                break;
            case NodeState::PONG_TOKEN:
                send(TokenType::PONG);
                break;
            case NodeState::BOTH_TOKENS:
                incarnate(ping);
                send(TokenType::PING);
                send(TokenType::PONG);
                break;
            }
        }
    }

    void processTokens() {
        while (true) {
            std::unique_lock<std::mutex> lock(tokenMutex);
            tokenCondition.wait(lock, [this]() { return !tokenQueue.empty(); });

            int token = tokenQueue.front();
            tokenQueue.pop();
            lock.unlock();

            processToken(token);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void processToken(int token) {
        // token < m: ignore
        // token == m: ping or pong lost
        // token > 0 save as ping
        // token < 0 save as pong


        if (utils::Abs(token) < m) {
            printf("Received an old token, value: %d\n", token);
            return;
        }

        if (token == m) {
            if (m > 0) {
                printf("regenerating Pong\n");
                regenerate(token);
                return;
            } else if (m < 0) {
                printf("regenerating Ping\n");
                regenerate(token);
                return;
            }
        }

        if (token > 0) {
            incarnate(token);
            if (state == NodeState::NO_TOKEN) {
                state = NodeState::PING_TOKEN;
            } else if (state == NodeState::PONG_TOKEN) {
                state = NodeState::BOTH_TOKENS;
            } else {
                printf("How did we get here, token > 0?");
            }
        } else if (token < 0) {
            incarnate(token);
            if (state == NodeState::NO_TOKEN) {
                state = NodeState::PONG_TOKEN;
            } else if (state == NodeState::PING_TOKEN) {
                state = NodeState::BOTH_TOKENS;
            } else {
                printf("How did we get here, token < 0?");
            }
        }
    }

    void send(TokenType tokenType) {
        int token = (tokenType == TokenType::PING) ? ping : pong;
        if (!client.send(token)) {
            printf("Error sending token: %d\n", token);
            disconnect();
        } else {
            m = token;
            printf("Token: %d sent successfully\n", token);

            if (state == NodeState::PING_TOKEN && tokenType == TokenType::PING) {
                state = NodeState::NO_TOKEN;
            } else if (state == NodeState::PONG_TOKEN && tokenType == TokenType::PONG) {
                state = NodeState::NO_TOKEN;
            } else if (state == NodeState::BOTH_TOKENS) {
                state = (tokenType == TokenType::PING) ? NodeState::PONG_TOKEN : NodeState::PING_TOKEN;
            }
        }
    }

    void regenerate(int value) {
        ping = utils::Abs(value);
        pong = -ping;
        state = NodeState::BOTH_TOKENS;
        printf("Regenerated ping: %d, pong: %d\n", ping, pong);
    }

    void incarnate(int value) {
        ping = utils::Abs(value) + 1;
        pong = -ping;
        printf("Incarnated ping: %d, pong: %d\n", ping, pong);
    }

    void disconnect() {
        if (!client.closeConnection()) {
            printf("Error closing client connection.");
        }
        std::exit(1);
    }
};
