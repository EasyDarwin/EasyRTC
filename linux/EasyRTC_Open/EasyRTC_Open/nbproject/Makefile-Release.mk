#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux
CND_DLIB_EXT=so
CND_CONF=Release
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/_ext/b0c5a78b/EasyRTC_websocket.o \
	${OBJECTDIR}/_ext/b0c5a78b/buff.o \
	${OBJECTDIR}/_ext/b0c5a78b/osmutex.o \
	${OBJECTDIR}/_ext/b0c5a78b/osthread.o \
	${OBJECTDIR}/_ext/b0c5a78b/websocketClient.o \
	${OBJECTDIR}/EasyRTCDeviceAPI.o \
	${OBJECTDIR}/RTCCaller.o \
	${OBJECTDIR}/RTCDevice.o \
	${OBJECTDIR}/RTCPeer.o \
	${OBJECTDIR}/WsHandle.o \
	${OBJECTDIR}/g711.o \
	${OBJECTDIR}/main.o


# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/easyrtc_open

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/easyrtc_open: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.c} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/easyrtc_open ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/_ext/b0c5a78b/EasyRTC_websocket.o: ../../common/EasyRTC_websocket.c
	${MKDIR} -p ${OBJECTDIR}/_ext/b0c5a78b
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b0c5a78b/EasyRTC_websocket.o ../../common/EasyRTC_websocket.c

${OBJECTDIR}/_ext/b0c5a78b/buff.o: ../../common/buff.c
	${MKDIR} -p ${OBJECTDIR}/_ext/b0c5a78b
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b0c5a78b/buff.o ../../common/buff.c

${OBJECTDIR}/_ext/b0c5a78b/osmutex.o: ../../common/osmutex.c
	${MKDIR} -p ${OBJECTDIR}/_ext/b0c5a78b
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b0c5a78b/osmutex.o ../../common/osmutex.c

${OBJECTDIR}/_ext/b0c5a78b/osthread.o: ../../common/osthread.c
	${MKDIR} -p ${OBJECTDIR}/_ext/b0c5a78b
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b0c5a78b/osthread.o ../../common/osthread.c

${OBJECTDIR}/_ext/b0c5a78b/websocketClient.o: ../../common/websocketClient.c
	${MKDIR} -p ${OBJECTDIR}/_ext/b0c5a78b
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b0c5a78b/websocketClient.o ../../common/websocketClient.c

${OBJECTDIR}/EasyRTCDeviceAPI.o: EasyRTCDeviceAPI.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/EasyRTCDeviceAPI.o EasyRTCDeviceAPI.c

${OBJECTDIR}/RTCCaller.o: RTCCaller.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/RTCCaller.o RTCCaller.c

${OBJECTDIR}/RTCDevice.o: RTCDevice.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/RTCDevice.o RTCDevice.c

${OBJECTDIR}/RTCPeer.o: RTCPeer.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/RTCPeer.o RTCPeer.c

${OBJECTDIR}/WsHandle.o: WsHandle.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/WsHandle.o WsHandle.c

${OBJECTDIR}/g711.o: g711.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/g711.o g711.c

${OBJECTDIR}/main.o: main.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -O2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main.o main.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
