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

#ifndef COMMON_WORLD_MOCKS_CREATURECTRL_MOCK_H_
#define COMMON_WORLD_MOCKS_CREATURECTRL_MOCK_H_

#include "gmock/gmock.h"

#include "creature_ctrl.h"

class MockCreatureCtrl : public CreatureCtrl
{
 public:
  MOCK_METHOD3(onCreatureSpawn, void(const WorldInterface& world_interface,
                                     const Creature& creature,
                                     const Position& position));

  MOCK_METHOD4(onCreatureDespawn, void(const WorldInterface& world_interface,
                                       const Creature& creature,
                                       const Position& position,
                                       int stackPos));

  MOCK_METHOD5(onCreatureMove, void(const WorldInterface& world_interface,
                                    const Creature& creature,
                                    const Position& oldPosition,
                                    int oldStackPos,
                                    const Position& newPosition));

  MOCK_METHOD4(onCreatureTurn, void(const WorldInterface& world_interface,
                                    const Creature& creature,
                                    const Position& position,
                                    int stackPos));

  MOCK_METHOD4(onCreatureSay, void(const WorldInterface& world_interface,
                                   const Creature& creature,
                                   const Position& position,
                                   const std::string& message));

  MOCK_METHOD3(onItemRemoved, void(const WorldInterface& world_interface,
                                   const Position& position,
                                   int stackPos));

  MOCK_METHOD3(onItemAdded, void (const WorldInterface& world_interface,
                                  const Item& item,
                                  const Position& position));

  MOCK_METHOD2(onTileUpdate, void(const WorldInterface& world_interface,
                                  const Position& position));
};

#endif  // COMMON_WORLD_MOCKS_CREATURECTRL_MOCK_H_
