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

#ifndef GAMEENGINE_GAMEENGINE_H_
#define GAMEENGINE_GAMEENGINE_H_

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "game_engine_queue.h"
#include "player.h"
#include "position.h"
#include "container_manager.h"
#include "item_position.h"

class OutgoingPacket;
class PlayerCtrl;
class World;

class GameEngine
{
 public:
  GameEngine(GameEngineQueue* gameEngineQueue, std::string loginMessage);

  // Delete copy constructors
  GameEngine(const GameEngine&) = delete;
  GameEngine& operator=(const GameEngine&) = delete;

  void spawn(const std::string& name, PlayerCtrl* player_ctrl);
  void despawn(CreatureId creatureId);

  void move(CreatureId creatureId, Direction direction);
  void movePath(CreatureId creatureId, const std::deque<Direction>& path);
  void cancelMove(CreatureId creatureId);
  void turn(CreatureId creatureId, Direction direction);

  void say(CreatureId creatureId,
           uint8_t type,
           const std::string& message,
           const std::string& receiver,
           uint16_t channelId);

  void moveItem(CreatureId creatureId,
                const ItemPosition& fromPosition,
                int itemId,
                int fromStackPos,
                const ItemPosition& toPosition,
                int count);
  void useItem(CreatureId creatureId,
               const ItemPosition& position,
               int itemId,
               int stackPosition,
               int newContainerId);
  void lookAt(CreatureId creatureId, const ItemPosition& position, int itemId, int stackPosition);

  void closeContainer(CreatureId creatureId, int localContainerId);

 private:
  // Use these instead of the unordered_maps directly
  Player& getPlayer(CreatureId creatureId) { return playerPlayerCtrl_.at(creatureId).player; }
  PlayerCtrl* getPlayerCtrl(CreatureId creatureId) { return playerPlayerCtrl_.at(creatureId).player_ctrl; }

  // Player + PlayerCtrl
  // TODO(simon): PlayerData
  struct PlayerPlayerCtrl
  {
    Player player;
    PlayerCtrl* player_ctrl;

    // Maps client container id (index in vector) to global container id
    std::array<int, 64> openContainers;
  };
  std::unordered_map<CreatureId, PlayerPlayerCtrl> playerPlayerCtrl_;

  GameEngineQueue* gameEngineQueue_;
  std::string loginMessage_;
  ContainerManager containerManager_;
};

#endif  // GAMEENGINE_GAMEENGINE_H_
