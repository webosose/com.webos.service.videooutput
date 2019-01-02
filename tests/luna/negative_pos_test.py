#!/usr/bin/python2

import unittest
import time
import luna_utils as luna

API_URL = "com.webos.service.videooutput/"
SUPPORT_SUB = True
SINK_MAIN = "MAIN"
SINK_SUB = "SUB0"
TEST_SOURCE = "HDMI"
TEST_SOURCE_PORT = 3
PID1 = "pipeline1"
SLEEP_TIME = 1

# Add x,y postion on POS_X_LIST, POS_Y_LIST for random position move test
WIDTH = 1920
HEIGHT = 1080

OUTPUT_RATIO = [1, 1.5, 0.5]

# Change Alpha value to add to POS_X
POS_XA = 700
# Change Alpha value to add to POS_Y
POS_YA = 200

# Set random outputRect
POS_X_LIST = [0, -960+POS_XA, 960+POS_XA, 2880+POS_XA, -960+POS_XA, 960+POS_XA, 2880+POS_XA, -960+POS_XA, 960+POS_XA, 2880+POS_XA, 0, 1920, 0, 1920]
POS_Y_LIST = [0, -540+POS_YA, -540+POS_YA, -540+POS_YA, 540+POS_YA, 540+POS_YA, 540+POS_YA, 1620+POS_YA, 1620+POS_YA, 1620+POS_YA, 0, 0, 1080, 1080]
OUT_W_LIST = [WIDTH, WIDTH, WIDTH, WIDTH, WIDTH, WIDTH, WIDTH, WIDTH, WIDTH, WIDTH, WIDTH, WIDTH, WIDTH, WIDTH]
OUT_H_LIST = [HEIGHT, HEIGHT, HEIGHT, HEIGHT, HEIGHT, HEIGHT, HEIGHT, HEIGHT, HEIGHT, HEIGHT, HEIGHT, HEIGHT, HEIGHT, HEIGHT]

# Set expected input/outputRect
'''
RET_POS_X_LIST = [0, -960, 960, 2880, -960, 960, 2880, -960, 960, 2880, 0, 1920, 0, 1920]
RET_POS_Y_LIST = [0, -540, -540, -540, 540, 540, 540, 1620, 1620, 1620, 0, 0, 1080, 1080]
RET_OUT_W_LIST = [1920, 1920, 1920, 1920, 1920, 1920, 1920, 1920, 1920, 1920, 1920, 1920, 1920, 1920]
RET_OUT_H_LIST = [1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080]
'''

VERBOSE_LOG = True

CONTENT_TYPE = "media"
FRAMERATE = 29.5
SCANTYPE = "progressive"
FULLSCREEN = False
TARGET_SINK = SINK_MAIN

class DemoScenario(luna.TestBase):

    def vlog(self, message):
        if VERBOSE_LOG:
            print(message)

    def mute(self, sink, blank):
        self.vlog("- Mute" + sink)
        self.checkLunaCallSuccessAndSubscriptionUpdate(
                API_URL + "blankVideo",
                {"sink": sink, "blank": blank},
                self.statusSub,
                {"video":[{"sink": sink, "muted": blank}]})

    def setUp(self):
        print("[setUp]")

################# Disconnect #####################
        self.vlog("- Disconnect old connection")
        luna.call(API_URL + "disconnect", { "sink": TARGET_SINK })

################# Subscribe #####################
        self.vlog("- subscription True")
        self.statusSub = luna.subscribe(API_URL + "getStatus", {"subscribe":True})

################# Register ####################
        self.vlog("- Register " + PID1)
        luna.call(API_URL + "register", { "context": PID1 })

################# Connect #####################
        self.vlog("- Connect")
        self.checkLunaCallSuccessAndSubscriptionUpdate(API_URL + "connect",
                { "outputMode": "DISPLAY", "sink": TARGET_SINK, "source": TEST_SOURCE, "sourcePort": TEST_SOURCE_PORT, "context": PID1 },
                self.statusSub,
                {"video":[{"sink": TARGET_SINK, "connectedSource": TEST_SOURCE, "connectedSourcePort": TEST_SOURCE_PORT}]})

################# setVideoData #####################
        self.vlog("- setVideoData")
        self.checkLunaCallSuccessAndSubscriptionUpdate(
                API_URL + "setVideoData",
                {"sink": TARGET_SINK,
                    "context": PID1,
                    "contentType": CONTENT_TYPE,
                    "frameRate":FRAMERATE,
                    "width":WIDTH,
                    "height":HEIGHT,
                    "scanType":SCANTYPE},
                self.statusSub,
                {"video":[{"sink": TARGET_SINK,
                    "fullScreen": FULLSCREEN,
                    "width":WIDTH,
                    "height":HEIGHT,
                    "frameRate":FRAMERATE
                    }]})


    def tearDown(self):
        print("[tearDown]")
        luna.cancelSubscribe(self.statusSub)
################# Disconnect #####################
        print("- Disconnect connection")
        luna.call(API_URL + "disconnect", { "sink": TARGET_SINK, "context": PID1 })

################# Unregister #####################
        self.vlog("- unregister " + PID1)
        luna.call(API_URL + "unregister", { "context": PID1 })

    def testRandomPosition(self):
        print("[testRandomPosition]")

        self.mute(TARGET_SINK, False)
        for ratio in OUTPUT_RATIO:
            for index in range(0, len(POS_X_LIST)):
################# setDisplayWindow #####################
                if index == 4:
                    self.vlog("- setDisplayWindow fullScreen true")
                    self.checkLunaCallSuccessAndSubscriptionUpdate(
                            API_URL + "display/setDisplayWindow",
                            {"sink": TARGET_SINK,
                                "context": PID1,
                                "fullScreen": True},
                            self.statusSub,
                            {"video":[{"sink": TARGET_SINK,
                                "fullScreen": True,
                                }]})
                    time.sleep(SLEEP_TIME)

                self.vlog("- setDisplayWindow " + str(POS_X_LIST[index]) + " " + str(POS_Y_LIST[index]) + " " + str(OUT_W_LIST[index]*ratio) + " " + str(OUT_H_LIST[index]*ratio))
                self.checkLunaCallSuccessAndSubscriptionUpdate(
                        API_URL + "display/setDisplayWindow",
                        {"sink": TARGET_SINK,
                            "context": PID1,
                            "fullScreen": FULLSCREEN,
                            "sourceInput": {"x":0, "y":0, "width":WIDTH, "height":HEIGHT},
                            "displayOutput": {"x":POS_X_LIST[index], "y":POS_Y_LIST[index], "width":OUT_W_LIST[index]*ratio, "height":OUT_H_LIST[index]*ratio}},
                        self.statusSub,
                        {})
                '''
                        {"video":[{"sink": TARGET_SINK,
                            "fullScreen": FULLSCREEN,
                            "width":WIDTH,
                            "height":HEIGHT,
                            "frameRate":FRAMERATE,
                            "sourceInput": {"x":0, "y":0, "width":WIDTH, "height":HEIGHT},
                            "displayOutput": {"x":POS_X_LIST[index], "y":POS_Y_LIST[index], "width":OUT_W_LIST[index], "height":OUT_H_LIST[index]}
                            }]})
                '''
                time.sleep(SLEEP_TIME)

if __name__ == '__main__':
    luna.VERBOSE = False
    unittest.main()
