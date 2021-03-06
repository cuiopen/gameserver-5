/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Simon Sandström
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WORLDSERVER_SRC_PROTOCOL_71_H_
#define WORLDSERVER_SRC_PROTOCOL_71_H_

#include "protocol.h"

#include <array>
#include <functional>
#include <string>
#include <memory>

// gameengine
#include "player.h"
#include "game_position.h"
#include "container.h"

// world
#include "creature.h"
#include "position.h"
#include "item.h"

class Connection;
class IncomingPacket;
class OutgoingPacket;
class GameEngineQueue;
class AccountReader;
class WorldInterface;

class Protocol71 : public Protocol
{
 public:
  Protocol71(const std::function<void(void)>& closeProtocol,
             std::unique_ptr<Connection>&& connection,
             GameEngineQueue* gameEngineQueue,
             AccountReader* accountReader);

  // Delete copy constructors
  Protocol71(const Protocol71&) = delete;
  Protocol71& operator=(const Protocol71&) = delete;

  // Called by World (from CreatureCtrl)
  void onCreatureSpawn(const WorldInterface& world_interface,
                       const Creature& creature,
                       const Position& position) override;
  void onCreatureDespawn(const WorldInterface& world_interface,
                         const Creature& creature,
                         const Position& position,
                         int stackPos) override;
  void onCreatureMove(const WorldInterface& world_interface,
                      const Creature& creature,
                      const Position& oldPosition,
                      int oldStackPos,
                      const Position& newPosition) override;
  void onCreatureTurn(const WorldInterface& world_interface,
                      const Creature& creature,
                      const Position& position,
                      int stackPos) override;
  void onCreatureSay(const WorldInterface& world_interface,
                     const Creature& creature,
                     const Position& position,
                     const std::string& message) override;

  void onItemRemoved(const WorldInterface& world_interface, const Position& position, int stackPos) override;
  void onItemAdded(const WorldInterface& world_interface, const Item& item, const Position& position) override;

  void onTileUpdate(const WorldInterface& world_interface, const Position& position) override;

  // Called by GameEngine (from PlayerCtrl)
  CreatureId getPlayerId() const override { return playerId_; }
  void setPlayerId(CreatureId playerId) override { playerId_ = playerId; }
  void onEquipmentUpdated(const Player& player, int inventoryIndex) override;
  void onOpenContainer(int localContainerId, const Container& container, const Item& item) override;
  void onCloseContainer(int localContainerId) override;
  void onContainerAddItem(int clientContainerId, const Item& item) override;
  void onContainerUpdateItem(int clientContainerId, int containerSlot, const Item& item) override;
  void onContainerRemoveItem(int clientContainerId, int containerSlot) override;
  void sendTextMessage(int message_type, const std::string& message) override;
  void sendCancel(const std::string& message) override;
  void cancelMove() override;

 private:
  bool isLoggedIn() const { return playerId_ != Creature::INVALID_ID; }
  bool isConnected() const { return static_cast<bool>(connection_); }

  // Connection callbacks
  void parsePacket(IncomingPacket* packet);
  void onDisconnected();

  // Helper functions for creating OutgoingPackets
  bool canSee(const Position& player_position, const Position& to_position) const;
  void addPosition(const Position& position, OutgoingPacket* packet) const;
  void addMapData(const WorldInterface& world_interface,
                  const Position& position,
                  int width,
                  int height,
                  OutgoingPacket* packet);
  void addCreature(const Creature& creature, OutgoingPacket* packet);
  void addItem(const Item& item, OutgoingPacket* packet) const;
  void addEquipment(const Equipment& equipment, int inventoryIndex, OutgoingPacket* packet) const;

  // Functions to parse IncomingPackets
  void parseLogin(IncomingPacket* packet);
  void parseMoveClick(IncomingPacket* packet);
  void parseMoveItem(IncomingPacket* packet);
  void parseUseItem(IncomingPacket* packet);
  void parseCloseContainer(IncomingPacket* packet);
  void parseOpenParentContainer(IncomingPacket* packet);
  void parseLookAt(IncomingPacket* packet);
  void parseSay(IncomingPacket* packet);

  // Helper functions for parsing IncomingPackets
  GamePosition getGamePosition(IncomingPacket* packet) const;
  ItemPosition getItemPosition(IncomingPacket* packet) const;

  std::function<void(void)> closeProtocol_;
  std::unique_ptr<Connection> connection_;
  GameEngineQueue* gameEngineQueue_;
  AccountReader* accountReader_;

  CreatureId playerId_;

  std::array<CreatureId, 64> knownCreatures_;
};

#endif  // WORLDSERVER_SRC_PROTOCOL_71_H_
