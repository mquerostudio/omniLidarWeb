#include <WebServer.h>

const char* ssid = "TP-LINK_26619E";
const char* password = "18670691";

WebServer server(80);

int lidarTime = 0;
int measTimePerRev = 0;
int lidarMaxDistance = 0;
int measNumPerRev = 0;
bool shouldDrawNewData = false;

const int maxPoints = 50; // You can change this value to store more or less points
int points[maxPoints][2]; // [][0] for X and [][1] for Y

const char* htmlContent = R"(
<!DOCTYPE html>
<html lang="es">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>omniLidar Web Control</title>
    <style>
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
            background-color: #4CAF50;
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
        <button type="submit" class="center-button">Guardar</button>
    </form>

    <canvas id='graphCanvas' width='500' height='500'></canvas>
    <script>
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
            var textdistance = 0;
            for (var i = 1; i <= 6; i++) {
                ctx.beginPath();
                ctx.arc(centerX, centerY, radius * (i / 6), 0, 2 * Math.PI);
                ctx.stroke();
                ctx.fillText(textdistance, centerX + radius * (i / 6), centerY-5);
                textdistance += Math.floor(canvas.width/6);
            }

            ctx.strokeStyle = 'black';
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
            ctx.stroke();
        }
        

        function clearCanvas(){
            ctx.clearRect(0, 0, canvas.width, canvas.height);
        }

        function drawPoints(points) {
            clearCanvas();
            drawTemplate();
            
            for(let i = 0; i < points.length; i++) {
                // Draw each point on the canvas
                const x = points[i][0];
                const y = points[i][1];
                ctx.beginPath();
                ctx.arc(x, y, 2, 0, 2 * Math.PI); // drawing a small circle as a point
                ctx.fillStyle = 'red'; // color of the point
                ctx.fill();
            }
        }

        function checkCommand() {
            fetch("/command")
            .then(response => response.text())
            .then(data => {
                if (data !== "noCommand") {
                    const pointStrings = data.split("|");
                    const pointArray = pointStrings.map(str => {
                        const coords = str.split(",");
                        return [parseInt(coords[0]), parseInt(coords[1])];
                    });
                    drawPoints(pointArray);
                }
            });
        }

        setInterval(checkCommand, 500);  // Check every second

    </script>
</body>

</html>
)";

void handleRoot() {
  if (server.method() == HTTP_POST) {
    lidarTime = server.arg("lidarTime").toInt();
    measTimePerRev = server.arg("measTimePerRev").toInt();
    lidarMaxDistance = server.arg("lidarMaxDistance").toInt();
    measNumPerRev = server.arg("measNumPerRev").toInt();

    Serial.println("Values saved:");
    Serial.print("Lidar Time: ");
    Serial.println(lidarTime);
    Serial.print("Meas Time per Revolution: ");
    Serial.println(measTimePerRev);
    Serial.print("Lidar Max Meas Distance: ");
    Serial.println(lidarMaxDistance);
    Serial.print("Meas Num per Revolution: ");
    Serial.println(measNumPerRev);
  }

  server.send(200, "text/html", htmlContent);
}

void handleCommand() {
  if (shouldDrawNewData) {
    String dataToSend = "";
    for (int i = 0; i < maxPoints; i++) {
      if (i > 0) {
        dataToSend += "|"; // use | as a separator between points
      }
      dataToSend += String(points[i][0]) + "," + String(points[i][1]);
    }
    server.send(200, "text/plain", dataToSend);
    shouldDrawNewData = false;  // reset it after sending
  } else {
    server.send(200, "text/plain", "noCommand");
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
  server.on("/command", HTTP_GET, handleCommand);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();

    static unsigned long lastTime = 0;
    if (millis() - lastTime > 1000) {
        lastTime = millis();

        // Generate random points
        for(int i = 0; i < maxPoints; i++) {
            points[i][0] = random(0, 500); // assuming canvas width is 500
            points[i][1] = random(0, 500); // assuming canvas height is 500
        }
        shouldDrawNewData = true;
    }
}
