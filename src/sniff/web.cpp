#include "web.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>
#include "../net/remote_config.h"
#include "../util/recname.h"
#include "../util/remotegate.h"
#include "store.h"

namespace sniff {
namespace web {
static WebServer* server = nullptr;
static remotegate::Gate gate(REMOTE_TOKEN);

static bool keyOk() {
    if (gate.keyOk(server->arg("k").c_str())) return true;
    server->send(403, "text/plain", "Wrong key");
    return false;
}

static void handleList() {
    if (!keyOk()) return;
    int nums[64];
    size_t sizes[64];
    const int n = store::list(nums, sizes, 64);
    String body;
    for (int i = 0; i < n; i++) {
        char line[40];
        snprintf(line, sizeof line, "%03d.log %u\n", nums[i], static_cast<unsigned>(sizes[i]));
        body += line;
    }
    server->send(200, "text/plain", body);
}

static void handleOne() {
    if (!keyOk()) return;
    const long n = server->arg("n").toInt();
    char p[24];
    recname::path(static_cast<int>(n), p, sizeof p);
    if (n <= 0 || !LittleFS.exists(p)) {
        server->send(404, "text/plain", "No such recording");
        return;
    }
    File f = LittleFS.open(p, FILE_READ);
    if (!f) {
        server->send(500, "text/plain", "Can't open it");
        return;
    }
    char disp[48];
    snprintf(disp, sizeof disp, "attachment; filename=\"%03ld.log\"", n);
    server->sendHeader("Content-Disposition", disp);
    server->streamFile(f, "text/plain");
    f.close();
}

void begin() {
    if (!gate.enabled() || server) return;
    server = new WebServer(80);
    server->on("/recs", HTTP_GET, handleList);
    server->on("/rec", HTTP_GET, handleOne);
    server->begin();
    Serial.println("[web] on: /recs?k=<token>, /rec?n=<N>&k=<token>");
}

void loop() {
    if (server) server->handleClient();
}
}
}
