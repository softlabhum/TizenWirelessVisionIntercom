 /*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

window.onload = function(){
    var container = document.querySelector("#camera-view");
    runWebSocket();

    function runWebSocket() {
        var wsUri = "ws://" + window.location.hostname + ":8888/";

        websocket = new WebSocket(wsUri);
        websocket.onopen = function(evt) { onOpen(evt) };
        websocket.onclose = function(evt) { onClose(evt) };
        websocket.onmessage = function(evt) { onMessage(evt) };
        websocket.onerror = function(evt) { onError(evt) };
    }

    function onOpen(evt)
    {
        console.log("CONNECTED");
        doSend("HELLO FROM BROWSER via WebSocket!!!!!!!!!!!!");
    }

    function onClose(evt)
    {
        console.log("DISCONNECTED");
    }

    function onMessage(evt)
    {
        var urlCreator = window.URL || window.webkitURL;
        var imageUrl = urlCreator.createObjectURL(evt.data);
        document.querySelector("#camera-view").src = imageUrl;

        doSend("ack", true);
    }

    function onError(evt)
    {
        console.log(evt.data);
    }

    function doSend(message, dont_print_log)
    {
        websocket.send(message);
        if (!dont_print_log)
            console.log("SENT: " + message);
    }

    var buttonOnElement = document.getElementById('button-on');
    buttonOnElement.addEventListener('click', function () {
        httpGetAsync("/command/on", function (res){
            console.log(res);
        })
    });

    var buttonOffElement = document.getElementById('button-off');
    buttonOffElement.addEventListener('click', function () {
        httpGetAsync("/command/off", function (res){
            console.log(res);
        })
    });

    var buttonMsgElement = document.getElementById('button-msg');
    buttonMsgElement.addEventListener('click', function () {
        httpGetAsync("/command/send", function (res){
            console.log(res);
        })
    });
};

function httpGetAsync(theUrl, callback)
{
    var xmlHttp = new XMLHttpRequest();
    xmlHttp.onreadystatechange = function() {
        if (xmlHttp.readyState == 4 && xmlHttp.status == 200)
            callback(xmlHttp.responseText);
    }
    xmlHttp.open("GET", theUrl, true); // true for asynchronous
    xmlHttp.send(null);
}
