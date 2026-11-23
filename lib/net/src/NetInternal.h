#pragma once
// Private to lib/net: shared state between Wifi.cpp and ImprovSerial.cpp. Not part of the public API.
#if FEATURE_WIFI
namespace net {
// True while an Improv provisioning attempt owns the WiFi connection; Wifi.cpp must not reconnect then.
bool improvProvisioning();
}  // namespace net
#endif
