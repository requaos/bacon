#pragma once

#include "SinglePortModule.h"
#include "configuration.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

#include <cstdint>
#include <map>

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
