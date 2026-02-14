## Signal Server使用方法

### 一、简介

SignalServer是EasyRTC团队提供的一套简洁的开源信令服务器，基于ws/wss协议开发，可实现简单的“设备-客户端”之间的“Offer+Answer”交互，用户可基于此套服务二次开发成自己的业务信令服务。

### 二、编译方法

* 编译主机：ubuntu 22.04
* 编译方法：
1. 安装uuid-dev: sudo apt-get install uuid-dev
2. 执行build.sh

### 三、配置文件

配置文件: signalserver.ini，如下：

```c
[base]
localport = 19000
stuncount = 1
turncount = 1
supportssl = 0

[stun0]
url = "stun.qq.com:3478"

[turn0]
url = "turn.openrelayproject.org:443?transport=tcp"
username = "user1"
credential = "pass1"

[ssl]
localport = 6689
pemcertfile = "example.crt"
pemkeyfile = "example.key"
keypassword = ""
```

### 四、运行方法

将signalserver.ini和signalserver可执行文件放在同一目录下，终端运行：

```shell
./signalserver
```






























