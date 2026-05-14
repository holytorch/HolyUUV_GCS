#pragma once

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <array>
#include <cstdint>

class StickPad;
class QPushButton;

// ─────────────────────────────────────────────────────────────────────────────
// JoystickWidget
// DAVE virtual_joystick.html의 Qt 네이티브 재현.
// HTML의 축/버튼 파라미터를 그대로 계승하고, 50 Hz로 MAVLink MANUAL_CONTROL
// 값을 갱신하는 joystickState 신호를 발신한다.
//
// 축 인덱스 (HTML 원본과 동일):
//   [0] SWAY  = -leftStick.x  (오른쪽 = 음수)
//   [1] FWD   =  leftStick.y  (아래쪽 = 양수)
//   [4] YAW   = -rightStick.x
//   [5] HEAVE =  rightStick.y
//
// 버튼 인덱스:
//   0=A  1=B  2=X  3=Y  4=LB  5=RB  6=LT  7=RT
//   8=View(Disarm)  9=Menu(Arm)  10=L3  11=R3
//   12=DUp  13=DDown  14=DLeft  15=DRight  16=Guide
//
// MAVLink MANUAL_CONTROL 변환:
//   x = -axes[1]*1000   (FWD → forward-backward, 부호 반전으로 화면-위=전진)
//   y = -axes[0]*1000   (SWAY → lateral)
//   z = (1-axes[5])/2*1000  (HEAVE → 0=하강, 500=중립, 1000=상승)
//   r = -axes[4]*1000   (YAW)
// ─────────────────────────────────────────────────────────────────────────────
class JoystickWidget : public QWidget {
    Q_OBJECT
public:
    explicit JoystickWidget(QWidget* parent = nullptr);

    // 중심부 ±5%는 0으로 처리 (마우스 미세 이동으로 의도치 않은 추력 방지).
    // QGC의 axis deadband와 동일 개념. 추후 설정에서 조정 가능하도록 public.
    static constexpr float DEADBAND = 0.05f;

    // MAVLink MANUAL_CONTROL 변환값
    int16_t  mcX() const;
    int16_t  mcY() const;
    int16_t  mcZ() const;
    int16_t  mcR() const;
    uint16_t mcButtons() const;

signals:
    // 50 Hz로 발신. x/y/r: [-1000, 1000], z: [0, 1000]
    void joystickState(int16_t x, int16_t y, int16_t z, int16_t r, uint16_t buttons);
    // 연결 요청 (Application이 수신해 UdpLink 생성)
    void connectRequested(const QString& host, quint16 port);
    void disconnectRequested();
    // 버튼 9(Menu) press → arm=true, 버튼 8(View) press → arm=false.
    // QPushButton::pressed의 edge에서만 발신되므로 자동 디바운싱됨.
    void armRequested(bool arm);

public slots:
    void centerSticks();
    void onLinkConnected();
    void onLinkDisconnected();

private slots:
    void onLeftStickChanged(float x, float y);
    void onRightStickChanged(float x, float y);
    void onSendTick();
    void updateStatusDisplay();

private:
    void         setupUi();
    QWidget*     _buildConnectionBar();      // 상단 Host/Port/Connect/Disconnect/상태
    QWidget*     _buildControllerCard();     // 상단 버튼 7개 + 4컬럼(스틱·DPad·ABXY·스틱)
    QWidget*     _buildStatusCard();         // 하단 4축 수치 + 버튼 + Center 버튼

    QPushButton* makeCtrlButton(const QString& label, int btnIndex,
                                const QString& extraStyle = {});
    QPushButton* makeFaceButton(const QString& label, int btnIndex,
                                const QString& gradient);
    void         connectButton(QPushButton* btn, int index);

    static constexpr int   AXIS_SWAY  = 0;
    static constexpr int   AXIS_FWD   = 1;
    static constexpr int   AXIS_YAW   = 4;
    static constexpr int   AXIS_HEAVE = 5;

    std::array<float, 6>  _axes    = {};
    std::array<int,   17> _buttons = {};
    bool                  _linkConnected = false;

    StickPad*    _leftPad    = nullptr;
    StickPad*    _rightPad   = nullptr;

    QLineEdit*   _editHost    = nullptr;
    QLineEdit*   _editPort    = nullptr;
    QPushButton* _btnConnect  = nullptr;
    QPushButton* _btnDisc     = nullptr;
    QLabel*      _lblConnSt   = nullptr;

    // 축별 표시 레이블 (deadband 적용 후 ±%로 표시).
    QLabel*      _lblFwdVal   = nullptr;
    QLabel*      _lblSwayVal  = nullptr;
    QLabel*      _lblYawVal   = nullptr;
    QLabel*      _lblHeaveVal = nullptr;
    QLabel*      _lblBtnVal   = nullptr;
};
