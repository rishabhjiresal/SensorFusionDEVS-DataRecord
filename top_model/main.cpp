#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <string>
#include <cadmium/modeling/coupling.hpp>
#include <cadmium/modeling/ports.hpp>
#include <cadmium/modeling/dynamic_model_translator.hpp>
#include <cadmium/concept/coupled_model_assert.hpp>
#include <cadmium/modeling/dynamic_coupled.hpp>
#include <cadmium/modeling/dynamic_atomic.hpp>
#include <cadmium/engine/pdevs_dynamic_runner.hpp>
#include <cadmium/logger/tuple_to_ostream.hpp>
#include <cadmium/logger/common_loggers.hpp>
#include <cadmium/io/iestream.hpp>

#ifdef RT_ARM_MBED
#include <cadmium/real_time/arm_mbed/rt_clock.hpp>
#endif
#include "../atomics/Subscriber.hpp"
#include "../atomics/Data_Parser.hpp"
#include "../atomics/Fusion.hpp"
#include "../atomics/DP_Packetizer.hpp"
#include "../atomics/Publisher.hpp"

#include <NDTime.hpp>

#ifdef RT_ARM_MBED

#include "MQTTDriver.h"
#include "../mbed.h"

#else

const char* t1 = "./inputs/Temperature_Sensor_Values1.txt";
const char* t2 = "./inputs/Temperature_Sensor_Values2.txt";
const char* t3 = "./inputs/Temperature_Sensor_Values3.txt";

const char* h1 = "./inputs/Temperature_Sensor_Values4.txt";
const char* h2 = "./inputs/Temperature_Sensor_Values5.txt";
const char* h3 = "./inputs/Temperature_Sensor_Values6.txt";

const char* c1 = "./inputs/Temperature_Sensor_Values7.txt";
const char* c2 = "./inputs/Temperature_Sensor_Values8.txt";
const char* c3 = "./inputs/Temperature_Sensor_Values9.txt";

const char* t = "./inputs/fused_t.txt";
const char* h = "./inputs/fused_h.txt";
const char* c = "./inputs/fused_c.txt";

const char* dp_publish = "./outputs/DP_Publish.txt";
const char* daq_publish = "./outputs/DAQ_Publish.txt";

#endif
using namespace std;

#ifndef RT_ARM_MBED
using hclock=chrono::high_resolution_clock;
#endif
using TIME = NDTime;

int main(int argc, char ** argv) {

  //This will end the main thread and create a new one with more stack.
  #ifdef RT_ARM_MBED

  MQTTDriver driver;
  string topics[] = {"ARSLAB/DATA/TEMP", "ARSLAB/DATA/HUM"};
  //Set RTC
  // set_time(1610538009);  // Set RTC time to Wed, 28 Oct 2009 11:35:37

  driver.init();
  printf("Connecting to the broker...\n\r");
  char clientID[20];
  srand(time(NULL));
  sprintf(clientID, "ARSLAB_CLIENT_%d", rand()%100);
  if(driver.connect((const char*) clientID)) {
    printf("Connected!\n\r");
  }

  //Logging is done over cout in RT_ARM_MBED
  struct oss_sink_provider{
    static std::ostream& sink(){
      return cout;
    }
  };
  #else
  // all simulation timing and I/O streams are ommited when running embedded

  auto start = hclock::now(); //to measure simulation execution time

  /*************** Loggers *******************/

  static std::ofstream out_data("MessageOutputs.txt");
  struct oss_sink_provider{
    static std::ostream& sink(){
      return out_data;
    }
  };
  #endif


  using info=cadmium::logger::logger<cadmium::logger::logger_info, cadmium::dynamic::logger::formatter<TIME>, oss_sink_provider>;
  using debug=cadmium::logger::logger<cadmium::logger::logger_debug, cadmium::dynamic::logger::formatter<TIME>, oss_sink_provider>;
  using state=cadmium::logger::logger<cadmium::logger::logger_state, cadmium::dynamic::logger::formatter<TIME>, oss_sink_provider>;
  using log_messages=cadmium::logger::logger<cadmium::logger::logger_messages, cadmium::dynamic::logger::formatter<TIME>, oss_sink_provider>;
  using routing=cadmium::logger::logger<cadmium::logger::logger_message_routing, cadmium::dynamic::logger::formatter<TIME>, oss_sink_provider>;
  using global_time=cadmium::logger::logger<cadmium::logger::logger_global_time, cadmium::dynamic::logger::formatter<TIME>, oss_sink_provider>;
  using local_time=cadmium::logger::logger<cadmium::logger::logger_local_time, cadmium::dynamic::logger::formatter<TIME>, oss_sink_provider>;
  using log_all=cadmium::logger::multilogger<info, debug, state, log_messages, routing, global_time, local_time>;
  using logger_top=cadmium::logger::multilogger<log_messages, global_time>;

  using AtomicModelPtr=std::shared_ptr<cadmium::dynamic::modeling::model>;
  using CoupledModelPtr=std::shared_ptr<cadmium::dynamic::modeling::coupled<TIME>>;

  /*************************************************/
  /*************** Atomic models *******************/
  /*************************************************/

  #ifdef RT_ARM_MBED
  AtomicModelPtr Subscriber1 = cadmium::dynamic::translate::make_dynamic_atomic_model<Subscriber, TIME>("Subscriber1", topics, &driver, false);
  AtomicModelPtr Publisher2 = cadmium::dynamic::translate::make_dynamic_atomic_model<Publisher, TIME>("Publisher2", "DATA/FUSED", &driver);
  #else
  AtomicModelPtr Subscriber1 = cadmium::dynamic::translate::make_dynamic_atomic_model<Subscriber, TIME>("Subscriber1", t);
  AtomicModelPtr Subscriber2 = cadmium::dynamic::translate::make_dynamic_atomic_model<Subscriber, TIME>("Subscriber2", h);
  AtomicModelPtr Publisher2 = cadmium::dynamic::translate::make_dynamic_atomic_model<Publisher, TIME>("Publisher2", dp_publish);
  #endif
  AtomicModelPtr Data_Parser1 = cadmium::dynamic::translate::make_dynamic_atomic_model<Data_Parser, TIME>("Data_Parser1");
  AtomicModelPtr Data_Parser2 = cadmium::dynamic::translate::make_dynamic_atomic_model<Data_Parser, TIME>("Data_Parser2");
  AtomicModelPtr Fusion1 = cadmium::dynamic::translate::make_dynamic_atomic_model<Fusion, TIME>("Fusion1");
  AtomicModelPtr Fusion2 = cadmium::dynamic::translate::make_dynamic_atomic_model<Fusion, TIME>("Fusion2");
  AtomicModelPtr DP_Packetizer1 = cadmium::dynamic::translate::make_dynamic_atomic_model<DP_Packetizer, TIME>("DP_Packetizer1");

  /*************************************************/
  /*************** Coupled model *******************/
  /*************************************************/

  cadmium::dynamic::modeling::Ports iports_DP = {};
  cadmium::dynamic::modeling::Ports oports_DP = {};

  cadmium::dynamic::modeling::Models submodels_DP = {Subscriber1,  Data_Parser1,  Data_Parser2, Fusion1, Fusion2, DP_Packetizer1, Publisher2};

  cadmium::dynamic::modeling::EICs eics_DP = {};
  cadmium::dynamic::modeling::EOCs eocs_DP = {};

  cadmium::dynamic::modeling::ICs ics_DP = {
    cadmium::dynamic::translate::make_IC<subscriber_defs::out1, Data_Parser_defs::in>("Subscriber1","Data_Parser1"),
    cadmium::dynamic::translate::make_IC<subscriber_defs::out2, Data_Parser_defs::in>("Subscriber1","Data_Parser2"),

    cadmium::dynamic::translate::make_IC<Data_Parser_defs::out1, Fusion_defs::in1>("Data_Parser1","Fusion1"),
    cadmium::dynamic::translate::make_IC<Data_Parser_defs::out2, Fusion_defs::in2>("Data_Parser1","Fusion1"),

    cadmium::dynamic::translate::make_IC<Data_Parser_defs::out1, Fusion_defs::in1>("Data_Parser2","Fusion2"),
    cadmium::dynamic::translate::make_IC<Data_Parser_defs::out2, Fusion_defs::in2>("Data_Parser2","Fusion2"),

    cadmium::dynamic::translate::make_IC<Fusion_defs::out, DP_Packetizer_defs::T>("Fusion1","DP_Packetizer1"),
    cadmium::dynamic::translate::make_IC<Fusion_defs::out, DP_Packetizer_defs::H>("Fusion2","DP_Packetizer1"),

    cadmium::dynamic::translate::make_IC<DP_Packetizer_defs::StJSONout, Publisher_defs::in> ("DP_Packetizer1", "Publisher2")
  };

  CoupledModelPtr DP = std::make_shared<cadmium::dynamic::modeling::coupled<TIME>>(
    "DP",
    submodels_DP,
    iports_DP,
    oports_DP,
    eics_DP,
    eocs_DP,
    ics_DP
  );

  /*************************************************/
  /******************* Runner **********************/
  /*************************************************/

  #ifdef RT_ARM_MBED
    // cadmium::dynamic::engine::runner<NDTime, logger_top> r(TOP, {0});
    // cadmium::dynamic::engine::runner<NDTime, cadmium::logger::not_logger> r(DAQ, {0});
    cadmium::dynamic::engine::runner<NDTime, cadmium::logger::not_logger> r(DP, {0});
  #else

  // cadmium::dynamic::engine::runner<NDTime, logger_top> r(DAQ, {0});
  cadmium::dynamic::engine::runner<NDTime, logger_top> r(DP, {0});
  // cadmium::dynamic::engine::runner<NDTime, logger_top> r(TOP, {0});
  #endif

  r.run_until(NDTime("100:00:00:000"));
  #ifndef RT_ARM_MBED
  return 0;
  #endif

}
