#pragma once
// Recordings over the hotspot, on port 80 (the LED panel's WebServer and REMOTE_TOKEN check):
//   GET /recs?k=<REMOTE_TOKEN>          one line per recording: "003.log 12345"
//   GET /rec?n=<N>&k=<REMOTE_TOKEN>     that recording, text/plain, as a download
// A wrong or missing key gets 403. Runs on the network task (net::isOnline first).
namespace sniff {
namespace web {
void begin();
void loop();
}
}
