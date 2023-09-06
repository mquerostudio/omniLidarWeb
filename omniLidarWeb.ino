#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <math.h>

const char* ssid = "TP-LINK_26619E";
const char* password = "18670691";

WebServer server(80);
WebSocketsServer webSocket(81);  // create a websocket server on port 81


int lidarTime = 0;
int measTimePerRev = 0;
int lidarMaxDistance = 2000;
int measNumPerRev = 0;

bool isStarted = false;
bool isStopped = false;

const int maxPoints = 10;
float points[maxPoints][2], pointsMod[maxPoints][2];

const char* htmlContent = R"(
<!DOCTYPE html>
<html lang="es">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>omniLidar Web Control</title>
    <style>
        h1 {
          color: green;
          font-size: 40px;
        }
        body {
            background-color: #FFD700;
            font-family: Arial, sans-serif;
            margin: 0;
            display: flex;
            text-align: center;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100vh;
        }

        .container {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 10px;
            padding: 20px;
        }

        .label {
            padding-left: 0px;
            font-size: 20px;
        }

        .input {
            padding: 5px;
            width: 30%;
        }

        .center-button {
            margin-top: 20px;
            background-color: #cc6600;
            color: white;
            border: none;
            padding: 15px 32px;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: 16px;
            border-radius: 0;
            cursor: pointer;
        }

        .center-button[disabled] {
            background-color: #ffe0b3;
            cursor: not-allowed; /* optional: change the cursor to indicate the button is disabled */
        }


        .start-button {
            margin-right: 10px;
            background-color: green;
            color: white;
            border: none;
            padding: 15px 32px;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: 16px;
            cursor: pointer;
        }

        .stop-button {
            margin-left: 10px;
            background-color: red;
            color: white;
            border: none;
            padding: 15px 32px;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: 16px;
            cursor: pointer;
        }


        #graphCanvas {
            max-width: 90%;
            height: auto;
            margin-top: 20px;
            border: 2px solid red;
        }
    </style>
</head>

<body>
    <h1>omniLidar Web Control</h1>
    <form method="POST" action="/">
        <div class="container">
            <div>
                <label class="label" for="lidarTime">Lidar - Time of measure [ms]:</label>
                <input class="input" type="number" id="lidarTime" name="lidarTime">
            </div>

            <div>
                <label class="label" for="measTimePerRev [ms]">Meas - Time of Revolution:</label>
                <input class="input" type="number" id="measTimePerRev" name="measTimePerRev">
            </div>
        </div>
        <div class="container">
            <div>
                <label class="label" for="lidarMaxDistance">Lidar - Max meas distance [ms]:</label>
                <input class="input" type="number" id="lidarMaxDistance" name="lidarMaxDistance">
            </div>
            <div>
                <label class="label" for="measNumPerRev">Meas - Num per revolution:</label>
                <input class="input" type="number" id="measNumPerRev" name="measNumPerRev">
            </div>
        </div>
        <button type="button" class="center-button start-button" id="startBtn">Start</button>
        <button type="submit" class="center-button">Save</button>
        <button type="button" class="center-button stop-button" id="stopBtn">Stop</button>
    </form>

    <canvas id='graphCanvas' width='500' height='500'></canvas>
    <script>

        var lidarMaxDistance = 2000;  // Initial default value

        var canvas = document.getElementById('graphCanvas');
        var ctx = canvas.getContext('2d');
        function drawTemplate(){

            ctx.fillStyle = 'lightgray';
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            ctx.fillStyle = 'blue';

            // Draw coordinate axis and circles
            var centerX = canvas.width / 2;
            var centerY = canvas.height / 2;
            var radius = Math.min(centerX, centerY) - 20;

            // Draw coordinate axis
            ctx.strokeStyle = 'black';
            ctx.beginPath();
            ctx.moveTo(centerX, 0);
            ctx.lineTo(centerX, canvas.height);
            ctx.stroke();

            ctx.beginPath();
            ctx.moveTo(0, centerY);
            ctx.lineTo(canvas.width, centerY);
            ctx.stroke();

            // Draw lines at 45 degrees
            ctx.strokeStyle = 'green';
            ctx.beginPath();
            ctx.moveTo(centerX - radius, centerY + radius);
            ctx.lineTo(centerX + radius, centerY - radius);
            ctx.stroke();

            ctx.beginPath();
            ctx.moveTo(centerX - radius, centerY - radius);
            ctx.lineTo(centerX + radius, centerY + radius);
            ctx.stroke();


            // Draw circles representing radians
            ctx.strokeStyle = 'gray';
            var distanceBetweenCircles = lidarMaxDistance / 5;
            var textdistance = 0;
            for (var i = 1; i <= 5; i++) {
                ctx.beginPath();
                ctx.arc(centerX, centerY, radius * (i / 5), 0, 2 * Math.PI);
                ctx.stroke();
                ctx.fillText(Math.round(textdistance) + "m", centerX + (radius - 20) * (i / 5), centerY-5);
                textdistance += distanceBetweenCircles;
            }
        }
        

        function clearCanvas(){
            ctx.clearRect(0, 0, canvas.width, canvas.height);
        }

        function drawPoints(points) {
            clearCanvas();
            drawTemplate();
            
            const scaleFactor = canvas.width / (2*lidarMaxDistance);

            for(let i = 0; i < points.length; i++) {
                const x = (points[i][0]*scaleFactor + canvas.width / 2); 
                const y = (points[i][1]*scaleFactor + canvas.height / 2);

                ctx.beginPath();
                ctx.arc(x, y, 2, 0, 2 * Math.PI); 
                ctx.fillStyle = 'red'; 
                ctx.fill();
            }
        }


        var socket = new WebSocket('ws://' + location.hostname + ':81/');
        socket.onmessage = function(event) {
            const data = event.data;
            
            if (data === "true") { // if isStopped is true
                document.querySelector(".center-button[type='submit']").disabled = false; // Enable save button

            } else if (data === "false") {
                document.querySelector(".center-button[type='submit']").disabled = true; // Disable save button
                
            } else if (data !== "noCommand") {
                const pointStrings = data.split("|");
                const pointArray = pointStrings.map(str => {
                    const coords = str.split(",");
                    return [parseInt(coords[0]), parseInt(coords[1])];
                });
                drawPoints(pointArray);
            }
        }

        document.getElementById('startBtn').addEventListener('click', function() {
            socket.send('start');
        });

        document.getElementById('stopBtn').addEventListener('click', function() {
            socket.send('stop');
        });

        // Handle form submission
        document.querySelector("form").addEventListener('submit', function(event) {
            event.preventDefault();  // prevent default form submission

            // Construct the form data object
            var formData = {
                lidarTime: document.getElementById("lidarTime").value,
                measTimePerRev: document.getElementById("measTimePerRev").value,
                lidarMaxDistance: document.getElementById("lidarMaxDistance").value,
                measNumPerRev: document.getElementById("measNumPerRev").value
            };

            // Send the form data as a JSON string through the WebSocket
            socket.send(JSON.stringify(formData));
        });


    </script>
</body>

</html>
)";

void handleRoot() {
  server.send(200, "text/html", htmlContent);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT:

      if (length > 0 && payload[0] == '{') {  // Check if payload is a JSON string (basic check)
          // Parse JSON payload for form data
          DynamicJsonDocument doc(1024);
          deserializeJson(doc, (char*)payload);
          lidarTime = doc["lidarTime"];
          measTimePerRev = doc["measTimePerRev"];
          lidarMaxDistance = doc["lidarMaxDistance"];
          measNumPerRev = doc["measNumPerRev"];
          // Print the values (you can remove this if not needed)
          Serial.println("Values saved via WebSocket:");
          Serial.print("Lidar Time: ");
          Serial.println(lidarTime);
          Serial.print("Meas Time per Revolution: ");
          Serial.println(measTimePerRev);
          Serial.print("Lidar Max Meas Distance: ");
          Serial.println(lidarMaxDistance);
          Serial.print("Meas Num per Revolution: ");
          Serial.println(measNumPerRev);
          return;
      }

      String msg = String((char*)payload);
      if (msg == "start") {
        Serial.println("Start Button");
        isStarted = true;
        isStopped = false;
      } else if (msg == "stop") {
        Serial.println("Stop Button");
        isStarted = false;
        isStopped = true;
      }

      String isStoppedStr = isStopped ? "true" : "false";
      webSocket.broadcastTXT(isStoppedStr);
      break;
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started");
}

void loop() {
  server.handleClient();
  webSocket.loop();

  static unsigned long lastTime = 0;
  if (millis() - lastTime > 1000) {
    lastTime = millis();

    float n = 0.0;
    String dataToSend = "";
    for (int i = 0; i < maxPoints; i++) {
      points[i][0] = random(500, 2000);                    //Distance
      points[i][1] = n;                                    //angle
      pointsMod[i][0] = points[i][0] * cos(points[i][1]);  //x points
      pointsMod[i][1] = points[i][0] * sin(points[i][1]);  //y points

      if (i > 0) {
        dataToSend += "|";
      }
      dataToSend += String(pointsMod[i][0]) + "," + String(pointsMod[i][1]);
      n = n + (2 * PI / maxPoints);
    }
    webSocket.broadcastTXT(dataToSend);
  }
}