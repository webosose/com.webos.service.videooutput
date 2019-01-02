#!/usr/bin/python2
import unittest
import luna_utils as luna
import time

API_URL = "com.webos.service.videooutput/"

VERBOSE_LOG = True
SUPPORT_REGISTER = False

SINK_MAIN = "MAIN"
SINK_SUB = "SUB0"

#TODO(ekwang): If you connect SUB, HAL error occurs. Just test MAIN in the current state
#SINK_LIST = [SINK_MAIN, SINK_SUB]
SINK_LIST = [SINK_MAIN]

PID1 = "pipeline1"
PID2 = "pipeline2"

PID_LIST = [PID1, PID2]

INPUT_RECT = {'X':0, 'Y':0, 'W':1920, 'H':1080}
OUTPUT_RECT = {'X':400, 'Y':400, 'W':1920, 'H':1080}

#Choose source type VDEC or HDMI for test input
#SOURCE_NAME = SOURCE_NAME
#SOURCE_PORT = 0
SOURCE_NAME = "HDMI"
SOURCE_PORT = 3

SOURCE_WIDTH = 1920
SOURCE_HEIGHT = 1080

SLEEP_TIME = 1

class TestVideoMethods(luna.TestBase):

    def vlog(self, message):
        if VERBOSE_LOG:
            print(message)

    def setUp(self):
        self.vlog("setUp")
        if SUPPORT_REGISTER:
            for pid in PID_LIST:
                self.vlog("register " + pid)
                luna.call(API_URL + "register", { "context": pid })

        self.statusSub = luna.subscribe(API_URL + "getStatus", {"subscribe":True})

    def tearDown(self):
        self.vlog("tearDown")
        for sink in SINK_LIST:
            self.vlog("disconnect " + sink)
            luna.call(API_URL + "disconnect", { "sink": sink })

        if SUPPORT_REGISTER:
            for pid in PID_LIST:
                self.vlog("unregister " + pid)
                luna.call(API_URL + "unregister", { "context": pid })

        luna.cancelSubscribe(self.statusSub)

    def connect(self, sink, source, port, pid):
        self.vlog("connect " + sink)
        self.checkLunaCallSuccessAndSubscriptionUpdate(API_URL + "connect",
                { "outputMode": "DISPLAY", "sink": sink, "source": source, "sourcePort": port },
                self.statusSub,
                {"video":[{"sink": sink, "connectedSource": source, "connectedSourcePort": port}]})

    def mute(self, sink, blank):
        self.vlog("- Mute" + sink)
        self.checkLunaCallSuccessAndSubscriptionUpdate(
                API_URL + "blankVideo",
                {"sink": sink, "blank": blank},
                self.statusSub,
                {"video":[{"sink": sink, "muted": blank}]})

    def disconnect(self, sink, pid):
        self.vlog("disconnect " + sink)
        self.checkLunaCallSuccessAndSubscriptionUpdate(API_URL + "disconnect", { "sink": sink },
                self.statusSub,
                {"video": [{"sink": sink, "connectedSource": None}]})

    def testConnectDisconnect(self):
        print("[testConnectDisconnect]")
        for source, ports in {"VDEC":[0,1], "HDMI":[0,1,2]}.iteritems():
            for port in ports:
                for sink in SINK_LIST:
                    for i in range(3):
                        self.connect(sink, source, port, "")
                        self.disconnect(sink, "")

    def testDualConnect(self):
        print("[testDualConnect]")
        self.connect(SINK_MAIN, SOURCE_NAME, SOURCE_PORT, "")
        if len(SINK_LIST) > 1:
            self.checkLunaCallSuccessAndSubscriptionUpdate(API_URL + "connect",
                                                      {"outputMode": "DISPLAY", "sink": SINK_SUB, "source": SOURCE_NAME, "sourcePort": SOURCE_PORT},
                                                      self.statusSub,
                                                      {"video": [{"sink": SINK_MAIN, "connectedSource": SOURCE_NAME, "connectedSourcePort": SOURCE_PORT},
                                                                 {"sink": SINK_SUB, "connectedSource": SOURCE_NAME, "connectedSourcePort": SOURCE_PORT}]})

        self.disconnect(SINK_MAIN, "")
        if len(SINK_LIST) > 1:
            self.disconnect(SINK_SUB, "")

    def testMute(self):
        print("[testMute]")
        for sink in SINK_LIST:
            self.connect(sink, SOURCE_NAME, SOURCE_PORT, "")

            for blank in [False, True]:
                self.mute(sink, blank)

    #test different orders of display window and media data

    def testSetDisplayWindowAndVideoData(self):
        print("[testSetDisplayWindowAndVideoData]")
        self.connect(SINK_MAIN, SOURCE_NAME, SOURCE_PORT, "")

        self.checkLunaCallSuccessAndSubscriptionUpdate(
                API_URL + "display/setDisplayWindow",
                {"sink": SINK_MAIN,
                    "fullScreen": False,
                    "sourceInput": {"x":INPUT_RECT['X'], "y":INPUT_RECT['Y'], "width":INPUT_RECT['W'], "height":INPUT_RECT['H']},
                    "displayOutput": {"x":OUTPUT_RECT['X'], "y":OUTPUT_RECT['Y'], "width":OUTPUT_RECT['W'], "height":OUTPUT_RECT['H']}},
                self.statusSub,
                {"video":[{"sink": "MAIN",
                    "fullScreen": False,
                    "width":0,
                    "height":0,
                    "frameRate":0,
                    "sourceInput": {"x":0, "y":0, "width":0, "height":0}, # no media data yet so can't determine appliedsourceInput yet
                    "displayOutput": {"x":OUTPUT_RECT['X'], "y":OUTPUT_RECT['Y'], "width":OUTPUT_RECT['W'], "height":OUTPUT_RECT['H']}
                    }]})

        self.checkLunaCallSuccessAndSubscriptionUpdate(
                API_URL + "setVideoData",
                {"sink": SINK_MAIN,
                    "contentType": "media",
                    "frameRate":29.5,
                    "width":SOURCE_WIDTH,
                    "height":SOURCE_HEIGHT,
                    "scanType":"progressive",
                    "adaptive": False},
                self.statusSub,
                {"video":[{"sink": "MAIN",
                    "fullScreen": False,
                    "width":SOURCE_WIDTH,
                    "height":SOURCE_HEIGHT,
                    "frameRate":29.5,
                    "sourceInput": {"x":0, "y":0, "width":SOURCE_WIDTH, "height":SOURCE_HEIGHT},
                    "displayOutput": {"x":OUTPUT_RECT['X'], "y":OUTPUT_RECT['Y'], "width":OUTPUT_RECT['W'], "height":OUTPUT_RECT['H']}
                    }]})

        self.mute(SINK_MAIN, False)
        time.sleep(SLEEP_TIME)

    def testSetVideoDataAndDisplayWindow(self):
        print("[testSetVideoDataAndDisplayWindow]")
        self.connect(SINK_MAIN, SOURCE_NAME, SOURCE_PORT, "")

        self.checkLunaCallSuccessAndSubscriptionUpdate(
                API_URL + "setVideoData",
                {"sink": SINK_MAIN,
                    "contentType": "media",
                    "frameRate":29.5,
                    "width":SOURCE_WIDTH,
                    "height":SOURCE_HEIGHT,
                    "scanType":"progressive",
                    "adaptive": False},
                self.statusSub,
                {"video":[{"sink": SINK_MAIN,
                    "fullScreen": False,
                    "width":SOURCE_WIDTH,
                    "height":SOURCE_HEIGHT,
                    "frameRate":29.5,
                    "sourceInput": {"x":0, "y":0, "width":0, "height":0},
                    "displayOutput": {"x":0, "y":0, "width":0, "height":0}
                    }]})

        self.checkLunaCallSuccessAndSubscriptionUpdate(
                API_URL + "display/setDisplayWindow",
                {"sink": "MAIN",
                    "fullScreen": False,
                    "sourceInput": {"x":INPUT_RECT['X'], "y":INPUT_RECT['Y'], "width":INPUT_RECT['W'], "height":INPUT_RECT['H']},
                    "displayOutput": {"x":OUTPUT_RECT['X'], "y":OUTPUT_RECT['Y'], "width":OUTPUT_RECT['W'], "height":OUTPUT_RECT['H']}},
                self.statusSub,
                {"video":[{"sink": SINK_MAIN,
                    "fullScreen": False,
                    "width":SOURCE_WIDTH,
                    "height":SOURCE_HEIGHT,
                    "frameRate":29.5,
                    "sourceInput": {"x":INPUT_RECT['X'], "y":INPUT_RECT['Y'], "width":INPUT_RECT['W'], "height":INPUT_RECT['H']},
                    "displayOutput": {"x":OUTPUT_RECT['X'], "y":OUTPUT_RECT['Y'], "width":OUTPUT_RECT['W'], "height":OUTPUT_RECT['H']}
                    }]})

        self.mute(SINK_MAIN, False)
        time.sleep(SLEEP_TIME)

    def testSetFullscreen(self):
        print("[testSetFullscreen]")
        self.connect(SINK_MAIN, SOURCE_NAME, SOURCE_PORT, "")

        self.checkLunaCallSuccessAndSubscriptionUpdate(
            API_URL + "setVideoData",
            {"sink": SINK_MAIN,
             "contentType": "media",
             "frameRate":29.5,
             "width":SOURCE_WIDTH,
             "height":SOURCE_HEIGHT,
             "scanType":"progressive",
             "adaptive": False},
            self.statusSub,
            {"video":[{"sink": SINK_MAIN,
                   "fullScreen": False,
                   "width":SOURCE_WIDTH,
                   "height":SOURCE_HEIGHT,
                   "frameRate":29.5,
                   "sourceInput": {"x":0, "y":0, "width":0, "height":0},
                   "displayOutput": {"x":0, "y":0, "width":0, "height":0}
               }]})

        self.checkLunaCallSuccessAndSubscriptionUpdate(
            API_URL + "display/setDisplayWindow",
            {"sink": SINK_MAIN,
             "fullScreen": True,
             "sourceInput": {"x":0, "y":0, "width":SOURCE_WIDTH, "height":SOURCE_HEIGHT}},
            self.statusSub,
            {"video":[{"sink": SINK_MAIN,
                   "fullScreen": True,
                   "width":SOURCE_WIDTH,
                   "height":SOURCE_HEIGHT,
                   "frameRate":29.5,
                   "sourceInput": {"x":0, "y":0, "width":SOURCE_WIDTH, "height":SOURCE_HEIGHT},
                   "displayOutput": {"x":0, "y":0, "width":3840, "height":2160}
               }]})

        self.mute(SINK_MAIN, False)
        time.sleep(SLEEP_TIME)

    def testSetCompositing(self):
        print("[testSetCompositing]")
        self.connect(SINK_MAIN, SOURCE_NAME, SOURCE_PORT, "")
        if len(SINK_LIST) > 1:
            self.connect(SINK_SUB, SOURCE_NAME, SOURCE_PORT, "")

        self.checkLunaCallSuccessAndSubscriptionUpdate(
            API_URL + "display/setCompositing",
            {"composeOrder": [{"sink":SINK_MAIN, "opacity":20, "zOrder":1},
                            {"sink":SINK_SUB, "opacity":31, "zOrder":0}]},
            self.statusSub, {"video":[{"sink": "MAIN", "opacity":20, "zOrder":1}]})

        self.checkLunaCallSuccessAndSubscriptionUpdate(
            API_URL + "display/setDisplayWindow",
            {"sink": SINK_MAIN, "fullScreen":True, "opacity":130},
            self.statusSub, {"video":[{"sink": SINK_MAIN, "opacity":130, "zOrder":1}]})

        if len(SINK_LIST) > 1:
            self.checkLunaCallSuccessAndSubscriptionUpdate(
                    API_URL + "display/setDisplayWindow",
                    {"sink": SINK_SUB, "fullScreen":True, "opacity":200},
                    self.statusSub, {"video":[{"sink": "SUB0", "opacity":200, "zOrder":0}]})

            self.checkLunaCallSuccessAndSubscriptionUpdate(
                    API_URL + "display/setDisplayWindow",
                    {"sink": SINK_SUB, "fullScreen":True, "opacity":230},
                    self.statusSub, {"video":[{"sink": "MAIN", "opacity":130, "zOrder":0}, {"sink": "SUB0", "opacity":230, "zOrder":1}]})

            self.checkLunaCallSuccessAndSubscriptionUpdate(
                    API_URL + "display/setDisplayWindow",
                    {"sink": SINK_SUB, "fullScreen":True, "opacity":30, "zOrder": 1},
                    self.statusSub, {"video":[{"sink": "MAIN", "opacity":130, "zOrder":0}, {"sink": "SUB0", "opacity":30, "zOrder":1}]})

if __name__ == '__main__':
    luna.VERBOSE = False
    unittest.main()
