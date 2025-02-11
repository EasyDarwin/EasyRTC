# EasyRTC

EasyRTC是一套完整的WebRTC工具包，包括设备端、服务端、客户端，利用EasyRTC可以构建一套完美的视频交互类应用，例如智能摄像头、可视猫眼门铃、可穿戴硬件、无人机、无人车、机器人等各种场景。


## WebP2P视频物联技术

EasyRTC新一代WebP2P视频物联技术，是一种基于ICE技术实现的点对点(P2P)通信平台，平台利用WebP2P、RTC、实时指令等技术，支持实时音视频通信、数据传输等功能，适用于多种物联网应用场景。

### 设备端
- 智能穿戴：AR眼镜、VR头显
- 智能家居：智能监控、智能音箱、智慧屏、智能门锁、健身镜、家庭机器人；
- 远程操控：无人机、无人车、行车记录仪；

### 客户端
- Android
- iOS
- Windows
- Mac
- H5
- 小程序

### 应用场景
- 实时监控
- 视频通话
- 紧急呼叫
- 远程控制
- 远程巡检
- 远程协作

## 目录说明

	./
	./embedded	//各种嵌入式平台EasyRTC设备端版本
	./include	//EasyRTC.h头文件
	./linux		//x86架构Linux平台EasyRTC库文件
	./windows	//x86架构Windows平台EasyRTC库文件

## 二次开发

为了方便用户快速将EasyRTC接入到自己的硬件产品中，我们分别提供了Windows和Linux两个示例程序：

- Windows版采集本地Camera和Mic的视音频，接入到EasyRTC信令系统与客户端进行双向通话；

- Linux版为了方便开发者理解整套代码和编译过程，采用读取本地视频文件和音频文件的方式，接入到EasyRTC信令系统，开发者在二次开发的时候，只需要将读取音视频文件部分替换成采集本地硬件音视频部分即可完成开发对接；


## EasyRTC官网及更多流媒体音视频技术资源

EasyRTC即时通信服务：[www.easyrtc.cn](https://www.easyrtc.cn)

EasyDarwin开源流媒体服务器：<a href="https://www.easydarwin.org" target="_blank" title="EasyDarwin开源流媒体服务器">www.EasyDarwin.org</a>
