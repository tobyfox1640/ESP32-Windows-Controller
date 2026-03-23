<center style="font-size: 70px">欢迎使用</center>

<center style="font-size: 50px">ESP32 Windows Controller</center>

---

<center style="font-size: 30px; margin: 40px">演示视频</center>

<video width="1280" height="350" controls style="display: block; margin: 0 auto;"><source src="video/2026-03-23 18-07-10.mp4" type="video/mp4">

---

## 原理
很简单，ESP32向Windows上的被控端软件发送JSON命令，被控端只需要照着执行就行了，全程静默执行，不会有任何执时弹出的命令行窗口

ESP32发送的JSON命令格式，例如打开资源管理器如下
```
{"type":"keyboard","data":"win+e","params":{}}
```
----

这是我第一次做开源项目，多多包涵

也欢迎大家给我和AI写的这个小玩意儿提isuee

自身能力有限只能让AI帮忙实现一部分功能了，所以代码很烂，可能还有我没发现的Bug，轻点骂（逃