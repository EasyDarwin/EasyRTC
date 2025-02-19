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
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/EasyRtcDevice.o \
	${OBJECTDIR}/FileParser/ESFileParser.o \
	${OBJECTDIR}/FileParser/ESParser.o \
	${OBJECTDIR}/FileParser/buff.o \
	${OBJECTDIR}/FileParser/g711.o \
	${OBJECTDIR}/FileParser/getsps.o \
	${OBJECTDIR}/FileParser/h265_sei.o \
	${OBJECTDIR}/FileParser/h265_stream.o \
	${OBJECTDIR}/gettimeofdayEx.o \
	${OBJECTDIR}/main.o \
	${OBJECTDIR}/osmutex.o \
	${OBJECTDIR}/osthread.o


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
LDLIBSOPTIONS=-LEasyRTCSDK -Wl,-rpath,'.' -lEasyRTC_x64

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/easyrtc_file

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/easyrtc_file: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.cc} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/easyrtc_file ${OBJECTFILES} ${LDLIBSOPTIONS} -lpthread

${OBJECTDIR}/EasyRtcDevice.o: EasyRtcDevice.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/EasyRtcDevice.o EasyRtcDevice.cpp

${OBJECTDIR}/FileParser/ESFileParser.o: FileParser/ESFileParser.cpp
	${MKDIR} -p ${OBJECTDIR}/FileParser
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/FileParser/ESFileParser.o FileParser/ESFileParser.cpp

${OBJECTDIR}/FileParser/ESParser.o: FileParser/ESParser.cpp
	${MKDIR} -p ${OBJECTDIR}/FileParser
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/FileParser/ESParser.o FileParser/ESParser.cpp

${OBJECTDIR}/FileParser/buff.o: FileParser/buff.cpp
	${MKDIR} -p ${OBJECTDIR}/FileParser
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/FileParser/buff.o FileParser/buff.cpp

${OBJECTDIR}/FileParser/g711.o: FileParser/g711.cpp
	${MKDIR} -p ${OBJECTDIR}/FileParser
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/FileParser/g711.o FileParser/g711.cpp

${OBJECTDIR}/FileParser/getsps.o: FileParser/getsps.cpp
	${MKDIR} -p ${OBJECTDIR}/FileParser
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/FileParser/getsps.o FileParser/getsps.cpp

${OBJECTDIR}/FileParser/h265_sei.o: FileParser/h265_sei.cpp
	${MKDIR} -p ${OBJECTDIR}/FileParser
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/FileParser/h265_sei.o FileParser/h265_sei.cpp

${OBJECTDIR}/FileParser/h265_stream.o: FileParser/h265_stream.cpp
	${MKDIR} -p ${OBJECTDIR}/FileParser
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/FileParser/h265_stream.o FileParser/h265_stream.cpp

${OBJECTDIR}/gettimeofdayEx.o: gettimeofdayEx.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gettimeofdayEx.o gettimeofdayEx.cpp

${OBJECTDIR}/main.o: main.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main.o main.cpp

${OBJECTDIR}/osmutex.o: osmutex.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/osmutex.o osmutex.cpp

${OBJECTDIR}/osthread.o: osthread.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -IEasyRTCSDK -IFileParser -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/osthread.o osthread.cpp

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
