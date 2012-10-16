ifeq (performance, $(findstring performance, $(MAKECMDGOALS)))
	BUILD_TYPE := -D BUILD_RELEASE
	BUILD_STRING := performance
	BUILD_FLAGS := -O3

else ifeq (release, $(findstring release, $(MAKECMDGOALS)))
	BUILD_TYPE := -D BUILD_RELEASE
	BUILD_STRING := release
	BUILD_FLAGS := -O1

else ifeq (no_output, $(findstring no_output, $(MAKECMDGOALS)))
	BUILD_TYPE := -D BUILD_RELEASE -D NO_OUTPUT
	BUILD_STRING := no_output
	BUILD_FLAGS := -O1

else ifeq (test, $(findstring test, $(MAKECMDGOALS)))
	BUILD_TYPE := -D BUILD_TEST
	BUILD_STRING := test
	BUILD_FLAGS := -ggdb
else
	BUILD_TYPE := -D BUILD_DEBUG
	BUILD_STRING := debug
	BUILD_FLAGS := -ggdb
endif


ifeq (no_mmt, $(findstring no_mmt, $(MAKECMDGOALS)))
	BUILD_TYPE := ${BUILD_TYPE} -D NO_MMT
else
	BUILD_TYPE := ${BUILD_TYPE} -D BUILD_MMT
endif


EMULAB_USER=alm2220
EMULAB_INSTALL_PATH=/users/${EMULAB_USER}/FCT

TARGET_MACHINE=sw1

TOOLS_PATH=~/.bin
CC=g++ ${BILD_TYPE}
CXX=g++ ${BUILD_TYPE}
LDFLAGS=-lpthread
CFLAGS=-Wall ${BUILD_FLAGS}
CXXFLAGS=${CFLAGS}

LIB_FILES = iniparser/dictionary.o iniparser/iniparser.o switched.cpp arraystream.cpp timer.cpp periodic.cpp mutex.cpp emulabutils.cpp  si_node.cpp rawutils.cpp packets.cpp mmt.cpp
LIB_OBJS = ${LIB_FILES:%.cpp=%.o}

SIPERF_OBJS = siperf.o ${LIB_OBJS}
TESTBENCH_OBJS = testbench.o ${LIB_OBJS}
SITEST_OBJS = sitest.o ${LIB_OBJS}
SIDUMP_OBJS = sidump.o libswitched.a
PACKET_BUILDER_OBJS = packetBuilder.o libswitched.a
ANNOUNCER_OBJS = announcer.o libswitched.a
ADDRESSTOOL_OBJS = addresstool.o libswitched.a
UNIT_TEST_OBJS = tests/advTests.o tests/main.o tests/periodicTest.o ${LIB_OBJS}

TARGETS = sitest sidump packetBuilder testbench siperf announcer addresstool periodic_test nodeinfo sockettool

all: libswitched.a ${TARGETS}
	#notify-send "FCT Build" "${BUILD_STRING} build complete"&
	#-mplayer /usr/share/sounds/gnome/default/alerts/sonar.ogg > /dev/null&

sockettool: sockettool.cpp ${LIB_OBJS}
	${CXX} sockettool.cpp ${LIB_OBJS} -o sockettool ${LDFLAGS}

nodeinfo: nodeinfo.cpp libswitched.a
	${CXX} nodeinfo.cpp libswitched.a -o nodeinfo ${LDFLAGS}

periodic_test: tests/periodicTest.cpp ${LIB_OBJS}
	${CXX} tests/periodicTest.cpp ${LIB_OBJS} -o tests/periodicTest ${LDFLAGS}

unit_tests: ${UNIT_TEST_OBJS}
	${CXX} ${UNIT_TEST_OBJS} -o unit_tests ${LDFLAGS}

libswitched.a: ${LIB_OBJS}
	rm -f $@
	ar ruP $@ ${LIB_OBJS}

siperf : ${SIPERF_OBJS}
	${CXX} ${SIPERF_OBJS} -o siperf ${LDFLAGS}

announcer: ${ANNOUNCER_OBJS}
	${CXX} ${ANNOUNCER_OBJS} -o announcer ${LDFLAGS}

addresstool: ${ADDRESSTOOL_OBJS}
	${CXX} ${ADDRESSTOOL_OBJS} -o addresstool ${LDFLAGS}

testbench: ${TESTBENCH_OBJS}
	${CXX} ${TESTBENCH_OBJS} -o testbench ${LDFLAGS}

testbench.nl: testbench
	nm -Bng testbench > testbench.nl

sitest: ${SITEST_OBJS}
	${CXX} ${SITEST_OBJS} -o sitest ${LDFLAGS}
#-pg -lc

sidump: $(SIDUMP_OBJS)
	$(CXX) $(SIDUMP_OBJS) -o sidump ${LDFLAGS}

packetBuilder: $(PACKET_BUILDER_OBJS)
	$(CXX) $(PACKET_BUILDER_OBJS) -o packetBuilder ${LDFLAGS}


clean:
	rm -f *.o *.a
	rm -f ${TARGETS}
	rm -f ${LIB_OBJS}

package:
	rm -f package.tar
	tar -cf package.tar ../src

emulab_install: clean package
	notify-send "FCT EMULab Install" "${BUILD_STRING} install starting"
	ssh ${EMULAB_USER}@users.emulab.net rm -f ${EMULAB_INSTALL_PATH}/fctsrc.tar
	ssh ${EMULAB_USER}@users.emulab.net rm -rf ${EMULAB_INSTALL_PATH}/src
	scp package.tar ${EMULAB_USER}@users.emulab.net:${EMULAB_INSTALL_PATH}/fctsrc.tar
	sleep 2
	ssh users.emulab.net tar -C ${EMULAB_INSTALL_PATH} -xf ${EMULAB_INSTALL_PATH}/fctsrc.tar
	#ssh ${EMULAB_USER}@users.emulab.net make -C src all ${BUILD_STRING}
	notify-send "FCT EMULab Install" "${BUILD_STRING} install completed" &
	-mplayer /usr/share/sounds/gnome/default/alerts/sonar.ogg > /dev/null


cluster_install: package
	notify-send "FCT Cluster Install" "${BUILD_STRING} build starting"
	-${TOOLS_PATH}/runAll.sh rm -f ~/fctsrc.tar
	-${TOOLS_PATH}/runAll.sh rm -rf ~/src
	-${TOOLS_PATH}/copyFile.sh package.tar ~/fctsrc.tar
	-${TOOLS_PATH}/parallelRun.sh tar -xf ~/fctsrc.tar
	-${TOOLS_PATH}/parallelRun.sh make -C ~/src/ clean
	-${TOOLS_PATH}/parallelRun.sh make -C ~/src all ${BUILD_STRING}
	-notify-send "FCT Cluster Install" "${BUILD_STRING} build complete" &
	-mplayer /usr/share/sounds/gnome/default/alerts/sonar.ogg > /dev/null


targeted_install: package
	ssh sw1 ssh ${TARGET_MACHINE} rm -f ~/fctsrc.tar
	ssh sw1 ssh ${TARGET_MACHINE} rm -rf ~/src
	-ssh sw1 mkdir staging
	scp package.tar sw1:~/staging/package.tar
	ssh sw1 scp ~/staging/package.tar ${TARGET_MACHINE}:~/fctsrc.tar
	ssh sw1 ssh ${TARGET_MACHINE} tar -xf fctsrc.tar
	ssh sw1 ssh ${TARGET_MACHINE} make -C ~/src/ clean
	ssh sw1 ssh ${TARGET_MACHINE} make -C ~/src

cluster_clean:
	-${TOOLS_PATH}/runAll.sh rm ~/fctsrc.tar
	-${TOOLS_PATH}/runAll.sh rm -rf ~/src


release:
	echo ${BUILD_STRING}

test:
	echo ${BUILD_STRING}

debug:
	echo ${BUILD_STRING}

