#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RTCPeer.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 创建新节点
EASYRTC_PEER_T* createPeerNode(void* devicePtr, const char* uuid, OPERATE_TYPE_E operateType) {
    EASYRTC_PEER_T* newNode = (EASYRTC_PEER_T*)malloc(sizeof(EASYRTC_PEER_T));
    if (newNode == NULL) {
        printf("内存分配失败\n");
        return NULL;
    }

    // 初始化成员
    memset(newNode, 0x0, sizeof(EASYRTC_PEER_T));
    
    newNode->createTime = time(NULL);

    strncpy(newNode->uuid, uuid, sizeof(newNode->uuid) - 1);
    newNode->uuid[sizeof(newNode->uuid) - 1] = '\0';
    //strcpy(newNode->uuid, uuid);

    newNode->mutex = (OSMutex*)malloc(sizeof(OSMutex));
    InitMutex(newNode->mutex);

    newNode->operateType = operateType;
    newNode->devicePtr = devicePtr;
    newNode->next = NULL;

    return newNode;
}

// 在链表尾部插入节点
void insertPeerAtTail(EASYRTC_PEER_T** head, void* devicePtr, const char* uuid, OPERATE_TYPE_E operateType) {
    EASYRTC_PEER_T* newNode = createPeerNode(devicePtr, uuid, operateType);
    if (newNode == NULL) {
        return;
    }

    if (*head == NULL) {
        *head = newNode;
        return;
    }

    EASYRTC_PEER_T* current = *head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = newNode;
}

// 根据UUID查找节点
EASYRTC_PEER_T* findPeerByUUID(EASYRTC_PEER_T* head, const char* uuid) {
    EASYRTC_PEER_T* current = head;
    while (current != NULL) {
        if (strcmp(current->uuid, uuid) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// 根据UUID && channelId && operateType查找节点
//EASYRTC_PEER_T* findPeerByUUIDAndChannelId(EASYRTC_PEER_T* head, const char* uuid, const int channelId)
//{
//    EASYRTC_PEER_T* current = head;
//    while (current != NULL) {
//        if ((strcmp(current->uuid, uuid) == 0) &&
//        (current->channelId == channelId)){
//            return current;
//        }
//        current = current->next;
//    }
//    return NULL;
//}


// 根据UUID删除节点
void deletePeerByUUID(EASYRTC_PEER_T** head, const char* uuid) {
    if (*head == NULL) {
        return;
    }

    EASYRTC_PEER_T* current = *head;
    EASYRTC_PEER_T* prev = NULL;

    // 查找要删除的节点
    while (current != NULL && strcmp(current->uuid, uuid) != 0) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        printf("未找到UUID为%s的节点\n", uuid);
        return;
    }

    // 删除节点
    if (prev == NULL) {
        *head = current->next;
    }
    else {
        prev->next = current->next;
    }

    // 释放资源（如果有）
    //if (current->peer_publisher.peer_ != NULL) {
    //    // rtcClosePeerConnection(current->peer_publisher.peer_);
    //}
    //if (current->peer_subscriber.peer_ != NULL) {
    //    // rtcClosePeerConnection(current->peer_subscriber.peer_);
    //}


    if (current->mutex)
    {
        DeinitMutex(current->mutex);
        free(current->mutex);
        current->mutex = NULL;
    }
    free(current);
}

// 更新peer的操作类型
void updatePeerOperateType(EASYRTC_PEER_T* peer, OPERATE_TYPE_E newType) {
    if (peer != NULL) {
        peer->operateType = newType;
    }
}

// 打印链表信息
void printPeerList(EASYRTC_PEER_T* head) {
    EASYRTC_PEER_T* current = head;
    printf("Peer链表:\n");
    while (current != NULL) {
        const char* operateStr = "";
        switch (current->operateType) {
        case OPERATE_TYPE_WAITING: operateStr = "WAITING"; break;
        case OPERATE_TYPE_ACCEPT: operateStr = "ACCEPT"; break;
        case OPERATE_TYPE_REFUSE: operateStr = "REFUSE"; break;
        case OPERATE_TYPE_WORKING: operateStr = "WORKING"; break;
        default: operateStr = "UNKNOWN";
        }

        //printf("UUID: %s, Name: %s, Operate: %s, Created: %llu\n",
            //current->uuid, current->transactionID, operateStr, current->createTime);
        current = current->next;
    }
    printf("End of list\n");
}

// 释放整个链表
void freePeerList(EASYRTC_PEER_T** head) {
    EASYRTC_PEER_T* current = *head;
    EASYRTC_PEER_T* next;

    while (current != NULL) {
        next = current->next;

        // 释放资源（如果有）
        //if (current->peer_publisher.peer_ != NULL) {
        //    // rtcClosePeerConnection(current->peer_publisher.peer_);
        //}
        //if (current->peer_subscriber.peer_ != NULL) {
        //    // rtcClosePeerConnection(current->peer_subscriber.peer_);
        //}

        if (current->mutex)
        {
            DeinitMutex(current->mutex);
            free(current->mutex);
            current->mutex = NULL;
        }

        free(current);
        current = next;
    }

    *head = NULL;
}


void    LockPeer(EASYRTC_PEER_T* peer, const char *function, const int line)
{
    if (NULL == peer)     return;
    if (NULL == peer->mutex)  return;

    //__Print__(peer, __FUNCTION__, __LINE__, 0, "%s line[%d] LockPeer Begin.\n", function, line);

    LockMutex(peer->mutex);

    //__Print__(peer, __FUNCTION__, __LINE__, 0, "%s line[%d] LockPeer End.\n", function, line);
}
void    UnlockPeer(EASYRTC_PEER_T* peer, const char* function, const int line)
{
    if (NULL == peer)     return;
    if (NULL == peer->mutex)  return;

    //__Print__(peer, __FUNCTION__, __LINE__, 0, "%s line[%d] UnlockPeer Begin.\n", function, line);

    UnlockMutex(peer->mutex);

    //__Print__(peer, __FUNCTION__, __LINE__, 0, "%s line[%d] UnlockPeer End.\n", function, line);
}

//// 设置peer连接状态
//void setPeerConnected(EASYRTC_PEER_T* peer, int connected) {
//    if (peer != NULL) {
//        peer->connected = connected;
//        if (connected) {
//            peer->disconnected = 0;
//            peer->operateType = OPERATE_TYPE_WORKING;
//        }
//    }
//}
//
//// 设置peer断开状态
//void setPeerDisconnected(EASYRTC_PEER_T* peer, int disconnected) {
//    if (peer != NULL) {
//        peer->disconnected = disconnected;
//        if (disconnected) {
//            peer->connected = 0;
//            peer->operateType = OPERATE_TYPE_WAITING;
//        }
//    }
//}

//
//// 示例调用代码
//int main() {
//    EASYRTC_PEER_T* peerList = NULL;
//
//    // 添加几个peer节点
//    insertPeerAtTail(&peerList, "123e4567-e89b-12d3-a456-426614174000", "Publisher1", OPERATE_TYPE_WAITING);
//    insertPeerAtTail(&peerList, "223e4567-e89b-12d3-a456-426614174001", "Subscriber1", OPERATE_TYPE_ACCEPT);
//    insertPeerAtTail(&peerList, "323e4567-e89b-12d3-a456-426614174002", "Publisher2", OPERATE_TYPE_WORKING);
//
//    // 打印链表
//    printPeerList(peerList);
//
//    // 查找并更新节点
//    const char* searchUUID = "223e4567-e89b-12d3-a456-426614174001";
//    EASYRTC_PEER_T* foundPeer = findPeerByUUID(peerList, searchUUID);
//    if (foundPeer != NULL) {
//        printf("\n找到Peer: %s (%s), 当前状态: %d\n", foundPeer->name, foundPeer->uuid, foundPeer->operateType);
//        updatePeerOperateType(foundPeer, OPERATE_TYPE_WORKING);
//        setPeerConnected(foundPeer, 1);
//    }
//    else {
//        printf("\n未找到UUID为%s的Peer\n", searchUUID);
//    }
//
//    // 打印更新后的链表
//    printf("\n更新后的链表:\n");
//    printPeerList(peerList);
//
//    // 删除节点
//    const char* deleteUUID = "123e4567-e89b-12d3-a456-426614174000";
//    printf("\n删除UUID为%s的Peer\n", deleteUUID);
//    deletePeerByUUID(&peerList, deleteUUID);
//
//    // 打印最终链表
//    printf("\n最终链表:\n");
//    printPeerList(peerList);
//
//    // 释放链表
//    freePeerList(&peerList);
//
//    return 0;
//}

