#ifndef WEB_COMMANDS_H
#define WEB_COMMANDS_H

struct AppData;

// Returns true if rally state changed (clients should receive a state snapshot).
bool webHandleCommand(AppData* data, const char* json);

#endif // WEB_COMMANDS_H
