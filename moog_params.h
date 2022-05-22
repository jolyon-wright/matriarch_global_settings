/*
 * moog_params.h
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

#pragma once

#include <mutex>
#include <thread>
#include <condition_variable>

#include "rtmidi/RtMidi.h"

class  moog_params_t {
   RtMidiIn* midi_in_;
   RtMidiOut* midi_out_;

   struct midi_result_t {
      midi_result_t();

      std::mutex mtx_;
      std::condition_variable cnd_;
      uint16_t result_;
      bool have_result_;
   };

   midi_result_t midi_result_;

   static void rtmidi_callback(double deltatime,
                               std::vector<unsigned char>* message,
                               void* userData);

  public:
   moog_params_t(RtMidiIn* MidiIn, RtMidiOut* MidiOut);
   ~moog_params_t() = default;
   moog_params_t() = delete;
   moog_params_t(const moog_params_t&) = delete;
   auto operator=(const moog_params_t&) -> moog_params_t& = delete;

   moog_params_t(const moog_params_t&&) = delete;
   auto operator=(const moog_params_t&&) -> moog_params_t&& = delete;

   auto get_param(uint16_t Param, uint16_t& Result) -> bool;
   auto set_param(uint16_t Param, uint16_t Value) -> bool;

   static const int MAX_PARAMS;
};
