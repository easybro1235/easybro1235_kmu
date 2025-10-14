#include <Servo.h>

// 서보 모터 설정
#define PIN_SERVO 10
Servo myServo;

// 초음파 센서 설정
#define PIN_TRIG 12
#define PIN_ECHO 13

// 차단기 동작 설정
const int ANGLE_DOWN = 10;     // 차단기가 내려간 각도
const int ANGLE_UP = 90;       // 차단기가 올라간 각도
const unsigned long MOVING_TIME = 2000; // 차단기 이동 시간 (2초)
const int DETECTION_DISTANCE = 20; // 차량 감지 거리 (cm)

// 상태 변수
unsigned long moveStartTime; // 움직임 시작 시간
int startAngle;
int targetAngle;
bool isMoving = false;
bool isUp = false;
// 센서 값 안정화를 위한 변수
int detectionCounter = 0; // 차량을 감지한 횟수
int noDetectionCounter = 0; // 차량이 없다고 판단한 횟수
const int CONFIRMATION_COUNT = 3; // 3번 연속으로 확인되면 상태 변경

// Sigmoid 함수를 이용한 각도 계산
float sigmoidEase(float progress, float total_duration, float start, float end) {
  // 진행률(progress)을 sigmoid 곡선에 적합한 -6 ~ 6 범위로 매핑
  float x = map(progress, 0, total_duration, -600, 600) / 100.0;
  // 시그모이드 함수 계산: 1 / (1 + e^-x)
  float sigmoid = 1 / (1 + exp(-x));
  // 계산된 값(0~1)을 시작 각도와 끝 각도 사이의 값으로 변환
  return start + (end - start) * sigmoid;
}

// 초음파 센서 거리 측정 함수
long getDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH);
  // 거리(cm) = 시간 * 0.034 / 2
  return duration * 17 / 1000;
}

void setup() {
  Serial.begin(57600);
  myServo.attach(PIN_SERVO); // 서보 핀 초기화
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  // 초기 상태: 차단기 내리기
  myServo.write(ANGLE_DOWN);
  delay(1000);
}

void loop() {
  // 1. 움직이는 중일 때는 아무것도 하지 않음
  if (isMoving) {
    unsigned long progress = millis() - moveStartTime;

    if (progress <= MOVING_TIME) {
      float angle = sigmoidEase(progress, MOVING_TIME, startAngle, targetAngle);
      myServo.write(angle);
    } else {
      myServo.write(targetAngle);
      isMoving = false;
      isUp = (targetAngle == ANGLE_UP);
      Serial.println(isUp ? "Movement finished: Barrier is UP" : "Movement finished: Barrier is DOWN");
    }
    return;
  }

  // 2. 움직이지 않을 때, 센서 값을 안정적으로 읽기
  long distance = getDistance();
  Serial.print("Distance: ");
  Serial.print(distance);

  bool isCarDetected = (distance > 0 && distance < DETECTION_DISTANCE);

  if (isCarDetected) {
    detectionCounter++;
    noDetectionCounter = 0; // 감지되었으므로 미감지 카운터는 리셋
  } else {
    noDetectionCounter++;
    detectionCounter = 0; // 미감지되었으므로 감지 카운터는 리셋
  }
  
  Serial.print(" / Detection Count: ");
  Serial.print(detectionCounter);
  Serial.print(" / No Detection Count: ");
  Serial.println(noDetectionCounter);


  // 3. 안정화된 값을 바탕으로 차단기 동작 결정
  // 3번 연속으로 차가 감지되고, 차단기가 내려가 있을 때 -> 올리기 시작
  if (detectionCounter >= CONFIRMATION_COUNT && !isUp) {
    Serial.println("Car confirmed! Opening barrier...");
    startAngle = ANGLE_DOWN;
    targetAngle = ANGLE_UP;
    moveStartTime = millis();
    isMoving = true;
    detectionCounter = 0; // 동작 후 카운터 리셋
  }
  // 3번 연속으로 차가 사라지고, 차단기가 올라가 있을 때 -> 내리기 시작
  else if (noDetectionCounter >= CONFIRMATION_COUNT && isUp) {
    Serial.println("Car has passed confirmed! Closing barrier...");
    startAngle = ANGLE_UP;
    targetAngle = ANGLE_DOWN;
    moveStartTime = millis();
    isMoving = true;
    noDetectionCounter = 0; // 동작 후 카운터 리셋
  }

  delay(50); // 너무 빠른 반복을 막기 위한 약간의 딜레이
}
