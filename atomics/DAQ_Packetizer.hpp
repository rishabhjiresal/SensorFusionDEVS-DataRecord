#ifndef BOOST_SIMULATION_DAQ_PACKETIZER_HPP
#define BOOST_SIMULATION_DAQ_PACKETIZER_HPP

#include <stdio.h>
#include <cadmium/modeling/ports.hpp>
#include <cadmium/modeling/message_bag.hpp>
#include <limits>
#include <math.h>
#include <assert.h>
#include <memory>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <algorithm>
#include <limits>
#include <random>

using namespace cadmium;
using namespace std;


struct DAQ_Packetizer_defs {
  struct T1 : public in_port<double>{};
  struct T2 : public in_port<double>{};
  struct T3 : public in_port<double>{};

  struct H1 : public in_port<double>{};
  struct H2 : public in_port<double>{};
  struct H3 : public in_port<double>{};

  struct C1 : public in_port<double>{};
  struct C2 : public in_port<double>{};
  struct C3 : public in_port<double>{};

  struct StJSONout : public out_port<string> {};
};

template<typename TIME>
class DAQ_Packetizer {
  using defs=DAQ_Packetizer_defs;
  public:
    DAQ_Packetizer() noexcept {
      for(int i = 0; i < 2; i++) { //hardcoded number of inputs in the conditional
        state.Values[i] = 0;
      }
      state.active = false;
    }

    struct state_type {
      double Values[2];  //Number of inputs
      string buffer;
      bool active;
    }; state_type state;

    using input_ports=std::tuple<typename defs::T1, typename defs::T2>;
    using output_ports=std::tuple<typename defs::StJSONout>;


    void internal_transition (){
      state.active = false;
    }

    void external_transition(TIME e, typename make_message_bags<input_ports>::type mbs){
      for(const auto &x : get_messages<typename defs::T1>(mbs)) {
        state.Values[0] = x;
      } for(const auto &x : get_messages<typename defs::T2>(mbs)) {
        state.Values[1] = x;
      }

      char tempBuff[32];
    
      sprintf(tempBuff, "{\"Temp\":[%.2f, %.2f]}", 
                                                  state.Values[0], 
                                                  state.Values[1]
      );

      string tempStr(tempBuff);
      state.buffer = tempStr;
      state.active = true;
    }

    void confluence_transition(TIME e, typename make_message_bags<input_ports>::type mbs) {
      internal_transition();
      external_transition(TIME(), std::move(mbs));
    }

    typename make_message_bags<output_ports>::type output() const {
        typename make_message_bags<output_ports>::type bags;

        string out;
        out = state.buffer;
        get_messages<typename defs::StJSONout>(bags).push_back(out);

        return bags;
    }

    TIME time_advance() const {
      if(state.active) {
        return TIME("00:00:00");
      }
      return std::numeric_limits<TIME>::infinity();
      // return TIME("01:00:00");

    }

    friend std::ostringstream& operator<<(std::ostringstream& os, const typename DAQ_Packetizer<TIME>::state_type& i) {
      os << "Sent Data by DAQ_Packetizer: " << i.buffer ;
      return os;
    }
};
#endif