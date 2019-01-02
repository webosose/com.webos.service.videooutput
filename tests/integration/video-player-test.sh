#!/bin/sh
# Opens various video player apps in sequence. Run and visually check if all apps play video correctly.
echo "Starting HTML5 dual video app"
luna-send -n 1 luna://com.webos.applicationManager/launch  '{ "id": "com.webos.app.dualvideo", "params": { } }'

sleep 10
echo "Starting QML single video app"
luna-send -n 1 luna://com.webos.applicationManager/launch '{ "id": "com.webos.app.test.qml.video", "params": { } } '

sleep 10
echo "Starting HTML5 dual video app"
luna-send -n 1 luna://com.webos.applicationManager/launch  '{ "id": "com.webos.app.dualvideo", "params": { } }'

sleep 10
echo "Starting QML dual video app"
luna-send -n 1 luna://com.webos.applicationManager/launch '{ "id": "com.webos.app.test.qml.dual-video", "params": { } } '


echo "Finished"
