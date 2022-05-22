/*
 * matriarch_global_settings.cpp
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

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <cxxopts.hpp>
#include <string>
#include <thread>

#include <fmt/core.h>
#include "rtmidi/RtMidi.h"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "moog_params.h"

using namespace std;

namespace {

auto get_options(cxxopts::ParseResult& Result, int Argc, char** Argv) -> bool;
auto find_matriarch_usb_device(RtMidi* MidiDevice) -> int;
auto set_params(const vector<string>& Settings, moog_params_t& Moog) -> bool;
}  // namespace

int main(int argc, char** argv) {
   int ret_val(-1);

   cxxopts::ParseResult pr;
   if (!get_options(pr, argc, argv)) {
      return -EINVAL;
   }
   unique_ptr<RtMidiIn> midi_in{new RtMidiIn()};
   unique_ptr<RtMidiOut> midi_out{new RtMidiOut()};

   int in_index{-1};
   int out_index{-1};

   in_index = find_matriarch_usb_device(midi_in.get());
   out_index = find_matriarch_usb_device(midi_out.get());

   if ((in_index != -1) && (out_index != -1)) {
      midi_in->openPort(in_index);
      midi_out->openPort(out_index);

      const bool is_query_requested{pr.count("query-param") > 0};
      const bool is_set_requested{pr.count("set-param") > 0};

      moog_params_t the_voice_of_god(midi_in.get(), midi_out.get());
      uint16_t result{0xFFFF};

      if (is_query_requested) {
         for (auto itm : pr["query-param"].as<vector<string>>()) {
            spdlog::trace("{}", itm);
            try {
               const uint16_t param{boost::lexical_cast<uint16_t>(itm)};

               if (!the_voice_of_god.get_param(param, result)) {
                  spdlog::critical("Failure retrieving parameter {}", itm);
                  ret_val = -EADDRNOTAVAIL;
               } else {
                  spdlog::info("ID {} is:{}", param, result);
               }
            } catch (...) {
               spdlog::critical("couldn't convert {} to an integer!", itm);
               ret_val = -EINVAL;
            }
         }
      }
      if (is_set_requested) {
         set_params(pr["set-param"].as<vector<string>>(), the_voice_of_god);
      }
      if (!is_query_requested && !is_set_requested) {
         assert(ret_val == -1);

         // if we are confused about what to do dump all the parameters
         for (auto n = 0; n < moog_params_t::MAX_PARAMS; n++) {
            if (!the_voice_of_god.get_param(n, result)) {
               spdlog::critical("Failure retrieving parameter {}", n);
               ret_val = -EADDRNOTAVAIL;
               break;
            }
            spdlog::info("ID {} is:{}", n, result);
         }
         if (ret_val != -EADDRNOTAVAIL) {
            ret_val = 0;
         }
      }
   } else {
      spdlog::critical("Matriarch not detected - Please connect using USB.");
      ret_val = -ENODEV;
   }
   return ret_val;
}

namespace {

auto get_options(cxxopts::ParseResult& Result, int Argc, char** Argv) -> bool {
   bool ret{true};
   cxxopts::Options options(
       "matriarch_global_settings",
       "Simplistic and unhelpful matriarch global settings utility.");

   options.add_options()("q,query-param", "Parameter ID to query",
                         cxxopts::value<vector<string>>())(
       "s,set-param", "Parameter ID to set", cxxopts::value<vector<string>>())(
       "h,help", "Print usage");

   Result = options.parse(Argc, Argv);
   if (Result.count("help") != 0) {
      std::cout << options.help() << std::endl;
      ret = false;
   }
   spdlog::set_level(spdlog::level::level_enum::trace);

   return ret;
}

auto find_matriarch_usb_device(RtMidi* MidiDevice) -> int {
   int ret_val{-1};

   for (unsigned n = 0; n < MidiDevice->getPortCount(); n++) {
      auto s{MidiDevice->getPortName(n)};
      spdlog::debug("found MIDI device {}", s);
      if (s.find("Matriarch") != std::string::npos) {
         ret_val = n;
      }
   }
   return ret_val;
}

auto set_params(const vector<string>& Settings, moog_params_t& Moog) -> bool {
   bool ret{true};

   for (auto s : Settings) {
      boost::char_separator<char> sep("= ");
      boost::tokenizer<boost::char_separator<char>> tokens(s, sep);
      vector<uint16_t> v;

      for (const auto& t : tokens) {
         try {
            v.push_back(boost::lexical_cast<uint16_t>(t));
         } catch (...) {
            spdlog::critical("couldn't convert {} to an integer!", t);
            ret = false;
            break;
         }
      }
      if (!ret) {
         break;
      }
      if (v.size() != 2) {
         spdlog::critical("expected x=y,found {} integer arguments", v.size());
      } else {
         uint16_t result{0xffff};

         // read the current value;
         if (!(ret = Moog.get_param(v[0], result))) {
            spdlog::critical(
                "Failed to read parameter {}. Not attempting write.", v[0]);
         } else {
            // do not write the value if it is the same:-
            if (result == v[1]) {
               spdlog::warn(
                   "Ignoring write request for Param {} as no change is "
                   "required; current value is already {}",
                   v[0], result);

            } else {
               spdlog::trace("read {} as the current value for param {}",
                             result, v[0]);
               if (!(ret = Moog.set_param(v[0], v[1]))) {
                  spdlog::critical("Failed to set parameter {}.", v[0]);
               } else {
                  // if that claimed to work we should be able to read back
                  // the same value:-
                  if (!(ret = Moog.get_param(v[0], result))) {
                     spdlog::critical(
                         "Failed to read back parameter {} after write. ",
                         v[0]);
                  } else {
                     if (result == v[1]) {
                        spdlog::info("read back verification successful!");
                     } else {
                        spdlog::warn("read back verification *not*successful!");
                     }
                  }
               }
            }
         }
      }
      if (!ret) {
         break;
      }
   }
   return ret;
}
}  // anonymous namespace
