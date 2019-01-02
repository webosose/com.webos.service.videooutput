#!/usr/bin/python2
import subprocess
import json
import threading
import time
import unittest

SERVICE = "luna://"
VERBOSE = False

# Primitive event-loop emulation for subscription events, using threads and a global lock
LOCK = threading.RLock()

class Subscription:
    def __init__(self):
        self.proc = None
        self.thread = None
        self.cond = threading.Condition()
        self.callback = None
        self.callbackArgs = None
        self.lastResponse = None
        self.cancelled = False

def log(message):
    print message

def call(method, params):
    args = ["luna-send", "-n", "1", SERVICE+method, json.dumps(params)]
    if VERBOSE:
        log("Calling " + method)
        log("Cmd: " + " ".join(args))

    output = subprocess.check_output(args)

    if VERBOSE:
        log("Output: " + output.strip())

    ret = json.loads(output)

    if not ret.get('returnValue'):
        if VERBOSE:
            log("luna call returned fail \n Args:" + " ".join(args)+ "\n Output:" + output)

    if VERBOSE:
        log("Ok")
    return ret

def callAsync(method, params, callback = None, callback_args = ()):

    def callImpl():
        ret = call(method, params)
        if callback:
            callback(ret, *callback_args)

    # Start a thread to run the call
    t = threading.Thread(target=callImpl)
    t.daemon = False
    t.start()


def lunaSubscribeHandler(method, subscription):
    while not subscription.cancelled:
        line = subscription.proc.stdout.readline()
        if line == "" or subscription.cancelled:
            break

        if VERBOSE:
            log("Subscription response to " + method)
            log("Result: " + line)

        ret = json.loads(line)
        if not ret.get('returnValue'):
            if VERBOSE:
                log("luna subscription returned bad output " + line)

        if subscription.callback:
            LOCK.acquire(True)
            result = subscription.callback(ret, *subscription.callback_args)
            LOCK.release()

            if result is not True:
                if VERBOSE:
                    log("Subscription to " + method + " cancelled")
                break

        #notify any waiters
        subscription.cond.acquire()
        subscription.lastResponse = ret
        subscription.cond.notify()
        subscription.cond.release()

    if not subscription.cancelled:
        subscription.proc.terminate()

def subscribe(method, params, callback = None, callback_args = ()):
    params["subscribe"] = True
    args = ["luna-send", "-i", SERVICE+method, json.dumps(params)]

    if VERBOSE:
        log("Subscribing to " + method)
        log("Cmd: " + " ".join(args))

    proc = subprocess.Popen(args, stdout=subprocess.PIPE)
    first_response = proc.stdout.readline()

    if VERBOSE:
        log("Result: " + first_response)

    ret = json.loads(first_response)

    if not ret.get('returnValue'):
        if VERBOSE:
            log("luna subscription returned bad output " + first_response)
        proc.terminate()
        return None

    if callback is not None:
        callback(ret, *callback_args)

    subscription = Subscription()
    subscription.proc = proc
    subscription.callback = callback
    subscription.callbackArgs = callback_args

    # Start a thread to read the next resposnes
    t = threading.Thread(target=lunaSubscribeHandler, args=(method, subscription))
    t.daemon = False

    subscription.thread = t

    t.start()
    return subscription


def cancelSubscribe(subscription):
    subscription.cancelled = True
    subscription.proc.terminate()

def setTimeout(timeoutMs, callback, params):
    def timeoutProc():
        while True:
            time.sleep(timeoutMs*0.001)
            LOCK.acquire(True)
            result = callback(*params)
            LOCK.release()
            if result is not True:
                break
    t = threading.Thread(target=timeoutProc)
    t.daemon = False
    t.start()

def awaitSubscriptionUpdate(subscription, timeoutMs = 1000):
    update = None

    subscription.cond.acquire()
    subscription.cond.wait(timeoutMs/1000.0)
    if subscription.lastResponse:
        update = subscription.lastResponse
        subscription.lastResponse = None
    subscription.cond.release()

    return update

#returns (ret, subscriptionRet)
def callAndAwaitSubscriptionUpdate(method, params, subscription, timeout = 1000):
    def resultHandler(result, callState):
        callState[0] = result
        callState[1] = True

    callState = [None, False]
    callAsync(method, params, resultHandler, (callState,))
    subscriptionResult = awaitSubscriptionUpdate(subscription, timeout)
    while not callState[1]:
        time.sleep(0.01)
    return (callState[0], subscriptionResult)

class TestBase(unittest.TestCase):

    def checkContainsData(self, ret, data):
        if isinstance(data, list):
            if not isinstance(ret, list):
                return False
            for di in data:
                found = False
                for ri in ret:
                    if self.checkContainsData(ri, di):
                        found = True
                        break
                if not found:
                    return False
            return True
        elif isinstance(data, dict):
            if not isinstance(ret, dict):
                return False
            for key in data:
                if key not in ret:
                    return False
                elif data[key] == "_any_":
                    continue
                elif not self.checkContainsData(ret[key], data[key]):
                    return False
            return True
        else:
            return ret == data

    def assertContainsData(self, ret, data, path = ""):
        if isinstance(data, list):
            self.assertTrue(isinstance(ret, list))
            for di in data:
                found = False
                for ri in ret:
                    if self.checkContainsData(ri, di):
                        found = True
                        break
                self.assertTrue(found)
        elif isinstance(data, dict):
            self.assertTrue(isinstance(ret, dict), "Dict expected in " + path + "but got " + str(ret))
            for key in data:
                self.assertTrue(key in ret)
                if (data[key] != "_any_"):
                    self.assertContainsData(ret[key], data[key], path + "."+key)
        else:
            self.assertEqual(ret, data)

    def assertIsSuccess(self, ret):
        if not self.checkContainsData(ret, {"returnValue": True}):
            self.assertTrue(False, "Call not a success:" + str(ret))

    def assertIsFail(self, ret):
        self.assertContainsData(ret, {"returnValue": False})

    def checkLunaCallSuccess(self, method, params):
        ret = call(method, params)
        self.assertIsSuccess(ret)

    def checkLunaCallFail(self, method, params):
        ret = call(method, params)
        self.assertIsFail(ret)

    def checkLunaCallSuccessAndSubscriptionUpdate(self, method, params, subscription, subscriptionData):
        (ret, sret) = callAndAwaitSubscriptionUpdate(method, params, subscription)
        self.assertIsSuccess(ret)
        self.assertTrue(sret is not None, "Subscription update not sent")
        if not self.checkContainsData(sret, subscriptionData):
            self.assertTrue(False, "Subscription response does not match.\n Response: " + str(sret) + "\n Pattern: " + str(subscriptionData))

    def checkLunaCallFailAndNoSubscriptionUpdate(self, method, params, subscription):
        (ret, sret) = callAndAwaitSubscriptionUpdate(method, params, subscription, 100)
        self.assertIsFail(ret)
        self.assertEquals(sret, None)
