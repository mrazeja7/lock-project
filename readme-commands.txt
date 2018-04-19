setup commands for Pi Zero (provided we use Jan's phone as a router)
{"Action":"WifiSetup","Wifi":{"SSID":"AndroidAP","Password":"wmbm6334"}}
{"Action":"MQTTSetup","MQTT":{"Host":"192.168.43.243","Port":"1883"}}

{"Action":"MQTTSubs","MQTT":{"Topics":["lock"]}}

controller-side debugging commands (post these on MQTT)
{"Type":"Status","LockStatus":"locked"}
{"Type":"Status","LockStatus":"unlocked"}

lock-side debugging commands (post these on MQTT)
{"Type":"Command","FlipLock":"true"}
