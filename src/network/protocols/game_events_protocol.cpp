#include "network/protocols/game_events_protocol.hpp"

#include "karts/abstract_kart.hpp"
#include "items/attachment.hpp"
#include "items/item.hpp"
#include "items/item_manager.hpp"
#include "items/powerup.hpp"
#include "modes/world.hpp"
#include "network/event.hpp"
#include "network/game_setup.hpp"
#include "network/protocol_manager.hpp"
#include "network/stk_host.hpp"
#include "network/stk_peer.hpp"

#include <stdint.h>

/** This class handles all 'major' game events. E.g. collecting an item,
 *  finishing a race etc. The game events manager is notified from the 
 *  game code, and it calls the corresponding function in this class.
 *  The server then notifies all clients. Clients receive the message
 *  in the synchronous notifyEvent function here, decode the message
 *  and call the original game code. The functions name are identical,
 *  e.g. kartFinishedRace(some parameter) is called from the GameEventManager
 *  on the server, and the received message is then handled by 
 *  kartFinishedRace(const NetworkString &).
 */
GameEventsProtocol::GameEventsProtocol() : Protocol(PROTOCOL_GAME_EVENTS)
{
}   // GameEventsProtocol

// ----------------------------------------------------------------------------
GameEventsProtocol::~GameEventsProtocol()
{
}   // ~GameEventsProtocol

// ----------------------------------------------------------------------------
bool GameEventsProtocol::notifyEvent(Event* event)
{
    if (event->getType() != EVENT_TYPE_MESSAGE)
        return true;
    NetworkString &data = event->data();
    if (data.size() < 5) // for token and type
    {
        Log::warn("GameEventsProtocol", "Too short message.");
        return true;
    }
    if ( event->getPeer()->getClientServerToken() != data.getUInt32())
    {
        Log::warn("GameEventsProtocol", "Bad token.");
        return true;
    }
    int8_t type = data.getUInt8();
    switch (type)
    {
        case GE_ITEM_COLLECTED:
            collectedItem(data);      break;
        case GE_KART_FINISHED_RACE:
            kartFinishedRace(data);   break;
        default:
            Log::warn("GameEventsProtocol", "Unkown message type.");
            break;
    }
    return true;
}   // notifyEvent

// ----------------------------------------------------------------------------
/** Called on the server when an item is collected.
 */
void GameEventsProtocol::collectedItem(Item* item, AbstractKart* kart)
{
    GameSetup* setup = STKHost::get()->getGameSetup();
    assert(setup);

    const std::vector<STKPeer*> &peers = STKHost::get()->getPeers();
    for (unsigned int i = 0; i < peers.size(); i++)
    {
        NetworkString *ns = getNetworkString(7);
        ns->setSynchronous(true);
        // Item picked : send item id, powerup type and kart race id
        uint8_t powerup = 0;
        if (item->getType() == Item::ITEM_BANANA)
            powerup = (int)(kart->getAttachment()->getType());
        else if (item->getType() == Item::ITEM_BONUS_BOX)
            powerup = (((int)(kart->getPowerup()->getType()) << 4) & 0xf0) 
                           + (kart->getPowerup()->getNum()         & 0x0f);

        ns->addUInt8(GE_ITEM_COLLECTED).addUInt32(item->getItemId())
           .addUInt8(powerup).addUInt8(kart->getWorldKartId());
        peers[i]->sendPacket(ns, /*reliable*/true);
        delete ns;
        Log::info("GameEventsProtocol",
                  "Notified a peer that a kart collected item %d.",
                  (int)(kart->getPowerup()->getType()));
    }
}   // collectedItem

// ----------------------------------------------------------------------------
/** Called on the client when an itemCollected message is received.
 */
void GameEventsProtocol::collectedItem(const NetworkString &data)
{
    if (data.size() < 6)
    {
            Log::warn("GameEventsProtocol", "Too short message.");
        }
        uint32_t item_id = data.getUInt32();
        uint8_t powerup_type = data.getUInt8();
        uint8_t kart_id = data.getUInt8();
        // now set the kart powerup
        AbstractKart* kart = World::getWorld()->getKart(kart_id);
        ItemManager::get()->collectedItem(ItemManager::get()->getItem(item_id),
                                          kart, powerup_type);
        Log::info("GameEventsProtocol", "Item %d picked by a player.",
                  powerup_type);
    }   // collectedItem

// ----------------------------------------------------------------------------
/** This function is called from the server when a kart finishes a race. It
 *  sends a notification to all clients about this event.
 *  \param kart The kart that finished the race.
 *  \param time The time at which the kart finished.
 */
void GameEventsProtocol::kartFinishedRace(AbstractKart *kart, float time)
{
    NetworkString *ns = getNetworkString(20);
    ns->setSynchronous(true);
    ns->addUInt8(GE_KART_FINISHED_RACE).addUInt8(kart->getWorldKartId())
       .addFloat(time);
    sendMessageToPeersChangingToken(ns, /*reliable*/true);
    delete ns;
}   // kartFinishedRace

// ----------------------------------------------------------------------------
/** This function is called on a client when it receives a kartFinishedRace
 *  event from the server. It updates the game with this information.
 *  \param ns The message from the server.
 */
void GameEventsProtocol::kartFinishedRace(const NetworkString &ns)
{
    uint8_t kart_id = ns.getUInt8();
    float time      = ns.getFloat();
    World::getWorld()->getKart(kart_id)->finishedRace(time,
                                                      /*from_server*/true);
}   // kartFinishedRace
