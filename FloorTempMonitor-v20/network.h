#ifndef NETWORK_H
#define NETWORK_H

#define _HOSTNAME "FloorTempMonitor"

void startWiFi();
void startTelnet();
void startMDNS(const char *Hostname);


#endif
