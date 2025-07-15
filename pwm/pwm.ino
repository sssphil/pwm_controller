#include <WiFi.h>
#include <WebServer.h>
#include <Arduino.h>
#include "Arduino.h"


/* avaialble param combinations for Xiao esp32c6
https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/AnalogOut/ledcFrequency/ledcFrequency.ino
Bit resolution | Min Frequency [Hz] | Max Frequency [Hz]
             1 |              19532 |           20039138
             2 |               9766 |           10019569
             3 |               4883 |            5009784
             4 |               2442 |            2504892
             5 |               1221 |            1252446
             6 |                611 |             626223
             7 |                306 |             313111
             8 |                153 |             156555
             9 |                 77 |              78277
            10 |                 39 |              39138
            11 |                 20 |              19569
            12 |                 10 |               9784
            13 |                  5 |               4892
            14 |                  3 |               2446
            15 |                  2 |               1223
            16 |                  1 |                611
            17 |                  1 |                305
            18 |                  1 |                152
            19 |                  1 |                 76
            20 |                  1 |                 38
*/

// === Wi-Fi Config ===
const char* ssid = "ESP32C6_PWM";
const char* password = "12345678";

// === PWM Config ===
int pwmPin = 15; // 15 for led
// const int maxDuty = (1 << resolution) - 1;
int pwmFreq = 1;
int pwmReso = 16;
int pwmDuty = 32768;
float pwmWidth = 1.e6 / pwmFreq / (1 << pwmReso) * pwmDuty; // pulse width 


// === Web Server ===
WebServer server(80);

// === HTML ===
String getHTML()
{
  const char* head = R"rawliteral(
    <!DOCTYPE html><html><body>
    <h2>ESP32C6 PWM Control</h2>
  )rawliteral";

  const char* tail = R"rawliteral(
    </body></html>
  )rawliteral";


  String html = "<form action=\"/set_pwm\" method=\"get\">\n";
  
  html += "  <label for=\"pin\">pin:</label>\n";
  html += "  <select id=\"pin\" name=\"pin\">\n";

  int pins[] = {0, 1, 2, 21, 22, 23, 16, 17,19, 20, 18, 15};
  String labels[12];
  for (int i=0; i < 11; i++)
  {
    labels[i] = "D" + String(i) + "(" + String(pins[i]) + ")";
  }
  labels[11] = "LED(15)";

  for (int i = 0; i < 12; i++)
  {
    html += "<option value=\"" + String(pins[i]) + "\"";
    if (pwmPin == pins[i]) html += " selected";
    html += ">" + labels[i] + "</option>";
  }
  html += "    </select>\n";


  html += "  <label for=\"value\">Frequncy:</label><br>\n";
  html += "  <input type=\"number\" id=\"freq\" name=\"freq\" min=\"0\" step=\"1\" value=\"" + String(pwmFreq) + "\"><br><br>\n";

  html += "  <label for=\"value\">Resolution:</label><br>\n";
  html += "  <input type=\"number\" id=\"reso\" name=\"reso\" min=\"0\" step=\"1\" value=\"" + String(pwmReso) + "\"><br><br>\n";

  html += "  <label for=\"value\">Width(us):</label><br>\n";
  html += "  <button type=\"submit\" name=\"action\" value=\"width\">Configure by Width</button>\n";
  html += "  <input type=\"number\" id=\"width\" name=\"width\" min=\"0\" value=\"" + String(pwmWidth) + "\"><br><br>\n";

  html += "  <label for=\"value\">Duty:</label><br>\n";
  html += "  <button type=\"submit\" name=\"action\" value=\"duty\">Configure by Duty</button>\n";
  html += "  <input type=\"number\" id=\"duty\" name=\"duty\" min=\"0\" step=\"1\" value=\"" + String(pwmDuty) + "\"><br><br>\n";

  html += "</form>";

  html += "<p>Tick: " + String(1000000. / pwmFreq / (1 << pwmReso)) + " us</p>";


  return head + html + tail;
}

// === PWM Setup ===
void setupPWM() {
  Serial.printf("Setting up (Pin, Freq, Reso, Duty): (%d, %d Hz, %d, %d)\n", pwmPin, pwmFreq, pwmReso, pwmDuty);
  bool res1 = ledcChangeFrequency(pwmPin, pwmFreq, pwmReso); // auto-assign channel, or use ledcAttachChannel(,,,)
  if (res1)
  {
    Serial.print("ledcAttach(,,) Success. ");
    bool res2 = ledcWrite(pwmPin, pwmDuty);
    if (res2)
    {
      Serial.println("ledcWrite(,) Success.");
      Serial.printf("Success (Pin, Freq, Reso, Duty): (%d, %d Hz, %d, %d)\n", pwmPin, pwmFreq, pwmReso, pwmDuty);
      return;
    }
    else
      Serial.println("ledcWrite(,) Failure.");
  }
  else
    Serial.println("ledcAttach(,,) Failure. ledcWrite(,) Skipped.");
  Serial.printf("Failure (Pin, Freq, Reso, Duty): (%d, %d Hz, %d, %d)\n", pwmPin, pwmFreq, pwmReso, pwmDuty);
}

void changeDuty(int delta) {
  pwmDuty += delta;
  int maxDuty = (1 <<pwmReso) - 1;
  if (pwmDuty > maxDuty) pwmDuty = maxDuty;
  if (pwmDuty < 0) pwmDuty = 0;
  ledcWrite(pwmPin, pwmDuty);
  Serial.printf("Duty: %d / %d\n", pwmDuty, maxDuty);
}

// === Handlers ===
void handleRoot() {
  String html = getHTML();
  server.send(200, "text/html", html);
}

void handleServer() {
  // pin update
  int pin = server.arg("pin").toInt();
  if (pin != pwmPin)
  {
    Serial.println("Getting new Pin: GPIO" + String(pwmPin));
    if (!ledcDetach(pwmPin)) 
    {
      Serial.println("Detaching current pin failed: GPIO" + String(pwmPin));
    }
    else
    {
      Serial.println("Succesfully detached: GPIO" + String(pwmPin));
      pwmPin = pin;
      if (!ledcAttach(pwmPin, pwmFreq, pwmReso))
      {
        Serial.println("Failed to attach: GPIO" + String(pwmPin));
      }
      else
        Serial.println("Succesfully attached: GPIO" + String(pwmPin));
    }
  }

  // pwm params update
  String action = server.arg("action");
  
  pwmFreq = server.arg("freq").toInt();
  pwmReso = server.arg("reso").toInt();

  if (pwmReso > 20) pwmReso = 20;
  if (pwmReso < 1) pwmReso = 1;

  if (action == "width")
  {
    pwmWidth = server.arg("width").toInt();
    // calc duty from width
    pwmDuty = int(pwmWidth / 1.e6 * pwmFreq * (1 << pwmReso));
    // quantization error
    pwmWidth = 1.e6 / pwmFreq / (1 << pwmReso) * pwmDuty;
  }
  else
  {
      // use duty from direct input
      pwmDuty = server.arg("duty").toInt();
      int maxDuty = (1 <<pwmReso);
      if (pwmDuty > maxDuty) pwmDuty = maxDuty;
      if (pwmDuty < 0) pwmDuty = 0;
      pwmWidth = 1.e6 / pwmFreq / (1 << pwmReso) * pwmDuty;
  }
    
  setupPWM();

  handleRoot();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Wi-Fi Access Point
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Web server routes
  server.on("/", handleRoot);
  // server.on("/set_freq", handleSetFreq);
  // server.on("/set_duty", handleSetDuty);
  server.on("/set_pwm", handleServer);
  server.begin();

  // Initial PWM
  ledcAttach(pwmPin, pwmFreq, pwmReso);
  ledcWrite(pwmPin, pwmDuty);
  setupPWM();
}

void loop() {
  server.handleClient();

  //TODO input could use '30h' for hz and '60u' for us
  if (0 && Serial.available()) {
    char c = Serial.read();
    if (c == '1') {
      pwmFreq = 5;
      setupPWM();
      Serial.println("Serial: Set freq to 5 Hz");
    } else if (c == '2') {
      pwmFreq = 15;
      setupPWM();
      Serial.println("Serial: Set freq to 15 Hz");
    } else if (c == '3') {
      pwmFreq = 30;
      setupPWM();
      Serial.println("Serial: Set freq to 30 Hz");
    } else if (c == '+') {
      changeDuty(1);
    } else if (c == '-') {
      changeDuty(-1);
    }
  }
}
