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

#include "incoming_packet.h"

#include <algorithm>
#include <vector>

IncomingPacket::IncomingPacket(const std::uint8_t* buffer, std::size_t length)
  : buffer_(buffer),
    length_(length),
    position_(0)
{
}

std::uint8_t IncomingPacket::peekU8() const
{
  return buffer_[position_];
}

std::uint8_t IncomingPacket::getU8()
{
  auto value = peekU8();
  position_ += 1;
  return value;
}

std::uint16_t IncomingPacket::peekU16() const
{
  std::uint16_t value = buffer_[position_];
  value |= (static_cast<std::uint16_t>(buffer_[position_ + 1]) << 8) & 0xFF00;
  return value;
}

std::uint16_t IncomingPacket::getU16()
{
  auto value = peekU16();
  position_ += 2;
  return value;
}

std::uint32_t IncomingPacket::peekU32() const
{
  std::uint32_t value = buffer_[position_];
  value |= (static_cast<std::uint32_t>(buffer_[position_ + 1]) << 8)  & 0xFF00;
  value |= (static_cast<std::uint32_t>(buffer_[position_ + 2]) << 16) & 0xFF0000;
  value |= (static_cast<std::uint32_t>(buffer_[position_ + 3]) << 24) & 0xFF000000;
  return value;
}

std::uint32_t IncomingPacket::getU32()
{
  auto value = peekU32();
  position_ += 4;
  return value;
}

std::string IncomingPacket::getString()
{
  std::uint16_t length = getU16();
  int temp = position_;
  position_ += length;
  return std::string(&buffer_[temp], &buffer_[temp + length]);
}

std::vector<std::uint8_t> IncomingPacket::peekBytes(int num_bytes) const
{
  std::vector<std::uint8_t> bytes(&buffer_[position_], &buffer_[position_ + num_bytes]);
  return bytes;
}

std::vector<std::uint8_t> IncomingPacket::getBytes(int num_bytes)
{
  auto bytes = peekBytes(num_bytes);
  position_ += num_bytes;
  return bytes;
}
