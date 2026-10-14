#pragma once

#include "SinglePortModule.h"
#include "configuration.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

#include <cstdint>
#include <map>

// Greeting texts. Override at build time by setting BEACON_WELCOME_MSG /
// BEACON_WELCOME_BACK_MSG when running build.sh — it generates
// BeaconMsgConfig.h next to this file, which takes precedence over these
// defaults. Keep overrides to a single line of at most ~200 bytes (the
// payload is truncated on air anyway).
#if __has_include("BeaconMsgConfig.h")
#include "BeaconMsgConfig.h"
#endif

#ifndef BEACON_MSG_WELCOME
#define BEACON_MSG_WELCOME "Welcome to the local mesh! - Beacon"
#endif

#ifndef BEACON_MSG_WELCOME_BACK
#define BEACON_MSG_WELCOME_BACK "Welcome back to the area! - Beacon"
#endif

// Append the beacon's own coordinates to greetings (build.sh:
// BEACON_INCLUDE_POSITION=1). Uses the node's current position — GPS fix
// or a fixed position set from the app — and silently omits the coordinates
// if none is known yet.
#ifndef BEACON_INCLUDE_POSITION
#define BEACON_INCLUDE_POSITION 0
#endif

/**
 * Beacon: a welcome-bot for the local mesh.
 *
 * Greets nodes the first time they are heard, and greets them again if they
 * disappear for more than RE_ENTRY_DURATION_MS. Runs purely on the routing
 * thread's receive path (no timers, no keep-awake locks) so the node can
 * still enter deep sleep between packets.
 */
class BeaconModule : public SinglePortModule
{
  public:
    BeaconModule();

    virtual void setup() override;

  protected:
    // Accept every over-the-air packet (any port number), like NeighborInfoModule.
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;

    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    void sendDm(const meshtastic_MeshPacket &rx, const char *text);
    void evictOldestNode(uint32_t now);

    // Node ID -> last time (millis) we heard a packet from it
    std::map<NodeNum, uint32_t> knownNodes;

    // Hard constraints to protect nRF52840 resources (256 KB RAM)
    static constexpr size_t MAX_TRACKED_NODES = 250;               // ~9.6 KB of RAM
    static constexpr uint32_t RE_ENTRY_DURATION_MS = 3UL * 60UL * 60UL * 1000UL; // 3 hours
};

extern BeaconModule *beaconModule;
