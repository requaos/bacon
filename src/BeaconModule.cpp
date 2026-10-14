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
        sendDm(mp, "Welcome to the local mesh! - Beacon");
        knownNodes[sender] = now;

        // Memory guard: drop the oldest entry if the limit is exceeded
        if (knownNodes.size() > MAX_TRACKED_NODES) {
            evictOldestNode(now);
        }
    } else {
        // SCENARIO 2 & 3: known node, check how long it has been away
        uint32_t timeSinceLastSeen = now - knownNodes[sender];

        if (timeSinceLastSeen > RE_ENTRY_DURATION_MS) {
            sendDm(mp, "Welcome back to the area! - Beacon");
        }

        // Reset the 3-hour clock
        knownNodes[sender] = now;
    }

    return ProcessMessage::CONTINUE;
}

void BeaconModule::sendDm(const meshtastic_MeshPacket &rx, const char *text)
{
    meshtastic_MeshPacket *p = allocDataPacket();
    p->to = rx.from;
    p->channel = rx.channel;
    p->want_ack = false;

    size_t len = strlen(text);
    if (len > sizeof(p->decoded.payload.bytes)) {
        len = sizeof(p->decoded.payload.bytes);
    }
    p->decoded.payload.size = len;
    memcpy(p->decoded.payload.bytes, text, len);

    service->sendToMesh(p);
}
