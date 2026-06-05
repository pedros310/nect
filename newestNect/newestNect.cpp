#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <sqlite3.h>
#include "hdr/sqlite_modern_cpp.h"

#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

struct Post {
    long long postId;
    std::string author;
    std::string text;
    int likes;
    std::string date;
    std::string username;
};
std::string contentTyper(std::string url) {
    std::string contentType = "text/plain"; // по умолчанию

    if (url.find(".html") != std::string::npos) contentType = "text/html; charset=utf-8";
    else if (url.find(".css") != std::string::npos) contentType = "text/css";
    else if (url.find(".png") != std::string::npos) contentType = "image/png";
    else if (url.find(".svg") != std::string::npos) contentType = "image/svg+xml"; 
    else if (url.find(".ico") != std::string::npos) contentType = "image/x-icon";

    return contentType;
}
std::string wayFinder(char buffer[]) {
    std::string requestStr(buffer);
    std::string data;

    short pos1 = requestStr.find(' ');
    short pos2 = requestStr.find(' ', pos1 + 1);

    data = requestStr.substr(pos1 + 1, pos2 - (pos1 + 1));
    if (data == "/") {
        data += "page.html";
        data = "page" + data;
    }
    else {
        data = "page" + data;
    }
    
    return data;
}

std::string postGen(std::vector<Post> feed, int i) {
    std::ifstream file("page/post.txt", std::ios::binary);
    std::stringstream bufferr;

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("{{nickname}}") != std::string::npos) {
            line = feed[i].author;
            bufferr << line << "\n";
        }
        else if (line.find("{{post_date}}") != std::string::npos) {
            line = feed[i].date;
            bufferr << line << "\n";
        }
        else if (line.find("{{post_content}}") != std::string::npos) {
            line = feed[i].text;
            bufferr << line << "\n";
        }
        else if (line.find("{{quantity}}") != std::string::npos) {
            line = std::to_string(feed[i].likes);
            bufferr << line << "\n";
        }
        else if (line.find("{{username}}") != std::string::npos) {
            line = feed[i].username;
            bufferr << line << "\n";
        }
        else {
            bufferr << line << "\n";
        }
    }

    std::string data = bufferr.str();
    return data;
}


void feedGen(std::stringstream &buffer) {
    sqlite::database db("nect.db");
    
    try {
        
        db << "CREATE TABLE posts ("
            "postId	INTEGER PRIMARY KEY,"
            "author	TEXT,"
            "text	TEXT,"
            "likes	INTEGER,"
            "date TEXT DEFAULT (datetime('now', 'localtime')),"
            "username TEXT"
            ");";

    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка БД: " << e.what() << std::endl;
    }
    
    std::vector<Post> feed;

    try {
        db << "SELECT postId, author, text, likes, date, username FROM posts ORDER BY postId DESC;"
            >> [&](long long postId, std::string author, std::string text, int likes, std::string date, std::string username) {
            std::cout << "[БД] Найдена запись! Автор: " << author << ", Текст: " << text << std::endl;
            feed.push_back({ postId, author, text, likes, date, username});
            };
    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка БД при селекте: " << e.what() << std::endl;
    }

    for (int i = 0; i < feed.size(); i++) {
        buffer << postGen(feed, i);
    }
}

std::string homePageGen(std::string filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cout << "ФАЙЛ НЕ НАЙДЕН: [" << filePath << "]" << std::endl;
        return "";

    }
    std::string line;
    std::stringstream buffer;
    std::string marker = "<!--marker-->";
    while (std::getline(file, line)) {
        if (line.find(marker) != std::string::npos) {
            std::cout << "[ОТЛАДКА] Наткнулся на блок feed в HTML! Начинаю вшивку постов..." << std::endl;
            buffer << line << "\n";
            feedGen(buffer);
        }
        else {
            buffer << line << "\n";
        }
    }
    std::string homePage = buffer.str();
    return homePage;
}
std::string pageGen(std::string filePath) {
    std::string fileContent;
    std::ifstream file(filePath, std::ios::binary);
    if (filePath == "page/page.html") {
        fileContent = homePageGen("page/page.html");
        return fileContent;
    }
    if (!file.is_open()) {
        std::cout << "ФАЙЛ НЕ НАЙДЕН: [" << filePath << "]" << std::endl;
        return "";
    }

    else {
        std::stringstream buffer;
        buffer << file.rdbuf();
        fileContent = buffer.str();
    }
    return fileContent;
}
void func() {

    short timer = 0;
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0); //создаем сокет серва; AF_INET-значит на IPv4, SOCK_STREAM-значит TCP 
    sockaddr_in servAddr; // объект в который мы запишем какой порт, протокол маршрутизации(IPv4) и то что слушаем все порты что позволит винда 
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(8080);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    bind(listenSocket, (sockaddr*)&servAddr, sizeof(servAddr)); //биндим; (sockaddr*)-тип данных для глупой ф-и &servAddr-ссылка на объедок, sizeof(servAddr)-снова передаем объедок, только в байтах
    listen(listenSocket, SOMAXCONN); // начало слушания

    std::cout << "Жду подключения от браузера..." << std::endl;

    while (timer == 0) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL); //новый канал свзяи для подключенного клиента

        char buffer[2048] = { 0 }; //точное число прочитанных байт
        
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); //читаем текст запроса который нам прислал браузер; клиентовский порт, буффер, (опять сайзоф) и уточняем чтобы до 2047 байта заполнялось, ведь в конце нам придется ставить \0 для корректного вывода в cout
        
        std::string url = wayFinder(buffer);
        
        std::string contentType = contentTyper(url);
        std::string page = pageGen(url);
        std::string lengthStr = std::to_string(pageGen(url).length());
        

        std::cout << url << std::endl;

        std::string response =
            "HTTP/1.1 200 OK\r\n"        // Статус: все хорошо, запрос принят
            "Content-Length: " + lengthStr + "\r\n"     // Говорим браузеру: "Дальше будет текст длиной x байт"
            "Content-Type: " + contentType + "; charset=utf-8\r\n"
            "\r\n" +                       // ПУСТАЯ СТРОКА (Разделитель заголовка и тела)
            page;

       int bytesSent = send(clientSocket, response.c_str(), response.length(), 0);
       closesocket(clientSocket);
    }
    closesocket(listenSocket);
    WSACleanup();
}



int main()
{
    setlocale(LC_ALL, "ru");
    func();
}

