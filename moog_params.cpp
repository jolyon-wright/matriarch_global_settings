/*
 * moog_params.cpp
 *
 * Copyright (c) 2022 Jolyon Wright.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */



#include "spdlog/spdlog.h"
#include <fmt/core.h>

#include "moog_params.h"

using namespace std;

namespace {

const int PARAMETER_ID_OFFSET = 4;
const int PARAMETER_VALUE_OFFSET = 5;
}  // namespace

const int moog_params_t::MAX_PARAMS = 75;

void moog_params_t::rtmidi_callback(double /*deltatime*/,
                                    std::vector<unsigned char>* message,
                                    void* userData) {
   auto* result = static_cast<midi_result_t*>(userData);
   assert(result != nullptr);
   {
      lock_guard<mutex> lk(result->mtx_);

      for (auto b : *message) {
         fmt::print("{:#02x} ", b);
      }
      fmt::print("\n");

      result->result_ = ((*message)[PARAMETER_VALUE_OFFSET] << 8) |
                        ((*message)[PARAMETER_VALUE_OFFSET + 1]);
      result->have_result_ = true;
   }
   result->cnd_.notify_one();
}

moog_params_t::moog_params_t(RtMidiIn* MidiIn, RtMidiOut* MidiOut)
    : midi_in_(MidiIn), midi_out_(MidiOut) {
   midi_in_->ignoreTypes(false, true, true);
   midi_in_->setCallback(moog_params_t::rtmidi_callback, &midi_result_);
}

auto moog_params_t::get_param(uint16_t Param, uint16_t& Result) -> bool {
   bool ret_val{false};

   assert(midi_in_->isPortOpen() && midi_out_->isPortOpen());

   if (Param > MAX_PARAMS) {
      return false;
   }
   vector<unsigned char> v{0xF0, 0x04, 0x17, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7};
   v[PARAMETER_ID_OFFSET] = Param;

   midi_out_->sendMessage(&v);

   {
      unique_lock lk(midi_result_.mtx_);
      if (midi_result_.cnd_.wait_for(
              lk, 1s, [this] { return midi_result_.have_result_; })) {
         Result = midi_result_.result_;
         ret_val = true;

         midi_result_.have_result_ = false;
      }
   }
   return ret_val;
}

auto moog_params_t::set_param(uint16_t Param, uint16_t Value) -> bool {
   bool ret_val{false};

   // TO SET A PARAMETER TO A VALUE:
   // F0 04 17 23 [Parameter ID], [value MSB], [value LSB], 00 00 00 00 00 00 00
   // 00 [Unit ID] F7

   spdlog::trace("{}={}", Param, Value);

   vector<unsigned char> req_param{0xF0, 0x04, 0x17, 0x23, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0xF7};

   req_param[PARAMETER_ID_OFFSET] = Param;  // delay sequence change is 25
   req_param[PARAMETER_VALUE_OFFSET] = (Value & 0xFF00) >> 8;
   req_param[PARAMETER_VALUE_OFFSET + 1] = (Value & 0x00FF);

   for (auto i : req_param) {
      fmt::print("{:#02x} ", i);
   }
   fmt::print("\n");

   midi_out_->sendMessage(&req_param);
#if 0
   {
      unique_lock lk(midi_result_.mtx_);
      if (midi_result_.cnd_.wait_for(
              lk, 2s, [this] { return midi_result_.have_result_; })) {
         // yes!
         ret_val = true;

         midi_result_.have_result_ = false;
      }
   }
#else
   // I never see the callback occur in this case; this is a puzzle!
   ret_val = true;
#endif
   return ret_val;
}

moog_params_t::midi_result_t::midi_result_t()
    : result_{0xFFFF}, have_result_{false} {}
