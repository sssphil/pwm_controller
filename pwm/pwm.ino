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
bool pwmInv = true;

// === Web Server ===
WebServer server(80);

// === HTML === size=1357
char html[1500];
const char* html_format = R"rawliteral(
<!DOCTYPE html><html><body>
<h2>ESP32C6 PWM Control</h2>
<form action="/set_pwm" method="get">
<label for="pin">pin:</label>
<select id="pin" name="pin">
<option value="0" %s>D0(0)</option>
<option value="1" %s>D1(1)</option>
<option value="2" %s>D2(2)</option>
<option value="21" %s>D3(21)</option>
<option value="22" %s>D4(22)</option>
<option value="23" %s>D5(23)</option>
<option value="16" %s>D6(16)</option>
<option value="17" %s>D7(17)</option>
<option value="19" %s>D8(19)</option>
<option value="20" %s>D9(20)</option>
<option value="18" %s>D10(18)</option>
<option value="15" %s>LED(15)</option>
</select><br>
<input type="checkbox" id="flag" name="flag" %s/>
<label for="flag">Invert output</label><br>
<label for="value">Frequncy:</label>
<input type="number" id="freq" name="freq" min="0" step="1" value="%d"/><br>
<label for="value">Resolution:</label>
<input type="number" id="reso" name="reso" min="0" step="1" value="%d"/><br>
<label for="value">Duty:</label>
<input type="number" id="duty" name="duty" min="0" step="1" value="%d"/>
<button type="submit" name="action" value="duty">Configure by Duty</button><br>
<label for="value">Width(us):</label>
<input type="number" id="width" name="width" min="0" step="any" value="%f"/>
<button type="submit" name="action" value="width">Configure by Width</button><br>
<p>Tick:%fus</p>
<p>High,Low:(%f, %f)us</p>
<p>%s</p>
</form>
</body></html>
)rawliteral";

const char pin_options[] = {0, 1, 2, 21, 22, 23, 16, 17, 19, 20, 18, 15};

void updateHTML()
{
  bool selection[12] = {0};
  char defaults[12][8];
  for (int i = 0; i < 12; i ++)
  {
    strcpy(defaults[i], pin_options[i] == pwmPin ? "selected" : "");
  }

  float tick = 1.e6 / pwmFreq / (1 << pwmReso);

  sprintf(html, html_format,
    defaults[0], defaults[1], defaults[2], defaults[3], defaults[4], defaults[5], defaults[6],
    defaults[7], defaults[8], defaults[9], defaults[10], defaults[11],
    pwmInv ? "checked" : "", pwmFreq, pwmReso, pwmDuty, pwmWidth,
    tick, tick * pwmDuty, tick * ((1 << pwmReso) - pwmDuty),
    pwmInv ? "Output INVERTED" : "");
}

// === PWM Setup ===
void setupPWM() {
  Serial.printf("[setupPWM] Setting up (Pin, Freq, Reso, Duty): (%d, %d Hz, %d, %d)\n", pwmPin, pwmFreq, pwmReso, pwmDuty);
  bool res1 = ledcChangeFrequency(pwmPin, pwmFreq, pwmReso); // auto-assign channel, or use ledcAttachChannel(,,,)
  if (res1)
  {
    Serial.println("\tledcAttach succeeded");
    if (ledcOutputInvert(pwmPin, pwmInv) && pwmInv) Serial.println("Output INVERTED");
    bool res2 = ledcWrite(pwmPin, pwmDuty);
    if (res2)
    {
      Serial.println("\tledcWrite succeeded.");
      Serial.printf("[setupPWM] Success (Pin, Freq, Reso, Duty): (%d, %d Hz, %d, %d)\n", pwmPin, pwmFreq, pwmReso, pwmDuty);
      return;
    }
    else
      Serial.println("\tledcWrite FAILED");
  }
  else
    Serial.println("\tledcAttach FAILED");
  Serial.printf("[setupPWM] FAILURE (Pin, Freq, Reso, Duty): (%d, %d Hz, %d, %d)\n", pwmPin, pwmFreq, pwmReso, pwmDuty);
}

// === Handlers ===
void handleRoot() {
  updateHTML();
  server.send(200, "text/html", html);
}

void handleServer() {
  // pin update
  int pin = server.arg("pin").toInt();
  if (pin != pwmPin)
  {
    Serial.printf("[handleServer] Switching pwm pin: GPIO%d -> GPIO%d\n", pwmPin, pin);
    if (!ledcDetach(pwmPin)) 
    {
      Serial.println("\tDetaching current GPIO FAILED");
    }
    else
    {
      Serial.println("\tSuccesfully detached current GPIO");
      if (pwmPin == 15) // turn LED off
      {
        digitalWrite(pwmPin, LOW);
      }

      if (ledcAttach(pin, pwmFreq, pwmReso))
      {
        pwmPin = pin;
        Serial.printf("[handleServer] Succeeded to set pwm on GPIO%d\n", pwmPin);
      }
      else
        Serial.printf("[handleServer] FAILED to set pwm on GPIO%d\n", pwmPin);
    }
  }

  // pwm params update
  if (server.hasArg("flag")) pwmInv = true;
    else pwmInv = false;

  String action = server.arg("action"); // configuration button by width or duty
  
  pwmFreq = server.arg("freq").toInt();
  pwmReso = server.arg("reso").toInt();

  if (pwmReso > 20) pwmReso = 20;
  if (pwmReso < 1) pwmReso = 1;

  if (action == "width")
  {
    pwmWidth = server.arg("width").toInt();
    // calc duty from width
    pwmDuty = int(pwmWidth / 1.e6 * pwmFreq * (1 << pwmReso));
  }
  else
  {
      // use duty from direct input
      pwmDuty = server.arg("duty").toInt();
      int maxDuty = (1 <<pwmReso);
      if (pwmDuty > maxDuty) pwmDuty = maxDuty;
      if (pwmDuty < 0) pwmDuty = 0;
  }
  // update width (even when using width due to quantization error)
  pwmWidth = 1.e6 / pwmFreq / (1 << pwmReso) * pwmDuty;
    
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
  server.on("/set_pwm", handleServer);
  server.begin();

  // Initial PWM
  ledcAttach(pwmPin, pwmFreq, pwmReso);
  ledcWrite(pwmPin, pwmDuty);
}

void loop() {
  server.handleClient();
}
