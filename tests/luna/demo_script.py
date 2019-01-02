#!/usr/bin/python2
import unittest
import time
import luna_utils as luna

API_URL = "com.webos.service.videooutput/"
SUPPORT_SUB = True
SINK_MAIN = "MAIN"
SINK_SUB = "SUB0"
#TEST_SOURCE = "VDEC"
#TEST_SOURCE_PORT  = 0 
TEST_SOURCE = "HDMI"
TEST_SOURCE_PORT  = 3 
PID1 = "pipeline1"

# Add x,y postion on POS_X, POS_Y for random position move test
WIDTH = 1920
HEIGHT = 1080
POS_X = [0, 0, 0, 1920, 500, 300, 1940, 1940, 300, 0]
POS_Y = [0, 1080, 0, 0, 500, 300, 300, 960, 960, 0]
WIDTH_LIST = [3840, 3840, 1920, 1920, 2840, 1600, 1600, 1600, 1600, 3840]
HEIGHT_LIST = [1080, 1080, 2160, 2160, 1160, 900, 900, 900, 900, 2160]

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
        SLEEP_TIME = 1

        self.mute(TARGET_SINK, False)
        for index in range(0, len(POS_X)):
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

            self.vlog("- setDisplayWindow " + str(POS_X[index]) + " " + str(POS_Y[index]) + " " + str(WIDTH_LIST[index]) + " " + str(HEIGHT_LIST[index]))
            self.checkLunaCallSuccessAndSubscriptionUpdate(
                    API_URL + "display/setDisplayWindow",
                    {"sink": TARGET_SINK,
                        "context": PID1,
                        "fullScreen": FULLSCREEN,
                        "sourceInput": {"x":0, "y":0, "width":WIDTH, "height":HEIGHT},
                        "displayOutput": {"x":POS_X[index], "y":POS_Y[index], "width":WIDTH_LIST[index], "height":HEIGHT_LIST[index]}},
                    self.statusSub,
                    {"video":[{"sink": TARGET_SINK,
                        "fullScreen": FULLSCREEN,
                        "width":WIDTH,
                        "height":HEIGHT,
                        "frameRate":FRAMERATE,
                        "sourceInput": {"x":0, "y":0, "width":WIDTH, "height":HEIGHT},
                        "displayOutput": {"x":POS_X[index], "y":POS_Y[index], "width":WIDTH_LIST[index], "height":HEIGHT_LIST[index]}
                        }]})

            time.sleep(SLEEP_TIME)

    def testLinearMove(self):
        print("[testLinearMove]")
        MOVE_COUNT = 10
        start_pos_x = 0
        start_pos_y = 0
        interval_x = 5
        interval_y = 5

        self.mute(TARGET_SINK, False)
        for move_count in range(0, MOVE_COUNT):
            start_pos_x = start_pos_x+interval_x
            start_pos_y = start_pos_y+interval_y
################# setDisplayWindow #####################
            self.vlog("- setDisplayWindow " + str(move_count) + " " + str(start_pos_x) + " " + str(start_pos_y))

# for better performance don't check status
            self.checkLunaCallSuccess(
                    API_URL + "display/setDisplayWindow",
                    {"sink": TARGET_SINK,
                        "context": PID1,
                        "fullScreen": FULLSCREEN,
                        "sourceInput": {"x":0, "y":0, "width":WIDTH, "height":HEIGHT},
                        "displayOutput": {"x":start_pos_x, "y":start_pos_y, "width":WIDTH, "height":HEIGHT}})

            '''
            self.checkLunaCallSuccessAndSubscriptionUpdate(
                    API_URL + "display/setDisplayWindow",
                    {"sink": TARGET_SINK,
                        "fullScreen": FULLSCREEN,
                        "sourceInput": {"x":0, "y":0, "width":WIDTH, "height":HEIGHT},
                        "displayOutput": {"x":start_pos_x, "y":start_pos_y, "width":WIDTH, "height":HEIGHT}},
                    self.statusSub,
                    {"video":[{"sink": TARGET_SINK,
                        "fullScreen": FULLSCREEN,
                        "width":WIDTH,
                        "height":HEIGHT,
                        "frameRate":FRAMERATE,
                        "sourceInput": {"x":0, "y":0, "width":WIDTH, "height":HEIGHT},
                        "displayOutput": {"x":start_pos_x, "y":start_pos_y, "width":WIDTH, "height":HEIGHT}
                        }]})
            '''

    def testLinearSizeChange(self):
        print("[testLinearSizeChange]")
        MOVE_COUNT = 50
        width = 3840
        height = 2160
        interval_w = 10
        interval_h = 10

        self.mute(TARGET_SINK, False)
        for repeat in range(0, 3):
            for move_count in range(0, MOVE_COUNT):
                if repeat%2 == 0:
                    width = width - interval_w
                    height = height - interval_h
                else:
                    width = width + interval_w
                    height = height + interval_h
################# setDisplayWindow #####################
                self.vlog("- setDisplayWindow " + str(move_count) + " " + str(width) + " " + str(height))

# for better performance don't check status
                self.checkLunaCallSuccess(
                        API_URL + "display/setDisplayWindow",
                        {"sink": TARGET_SINK,
                            "context": PID1,
                            "fullScreen": FULLSCREEN,
                            "sourceInput": {"x":0, "y":0, "width":WIDTH, "height":HEIGHT},
                            "displayOutput": {"x":0, "y":0, "width":width, "height":height}})

                '''
                self.checkLunaCallSuccessAndSubscriptionUpdate(
                    API_URL + "display/setDisplayWindow",
                    {"sink": TARGET_SINK,
                        "fullScreen": FULLSCREEN,
                        "sourceInput": {"x":0, "y":0, "width":WIDTH, "height":HEIGHT},
                        "displayOutput": {"x":start_pos_x, "y":start_pos_y, "width":WIDTH, "height":HEIGHT}},
                    self.statusSub,
                    {"video":[{"sink": TARGET_SINK,
                        "fullScreen": FULLSCREEN,
                        "width":WIDTH,
                        "height":HEIGHT,
                        "frameRate":FRAMERATE,
                        "sourceInput": {"x":0, "y":0, "width":WIDTH, "height":HEIGHT},
                        "displayOutput": {"x":start_pos_x, "y":start_pos_y, "width":WIDTH, "height":HEIGHT}
                        }]})
                '''

if __name__ == '__main__':
    luna.VERBOSE = False
    unittest.main()
