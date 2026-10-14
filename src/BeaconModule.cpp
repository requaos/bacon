#include "BeaconModule.h"

#include "MeshService.h"
#include "NodeDB.h"
#include "mesh/MeshTypes.h"

#include <Arduino.h>
#include <cstring>

BeaconModule *beaconModule;

BeaconModule::BeaconModule() : SinglePortModule("beacon", meshtastic_PortNum_TEXT_MESSAGE_APP)
{
    // See packets that are not addressed to this node as well.
    isPromiscuous = true;
}

void BeaconModule::setup()
{
    uint32_t now = millis();

    // Seed the tracker from the node DB so already-known neighbours do not
    // get a "welcome" flood every boot.
    size_t numNodes = nodeDB->getNumMeshNodes();
    for (size_t i = 0; i < numNodes && knownNodes.size() < MAX_TRACKED_NODES; i++) {
        meshtastic_NodeInfoLite *ni = nodeDB->getMeshNodeByIndex(i);
        if (ni) {
            knownNodes[ni->num] = now;
        }
    }
}

bool BeaconModule::wantPacket(const meshtastic_MeshPacket *p)
{
    // React to any over-the-air packet; ignore ones that only arrived via MQTT.
    return !p->via_mqtt;
}

void BeaconModule::evictOldestNode(uint32_t now)
{
    auto oldestIt = knownNodes.begin();
    uint32_t maxAge = 0;

    for (auto it = knownNodes.begin(); it != knownNodes.end(); ++it) {
        uint32_t age = now - it->second; // unsigned arithmetic is wrap-safe
        if (age >= maxAge) {
            maxAge = age;
            oldestIt = it;
        }
    }

    if (oldestIt != knownNodes.end()) {
        LOG_INFO("Beacon: evicting node 0x%08x from tracker (cache full)\n", oldestIt->first);
        knownNodes.erase(oldestIt);
    }
}

ProcessMessage BeaconModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    NodeNum sender = mp.from;

    if (sender == 0 || sender == nodeDB->getNodeNum()) {
        return ProcessMessage::CONTINUE;
    }

    uint32_t now = millis();

    if (knownNodes.find(sender) == knownNodes.end()) {
        // SCENARIO 1: brand new node
        sendDm(mp, BEACON_MSG_WELCOME);
        knownNodes[sender] = now;

        // Memory guard: drop the oldest entry if the limit is exceeded
        if (knownNodes.size() > MAX_TRACKED_NODES) {
            evictOldestNode(now);
        }
    } else {
        // SCENARIO 2 & 3: known node, check how long it has been away
        uint32_t timeSinceLastSeen = now - knownNodes[sender];

        if (timeSinceLastSeen > RE_ENTRY_DURATION_MS) {
            sendDm(mp, BEACON_MSG_WELCOME_BACK);
        }

        // Reset the 3-hour clock
        knownNodes[sender] = now;
    }

    return ProcessMessage::CONTINUE;
}

// Append s to buf, returning the new total length.
static size_t appendText(char *buf, size_t len, size_t cap, const char *s)
{
    while (*s && len + 1 < cap) {
        buf[len++] = *s++;
    }
    return len;
}

#if BEACON_INCLUDE_POSITION
// Append coord (1e-7 fixed-point degrees) as decimal degrees with 5
// fractional digits. Integer math only: newlib-nano has no %f support.
static size_t appendCoord(char *buf, size_t len, size_t cap, int32_t coord)
{
    if (len + 1 >= cap) {
        return len;
    }
    bool neg = coord < 0;
    uint32_t abs7 = neg ? (uint32_t)-coord : (uint32_t)coord; // |coord| <= 1.8e9, no INT32_MIN risk
    uint32_t deg = abs7 / 10000000U;
    uint32_t frac = (abs7 % 10000000U) / 100U;
    int n = snprintf(buf + len, cap - len, "%s%lu.%05lu", neg ? "-" : "", (unsigned long)deg, (unsigned long)frac);
    if (n < 0 || (size_t)n > cap - len - 1) {
        n = (int)(cap - len - 1);
    }
    return len + (size_t)n;
}
#endif

void BeaconModule::sendDm(const meshtastic_MeshPacket &rx, const char *text)
{
    // Matches meshtastic_Data_payload_t capacity (233 bytes).
    char payload[233];
    size_t len = 0;

    len = appendText(payload, 0, sizeof(payload), text);

#if BEACON_INCLUDE_POSITION
    // Same source PositionModule broadcasts from; 0/0 means "no position".
    if (localPosition.latitude_i != 0 || localPosition.longitude_i != 0) {
        len = appendText(payload, len, sizeof(payload), " (");
        len = appendCoord(payload, len, sizeof(payload), localPosition.latitude_i);
        len = appendText(payload, len, sizeof(payload), ", ");
        len = appendCoord(payload, len, sizeof(payload), localPosition.longitude_i);
        len = appendText(payload, len, sizeof(payload), ")");
    }
#endif

    meshtastic_MeshPacket *p = allocDataPacket();
    p->to = rx.from;
    p->channel = rx.channel;
    p->want_ack = false;

    if (len > sizeof(p->decoded.payload.bytes)) {
        len = sizeof(p->decoded.payload.bytes);
    }
    p->decoded.payload.size = len;
    memcpy(p->decoded.payload.bytes, payload, len);

    service->sendToMesh(p);
}
