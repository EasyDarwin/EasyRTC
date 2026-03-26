## 示例说明

该Linux工程由NetBeans工具构建，以读取本地视频&音频文件的方式作为音视频源，模拟EasyRTC设备，接入到信令服务，主要就是为了更简单地演示EasyRTC原生库的调用过程。

### 1. 先编译EasyRTC_Open
    cd EasyRTC_Open
    make

### 2. 再编译EasyRTC-P2P_File
    cd EasyRTC-P2P_File
    make

### 3. 运行easyrtc-p2p_file

文件列表, 共5个文件: 
- easyrtc-p2p_file
- libEasyRTC.so
- libEasyRTC_Open.so
- 1M.h264
- music.pcm

运行方法:	easyrtc-p2p_file [信令服务器地址] [信令服务器端口] [设备id]
例如:

    ./easyrtc-p2p_file rts.easyrtc.cn 19000 e3a18274-7554-4a9b-0000-b02abae6517f
