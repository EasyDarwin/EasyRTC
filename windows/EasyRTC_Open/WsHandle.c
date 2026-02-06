#include "RTCDevice.h"
#include <stdio.h>
#include "EasyRTC_websocket.h"
#include "websocketClient.h"
#include "wsProtocol.h"

#ifdef ANDROID
#include "jni/JNI_EasyRTCDevice.h"
#endif



void trim(char* strIn, char* strOut)
{
    if (strIn == NULL)
    {
        strOut[0] = 0;
        return;
    }

    char* start, * end, * temp;
    temp = strIn;

    while (*temp == ' ')
    {
        ++temp;
    }

    start = temp;
    temp = strIn + strlen(strIn) - 1;

    while (*temp == ' ')
    {
        --temp;
    }

    end = temp;
    for (strIn = start; strIn <= end; )
    {
        *strOut++ = *strIn++;
    }

    *strOut = '\0';
}

int GetUUIDSFromString(char* struuid, uint32_t* myids)
{
    int isR = 1; //默认格式正确,下面为0则代表格式不正确
    do
    {
        if (strlen(struuid) != 36) { isR = 0; break; }

        int sepcount = 0;
        unsigned char c = 0;

        for (int i = 0; i < 36; i++)
        {
            c = struuid[i];
            if (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')))
            {
            }
            else if ((c >= 'A') && (c <= 'F'))
            {
                struuid[i] = c + 'a' - 'A';
            }
            else if (c == '-')
            {
                if ((i != 8) && (i != 13) && (i != 18) && (i != 23))
                {
                    isR = 0;
                    break;
                }
                sepcount++;
            }
            else
            {
                isR = 0;
                break;
            }
        }

        if (sepcount != 4) isR = 0;
    } while (0);

    if (isR == 0) return -1;

    {
        //下面是把类似 AE01A752-AD94-4107-B137-643ACC93756E 这样的字符串转为4个DWORD
        int i;
        unsigned char dataSZ[8] = { 0 };
        uint32_t data1_0 = 0;
        uint32_t data2_0 = 0;
        uint32_t data3_0 = 0;
        uint32_t data4_0 = 0;
        unsigned char* uuid = (unsigned char*)&struuid[0];

        {
            dataSZ[0] = uuid[0];
            dataSZ[1] = uuid[1];
            dataSZ[2] = uuid[2];
            dataSZ[3] = uuid[3];
            dataSZ[4] = uuid[4];
            dataSZ[5] = uuid[5];
            dataSZ[6] = uuid[6];
            dataSZ[7] = uuid[7];

            for (i = 0; i < 8; i++)
            {
                if (dataSZ[i] >= 'a')
                    data1_0 = (data1_0 << 4) + (10 + dataSZ[i] - 'a');
                else
                    data1_0 = (data1_0 << 4) + (dataSZ[i] - '0');
            }
        }

        {
            dataSZ[0] = uuid[9];
            dataSZ[1] = uuid[10];
            dataSZ[2] = uuid[11];
            dataSZ[3] = uuid[12];
            dataSZ[4] = uuid[14];
            dataSZ[5] = uuid[15];
            dataSZ[6] = uuid[16];
            dataSZ[7] = uuid[17];

            for (i = 0; i < 8; i++)
            {
                if (dataSZ[i] >= 'a')
                    data2_0 = (data2_0 << 4) + (10 + dataSZ[i] - 'a');
                else
                    data2_0 = (data2_0 << 4) + (dataSZ[i] - '0');
            }
        }

        {
            dataSZ[0] = uuid[19];
            dataSZ[1] = uuid[20];
            dataSZ[2] = uuid[21];
            dataSZ[3] = uuid[22];
            dataSZ[4] = uuid[24];
            dataSZ[5] = uuid[25];
            dataSZ[6] = uuid[26];
            dataSZ[7] = uuid[27];

            for (i = 0; i < 8; i++)
            {
                if (dataSZ[i] >= 'a')
                    data3_0 = (data3_0 << 4) + (10 + dataSZ[i] - 'a');
                else
                    data3_0 = (data3_0 << 4) + (dataSZ[i] - '0');
            }
        }

        {
            dataSZ[0] = uuid[28];
            dataSZ[1] = uuid[29];
            dataSZ[2] = uuid[30];
            dataSZ[3] = uuid[31];
            dataSZ[4] = uuid[32];
            dataSZ[5] = uuid[33];
            dataSZ[6] = uuid[34];
            dataSZ[7] = uuid[35];

            for (i = 0; i < 8; i++)
            {
                if (dataSZ[i] >= 'a')
                    data4_0 = (data4_0 << 4) + (10 + dataSZ[i] - 'a');
                else
                    data4_0 = (data4_0 << 4) + (dataSZ[i] - '0');
            }
        }

        myids[0] = data1_0;
        myids[1] = data2_0;
        myids[2] = data3_0;
        myids[3] = data4_0;
        return 0;
    }
}

int GetStringFromUUID(char *outString, unsigned int* hisid)
{
    if (NULL == hisid)  return -1;

    char uuid[64] = { 0 };
    sprintf(uuid, "%08X-%04X-%04X-%04X-%04X%08X",
        hisid[0], hisid[1] >> 16,
        hisid[1] & 0xFFFF, hisid[2] >> 16,
        hisid[2] & 0xFFFF, hisid[3]);

    strcpy(outString, uuid);
    return 0;
}
int EasyRTC_Build_TransactionID(EASYRTC_DEVICE_T* pDevice, char* outTransactionID)
{
    pDevice->transactionID++;
    if (pDevice->transactionID >= 0xFFFFFF)pDevice->transactionID = 1;

    sprintf(outTransactionID, "%p%u", pDevice, pDevice->transactionID);

    return 0;
}

int SendRegister(EASYRTC_DEVICE_T* pDevice)
{
    int ret = 0;
    if (0 == strcmp(pDevice->local_id, "\0"))       // local_id为空, 说明自身仅为caller
    {
        pDevice->registerStatus = 0x01;             // 直接置为成功
        websocketSetRegisterStatus(pDevice->websocket, pDevice->registerStatus);

        RTC_Caller_Connect(pDevice, NULL);
    }
    else
    {
        char myidOut[64] = { 0 };
        char mysnOut[64] = { 0 };
        trim(pDevice->local_id, myidOut);
        //trim("92E22DB5-E74F-4EE2-BAB3-BD8999D15911", mysnOut); //用于校验g_strUUID在信令服务器端的合法性,目前这个没做校验,随意弄一个uuid字符串就可以

        uint32_t myids[4] = { 0 };
        uint32_t mysns[4] = { 0 };
        if (GetUUIDSFromString(myidOut, myids) != 0) return -3;
        //if (GetUUIDSFromString(mysnOut, mysns) != 0) return -4;


        pDevice->local_uuid[0] = myids[0];
        pDevice->local_uuid[1] = myids[1];
        pDevice->local_uuid[2] = myids[2];
        pDevice->local_uuid[2] = myids[3];

        int len = sizeof(REQ_LOGINUSER_INFO);
        REQ_LOGINUSER_INFO* reqInfo = (REQ_LOGINUSER_INFO*)malloc(len);
        if (NULL == reqInfo)        return EASYRTC_ERROR_NOT_ENOUGH_MEMORY;

        memset(reqInfo, 0x00, len);
        reqInfo->length = len;
        reqInfo->msgtype = HP_REQLOGINUSER_INFO;
        reqInfo->myid[0] = myids[0];
        reqInfo->myid[1] = myids[1];
        reqInfo->myid[2] = myids[2];
        reqInfo->myid[3] = myids[3];
        reqInfo->mysn[0] = mysns[0];
        reqInfo->mysn[1] = mysns[1];
        reqInfo->mysn[2] = mysns[2];
        reqInfo->mysn[3] = mysns[3];

        ret = websocketSendData(pDevice->websocket, WS_OPCODE_BIN, (char*)reqInfo, reqInfo->length);
        free(reqInfo);
    }

    return ret;
}


void WebsocketDataHandler(EASYRTC_DEVICE_T* pDevice, const unsigned char* data, size_t len, int* errCode)
{
    BASE_MSG_INFO* pBaseInfo = (BASE_MSG_INFO*)data;
    if ((len < sizeof(BASE_MSG_INFO)) || (len != pBaseInfo->length)) return; //前者确保了后者 pBaseInfo->length 有效

    //前面两个是客户端的响应包
    switch (pBaseInfo->msgtype)
    {
    case HP_ACKLOGINUSER_INFO:
    {
        ACK_LOGINUSER_INFO* pRecvInfo = (ACK_LOGINUSER_INFO*)data;

        if (pRecvInfo->status == 0x00)      // 登陆成功
        {
            pDevice->registerStatus = 0x01;
            websocketSetRegisterStatus(pDevice->websocket, pDevice->registerStatus);

            __Print__(NULL, __FUNCTION__, __LINE__, false, "Server connection successful. uuid: %08X-%04X-%04X-%04X-%04X%08X  status:%d\n",
                pRecvInfo->myid[0], pRecvInfo->myid[1] >> 16, pRecvInfo->myid[1] & 0xFFFF, pRecvInfo->myid[2] >> 16, pRecvInfo->myid[2] & 0xFFFF, pRecvInfo->myid[3], pRecvInfo->status);

            pDevice->local_uuid[0] = pRecvInfo->myid[0];
            pDevice->local_uuid[1] = pRecvInfo->myid[1];
            pDevice->local_uuid[2] = pRecvInfo->myid[2];
            pDevice->local_uuid[3] = pRecvInfo->myid[3];
        }
        else
        {
            __Print__(NULL, __FUNCTION__, __LINE__, false, "Login failed. uuid: %08X-%04X-%04X-%04X-%04X%08X  status:%d\n",
                pRecvInfo->myid[0], pRecvInfo->myid[1] >> 16, pRecvInfo->myid[1] & 0xFFFF, pRecvInfo->myid[2] >> 16, pRecvInfo->myid[2] & 0xFFFF, pRecvInfo->myid[3], pRecvInfo->status);
        }
    }
    break;
    case HP_ACKCONNECTUSER_INFO:
    {
        ACK_CONNECTUSER_INFO* pRecvInfo = (ACK_CONNECTUSER_INFO*)data;

        printf("%s line[%d]\n", __FUNCTION__, __LINE__);

        __Print__(NULL, __FUNCTION__, __LINE__, false, "Connect peer successful. uuid: %08X-%04X-%04X-%04X-%04X%08X  status:%d\n",
            pRecvInfo->myid[0], pRecvInfo->myid[1] >> 16, pRecvInfo->myid[1] & 0xFFFF, pRecvInfo->myid[2] >> 16, pRecvInfo->myid[2] & 0xFFFF, pRecvInfo->myid[3], pRecvInfo->status);

        pDevice->peer_uuid[0] = pRecvInfo->hisid[0];
        pDevice->peer_uuid[1] = pRecvInfo->hisid[1];
        pDevice->peer_uuid[2] = pRecvInfo->hisid[2];
        pDevice->peer_uuid[3] = pRecvInfo->hisid[3];

        //
        pDevice->local_uuid[0] = pRecvInfo->myid[0];
        pDevice->local_uuid[1] = pRecvInfo->myid[1];
        pDevice->local_uuid[2] = pRecvInfo->myid[2];
        pDevice->local_uuid[3] = pRecvInfo->myid[3];

        sprintf(pDevice->local_id, "%08X-%04X-%04X-%04X-%04X%08X",
            pRecvInfo->myid[0], pRecvInfo->myid[1] >> 16,
            pRecvInfo->myid[1] & 0xFFFF, pRecvInfo->myid[2] >> 16,
            pRecvInfo->myid[2] & 0xFFFF, pRecvInfo->myid[3]);
    }
    break;
    case HP_NTIWEBRTCOFFER_INFO2:       // Caller端:  收到设备端发过来的offer
    {
        NTI_WEBRTCOFFER_INFO2* pRecvInfo = (NTI_WEBRTCOFFER_INFO2*)data;
        if (len != (sizeof(NTI_WEBRTCOFFER_INFO2) + pRecvInfo->sdplen + pRecvInfo->sturndataslen)) return;

        memset(&pDevice->serverConfig, 0, sizeof(EasyRTCServerConfig));

        int offset = 0;
        for (int i = 0; i < pRecvInfo->sturncount && i<8; i++)
        {
            if (pRecvInfo->sturntypes[i] == 0x00)
            {
                sprintf(pDevice->serverConfig.stun_servers[pDevice->serverConfig.stun_server_count++],
                    "stun:%s", pRecvInfo->sturndatas + offset);
                offset += ((int)strlen(pRecvInfo->sturndatas + offset) + 1);
            }
            else if (pRecvInfo->sturntypes[i] == 0x01)
            {
                // 这是非juice
                sprintf(pDevice->serverConfig.turn_servers[pDevice->serverConfig.turn_server_count],
                    "turn:%s", pRecvInfo->sturndatas + offset);
                offset += ((int)strlen(pRecvInfo->sturndatas + offset) + 1);
            }
            else if (pRecvInfo->sturntypes[i] == 0x02)
            {
                sprintf(pDevice->serverConfig.turn_username, "%s", pRecvInfo->sturndatas + offset);
                offset += ((int)strlen(pRecvInfo->sturndatas + offset) + 1);
            }
            else if (pRecvInfo->sturntypes[i] == 0x03)
            {
                sprintf(pDevice->serverConfig.turn_password, "%s", pRecvInfo->sturndatas + offset);
                offset += ((int)strlen(pRecvInfo->sturndatas + offset) + 1);

                pDevice->serverConfig.turn_server_count++;  //不管哪一种 turn, 不管是否有账号和密码,都必须传递账号和密码过来,哪怕为空
            }
            else if (pRecvInfo->sturntypes[i] == 0x04)
            {
                //这是 juice
                sprintf(pDevice->serverConfig.turn_servers[pDevice->serverConfig.turn_server_count],
                    "turn:%s", pRecvInfo->sturndatas + offset);
                offset += ((int)strlen(pRecvInfo->sturndatas + offset) + 1);
            }
        }

        char* offersdp = pRecvInfo->sturndatas + pRecvInfo->sturndataslen;

        char peer_id[128] = { 0 };
        sprintf(peer_id, "%08X-%04X-%04X-%04X-%04X%08X",
            pRecvInfo->hisid[0], pRecvInfo->hisid[1] >> 16,
            pRecvInfo->hisid[1] & 0xFFFF, pRecvInfo->hisid[2] >> 16,
            pRecvInfo->hisid[2] & 0xFFFF, pRecvInfo->hisid[3]);

        EASYRTC_PEER_T* peer = findPeerByUUID(pDevice->peerList, peer_id);
        if (NULL != peer)
        {
            // 此处释放相应对象
            EasyRTC_ReleasePeer(peer);

            // 从队列中删除
            LockPeerList(pDevice, __FUNCTION__, __LINE__);			    // Lock
            deletePeerByUUID(&pDevice->peerList, peer_id);
            UnlockPeerList(pDevice, __FUNCTION__, __LINE__);			// Unlock
        }

        if (1)
        {
            insertPeerAtTail(&pDevice->peerList, pDevice, peer_id, OPERATE_TYPE_WAITING);

            peer = findPeerByUUID(pDevice->peerList, peer_id);
            if (NULL == peer)
            {
                return;
            }

            peer->hisid[0] = pRecvInfo->hisid[0];
            peer->hisid[1] = pRecvInfo->hisid[1];
            peer->hisid[2] = pRecvInfo->hisid[2];
            peer->hisid[3] = pRecvInfo->hisid[3];

            int ret = CallbackData(pDevice, peer_id, EASYRTC_CALLBACK_TYPE_CALL_SUCCESS, 0, 0, NULL, 0, 0, 0);

            unsigned int mediaType = EASYRTC_MDIA_TYPE_VIDEO | EASYRTC_MDIA_TYPE_AUDIO;
            EasyRTC_RTP_TRANSCEIVER_DIRECTION direction = EasyRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

            pDevice->channelInfo.mediaType = mediaType;
            pDevice->channelInfo.direction = direction;

            EasyRTC_CreatePeer(pDevice, peer, EASYRTC_PEER_SUBSCRIBER, offersdp);
            EasyRTC_CreateAnswer(peer->peerConnection[EASYRTC_PEER_SUBSCRIBER].peer_, offersdp, __EasyRTC_IceCandidate_Callback, &peer->peerConnection[EASYRTC_PEER_SUBSCRIBER]);
        }
    }
    break;
    case HP_REQGETWEBRTCOFFER_INFO:     // 设备端: 有Caller请求offer
    {
        REQ_GETWEBRTCOFFER_INFO* pRecvInfo = (REQ_GETWEBRTCOFFER_INFO*)data;

        memset(&pDevice->serverConfig, 0, sizeof(EasyRTCServerConfig));

        char peer_uuid[64] = { 0 };
        GetStringFromUUID(peer_uuid, &pRecvInfo->hisid[0]);

        printf("收到了uuid为: %s 的连接请求\n", peer_uuid);

        int iPos = 0;
        int turncount = 0;
        int iceServersCount = 0;
        char* pStrDatas = pRecvInfo->sturndatas;

        for (int i = 0; i < pRecvInfo->sturncount; i++)
        {
            if (pRecvInfo->sturntypes[i] == 0x00)
            {
                sprintf(pDevice->serverConfig.stun_servers[pDevice->serverConfig.stun_server_count++], "stun:%s", pStrDatas + iPos);
                iPos += (int)(strlen(pStrDatas + iPos) + 1);
                iceServersCount++;
            }
            else if (pRecvInfo->sturntypes[i] == 0x01)
            {
                // 这是非juice
                sprintf(pDevice->serverConfig.turn_servers[pDevice->serverConfig.turn_server_count], "turn:%s", pStrDatas + iPos);
                iPos += (int)(strlen(pStrDatas + iPos) + 1);
            }
            else if (pRecvInfo->sturntypes[i] == 0x02)
            {
                sprintf(pDevice->serverConfig.turn_username, "%s", pStrDatas + iPos);
                //printf("%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].username);
                iPos += (int)(strlen(pStrDatas + iPos) + 1);
            }
            else if (pRecvInfo->sturntypes[i] == 0x03)
            {
                sprintf(pDevice->serverConfig.turn_password, "%s", pStrDatas + iPos);
                //printf("%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].credential);
                iPos += (int)(strlen(pStrDatas + iPos) + 1);
                iceServersCount++; //不管哪一种 turn, 不管是否有账号和密码,都必须传递账号和密码过来,哪怕为空
                pDevice->serverConfig.turn_server_count++;
                turncount++;
            }
            else if (pRecvInfo->sturntypes[i] == 0x04)
            {
                //这是为自己实现的turn server预留的类型
                sprintf(pDevice->serverConfig.turn_servers[pDevice->serverConfig.turn_server_count], "turn:%s", pStrDatas + iPos);
                //printf("%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].urls);
                iPos += (int)(strlen(pStrDatas + iPos) + 1);
            }
        }

        EASYRTC_PEER_T* peer = findPeerByUUID(pDevice->peerList, peer_uuid);
        if (NULL != peer)
        {
            // 此处释放相应对象
            EasyRTC_ReleasePeer(peer);

            // 从队列中删除
            LockPeerList(pDevice, __FUNCTION__, __LINE__);			    // Lock
            deletePeerByUUID(&pDevice->peerList, peer_uuid);
            UnlockPeerList(pDevice, __FUNCTION__, __LINE__);			// Unlock
        }

        insertPeerAtTail(&pDevice->peerList, pDevice, peer_uuid, OPERATE_TYPE_WAITING);

        peer = findPeerByUUID(pDevice->peerList, peer_uuid);
        if (NULL == peer)
        {
            // 未创建成功
            return;
        }

        peer->caller_id[0] = pRecvInfo->hisid[0];
        peer->caller_id[1] = pRecvInfo->hisid[1];
        peer->caller_id[2] = pRecvInfo->hisid[2];
        peer->caller_id[3] = pRecvInfo->hisid[3];

        // 回调给上层, 让上层决定是否接受该连接
        int ret = CallbackData(pDevice, peer_uuid, EASYRTC_CALLBACK_TYPE_PASSIVE_CALL, 0, 0, NULL, 0, 0, 0);
        if (ret == 1)
        {
            // 返回1直接接受连接
            RTC_Device_PassiveCallResponse(pDevice, peer_uuid, 0);
        }
    }
    break;
    case HP_NTIWEBRTCANSWER_INFO:   // 设备端: 发送offer后收到对方的answer
    {
        NTI_WEBRTCANSWER_INFO* pRecvInfo = (NTI_WEBRTCANSWER_INFO*)data;

        char* sdp = pRecvInfo->answersdp;
        if ((sdp == NULL) || (sdp[0] == 0) || (pRecvInfo->length != len) || (pRecvInfo->length != (sizeof(NTI_WEBRTCANSWER_INFO) + pRecvInfo->sdplen + 1))) return;

        char peer_uuid[64] = { 0 };
        GetStringFromUUID(peer_uuid, &pRecvInfo->hisid[0]);

        EASYRTC_PEER_T* peer = findPeerByUUID(pDevice->peerList, peer_uuid);
        if (NULL != peer)
        {
            int retStatus = EasyRTC_SetRemoteDescription(peer->peerConnection[EASYRTC_PEER_PUBLISHER].peer_, pRecvInfo->answersdp);
            if (retStatus != 0)
            {
                __Print__(NULL, __FUNCTION__, __LINE__, false, "Failed to set remote description with status: %d\n", retStatus);
                break;
            }
        }
    }
    break;
    case HP_ACKGETONLINEDEVICES_INFO:
    {
        ACK_GETONLINEDEVICES_INFO* pRecvInfo = (ACK_GETONLINEDEVICES_INFO*)data;

        int count = (pRecvInfo->length - sizeof(ACK_GETONLINEDEVICES_INFO));
        count /= (sizeof(unsigned int) * 4);
        for (int i = 0; i < count; i ++)
        {
            char uuid[64] = { 0 };
            GetStringFromUUID(uuid, (unsigned int*)&pRecvInfo->hisids[i][0]);

            int ret = CallbackData(pDevice, uuid, EASYRTC_CALLBACK_TYPE_ONLINE_DEVICE, 0, 0, NULL, 0, 0, 0);
        }
    }
    break;
    default:
        break;
    }

    return;
}
