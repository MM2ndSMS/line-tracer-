#include <stdio.h> // 표준 입출력 라이브러리 포함
#include <signal.h> // 시그널 처리를 위한 라이브러리 포함
#include <stdlib.h> // 표준 라이브러리 포함
#include <string.h> // 문자열 처리를 위한 라이브러리 포함
#include <wiringPi.h> // GPIO 제어를 위한 WiringPi 라이브러리 포함
#include <wiringSerial.h> // 시리얼 통신을 위한 WiringPi 라이브러리 포함
#include <termio.h> // 터미널 I/O를 위한 라이브러리 포함
#include <softPwm.h> // 소프트웨어 PWM을 위한 WiringPi 라이브러리 포함
#include <opencv2/opencv.hpp> // OpenCV 라이브러리 포함
#include <iostream> // C++ 표준 입출력 라이브러리 포함

using namespace cv; // OpenCV 네임스페이스 사용
using namespace std; // 표준 네임스페이스 사용

#define IMG_Width 640 // 이미지 가로 크기 정의
#define IMG_Height 480 // 이미지 세로 크기 정의

#define ASSIST_BASE_LINE 320 // 보조 기준선 위치 정의
#define ASSIST_BASE_WIDTH 60 // 보조 기준선 너비 정의

int guide_width1 = 50; // 가이드 라인의 너비 설정
int guide_height1 = 20; // 가이드 라인의 높이 설정
int guide_center = IMG_Width / 2; // 가이드 라인의 중심 설정

#define ENA 6 // 모터 ENA 핀 정의
#define IN1 4 // 모터 IN1 핀 정의
#define IN2 5 // 모터 IN2 핀 정의
#define ENB 0 // 모터 ENB 핀 정의
#define IN3 2 // 모터 IN3 핀 정의
#define IN4 3 // 모터 IN4 핀 정의

#define MAX_PWM_DUTY 100 // 최대 PWM 듀티 사이클 정의

#define TRIG 21 // 초음파 센서 TRIG 핀 정의
#define ECHO 22 // 초음파 센서 ECHO 핀 정의

#define baud_rate 115200 // 시리얼 통신 속도 정의

void sig_Handler(int sig); // 시그널 핸들러 함수 선언

int getch(void) // 키 입력을 비차단 방식으로 받는 함수
{
    int ch; // 키 입력 값을 저장할 변수
    struct termios buf; // 터미널 I/O 설정을 위한 구조체
    struct termios save; // 기존 터미널 설정을 저장할 구조체

    tcgetattr(0, &save); // 현재 터미널 설정 저장
    buf = save; // 기존 설정을 buf에 복사
    buf.c_lflag &= ~(ICANON | ECHO); // ICANON과 ECHO 비활성화
    buf.c_cc[VMIN] = 1; // 최소 입력 문자 수 설정
    buf.c_cc[VTIME] = 0; // 읽기 대기 시간 설정
    tcsetattr(0, TCSAFLUSH, &buf); // 새로운 설정 적용
    ch = getchar(); // 키 입력 받기
    tcsetattr(0, TCSAFLUSH, &save); // 원래 설정 복원
    return ch; // 입력된 키 반환
}

int GPIO_control_setup(void) // GPIO 제어 설정 함수
{
    if (wiringPiSetup() == -1) // wiringPi 초기화 실패 시
    {
        printf("wiringPi 설정 에러!\n"); // 에러 메시지 출력
        return -1; // 오류 코드 반환
    }

    pinMode(ENA, OUTPUT); // ENA 핀을 출력 모드로 설정
    pinMode(IN1, OUTPUT); // IN1 핀을 출력 모드로 설정
    pinMode(IN2, OUTPUT); // IN2 핀을 출력 모드로 설정
    pinMode(ENB, OUTPUT); // ENB 핀을 출력 모드로 설정
    pinMode(IN3, OUTPUT); // IN3 핀을 출력 모드로 설정
    pinMode(IN4, OUTPUT); // IN4 핀을 출력 모드로 설정
    pinMode(TRIG, OUTPUT); // TRIG 핀을 출력 모드로 설정
    pinMode(ECHO, INPUT); // ECHO 핀을 입력 모드로 설정

    softPwmCreate(ENA, 1, MAX_PWM_DUTY); // ENA 핀에 소프트 PWM 생성
    softPwmCreate(ENB, 1, MAX_PWM_DUTY); // ENB 핀에 소프트 PWM 생성

    softPwmWrite(ENA, 0); // ENA 핀의 PWM 초기값 설정
    softPwmWrite(ENB, 0); // ENB 핀의 PWM 초기값 설정
    return 0; // 설정 완료
}

void motor_control_r(int pwm) // 오른쪽 모터 제어 함수
{
    if (pwm > 0) // PWM 값이 양수일 때
    {
        digitalWrite(IN1, LOW); // IN1 핀 LOW 설정
        digitalWrite(IN2, HIGH); // IN2 핀 HIGH 설정
        digitalWrite(ENA, pwm); // ENA 핀에 PWM 값 설정
    }
    else if (pwm == 0) // PWM 값이 0일 때
    {
        digitalWrite(IN1, LOW); // IN1 핀 LOW 설정
        digitalWrite(IN2, LOW); // IN2 핀 LOW 설정
        digitalWrite(ENA, 0); // ENA 핀 0 설정
    }
    else // PWM 값이 음수일 때
    {
        digitalWrite(IN1, HIGH); // IN1 핀 HIGH 설정
        digitalWrite(IN2, LOW); // IN2 핀 LOW 설정
        softPwmWrite(ENA, -pwm); // ENA 핀에 음수 PWM 값 설정
    }
}

void motor_control_l(int pwm) // 왼쪽 모터 제어 함수
{
    if (pwm > 0) // PWM 값이 양수일 때
    {
        digitalWrite(IN3, LOW); // IN3 핀 LOW 설정
        digitalWrite(IN4, HIGH); // IN4 핀 HIGH 설정
        digitalWrite(ENB, pwm); // ENB 핀에 PWM 값 설정
    }
    else if (pwm == 0) // PWM 값이 0일 때
    {
        digitalWrite(IN3, LOW); // IN3 핀 LOW 설정
        digitalWrite(IN4, LOW); // IN4 핀 LOW 설정
        digitalWrite(ENB, 0); // ENB 핀 0 설정
    }
    else // PWM 값이 음수일 때
    {
        digitalWrite(IN3, HIGH); // IN3 핀 HIGH 설정
        digitalWrite(IN4, LOW); // IN4 핀 LOW 설정
        softPwmWrite(ENB, -pwm); // ENB 핀에 음수 PWM 값 설정
    }
}

float ultrasonic_sensor(void) // 초음파 센서 거리 측정 함수
{
    long start_time, end_time; // 측정 시작 및 종료 시간 변수
    long temp_time1, temp_time2; // 임시 시간 변수
    int duration; // 신호 지속 시간 변수
    float distance; // 거리 변수

    digitalWrite(TRIG, LOW); // TRIG 핀 LOW 설정
    delayMicroseconds(5); // 5 마이크로초 대기
    digitalWrite(TRIG, HIGH); // TRIG 핀 HIGH 설정
    delayMicroseconds(10); // 10 마이크로초 대기
    digitalWrite(TRIG, LOW); // TRIG 핀 LOW 설정

    delayMicroseconds(200); // 200 마이크로초 대기

    temp_time1 = micros(); // 현재 시간 기록

    while (digitalRead(ECHO) == LOW) // ECHO 핀이 HIGH가 될 때까지 대기
    {
        temp_time2 = micros(); // 현재 시간 기록
        duration = temp_time2 - temp_time1; // 신호 지속 시간 계산
        if (duration > 1000) return -1; // 타임아웃 시 -1 반환
    }

    start_time = micros(); // 시작 시간 기록

    while (digitalRead(ECHO) == HIGH) // ECHO 핀이 LOW가 될 때까지 대기
    {
        temp_time2 = micros(); // 현재 시간 기록
        duration = temp_time2 - temp_time1; // 신호 지속 시간 계산
        if (duration > 2000) return -1; // 타임아웃 시 -1 반환
    }
    end_time = micros(); // 종료 시간 기록

    duration = end_time - start_time; // 신호 지속 시간 계산
    distance = duration / 58; // 거리 계산

    return distance; // 계산된 거리 반환
}

Mat Canny_Edge_Detection(Mat img) // 캐니 엣지 검출 함수
{
    Mat mat_blur_img, mat_canny_img; // 블러 이미지와 캐니 이미지 변수
    blur(img, mat_blur_img, Size(3,3)); // 블러 처리
    Canny(mat_blur_img, mat_canny_img, 270, 170, 3); // 캐니 엣지 검출

    return mat_canny_img; // 캐니 엣지 검출 이미지 반환
}

Mat Draw_Guide_Line(Mat img) // 가이드 라인 그리기 함수
{
    Mat result_img; // 결과 이미지 변수
    img.copyTo(result_img); // 원본 이미지를 결과 이미지로 복사
    rectangle(result_img, Point(50, ASSIST_BASE_LINE - ASSIST_BASE_WIDTH), Point(IMG_Width - 50, ASSIST_BASE_LINE + ASSIST_BASE_WIDTH), Scalar(0, 255, 0), 1, LINE_AA); // 보조 라인 그리기

    line(result_img, Point(guide_center - guide_width1, ASSIST_BASE_LINE), Point(guide_center, ASSIST_BASE_LINE), Scalar(0, 255, 255), 1, 0); // 가이드 라인 왼쪽 부분 그리기
    line(result_img, Point(guide_center, ASSIST_BASE_LINE), Point(guide_center + guide_width1, ASSIST_BASE_LINE), Scalar(0, 255, 255), 1, 0); // 가이드 라인 오른쪽 부분 그리기

    line(result_img, Point(guide_center - guide_width1, ASSIST_BASE_LINE - guide_height1), Point(guide_center - guide_width1, ASSIST_BASE_LINE + guide_height1), Scalar(0, 255, 255), 1, 0); // 가이드 라인 왼쪽 세로선 그리기
    line(result_img, Point(guide_center + guide_width1, ASSIST_BASE_LINE - guide_height1), Point(guide_center + guide_width1, ASSIST_BASE_LINE + guide_height1), Scalar(0, 255, 255), 1, 0); // 가이드 라인 오른쪽 세로선 그리기

    line(img, Point(IMG_Width / 2, ASSIST_BASE_LINE - guide_height1 * 1), Point(IMG_Width / 2, ASSIST_BASE_LINE + guide_height1 * 1), Scalar(255, 255, 255), 2, 0); // 중앙 라인 그리기

    return result_img; // 결과 이미지 반환
}

int main(void) // 메인 함수
{
    int fd; // 파일 디스크립터 변수
    int pwm_r = 0; // 오른쪽 모터 PWM 변수
    int pwm_l = 0; // 왼쪽 모터 PWM 변수
    unsigned char test; // 테스트 변수

    int img_width, img_height; // 이미지 너비와 높이 변수

    img_width = 640; // 이미지 너비 설정
    img_height = 480; // 이미지 높이 설정

    Mat mat_image_org_color; // 원본 컬러 이미지 변수
    Mat mat_image_org_color_Overlay; // 오버레이 이미지 변수
    Mat mat_image_org_gray; // 그레이스케일 이미지 변수
    Mat mat_image_org_gary_result; // 결과 그레이스케일 이미지 변수
    Mat mat_image_canny_edge; // 캐니 엣지 이미지 변수
    Mat mat_image_line_image = Mat(IMG_Height, IMG_Width, CV_8UC1, Scalar(0)); // 라인 이미지 변수
    Mat image; // 임시 이미지 변수

    Scalar GREEN(0, 255, 0); // 녹색 스칼라 정의
    Scalar RED(0, 0, 255); // 빨간색 스칼라 정의
    Scalar BLUE(255, 0, 0); // 파란색 스칼라 정의
    Scalar YELLOW(0, 255, 255); // 노란색 스칼라 정의

    mat_image_org_color = imread("/home/pi/Team_aMAP/Line_Tracer/1/line_1.jpg"); // 이미지 파일 읽기
    mat_image_org_color.copyTo(mat_image_org_color_Overlay); // 오버레이 이미지로 복사

    if (mat_image_org_color.empty()) // 이미지 파일이 비어 있을 경우
    {
        cerr << "비어있는 비디오\n"; // 오류 메시지 출력
        return -1; // 오류 코드 반환
    }

    img_width = mat_image_org_color.size().width; // 이미지 너비 설정
    img_height = mat_image_org_color.size().height; // 이미지 높이 설정

    namedWindow("Display window", CV_WINDOW_NORMAL); // 디스플레이 창 생성
    resizeWindow("Display window", img_width, img_height); // 디스플레이 창 크기 조정
    moveWindow("Display window", 10, 10); // 디스플레이 창 위치 이동

    namedWindow("Gray Image window", CV_WINDOW_NORMAL); // 그레이 이미지 창 생성
    resizeWindow("Gray Image window", img_width, img_height); // 그레이 이미지 창 크기 조정
    moveWindow("Gray Image window", 700, 10); // 그레이 이미지 창 위치 이동

    namedWindow("Canny Edge Image window", CV_WINDOW_NORMAL); // 캐니 엣지 이미지 창 생성
    resizeWindow("Canny Edge Image window", img_width, img_height); // 캐니 엣지 이미지 창 크기 조정
    moveWindow("Canny Edge Image window", 10, 500); // 캐니 엣지 이미지 창 위치 이동

    if (GPIO_control_setup() == -1) // GPIO 설정 실패 시
    {
        return -1; // 오류 코드 반환
    }

    signal(SIGINT, sig_Handler); // 시그널 핸들러 설정
    test = 'B'; // 테스트 변수 초기화

    while (1) // 무한 루프
    {
        cvtColor(mat_image_org_color, mat_image_org_gray, CV_RGB2GRAY); // 컬러 이미지를 그레이스케일로 변환
        threshold(mat_image_org_gray, mat_image_canny_edge, 200, 255, THRESH_BINARY); // 이진화 처리
        mat_image_canny_edge = Canny_Edge_Detection(mat_image_org_gray); // 캐니 엣지 검출

        mat_image_org_color_Overlay = Draw_Guide_Line(mat_image_org_color); // 가이드 라인 그리기

        vector<Vec4i> linesP; // 선 벡터 변수
        HoughLinesP(mat_image_canny_edge, linesP, 1, CV_PI / 180, 35, 40, 100); // 허프 변환을 통해 선 검출
        printf("라인 수 : %3d\n", linesP.size()); // 검출된 선 수 출력
        for (int i = 0; i < linesP.size(); i++) // 검출된 선들에 대해 반복
        {
            Vec4i L = linesP[i]; // 선 벡터 요소
            line(mat_image_line_image, Point(L[0], L[1]), Point(L[2], L[3]), Scalar(255), 3, LINE_AA); // 라인 이미지에 선 그리기
            line(mat_image_org_color_Overlay, Point(L[0], L[1]), Point(L[2], L[3]), Scalar(0, 0, 255), 3, LINE_AA); // 오버레이 이미지에 선 그리기
            printf("L :[%3d, %3d], [%3d, %3d]\n", L[0], L[1], L[2], L[3]); // 선 좌표 출력
        }
        printf("\n\n\n"); // 줄바꿈 출력

        imshow("Display window", mat_image_org_color_Overlay); // 디스플레이 창에 오버레이 이미지 출력
        imshow("Gray Image window", mat_image_line_image); // 그레이 이미지 창에 라인 이미지 출력
        imshow("Canny Edge Image window", mat_image_canny_edge); // 캐니 엣지 이미지 창에 캐니 엣지 이미지 출력

        if (waitKey(10) > 0) // 키 입력 대기
            break; // 루프 종료
    }

    return 0; // 프로그램 종료
}

void sig_Handler(int sig) // 시그널 핸들러 함수
{
    printf("\n\n\n\n프로그램 및 모터 정지\n\n\n"); // 메시지 출력
    motor_control_r(0); // 오른쪽 모터 정지
    motor_control_l(0); // 왼쪽 모터 정지
    exit(0); // 프로그램 종료
}
