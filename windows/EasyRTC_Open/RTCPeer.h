#ifndef __EASYRTC_PEER_H__
#define __EASYRTC_PEER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "osmutex.h"
#include "EasyRTCAPI.h"
#include "buff.h"

typedef enum __OPERATE_TYPE_E
{
    OPERATE_TYPE_WAITING = 0,
    OPERATE_TYPE_ACCEPT = 1,
    OPERATE_TYPE_REFUSE,
    OPERATE_TYPE_WORKING,
    OPERATE_TYPE_DELETE
}OPERATE_TYPE_E;

typedef enum __EASYRTC_PEER_TYPE_E
{
    EASYRTC_PEER_PUBLISHER  =   0,
    EASYRTC_PEER_SUBSCRIBER,
    EASYRTC_PEER_MAX
}EASYRTC_PEER_TYPE_E;

// 链表节点结构

typedef struct __EASY_PEER_CONNECTION_T
{
    EASYRTC_PEER_TYPE_E     type;
    int connected;
    int disconnected;
    int noneedsI;

    int     turncount;
    int     icecount;
    int     icesended;



    EasyRTC_PeerConnection  peer_;
    EasyRTC_DataChannel dc_;

    EasyRTC_Transceiver videoTransceiver_;
    EasyRTC_Transceiver audioTransceiver_;

    EasyRTC_MediaStreamTrack video_track_;
    EasyRTC_MediaStreamTrack audio_track_;

    unsigned int    videoCodecID;
    char    vps[1024];
    char    sps[1024];
    char    pps[512];
    int     vpsLength;
    int     spsLength;
    int     ppsLength;
    int     startCodeOffset;
    BUFF_T  fullFrameData;

    void* peerPtr;
}EASY_PEER_CONNECTION_T;

typedef struct __EASYRTC_PEER_T 
{
    time_t createTime;          // 创建时间

    char uuid[64];
    unsigned int hisid[4]; //设备端uuid
    unsigned int caller_id[4];

    EASY_PEER_CONNECTION_T  peerConnection[EASYRTC_PEER_MAX];   // 此处定义了PUBLISHER 和 SUBSCRIBER

    OSMutex* mutex;
    OPERATE_TYPE_E operateType; // 操作类型
    void* devicePtr;            // 指向设备指针

    struct __EASYRTC_PEER_T* next;  // 指向下一个节点的指针
} EASYRTC_PEER_T;

// 创建新节点
EASYRTC_PEER_T* createPeerNode(void *devicePtr, const char* uuid, OPERATE_TYPE_E operateType);

// 在链表尾部插入节点
void insertPeerAtTail(EASYRTC_PEER_T** head, void* devicePtr, const char* uuid, OPERATE_TYPE_E operateType);

// 根据UUID查找节点
EASYRTC_PEER_T* findPeerByUUID(EASYRTC_PEER_T* head, const char* uuid);

// 根据UUID删除节点
void deletePeerByUUID(EASYRTC_PEER_T** head, const char* uuid);

// 更新peer的操作类型
void updatePeerOperateType(EASYRTC_PEER_T* peer, OPERATE_TYPE_E newType);

// 打印链表信息
void printPeerList(EASYRTC_PEER_T* head);

// 释放整个链表
void freePeerList(EASYRTC_PEER_T** head);


void    LockPeer(EASYRTC_PEER_T* peer, const char* function, const int line);
void    UnlockPeer(EASYRTC_PEER_T* peer, const char* function, const int line);


#endif
