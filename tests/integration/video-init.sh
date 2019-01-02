#!/bin/sh

echo "luna://com.webos.service.videooutput/connect"
luna-send -n 1 -f luna://com.webos.service.videooutput/connect '{"appId":"eos.bare", "source": "VDEC", "sourcePort":0, "sink":"DISP0_MAIN", "outputMode":"DISPLAY"}'

sleep 1
echo "luna://com.webos.service.videooutput/setVideoData"
luna-send -n 1 -f luna://com.webos.service.videooutput/setVideoData '{"sink":"DISP0_MAIN","contentType":"movie","frameRate":24,"width":1920,"height":1080,"adaptive":false,"scanType":"progressive"}'

sleep 1
echo "luna://com.webos.service.videooutput/setDisplayWindow"
luna-send -n 1 luna://com.webos.service.videooutput/display/setDisplayWindow '{"sink":"DISP0_MAIN", "fullScreen":false, "displayOutput": {"x":1000, "y":500, "width":920, "height":580}}'

sleep 1
echo "luna://com.webos.service.videooutput/blankVideo"
luna-send -n 1 -f luna://com.webos.service.videooutput/blankVideo '{"sink":"DISP0_MAIN", "blank":false}'

sleep 1
echo "luna://com.webos.service.videooutput/disconnect"
luna-send -n 1 -f luna://com.webos.service.videooutput/disconnect '{"sink":"DISP0_MAIN"}'

echo "Finished"
