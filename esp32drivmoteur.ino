#include <WiFi.h>
#include <Adafruit_PWMServoDriver.h>

// Informations Wi-Fi
const char* ssid = "SFR_4FB0";
const char* password = "yyp58vriy4kaw94wuix8";

// Initialisation du pilote PCA9685
Adafruit_PWMServoDriver board1 = Adafruit_PWMServoDriver(0x40);

// Fréquence des moteurs (50 Hz typiquement pour PWM)
#define SERVOMIN  125
#define SERVOMAX  575

// Déclaration des broches des moteurs
const int motorPins[6] = {0, 1, 2, 3, 4, 5}; // Moteurs 0 à 5
WiFiServer server(80); // Serveur web sur le port 80

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
    let selectedMotors = []; // Liste des moteurs sélectionnés

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
        alert("Veuillez sélectionner au moins un moteur.");
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

  <h2>Sélection des moteurs</h2>
  <div>
    <label><input type="checkbox" onchange="toggleMotor('0')"> Moteur 0</label>
    <label><input type="checkbox" onchange="toggleMotor('1')"> Moteur 1</label>
    <label><input type="checkbox" onchange="toggleMotor('2')"> Moteur 2</label>
    <label><input type="checkbox" onchange="toggleMotor('3')"> Moteur 3</label>
    <label><input type="checkbox" onchange="toggleMotor('4')"> Moteur 4</label>
    <label><input type="checkbox" onchange="toggleMotor('5')"> Moteur 5</label>
  </div>

  <p>Moteurs sélectionnés : <span id="selectedMotors">Aucun</span></p>

  <h2>Commandes</h2>
  <div>
    <button onclick="sendCommand('Up')">Marche Avant</button>
    <button onclick="sendCommand('Down')">Marche Arrière</button>
    <button onclick="sendCommand('360Up')">360° Avant</button>
    <button onclick="sendCommand('360Down')">360° Arrière</button>
  </div>

  <p>État actuel : <span id="status">Initialisation...</span></p>
</body>
</html>
)====";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // Connexion Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connexion au Wi-Fi...");
  }
  Serial.println("Connecté au Wi-Fi");
  Serial.println(WiFi.localIP());

  server.begin(); // Démarrage du serveur web
  board1.begin();
  board1.setPWMFreq(60); // Fréquence de 60 Hz pour PWM

  // Initialisation des moteurs
  for (int i = 0; i < 6; i++) {
    board1.setPWM(motorPins[i], 0, 0); // Arrêt initial des moteurs
  }
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

// Convertir un angle en largeur d'impulsion
int angleToPulse(int angle) {
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}

// Analyse des arguments GET
String parseArgument(String request, String arg) {
  int startIndex = request.indexOf(arg + "=");
  if (startIndex == -1) return "";
  startIndex += arg.length() + 1;
  int endIndex = request.indexOf("&", startIndex);
  if (endIndex == -1) endIndex = request.indexOf(" ", startIndex);
  return request.substring(startIndex, endIndex);
}

// Commandes des moteurs
String handleCommand(String command, String motors) {
  Serial.println("Commande reçue: " + command);
  if (motors.length() == 0) {
    Serial.println("Aucun moteur sélectionné");
    return "Aucun moteur sélectionné.";
  }

  int angle = (command == "Up") ? 180 : (command == "Down") ? 0 : -1;
  if (angle == -1) {
    if (command == "360Up") {
      angle = 180; // Rotation avant complète
    } else if (command == "360Down") {
      angle = 0; // Rotation arrière complète
    } else {
      return "Commande inconnue.";
    }
  }

  String response = "Commande " + command + " exécutée sur moteurs : " + motors;
  Serial.println(response);

  // Activation des moteurs sélectionnés
  for (int i = 0; i < motors.length(); i++) {
    int motor = motors.substring(i, i + 1).toInt();
    Serial.print("Activer moteur ");
    Serial.println(motor);
    board1.setPWM(motorPins[motor], 0, angleToPulse(angle));
  }

  // Durée de 1 seconde pour simuler la rotation complète
  delay(1000);

  // Arrêt des moteurs après la durée définie
  for (int i = 0; i < motors.length(); i++) {
    int motor = motors.substring(i, i + 1).toInt();
    Serial.print("Arrêter moteur ");
    Serial.println(motor);
    board1.setPWM(motorPins[motor], 0, 0);
  }

  return response;
} 