/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Simon Sandström
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

#include "protocol_71.h"

#include <deque>
#include <utility>

#include "gameengineproxy.h"
#include "server.h"
#include "incomingpacket.h"
#include "outgoingpacket.h"
#include "worldinterface.h"
#include "logger.h"
#include "tile.h"
#include "account.h"

Protocol71::Protocol71(const std::function<void(void)>& closeProtocol,
                       GameEngineProxy* gameEngineProxy,
                       WorldInterface* worldInterface,
                       ConnectionId connectionId,
                       Server* server,
                       AccountReader* accountReader)
  : closeProtocol_(closeProtocol),
    playerId_(Creature::INVALID_ID),
    gameEngineProxy_(gameEngineProxy),
    worldInterface_(worldInterface),
    connectionId_(connectionId),
    server_(server),
    accountReader_(accountReader)
{
  std::fill(knownCreatures_.begin(), knownCreatures_.end(), Creature::INVALID_ID);
}

void Protocol71::disconnected()
{
  // We may not send any more packets now
  server_ = nullptr;

  if (isLoggedIn())
  {
    // We are logged in to the game, add a task to despawn
    gameEngineProxy_->addTask(&GameEngine::playerDespawn, playerId_);
  }
  else
  {
    // We are not logged in to the game, close the protocol now
    closeProtocol_();  // WARNING: This instance is deleted after this call
  }
}

void Protocol71::parsePacket(IncomingPacket* packet)
{
  if (!isConnected())
  {
    LOG_ERROR("%s: not connected", __func__);
    return;
  }

  if (!isLoggedIn())
  {
    // Not logged in, only allow login packet
    auto packetType = packet->getU8();
    if (packetType == 0x0A)
    {
      parseLogin(packet);
    }
    else
    {
      LOG_ERROR("%s: Expected login packet but received packet type: 0x%X", __func__, packetType);
      server_->closeConnection(connectionId_, true);
    }

    return;
  }

  while (!packet->isEmpty())
  {
    uint8_t packetId = packet->getU8();
    switch (packetId)
    {
      case 0x14:
      {
        gameEngineProxy_->addTask(&GameEngine::playerDespawn, playerId_);
        break;
      }

      case 0x64:
      {
        parseMoveClick(packet);
        break;
      }

      case 0x65:  // Player move, North = 0
      case 0x66:  // East  = 1
      case 0x67:  // South = 2
      case 0x68:  // West  = 3
      {
        gameEngineProxy_->addTask(&GameEngine::playerMove, playerId_, static_cast<Direction>(packetId - 0x65));
        break;
      }

      case 0x69:
      {
        gameEngineProxy_->addTask(&GameEngine::playerCancelMove, playerId_);
        break;
      }

      case 0x6F:  // Player turn, North = 0
      case 0x70:  // East  = 1
      case 0x71:  // South = 2
      case 0x72:  // West  = 3
      {
        gameEngineProxy_->addTask(&GameEngine::playerTurn, playerId_, static_cast<Direction>(packetId - 0x6F));
        break;
      }

      case 0x78:
      {
        parseMoveItem(packet);
        break;
      }

      case 0x82:
      {
        parseUseItem(packet);
        break;
      }

      case 0x8C:
      {
        parseLookAt(packet);
        break;
      }

      case 0x96:
      {
        parseSay(packet);
        break;
      }

      case 0xBE:
      {
        // TODO(gurka): This packet more likely means "stop all actions", not only moving
        parseCancelMove(packet);
        break;
      }

      default:
      {
        LOG_ERROR("Unknown packet from player id: %d, packet id: 0x%X", playerId_, packetId);
        return;  // Don't read any more, even though there might be more packets that we can parse
      }
    }
  }
}

void Protocol71::onCreatureSpawn(const Creature& creature, const Position& position)
{
  if (!isConnected())
  {
    return;
  }

  OutgoingPacket packet;

  packet.addU8(0x6A);
  addPosition(position, &packet);
  addCreature(creature, &packet);

  // Spawn/login bubble
  packet.addU8(0x83);
  addPosition(position, &packet);
  packet.addU8(0x0A);

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::onCreatureDespawn(const Creature& creature, const Position& position, uint8_t stackPos)
{
  if (!isConnected())
  {
    if (creature.getCreatureId() == playerId_)
    {
      // We are no longer in game and the connection has been closed, close the protocol
      closeProtocol_();  // WARNING: This instance is deleted after this call
    }
    return;
  }

  OutgoingPacket packet;

  // Logout poff
  packet.addU8(0x83);
  addPosition(position, &packet);
  packet.addU8(0x02);

  packet.addU8(0x6C);
  addPosition(position, &packet);
  packet.addU8(stackPos);

  server_->sendPacket(connectionId_, std::move(packet));

  if (creature.getCreatureId() == playerId_)
  {
    // This player despawned!
    server_->closeConnection(connectionId_, false);
    closeProtocol_();  // WARNING: This instance is deleted after this call
  }
}

void Protocol71::onCreatureMove(const Creature& creature,
                                const Position& oldPosition,
                                uint8_t oldStackPos,
                                const Position& newPosition,
                                uint8_t newStackPos)
{
  if (!isConnected())
  {
    return;
  }

  // TODO(gurka): This should be handled by GameEngine...
  // or actually by World (it handles all World logic, and should decide when a creature may move again)
  /*
  if (creature.getCreatureId() == playerId_)
  {
    // Set next walk time
    const auto& tile = worldInterface_->getTile(newPosition);

    auto groundSpeed = tile.getGroundSpeed();
    auto creatureSpeed = creature.getSpeed();
    auto duration = (1000 * groundSpeed) / creatureSpeed;

    // Walking diagonally?
    if (oldPosition.getX() != newPosition.getX() &&
        oldPosition.getY() != newPosition.getY())
    {
      // Or is it times 3?
      duration *= 2;
    }

    auto now = boost::posix_time::ptime(boost::posix_time::microsec_clock::local_time());
    nextWalkTime_ = now + boost::posix_time::millisec(duration);

    LOG_DEBUG("%s: creatureId: %d, groundSpeed: %d, creatureSpeed: %d, duration: %d",
              __func__, creature.getCreatureId(), groundSpeed, creatureSpeed, duration);
  }
  */

  // Build outgoing packet
  OutgoingPacket packet;

  bool canSeeOldPos = canSee(oldPosition);
  bool canSeeNewPos = canSee(newPosition);

  if (canSeeOldPos && canSeeNewPos)
  {
    packet.addU8(0x6D);
    addPosition(oldPosition, &packet);
    packet.addU8(oldStackPos);
    addPosition(newPosition, &packet);
  }
  else if (canSeeOldPos)
  {
    packet.addU8(0x6C);
    addPosition(oldPosition, &packet);
    packet.addU8(oldStackPos);
  }
  else if (canSeeNewPos)
  {
    packet.addU8(0x6A);
    addPosition(newPosition, &packet);
    addCreature(creature, &packet);
  }

  if (creature.getCreatureId() == playerId_)
  {
    // This player moved, send new map data
    if (oldPosition.getY() > newPosition.getY())
    {
      // Get north block
      packet.addU8(0x65);
      addMapData(Position(oldPosition.getX() - 8, newPosition.getY() - 6, 7), 18, 1, &packet);
      packet.addU8(0x7E);
      packet.addU8(0xFF);
    }
    else if (oldPosition.getY() < newPosition.getY())
    {
      // Get south block
      packet.addU8(0x67);
      addMapData(Position(oldPosition.getX() - 8, newPosition.getY() + 7, 7), 18, 1, &packet);
      packet.addU8(0x7E);
      packet.addU8(0xFF);
    }

    if (oldPosition.getX() > newPosition.getX())
    {
      // Get west block
      packet.addU8(0x68);
      addMapData(Position(newPosition.getX() - 8, newPosition.getY() - 6, 7), 1, 14, &packet);
      packet.addU8(0x62);
      packet.addU8(0xFF);
    }
    else if (oldPosition.getX() < newPosition.getX())
    {
      // Get west block
      packet.addU8(0x66);
      addMapData(Position(newPosition.getX() + 9, newPosition.getY() - 6, 7), 1, 14, &packet);
      packet.addU8(0x62);
      packet.addU8(0xFF);
    }
  }

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::onCreatureTurn(const Creature& creature, const Position& position, uint8_t stackPos)
{
  if (!isConnected())
  {
    return;
  }

  OutgoingPacket packet;

  packet.addU8(0x6B);
  addPosition(position, &packet);
  packet.addU8(stackPos);

  packet.addU8(0x63);
  packet.addU8(0x00);
  packet.addU32(creature.getCreatureId());
  packet.addU8(static_cast<uint8_t>(creature.getDirection()));

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::onCreatureSay(const Creature& creature, const Position& position, const std::string& message)
{
  if (!isConnected())
  {
    return;
  }

  OutgoingPacket packet;

  packet.addU8(0xAA);
  packet.addString(creature.getName());
  packet.addU8(0x01);  // Say type

  // if type <= 3
  addPosition(position, &packet);

  packet.addString(message);

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::onItemRemoved(const Position& position, uint8_t stackPos)
{
  if (!isConnected())
  {
    return;
  }

  OutgoingPacket packet;

  packet.addU8(0x6C);
  addPosition(position, &packet);
  packet.addU8(stackPos);

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::onItemAdded(const Item& item, const Position& position)
{
  if (!isConnected())
  {
    return;
  }

  OutgoingPacket packet;

  packet.addU8(0x6A);
  addPosition(position, &packet);
  addItem(item, &packet);

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::onTileUpdate(const Position& position)
{
  if (!isConnected())
  {
    return;
  }

  OutgoingPacket packet;

  packet.addU8(0x69);
  addPosition(position, &packet);
  addMapData(position, 1, 1, &packet);
  packet.addU8(0x00);
  packet.addU8(0xFF);

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::onPlayerSpawn(const Player& player, const Position& position, const std::string& loginMessage)
{
  playerId_ = player.getCreatureId();

  if (!isConnected())
  {
    return;
  }

  OutgoingPacket packet;

  packet.addU8(0x0A);  // Login
  packet.addU32(playerId_);

  packet.addU8(0x32);  // ??
  packet.addU8(0x00);

  packet.addU8(0x64);  // Full (visible) map
  addPosition(position, &packet);  // Position

  addMapData(Position(position.getX() - 8, position.getY() - 6, position.getZ()), 18, 14, &packet);

  for (auto i = 0; i < 12; i++)
  {
    packet.addU8(0xFF);
  }

  packet.addU8(0xE4);  // Light?
  packet.addU8(0xFF);

  packet.addU8(0x83);  // Magic effect (login)
  packet.addU16(position.getX());
  packet.addU16(position.getY());
  packet.addU8(position.getZ());
  packet.addU8(0x0A);

  // Player stats
  packet.addU8(0xA0);
  packet.addU16(player.getHealth());
  packet.addU16(player.getMaxHealth());
  packet.addU16(player.getCapacity());
  packet.addU32(player.getExperience());
  packet.addU8(player.getLevel());
  packet.addU16(player.getMana());
  packet.addU16(player.getMaxMana());
  packet.addU8(player.getMagicLevel());

  packet.addU8(0x82);  // Light?
  packet.addU8(0x6F);
  packet.addU8(0xD7);

  // Player skills
  packet.addU8(0xA1);
  for (auto i = 0; i < 7; i++)
  {
    packet.addU8(10);
  }


  for (auto i = 1; i <= 10; i++)
  {
    addEquipment(player, i, &packet);
  }

  // Login message
  packet.addU8(0xB4);  // Message
  packet.addU8(0x11);  // Message type
  packet.addString(loginMessage);  // Message text

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::onEquipmentUpdated(const Player& player, int inventoryIndex)
{
  if (!isConnected())
  {
    return;
  }

  OutgoingPacket packet;

  addEquipment(player, inventoryIndex, &packet);

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::onUseItem(const Item& item)
{
  if (!isConnected())
  {
    return;
  }

  if (!item.hasAttribute("maxitems"))
  {
    LOG_ERROR("onUseItem(): Container Item: %d missing \"maxitems\" attribute", item.getItemId());
    return;
  }

  OutgoingPacket packet;

  packet.addU8(0x6E);
  packet.addU8(0x00);  // Level / Depth

  packet.addU16(item.getItemId());  // Container ID
  packet.addString(item.getName());
  packet.addU16(item.getAttribute<int>("maxitems"));

  packet.addU8(0x00);  // Number of items

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::sendTextMessage(const std::string& message)
{
  if (!isConnected())
  {
    return;
  }

  OutgoingPacket packet;

  packet.addU8(0xB4);
  packet.addU8(0x13);
  packet.addString(message);

  server_->sendPacket(connectionId_, std::move(packet));
}

void Protocol71::sendCancel(const std::string& message)
{
  if (!isConnected())
  {
    return;
  }

  OutgoingPacket packet;

  packet.addU8(0xB4);
  packet.addU8(0x14);
  packet.addString(message);

  server_->sendPacket(connectionId_, std::move(packet));
}

// TODO(gurka): fix
/*
void Protocol71::queueMoves(const std::deque<Direction>& moves)
{
  // Append or replace? Assume replace for now...
  queuedMoves_ = moves;
}

Direction Protocol71::getNextQueuedMove()
{
  Direction direction = queuedMoves_.front();
  queuedMoves_.pop_front();
  return direction;
}

void Protocol71::cancelMove()
{
  queuedMoves_.clear();

  OutgoingPacket packet;
  packet.addU8(0xB5);
  server_->sendPacket(connectionId_, std::move(packet));
}

boost::posix_time::ptime Protocol71::getNextWalkTime() const
{
  return nextWalkTime_;
}
*/

bool Protocol71::canSee(const Position& position) const
{
  const Position& playerPosition = worldInterface_->getCreaturePosition(playerId_);
  return position.getX() >  playerPosition.getX() - 9 &&
         position.getX() <= playerPosition.getX() + 9 &&
         position.getY() >  playerPosition.getY() - 7 &&
         position.getY() <= playerPosition.getY() + 7;
}

void Protocol71::addPosition(const Position& position, OutgoingPacket* packet) const
{
  packet->addU16(position.getX());
  packet->addU16(position.getY());
  packet->addU8(position.getZ());
}

void Protocol71::addMapData(const Position& position, int width, int height, OutgoingPacket* packet)
{
  auto tiles = worldInterface_->getMapBlock(position, width, height);
  decltype(tiles)::const_iterator it = tiles.begin();

  for (auto x = 0; x < width; x++)
  {
    for (auto y = 0; y < height; y++)
    {
      const auto* tile = *it;
      if (tile != nullptr)
      {
        const auto& items = tile->getItems();
        const auto& creatureIds = tile->getCreatureIds();
        auto itemIt = items.cbegin();
        auto creatureIt = creatureIds.cbegin();

        // Client can only handle ground + 9 items/creatures at most
        auto count = 0;

        // Add ground Item
        addItem(*itemIt, packet);
        count++;
        ++itemIt;

        // if splash; add; count++

        // Add top Items
        while (count < 10 && itemIt != items.cend())
        {
          if (!itemIt->alwaysOnTop())
          {
            break;
          }

          addItem(*itemIt, packet);
          count++;
          ++itemIt;
        }

        // Add Creatures
        while (count < 10 && creatureIt != creatureIds.cend())
        {
          const Creature& creature = worldInterface_->getCreature(*creatureIt);
          addCreature(creature, packet);
          count++;
          ++creatureIt;
        }

        // Add bottom Items
        while (count < 10 && itemIt != items.cend())
        {
          addItem(*itemIt, packet);
          count++;
          ++itemIt;
        }
      }

      if (x != width - 1 || y != height - 1)
      {
        packet->addU8(0x00);
        packet->addU8(0xFF);
      }

      ++it;
    }
  }
}

void Protocol71::addCreature(const Creature& creature, OutgoingPacket* packet)
{
  // First check if we know about this creature or not
  auto it = std::find(knownCreatures_.begin(), knownCreatures_.end(), creature.getCreatureId());
  if (it == knownCreatures_.end())
  {
    // Find an empty spot
    auto unused = std::find(knownCreatures_.begin(), knownCreatures_.end(), Creature::INVALID_ID);
    if (unused == knownCreatures_.end())
    {
      // No empty spot!
      // TODO(gurka): Figure out how to handle this
      LOG_ERROR("%s: knownCreatures_ is full!");
    }
    else
    {
      *unused = creature.getCreatureId();
    }

    packet->addU8(0x61);
    packet->addU8(0x00);
    packet->addU32(0x00);  // creatureId to remove (0x00 = none)
    packet->addU32(creature.getCreatureId());
    packet->addString(creature.getName());
  }
  else
  {
    // We already know about this creature
    packet->addU8(0x62);
    packet->addU8(0x00);
    packet->addU32(creature.getCreatureId());
  }

  packet->addU8(creature.getHealth() / creature.getMaxHealth() * 100);
  packet->addU8(static_cast<uint8_t>(creature.getDirection()));
  packet->addU8(creature.getOutfit().type);
  packet->addU8(creature.getOutfit().head);
  packet->addU8(creature.getOutfit().body);
  packet->addU8(creature.getOutfit().legs);
  packet->addU8(creature.getOutfit().feet);

  packet->addU8(0x00);
  packet->addU8(0xDC);

  packet->addU16(creature.getSpeed());
}

void Protocol71::addItem(const Item& item, OutgoingPacket* packet) const
{
  packet->addU16(item.getItemId());
  if (item.isStackable())
  {
    packet->addU8(item.getCount());
  }
  else if (item.isMultitype())
  {
    packet->addU8(item.getSubtype());
  }
}

void Protocol71::addEquipment(const Player& player, int inventoryIndex, OutgoingPacket* packet) const
{
  const auto& equipment = player.getEquipment();
  const auto& item = equipment.getItem(inventoryIndex);

  if (!item.isValid())
  {
    packet->addU8(0x79);  // No Item in this slot
    packet->addU8(inventoryIndex);
  }
  else
  {
    packet->addU8(0x78);
    packet->addU8(inventoryIndex);
    addItem(item, packet);
  }
}

void Protocol71::parseLogin(IncomingPacket* packet)
{
  packet->getU8();  // Unknown (0x02)
  uint8_t client_os = packet->getU8();
  uint16_t client_version = packet->getU16();
  packet->getU8();  // Unknown
  std::string character_name = packet->getString();
  std::string password = packet->getString();

  LOG_DEBUG("Client OS: %d Client version: %d Character: %s Password: %s",
            client_os,
            client_version,
            character_name.c_str(),
            password.c_str());

  // Check if character exists
  if (!accountReader_->characterExists(character_name))
  {
    OutgoingPacket response;
    response.addU8(0x14);
    response.addString("Invalid character.");
    server_->sendPacket(connectionId_, std::move(response));
    server_->closeConnection(connectionId_, false);
    return;
  }
  // Check if password is correct
  else if (!accountReader_->verifyPassword(character_name, password))
  {
    OutgoingPacket response;
    response.addU8(0x14);
    response.addString("Invalid password.");
    server_->sendPacket(connectionId_, std::move(response));
    server_->closeConnection(connectionId_, false);
    return;
  }

  // Login OK, add Player to GameEngine
  gameEngineProxy_->addPlayer(character_name, this);
}

void Protocol71::parseMoveClick(IncomingPacket* packet)
{
  std::deque<Direction> moves;
  uint8_t pathLength = packet->getU8();

  if (pathLength == 0)
  {
    LOG_ERROR("%s: Path length is zero!", __func__);
    return;
  }

  for (auto i = 0; i < pathLength; i++)
  {
    moves.push_back(static_cast<Direction>(packet->getU8()));
  }

  gameEngineProxy_->addTask(&GameEngine::playerMovePath, playerId_, moves);
}

void Protocol71::parseMoveItem(IncomingPacket* packet)
{
  // There are four options here:
  // Moving from inventory to inventory
  // Moving from inventory to Tile
  // Moving from Tile to inventory
  // Moving from Tile to Tile
  if (packet->peekU16() == 0xFFFF)
  {
    // Moving from inventory ...
    packet->getU16();

    auto fromInventoryId = packet->getU8();
    auto unknown = packet->getU16();
    auto itemId = packet->getU16();
    auto unknown2 = packet->getU8();

    if (packet->peekU16() == 0xFFFF)
    {
      // ... to inventory
      packet->getU16();
      auto toInventoryId = packet->getU8();
      auto unknown3 = packet->getU16();
      auto countOrSubType = packet->getU8();

      LOG_DEBUG("%s: Moving %u (countOrSubType %u) from inventoryId %u to iventoryId %u (unknown: %u, unknown2: %u, unknown3: %u)",
                __func__,
                itemId,
                countOrSubType,
                fromInventoryId,
                toInventoryId,
                unknown,
                unknown2,
                unknown3);

      gameEngineProxy_->addTask(&GameEngine::playerMoveItemFromInvToInv, playerId_, fromInventoryId, itemId, countOrSubType, toInventoryId);
    }
    else
    {
      // ... to Tile
      auto toPosition = getPosition(packet);
      auto countOrSubType = packet->getU8();

      LOG_DEBUG("%s: Moving %u (countOrSubType %u) from inventoryId %u to %s (unknown: %u, unknown2: %u)",
                __func__,
                itemId,
                countOrSubType,
                fromInventoryId,
                toPosition.toString().c_str(),
                unknown,
                unknown2);

      gameEngineProxy_->addTask(&GameEngine::playerMoveItemFromInvToPos, playerId_, fromInventoryId, itemId, countOrSubType, toPosition);
    }
  }
  else
  {
    // Moving from Tile ...
    auto fromPosition = getPosition(packet);
    auto itemId = packet->getU16();
    auto fromStackPos = packet->getU8();

    if (packet->peekU16() == 0xFFFF)
    {
      // ... to inventory
      packet->getU16();

      auto toInventoryId = packet->getU8();
      auto unknown = packet->getU16();
      auto countOrSubType = packet->getU8();

      LOG_DEBUG("%s: Moving %u (countOrSubType %u) from %s (stackpos: %u) to inventoryId %u (unknown: %u)",
                __func__,
                itemId,
                countOrSubType,
                fromPosition.toString().c_str(),
                fromStackPos,
                toInventoryId,
                unknown);

      gameEngineProxy_->addTask(&GameEngine::playerMoveItemFromPosToInv, playerId_, fromPosition, fromStackPos, itemId, countOrSubType, toInventoryId);
    }
    else
    {
      // ... to Tile
      auto toPosition = getPosition(packet);
      auto countOrSubType = packet->getU8();

      LOG_DEBUG("%s: Moving %u (countOrSubType %u) from %s (stackpos: %u) to %s (unknown: %u)",
                __func__,
                itemId,
                countOrSubType,
                fromPosition.toString().c_str(),
                fromStackPos,
                toPosition.toString().c_str());

      gameEngineProxy_->addTask(&GameEngine::playerMoveItemFromPosToPos, playerId_, fromPosition, fromStackPos, itemId, countOrSubType, toPosition);
    }
  }
}

void Protocol71::parseUseItem(IncomingPacket* packet)
{
  // There are two options here:
  if (packet->peekU16() == 0xFFFF)
  {
    // Use Item in inventory
    packet->getU16();
    auto inventoryIndex = packet->getU8();
    auto unknown = packet->getU16();
    auto itemId = packet->getU16();
    auto unknown2 = packet->getU16();

    LOG_DEBUG("%s: Item %u at inventory index: %u (unknown: %u, unknown2: %u)",
              __func__,
              itemId,
              inventoryIndex,
              unknown,
              unknown2);

    gameEngineProxy_->addTask(&GameEngine::playerUseInvItem, playerId_, itemId, inventoryIndex);
  }
  else
  {
    // Use Item on Tile
    auto position = getPosition(packet);
    auto itemId = packet->getU16();
    auto stackPosition = packet->getU8();
    auto unknown = packet->getU8();

    LOG_DEBUG("%s: Item %u at Tile: %s stackPos: %u (unknown: %u)",
              __func__,
              itemId,
              position.toString().c_str(),
              stackPosition,
              unknown);

    gameEngineProxy_->addTask(&GameEngine::playerUsePosItem, playerId_, itemId, position, stackPosition);
  }
}

void Protocol71::parseLookAt(IncomingPacket* packet)
{
  // There are two options here:
  if (packet->peekU16() == 0xFFFF)
  {
    // Look at Item in inventory
    packet->getU16();
    auto inventoryIndex = packet->getU8();
    auto unknown = packet->getU16();
    auto itemId = packet->getU16();
    auto unknown2 = packet->getU8();

    LOG_DEBUG("%s: Item %u at inventory index: %u (unknown: %u, unknown2: %u)",
              __func__,
              itemId,
              inventoryIndex,
              unknown,
              unknown2);

    gameEngineProxy_->addTask(&GameEngine::playerLookAtInvItem, playerId_, inventoryIndex, itemId);
  }
  else
  {
    // Look at Item on Tile
    auto position = getPosition(packet);
    auto itemId = packet->getU16();
    auto stackPos = packet->getU8();

    LOG_DEBUG("%s: Item %u at Tile: %s stackPos: %u",
              __func__,
              itemId,
              position.toString().c_str(),
              stackPos);

    gameEngineProxy_->addTask(&GameEngine::playerLookAtPosItem, playerId_, position, itemId, stackPos);
  }
}

void Protocol71::parseSay(IncomingPacket* packet)
{
  uint8_t type = packet->getU8();

  std::string receiver = "";
  uint16_t channelId = 0;

  switch (type)
  {
    case 0x06:  // PRIVATE
    case 0x0B:  // PRIVATE RED
      receiver = packet->getString();
      break;
    case 0x07:  // CHANNEL_Y
    case 0x0A:  // CHANNEL_R1
      channelId = packet->getU16();
      break;
    default:
      break;
  }

  std::string message = packet->getString();

  gameEngineProxy_->addTask(&GameEngine::playerSay, playerId_, type, message, receiver, channelId);
}

void Protocol71::parseCancelMove(IncomingPacket* packet)
{
  gameEngineProxy_->addTask(&GameEngine::playerCancelMove, playerId_);
}

Position Protocol71::getPosition(IncomingPacket* packet) const
{
  auto x = packet->getU16();
  auto y = packet->getU16();
  auto z = packet->getU8();
  return Position(x, y, z);
}