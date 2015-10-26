extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <string.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <cxxtools/jsondeserializer.h>
#include <cxxtools/directory.h>

#include "agents.h"
#include "bios_agent.h"

#define ALERT_DOWN 0
#define ALERT_UP 1
std::map<std::string, int> rule_state;

std::map<std::string, double> cache;

struct config {
  std::vector<std::string> in;
  std::string lua_code;
  std::string out;
  std::string rule_name;
};

std::map<std::string, std::vector<config>> configs;

std::map<zrex_t*, config> r_configs;

int main (int argc, char** argv) {
  char buff[256];
  int error;

  bios_agent_t *client = bios_agent_new("ipc://@/malamute", argv[0]);

  // Read configuration
  cxxtools::Directory d(".");
  for(auto fn : d) {
    config cfg;
    std::string rex;
    if(fn.length() < 5) continue;
    if(fn.compare(fn.length() - 5, 5, ".json") !=0 )
      continue;
    cfg.rule_name = fn;
    std::ifstream f(fn);
    cxxtools::JsonDeserializer json(f);
    json.deserialize();
    const cxxtools::SerializationInfo *si = json.si();
    si->getMember("evaluation") >>= cfg.lua_code;
    if(si->findMember("in"))
      si->getMember("in") >>= cfg.in;
    if(si->findMember("in_rex"))
      si->getMember("in_rex") >>= rex;
    si->getMember("out") >>= cfg.out;

    // Subscribe to all streams & store configuration
    for(auto it : cfg.in) {
      bios_agent_set_consumer(client, bios_get_stream_measurements(), it.c_str());
      printf("Registered to receive '%s'\n", it.c_str());
      if(configs.find(it.c_str()) == configs.end())
        configs.insert(std::make_pair(it.c_str(), std::vector<config>({})));
      auto cit = configs.find(it.c_str());
      cit->second.push_back(cfg);
    }
    if(!rex.empty()) {
      bios_agent_set_consumer(client, bios_get_stream_measurements(), rex.c_str());
      printf("Registered to receive '%s'\n", rex.c_str());
      r_configs.insert(std::make_pair(zrex_new(rex.c_str()), cfg));
    }
  }


  while(!zsys_interrupted) {
    ymsg_t *yn = bios_agent_recv(client);
    if(yn == NULL)
      continue;

    // Update cache with updated values
    std::string topic = bios_agent_subject(client);
    double value = ((double)ymsg_get_int32(yn, "value")) *
          pow(10.0, (double)ymsg_get_int32(yn, "scale" ));
    printf("Got message '%s' with value %lf\n", topic.c_str(), value);
    auto f = cache.find(topic);
    if(f != cache.end()) {
      f->second = value;
    } else {
      cache.insert(std::make_pair(topic, value));
    }
    ymsg_destroy(&yn);

    // Handle non-regex configs
    auto top = configs.find(topic);
    for(auto cfg : configs.find(topic)->second) {
      // Compute
      lua_State *L = lua_open();
      for(auto i : cfg.in) {
        auto it = cache.find(i);
        if(it == cache.end()) {
          printf("Do not have everything for '%s' yet\n", cfg.out.c_str());
          goto next_cfg;
        }
        std::string var = it->first;
        var[var.find('@')] = '_';
        printf("Setting variable '%s' to %lf\n", var.c_str(), it->second);
        lua_pushnumber(L, it->second);
        lua_setglobal(L, var.c_str());
      }
      error = luaL_loadbuffer(L, cfg.lua_code.c_str(), cfg.lua_code.length(), "line") ||
              lua_pcall(L, 0, 2, 0);

      if (error) {
        fprintf(stderr, "%s", lua_tostring(L, -1));
        lua_pop(L, 1);  /* pop error message from the stack */
      } else if(lua_isnumber(L, -1)){
        //the rule is TRUE, now detect if it is already up
        auto r = rule_state.find(cfg.rule_name);
        if(r == rule_state.end()) {
            //first time the rule applies 
            fprintf(stdout, "RULE %s : ALERT IS UP (%s = %lf), %s\n", cfg.rule_name.c_str(), cfg.out.c_str(), lua_tonumber(L, -1), lua_tostring(L, -2));
            rule_state.insert(std::make_pair(cfg.rule_name,ALERT_UP ));
        }else{
            switch(r->second){
              case ALERT_DOWN:
                //the alert goes up
                r->second=ALERT_UP;
                fprintf(stdout, "RULE %s : ALERT GOES UP (%s = %lf), %s\n", cfg.rule_name.c_str(), cfg.out.c_str(), lua_tonumber(L, -1), lua_tostring(L, -2));
                break;
              case ALERT_UP:
                //the alert already up
                fprintf(stdout, "RULE %s : ALERT ALREADY UP (%s = %lf), %s\n", cfg.rule_name.c_str(), cfg.out.c_str(), lua_tonumber(L, -1), lua_tostring(L, -2));
                break;
           }
        }
      } else {
       //the rule is FALSE, now detect if it is already down
        auto r = rule_state.find(cfg.rule_name);
        if(r == rule_state.end()) {
            //first time the rule applies 
            fprintf(stdout, "RULE %s : ALERT IS DOWN \n", cfg.rule_name.c_str() );
            rule_state.insert(std::make_pair(cfg.rule_name,ALERT_DOWN ));
        }else{
            switch(r->second){
              case ALERT_UP:
                //the alert goes down 
                r->second=ALERT_DOWN;
                fprintf(stdout, "RULE %s : ALERT GOES DOWN\n", cfg.rule_name.c_str());
                break;
              case ALERT_DOWN:
                //the alert already down 
                fprintf(stdout, "RULE %s : ALERT ALREADY DOWN\n", cfg.rule_name.c_str());
                break;
           }
        }

      }
next_cfg:
      lua_close(L);
    }

    // Handle regex configs
    for(auto it : r_configs) {
      if(!zrex_matches(it.first, topic.c_str()))
        continue;
      auto cfg = it.second;
      // Compute
      lua_State *L = lua_open();
      lua_pushnumber(L, value);
      lua_setglobal(L, "value");
      lua_pushstring(L, topic.c_str());
      lua_setglobal(L, "topic");
      error = luaL_loadbuffer(L, cfg.lua_code.c_str(), cfg.lua_code.length(), "line") ||
              lua_pcall(L, 0, 3, 0);

      if (error) {
        fprintf(stderr, "%s", lua_tostring(L, -1));
        lua_pop(L, 1);  /* pop error message from the stack */
      } else if(lua_isnumber(L, -1)) {
        //the rule is TRUE, now detect if it is already up
        //concate rule name with topic name
        std::string rule_topic_name=cfg.rule_name+"|"+lua_tostring(L, -3);
        auto r = rule_state.find(rule_topic_name);
        if(r == rule_state.end()) {
            //first time the rule applies 
            fprintf(stdout, "RULE %s : ALERT IS UP (%s = %lf), %s\n", rule_topic_name.c_str(), lua_tostring(L, -3), lua_tonumber(L, -1), lua_tostring(L, -2));
            rule_state.insert(std::make_pair(rule_topic_name,ALERT_UP ));
        }else{
            switch(r->second){
              case ALERT_DOWN:
                //the alert goes up
                r->second=ALERT_UP;
                fprintf(stdout, "RULE %s : ALERT GOES UP (%s = %lf), %s\n", rule_topic_name.c_str(), lua_tostring(L, -3), lua_tonumber(L, -1), lua_tostring(L, -2));
                break;
              case ALERT_UP:
                //the alert already up
                fprintf(stdout, "RULE %s : ALERT ALREADY UP (%s = %lf), %s\n", rule_topic_name.c_str(), lua_tostring(L, -3), lua_tonumber(L, -1), lua_tostring(L, -2));
                break;
           }
        }
      }else{
        //the rule is FALSE, now detect if it is already down
        //concate rule name with topic name
        //TODO : get lua_tostring(L, -3) which not available here
        std::string rule_topic_name=cfg.rule_name+"|";//+lua_tostring(L, -3);
        auto r = rule_state.find(rule_topic_name);
        if(r == rule_state.end()) {
            //first time the rule applies 
            fprintf(stdout, "RULE %s : ALERT IS DOWN \n", rule_topic_name.c_str() );
            rule_state.insert(std::make_pair(rule_topic_name,ALERT_DOWN ));
        }else{
            switch(r->second){
              case ALERT_UP:
                //the alert goes down 
                r->second=ALERT_DOWN;
                fprintf(stdout, "RULE %s : ALERT GOES DOWN\n", rule_topic_name.c_str());
                break;
              case ALERT_DOWN:
                //the alert already down 
                fprintf(stdout, "RULE %s : ALERT ALREADY DOWN\n", rule_topic_name.c_str());
                break;
           }
        }

      }
next_r_cfg:
      lua_close(L);
    }

  }
  bios_agent_destroy(&client);
  return 0;
}
