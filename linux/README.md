Linux工程由NetBeans工具构建;
1. 先编译EasyRTC_Open
cd EasyRTC_Open
make

2. 再编译EasyRTC-P2P_File
cd EasyRTC-P2P_File
make


3. 运行
文件列表, 共5个文件: 
easyrtc-p2p_file
libEasyRTC.so
libEasyRTC_Open.so
1M.h264
music.pcm

运行:	easyrtc-p2p_file 服务器地址 服务器端口 设备id
示例: ./easyrtc-p2p_file rts.easyrtc.cn 19000 e3a18274-7554-4a9b-0000-b02abae6517f
