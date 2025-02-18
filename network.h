#ifndef NETWORK_H
#define NETWORK_H

class NetworkClient {
public:
    NetworkClient(int nodePort, const std::string& nextAddress, int nextPort, float lossProbability)
        : nodePort(nodePort), nextAddress(nextAddress), nextPort(nextPort), isConnected(false), lossProbability(lossProbability) {}

    ~NetworkClient() {
        closeConnection();
    }

    // Set the callback function for receiving messages
    void setReceiveCallback(const std::function<void(int)>& callback) {
        receiveCallback = callback;
    }

    // Listen on the given port for incoming connections
    void listen() {
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            perror("Error creating server socket");
            exit(EXIT_FAILURE);
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(nodePort);

        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            perror("Error binding server socket");
            exit(EXIT_FAILURE);
        }

        if (::listen(serverSocket, 5) < 0) {
            perror("Error listening on server socket");
            exit(EXIT_FAILURE);
        }

        std::cout << "Listening on port " << nodePort << std::endl;

        while (true) {
            sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

            if (clientSocket < 0) {
                perror("Error accepting client connection");
                continue;
            }

            std::thread(&NetworkClient::messageHandler, this, clientSocket).detach();
        }

        close(serverSocket);
    }

    // Send a value to the next node in the ring
    bool send(int value) {
        if (shouldLoseMessage(value)) {
            std::cout << "Token lost: " << value << std::endl;
            return true; 
        }

        std::lock_guard<std::mutex> lock(connectionMutex);
        if (!isConnected) {
            if (!connectToNextNode()) {
                return false;
            }
        }

        std::string message = std::to_string(value) + "\n";
        ssize_t sent = ::send(nextSocket, message.c_str(), message.size(), 0);
        if (sent < 0) {
            perror("Error sending message");
            closeConnection();
            return false;
        }

        // std::cout << "Sent token: " << value << std::endl;  // Add logging
        return true;
    }

    // Close the connection
    bool closeConnection() {
        std::lock_guard<std::mutex> lock(connectionMutex);
        if (isConnected) {
            close(nextSocket);
            isConnected = false;
        }
        return true;
    }

private:
    int nodePort;
    std::string nextAddress;
    int nextPort;
    int nextSocket;
    std::atomic<bool> isConnected;
    std::mutex connectionMutex;
    std::function<void(int)> receiveCallback;
    float lossProbability;

    //generate token losing
    bool shouldLoseMessage(int value) {
        if (value > 0){
            return false;
        }
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 100.0f);

        return dis(gen) < lossProbability; // True if the message is lost
    }

    // Connect to the next node in the ring
    bool connectToNextNode() {
        nextSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (nextSocket < 0) {
            perror("Error creating socket");
            return false;
        }

        sockaddr_in nextAddr{};
        nextAddr.sin_family = AF_INET;
        nextAddr.sin_port = htons(nextPort);

        if (inet_pton(AF_INET, nextAddress.c_str(), &nextAddr.sin_addr) <= 0) {
            perror("Invalid address for next node");
            return false;
        }

        if (connect(nextSocket, (struct sockaddr*)&nextAddr, sizeof(nextAddr)) < 0) {
            perror("Error connecting to next node");
            return false;
        }

        isConnected = true;
        return true;
    }

    // Handle an incoming connection
    void messageHandler(int clientSocket) {
        char buffer[1024];

        while (true) {
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

            if (bytesRead <= 0) {
                if (bytesRead == 0) {
                    std::cout << "Client disconnected" << std::endl;
                } else {
                    perror("Error reading message");
                }
                close(clientSocket);
                return;
            }

            std::string message(buffer);
            message.erase(message.find_last_not_of(" \n\r\t") + 1);

            try {
                int value = std::stoi(message);
                printf("received token: %d\n", value);
                if (receiveCallback) {
                    receiveCallback(value);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing message: " << message << " - " << e.what() << std::endl;
            }
        }
    }
};

#endif // NETWORK_H
