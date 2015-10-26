CORE=../../core

all: lua_poc generate_measurement
clean:
	rm -f lua_poc generate_measurement

lua_poc: lua.cc
	g++ -o $@ -std=c++11 $< -I${CORE}/include -I${CORE}/src/include -lcxxtools -llua -lzmq -lczmq -lbiosapi -L${CORE}/.libs

generate_measurement: generate_measurement.cc
	g++ -o $@ -std=c++11 $< -I${CORE}/include -I${CORE}/src/msg -I${CORE}/src/include -lcxxtools -lzmq -lczmq -lbiosapi -L${CORE}/.libs
