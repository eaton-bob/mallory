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

std::map<std::string, double> cache;

struct config {
  std::vector<std::string> in;
  std::string lua_code;
  std::string out;
};

std::map<std::string, std::vector<config>> configs;

int main (int argc, char** argv) {
  char buff[256];
  int error;

  bios_agent_t *client = bios_agent_new("ipc://@/malamute", argv[0]);

  // Read configuration
  cxxtools::Directory d(".");
  for(auto fn : d) {
    config cfg;
    if(fn.length() < 5) continue;
    if(fn.compare(fn.length() - 5, 5, ".json") !=0 )
      continue;
    std::ifstream f(fn);
    cxxtools::JsonDeserializer json(f);
    json.deserialize();
    const cxxtools::SerializationInfo *si = json.si();
    si->getMember("evaluation") >>= cfg.lua_code;
    si->getMember("in") >>= cfg.in;
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

      if(lua_isnumber(L, -1))
          fprintf(stdout, "ALERT (%s = %lf), %s\n", cfg.out.c_str(), lua_tonumber(L, -1), lua_tostring(L, -2));
      if (error) {
        fprintf(stderr, "%s", lua_tostring(L, -1));
        lua_pop(L, 1);  /* pop error message from the stack */
      }
next_cfg:
      lua_close(L);
    }
  }
  bios_agent_destroy(&client);
  return 0;
}
