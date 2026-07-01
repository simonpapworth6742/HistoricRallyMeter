#ifndef RALLY_WEB_SERVER_H
#define RALLY_WEB_SERVER_H

#include <cstdint>
#include <string>

struct AppData;

class RallyWebServer {
public:
    explicit RallyWebServer(AppData* data);
    ~RallyWebServer();

    bool start(int port);
    void stop();
    void poll(int64_t now_ms);

    std::string getWebUrl() const;

private:
    void* impl_;
    AppData* data_;
};

void webNotifyStateChanged(AppData* data);

#endif // RALLY_WEB_SERVER_H
