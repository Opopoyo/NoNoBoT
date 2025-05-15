//  Pilote de bouclier servo PWM, 16 canaux, 12 bits, PCA9685PW, interface I2C  
//  Contrôle de 6 servomoteurs via interface web ESP32

#include <WiFi.h>
#include <Adafruit_PWMServoDriver.h>

const char* ssid = "TON_SSID";
const char* password = "TON_MDP";

// Initialisation du PCA9685 à l'adresse I2C 0x40
Adafruit_PWMServoDriver board1 = Adafruit_PWMServoDriver(0x40);

// Limites PWM typiques pour servos
#define SERVOMIN  125
#define SERVOMAX  575

// Brochage logique des moteurs (0 à 5)
const int motorPins[6] = {0, 1, 2, 3, 4, 5};
WiFiServer server(80);

String webPage = R"====(
<!DOCTYPE html>
<html>
<head>
  <title>Contrôle des moteurs</title>
  <style>
    button { margin: 5px; padding: 10px; }
    input[type="checkbox"] { margin: 10px; }
  </style>
  <script>
    let selectedMotors = [];
    function toggleMotor(motor) {
      if (selectedMotors.includes(motor)) {
        selectedMotors = selectedMotors.filter(m => m !== motor);
      } else {
        selectedMotors.push(motor);
      }
      document.getElementById("selectedMotors").innerText = selectedMotors.join(", ") || "Aucun";
    }
    function sendCommand(command) {
      if (selectedMotors.length === 0) {
        alert("Sélectionne au moins un moteur !");
        return;
      }
      fetch('/command?cmd=' + command + '&motors=' + selectedMotors.join(","))
        .then(response => response.text())
        .then(data => {
          document.getElementById('status').innerText = data;
        });
    }
  </script>
</head>
<body>
  <h1>Contrôle des moteurs</h1>
  <div>
    <label><input type="checkbox" onchange="toggleMotor(0)"> Moteur 0</label>
    <label><input type="checkbox" onchange="toggleMotor(1)"> Moteur 1</label>
    <label><input type="checkbox" onchange="toggleMotor(2)"> Moteur 2</label>
    <label><input type="checkbox" onchange="toggleMotor(3)"> Moteur 3</label>
    <label><input type="checkbox" onchange="toggleMotor(4)"> Moteur 4</label>
    <label><input type="checkbox" onchange="toggleMotor(5)"> Moteur 5</label>
  </div>
  <p>Moteurs sélectionnés : <span id="selectedMotors">Aucun</span></p>
  <div>
    <button onclick="sendCommand('Up')">Marche Avant</button>
    <button onclick="sendCommand('Down')">Marche Arrière</button>
    <button onclick="sendCommand('360Up')">360° Avant</button>
    <button onclick="sendCommand('360Down')">360° Arrière</button>
  </div>
  <p>État : <span id="status">Prêt.</span></p>
</body>
</html>
)====";

// Fonctions utilitaires
int angleToPulse(int angle) {
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}

// Split une string sur ','
std::vector<int> parseMotors(const String& motors) {
  std::vector<int> result;
  int start = 0, end = 0;
  while ((end = motors.indexOf(',', start)) != -1) {
    int val = motors.substring(start, end).toInt();
    if (val >= 0 && val < 6) result.push_back(val);
    start = end + 1;
  }
  int val = motors.substring(start).toInt();
  if (val >= 0 && val < 6) result.push_back(val);
  return result;
}

// Parse un argument GET
String parseArgument(const String& request, const String& arg) {
  int startIndex = request.indexOf(arg + "=");
  if (startIndex == -1) return "";
  startIndex += arg.length() + 1;
  int endIndex = request.indexOf("&", startIndex);
  if (endIndex == -1) endIndex = request.indexOf(" ", startIndex);
  return request.substring(startIndex, endIndex);
}

String handleCommand(const String& command, const String& motors) {
  Serial.println("Commande reçue: " + command);
  std::vector<int> motArr = parseMotors(motors);
  if (motArr.empty()) return "Aucun moteur sélectionné.";

  int angle = -1;
  if (command == "Up" || command == "360Up") angle = 180;
  else if (command == "Down" || command == "360Down") angle = 0;
  else return "Commande inconnue.";

  // Activation
  for (int motor : motArr) {
    Serial.printf("Activer moteur %d\n", motor);
    board1.setPWM(motorPins[motor], 0, angleToPulse(angle));
  }

  delay(1000); // 1s d’activation

  // Arrêt
  for (int motor : motArr) {
    Serial.printf("Arrêter moteur %d\n", motor);
    board1.setPWM(motorPins[motor], 0, 0);
  }
  String motStr = "";
  for (size_t i = 0; i < motArr.size(); ++i) {
    motStr += String(motArr[i]);
    if (i < motArr.size() - 1) motStr += ",";
  }
  return "Commande " + command + " exécutée sur moteurs : " + motStr;
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("Connexion...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnecté !");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());

  server.begin();
  board1.begin();
  board1.setPWMFreq(60);

  for (int i = 0; i < 6; i++)
    board1.setPWM(motorPins[i], 0, 0);
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();

    if (request.indexOf("GET /command") >= 0) {
      String command = parseArgument(request, "cmd");
      String motors = parseArgument(request, "motors");
      String response = handleCommand(command, motors);
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println();
      client.println(response);
    } else {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.println(webPage);
    }
    delay(1);
    client.stop();
  }
}
