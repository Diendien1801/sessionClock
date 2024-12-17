#include <TM1637Display.h>
#include <Arduino.h>

// Khai báo các chân kết nối
#define TM1637_CLK 12
#define TM1637_DIO 14
#define ROTARY_CLK 18
#define ROTARY_DT 19
#define ROTARY_SW 21
#define BUTTON_RED 32
#define SELECT_SWITCH 33
#define BUZZER 25
#define BUTTON_STOP 27

// Khai báo màn hình LED 7 đoạn
TM1637Display display(TM1637_CLK, TM1637_DIO);

// Biến lưu trữ giá trị
int value = 1;
int lastStateCLK;
int currentStateCLK;
int lastButtonState = HIGH;
int lastStopButtonState = HIGH;
bool counting = false;
bool paused = false;
bool stopped = false;
bool breakTimeCounting = false;

// Các biến lưu trữ giá trị cho các chế độ
int sessionCount = 1;
int studyTime = 30;
int breakTime = 10;
int mode = 1; // 1: Chọn số lượng session, 2: Thời gian học, 3: Thời gian nghỉ
int currentSession = 0;
int remainingTime = 0; // Thời gian còn lại cho quá trình đếm ngược

// Biến theo dõi thời gian bấm giữ nút đỏ
unsigned long redButtonPressTime = 0;
bool redButtonHeld = false;

// Biến theo dõi thời gian bấm giữ nút STOP
unsigned long stopButtonPressTime = 0;
bool stopButtonHeld = false;

// Biến theo dõi thời gian bấm giữ nút SELECT
unsigned long selectButtonPressTime = 0;
bool selectButtonHeld = false;

void setup() {
  Serial.begin(115200);

  // Cấu hình các chân GPIO
  pinMode(ROTARY_CLK, INPUT);
  pinMode(ROTARY_DT, INPUT);
  pinMode(ROTARY_SW, INPUT_PULLUP);
  pinMode(BUTTON_RED, INPUT_PULLUP);
  pinMode(SELECT_SWITCH, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON_STOP, INPUT_PULLUP);

  // Thiết lập độ sáng màn hình
  display.setBrightness(0x0f);

  // Hiển thị giá trị ban đầu
  displayModeAndValue(mode, sessionCount);

  // Đọc trạng thái ban đầu của chân CLK
  lastStateCLK = digitalRead(ROTARY_CLK);

  Serial.println("Setup complete");
}

void loop() {
  currentStateCLK = digitalRead(ROTARY_CLK);
  int currentButtonState = digitalRead(SELECT_SWITCH);
  int redButtonState = digitalRead(BUTTON_RED);
  int stopButtonState = digitalRead(BUTTON_STOP);

  // Kiểm tra trạng thái bấm giữ nút đỏ
  if (redButtonState == LOW) {
    if (!redButtonHeld) {
      if (redButtonPressTime == 0) {
        redButtonPressTime = millis();
      } else if (millis() - redButtonPressTime >= 2000) {
        // Nếu bấm giữ trong 2 giây, reset thời gian
        redButtonHeld = true;
        if (breakTimeCounting) {
          remainingTime = breakTime * 60; // Reset thời gian chơi
          Serial.println("Break time reset");
        } else {
          remainingTime = studyTime * 60; // Reset thời gian học
          Serial.println("Study time reset");
        }
        displayTime(remainingTime);
      }
    }
  } else {
    redButtonPressTime = 0;
    redButtonHeld = false;
  }

  // Kiểm tra nút đỏ (bắt đầu chu trình đếm ngược)
  if (redButtonState == LOW && !counting && !redButtonHeld) {
    counting = true;
    paused = false;
    currentSession = 1;
    remainingTime = studyTime * 60; // Đổi sang giây
    Serial.println("Started counting");
  }

  // Xử lý nút STOP (bấm giữ 2 giây để dừng/tiếp tục)
  if (stopButtonState == LOW) {
    if (!stopButtonHeld) {
      if (stopButtonPressTime == 0) {
        stopButtonPressTime = millis();
      } else if (millis() - stopButtonPressTime >= 2000) {
        stopButtonHeld = true;
        if (counting) {
          paused = !paused;
          Serial.println(paused ? "Timer paused" : "Timer resumed");
        }
      }
    }
  } else {
    stopButtonPressTime = 0;
    stopButtonHeld = false;
  }

  // Nếu đang trong quá trình đếm ngược và không bị tạm dừng
  if (counting && !paused) {
    if (remainingTime > 0) {
      delay(1000); // Đếm giây
      remainingTime--;
      displayTime(remainingTime);
    } else {
      // Nếu hết thời gian
      buzz();

      if (breakTimeCounting) {
        // Kết thúc thời gian nghỉ
        breakTimeCounting = false;
        currentSession++;
        if (currentSession > sessionCount) {
          counting = false; // Kết thúc toàn bộ chu trình
          display.showNumberDecEx(0xFFFF, 0, true); // Hiển thị --:--
          Serial.println("All sessions completed");
        } else {
          // Bắt đầu lại thời gian học
          remainingTime = studyTime * 60;
          Serial.print("Starting study time for session: ");
          Serial.println(currentSession);
        }
      } else {
        // Kết thúc thời gian học, bắt đầu thời gian nghỉ
        breakTimeCounting = true;
        remainingTime = breakTime * 60;
        Serial.println("Starting break time");
      }
    }
  }

  // Chuyển đổi chế độ bằng nút SELECT
if (currentButtonState == LOW && lastButtonState == HIGH) {
    if (counting) {
        // Dừng chế độ đếm ngược và reset thời gian
        counting = false;
        paused = false;
        breakTimeCounting = false;
        currentSession = 0;
        remainingTime = 0;
        Serial.println("Countdown stopped and reset for mode selection");
    }

    // Lưu giá trị hiện tại trước khi chuyển chế độ
    switch (mode) {
      case 1:
        sessionCount = value;
        break;
      case 2:
        studyTime = value;
        break;
      case 3:
        breakTime = value;
        break;
    }

    // Thay đổi chế độ
    mode++;
    if (mode > 3) mode = 1;

    // Tải giá trị tương ứng của chế độ mới
    switch (mode) {
      case 1:
        value = sessionCount;
        break;
      case 2:
        value = studyTime;
        break;
      case 3:
        value = breakTime;
        break;
    }

    // Hiển thị giá trị mới
    displayModeAndValue(mode, value);
    Serial.print("Switched to mode: ");
    Serial.println(mode);
    delay(200);
}
  lastButtonState = currentButtonState;

  // Xử lý quay encoder (điều chỉnh giá trị trong từng chế độ)
if (currentStateCLK != lastStateCLK && currentStateCLK == LOW && !counting) {
    if (digitalRead(ROTARY_DT) != currentStateCLK) {
        value += (mode == 2) ? 5 : 1;
    } else {
        value -= (mode == 2) ? 5 : 1;
    }

    // Giới hạn giá trị
    switch (mode) {
      case 1:
        if (value < 1) value = 1;
        if (value > 5) value = 5;
        sessionCount = value; // Cập nhật sessionCount
        break;
      case 2:
        if (value < 5) value = 5;
        if (value > 60) value = 60;
        studyTime = value; // Cập nhật studyTime
        break;
      case 3:
        if (value < 1) value = 1;
        if (value > 30) value = 30;
        breakTime = value; // Cập nhật breakTime
        break;
    }

    // Hiển thị giá trị
    displayModeAndValue(mode, value);
    Serial.print("Mode: ");
    Serial.print(mode);
    Serial.print(" Value: ");
    Serial.println(value);
    delay(200);
}

  lastStateCLK = currentStateCLK;
}

void displayModeAndValue(int mode, int value) {
  uint8_t data[4];
  data[0] = display.encodeDigit(mode);
  data[1] = 0x40; // Dấu ":"
  data[2] = display.encodeDigit(value / 10);
  data[3] = display.encodeDigit(value % 10);
  display.setSegments(data);
}

void displayTime(int seconds) {
  int minutes = seconds / 60;
  int remainingSeconds = seconds % 60;
  display.showNumberDecEx(minutes * 100 + remainingSeconds, 0x40, true, 4, 0);
}

void buzz() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    delay(200);
  }
}
