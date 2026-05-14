#include "JoystickWidget.h"
#include "StickPad.h"

#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QTimer>
#include <QIntValidator>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// applyDeadband()
// 중심 근처 |v| < DEADBAND 입력을 0으로 만들고, 그 외 구간은 [0, ±1]로 재스케일.
// 데드밴드 직후에 값이 점프하지 않고 연속적으로 증가하도록 정규화한다.
//
//   |v| < 0.05  →  0
//   v = 0.10    →  (0.10 - 0.05) / (1.0 - 0.05) ≈ 0.053
//   v = 1.00    →  1.0
// ─────────────────────────────────────────────────────────────────────────────
static float applyDeadband(float v)
{
    const float db = JoystickWidget::DEADBAND;
    const float a  = std::fabs(v);
    if (a < db) return 0.0f;
    return (v < 0.0f ? -1.0f : 1.0f) * (a - db) / (1.0f - db);
}

// 정수 퍼센트 표시용 (예: -73 → "−73%", 0 → "0%", 42 → "+42%")
static QString formatPct(float v)
{
    const int pct = static_cast<int>(std::round(v * 100.0f));
    if (pct > 0) return QString("+%1%").arg(pct);
    if (pct < 0) return QString("−%1%").arg(-pct);   // U+2212 minus
    return QString("0%");
}

// ─────────────────────────────────────────────────────────────────────────────
// 공통 스타일 상수 (HTML CSS 변수에 대응)
// ─────────────────────────────────────────────────────────────────────────────
static const char* kBg       = "background-color: #0d0e18;";
static const char* kPanel    = "background-color: rgba(7,18,25,0.84); "
                                "border: 1px solid rgba(171,222,255,0.15); "
                                "border-radius: 20px;";
static const char* kAccent   = "#61d3ff";
static const char* kMuted    = "#8faabc";
static const char* kText     = "#edf8ff";

// 입력창 (Host/Port) 공통 스타일
static const char* kInput =
    "QLineEdit { background: rgba(0,0,0,0.3); border: 1px solid rgba(171,222,255,0.2);"
    "border-radius: 6px; padding: 4px 8px; color: #edf8ff; font-family: monospace; }";

// 컬러 칩형 버튼 — 베이스. %1 = 컬러 RGB 트리플 (예: "30,240,181")
static QString chipButtonStyle(const char* rgb, const char* textColor)
{
    return QString(
        "QPushButton { background: rgba(%1,0.15); border: 1px solid rgba(%1,0.4);"
        "border-radius: 8px; padding: 5px 16px; color: %2; font-weight: 700; }"
        "QPushButton:hover    { background: rgba(%1,0.25); }"
        "QPushButton:pressed  { background: rgba(%1,0.1); }"
        "QPushButton:disabled { background: rgba(255,255,255,0.04); "
        "                       border-color: rgba(255,255,255,0.1); color: #555; }"
    ).arg(rgb, textColor);
}

// 모든 제어 버튼(LT/LB/View/Menu/RB/RT/D-Pad/L3/R3)의 공통 베이스
static const char* kCtrlBtn  =
    "QPushButton {"
    "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
    "    stop:0 rgba(255,255,255,0.09), stop:1 rgba(255,255,255,0.02));"
    "  border: 1px solid rgba(255,255,255,0.14);"
    "  border-radius: 999px;"
    "  color: #edf8ff;"
    "  font-weight: 800;"
    "  font-size: 11px;"
    "  letter-spacing: 0.08em;"
    "}"
    "QPushButton:pressed {"
    "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
    "    stop:0 rgba(97,211,255,0.28), stop:1 rgba(30,240,181,0.22));"
    "  border-color: rgba(97,211,255,0.5);"
    "}";

// 컬럼 타이틀 / 노트 / 상태행 헬퍼 — setupUi 안 람다였던 것 추출.
static QLabel* makeTitle(const QString& text, QWidget* parent)
{
    QLabel* l = new QLabel(text, parent);
    l->setStyleSheet(QString("color: %1; font-weight: 700; font-size: 10px; "
                             "letter-spacing: 0.08em;").arg(kAccent));
    l->setAlignment(Qt::AlignHCenter);
    return l;
}
static QLabel* makeNote(const QString& text, QWidget* parent)
{
    QLabel* l = new QLabel(text, parent);
    l->setStyleSheet(QString("color: %1; font-size: 10px;").arg(kMuted));
    l->setAlignment(Qt::AlignHCenter);
    l->setWordWrap(true);
    return l;
}
// 상태 카드 한 줄: [라벨 90px] [값 monospace]
static QFrame* makeStatusRow(const QString& label, QLabel*& valueOut, QWidget* parent)
{
    QFrame* row = new QFrame(parent);
    row->setStyleSheet("QFrame { background: rgba(0,0,0,0.18); "
                       "border: 1px solid rgba(255,255,255,0.05); border-radius: 12px; }");
    QHBoxLayout* rl = new QHBoxLayout(row);
    rl->setContentsMargins(12, 8, 12, 8);

    QLabel* lbl = new QLabel(label, parent);
    lbl->setStyleSheet(QString("color: %1; font-size: 11px; "
                               "text-transform: uppercase; letter-spacing: 0.06em;").arg(kMuted));
    lbl->setFixedWidth(90);
    rl->addWidget(lbl);

    valueOut = new QLabel("—", parent);
    valueOut->setStyleSheet(QString("color: %1; font-family: monospace; font-size: 11px;").arg(kText));
    rl->addWidget(valueOut);
    return row;
}

// ─────────────────────────────────────────────────────────────────────────────
// JoystickWidget()
// ─────────────────────────────────────────────────────────────────────────────
JoystickWidget::JoystickWidget(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(QString("JoystickWidget { %1 }").arg(kBg));
    setupUi();

    QTimer* sendTimer = new QTimer(this);
    sendTimer->setInterval(20);  // 50 Hz
    connect(sendTimer, &QTimer::timeout, this, &JoystickWidget::onSendTick);
    sendTimer->start();
}


// ─────────────────────────────────────────────────────────────────────────────
// mcX/Y/Z/R()
// HTML axes → MAVLink MANUAL_CONTROL 변환. 중심부 deadband 적용.
// 부호 변환 근거: HTML 좌표계(화면 Y ↓ = 양수)와 ArduSub 관례(전진 = 양수) 역전.
// ─────────────────────────────────────────────────────────────────────────────
int16_t JoystickWidget::mcX() const
{
    return static_cast<int16_t>(-applyDeadband(_axes[AXIS_FWD]) * 1000.0f);
}

int16_t JoystickWidget::mcY() const
{
    return static_cast<int16_t>(-applyDeadband(_axes[AXIS_SWAY]) * 1000.0f);
}

int16_t JoystickWidget::mcZ() const
{
    // 0=하강, 500=중립, 1000=상승. axes[HEAVE]: ↑ = 음수
    return static_cast<int16_t>((1.0f - applyDeadband(_axes[AXIS_HEAVE])) * 500.0f);
}

int16_t JoystickWidget::mcR() const
{
    return static_cast<int16_t>(-applyDeadband(_axes[AXIS_YAW]) * 1000.0f);
}

uint16_t JoystickWidget::mcButtons() const
{
    uint16_t mask = 0;
    for (int i = 0; i < 16; ++i) {
        if (_buttons[i]) mask |= static_cast<uint16_t>(1u << i);
    }
    return mask;
}


// ─────────────────────────────────────────────────────────────────────────────
// onLinkConnected() / onLinkDisconnected()
// LinkManager 상태 변화 시 Connection 패널 레이블과 버튼 상태를 갱신한다.
// ─────────────────────────────────────────────────────────────────────────────
void JoystickWidget::onLinkConnected()
{
    _linkConnected = true;
    _lblConnSt->setText("● Connected");
    _lblConnSt->setStyleSheet("color: #1ef0b5; font-weight: bold; font-size: 12px;");
    _btnConnect->setEnabled(false);
    _btnDisc->setEnabled(true);
    _editHost->setEnabled(false);
    _editPort->setEnabled(false);
}

void JoystickWidget::onLinkDisconnected()
{
    _linkConnected = false;
    _lblConnSt->setText("● Disconnected");
    _lblConnSt->setStyleSheet("color: #ff6b6b; font-weight: bold; font-size: 12px;");
    _btnConnect->setEnabled(true);
    _btnDisc->setEnabled(false);
    _editHost->setEnabled(true);
    _editPort->setEnabled(true);
}


// ─────────────────────────────────────────────────────────────────────────────
// centerSticks()
// 두 스틱을 모두 중립으로 되돌린다.
// ─────────────────────────────────────────────────────────────────────────────
void JoystickWidget::centerSticks()
{
    _leftPad->center();
    _rightPad->center();
}


// ─────────────────────────────────────────────────────────────────────────────
// onLeftStickChanged() / onRightStickChanged()
// 스틱 값만 _axes에 저장한다. 화면 갱신은 onSendTick(50Hz)이 일괄 처리해
// 마우스 이벤트당 QString 재생성을 피한다.
// ─────────────────────────────────────────────────────────────────────────────
void JoystickWidget::onLeftStickChanged(float x, float y)
{
    _axes[AXIS_SWAY] = -x;
    _axes[AXIS_FWD]  =  y;
}

void JoystickWidget::onRightStickChanged(float x, float y)
{
    _axes[AXIS_YAW]   = -x;
    _axes[AXIS_HEAVE] =  y;
}


// ─────────────────────────────────────────────────────────────────────────────
// onSendTick()
// 50Hz 타이머 콜백. 연결됐을 때만 joystickState를 발신해 불필요한 송신을 막는다.
// 화면 표시는 매 틱 갱신 (50Hz면 시각적으로 충분).
// ─────────────────────────────────────────────────────────────────────────────
void JoystickWidget::onSendTick()
{
    if (_linkConnected)
        emit joystickState(mcX(), mcY(), mcZ(), mcR(), mcButtons());
    updateStatusDisplay();
}


// ─────────────────────────────────────────────────────────────────────────────
// updateStatusDisplay()
// 실제로 송신되는 값(deadband 적용 후)을 사람이 읽기 쉬운 ±% 형태로 표시.
//   Forward  +: 전진,  −: 후진
//   Lateral  +: 우측,  −: 좌측
//   Yaw      +: 우회전, −: 좌회전
//   Heave    +: 상승,  −: 하강 (MAVLink z=500 기준 ±500 → ±100%)
// ─────────────────────────────────────────────────────────────────────────────
void JoystickWidget::updateStatusDisplay()
{
    // mc* 함수와 동일한 변환 (부호 + deadband)으로 표시 값 계산.
    const float fwd   = -applyDeadband(_axes[AXIS_FWD]);    // 스틱 ↑ = +
    const float sway  = -applyDeadband(_axes[AXIS_SWAY]);   // 스틱 → = +
    const float yaw   = -applyDeadband(_axes[AXIS_YAW]);    // 스틱 → = +
    const float heave = -applyDeadband(_axes[AXIS_HEAVE]);  // 스틱 ↑ = +

    _lblFwdVal  ->setText(formatPct(fwd));
    _lblSwayVal ->setText(formatPct(sway));
    _lblYawVal  ->setText(formatPct(yaw));
    _lblHeaveVal->setText(formatPct(heave));

    static const char* kNames[] = {
        "A","B","X","Y","LB","RB","LT","RT",
        "View","Menu","L3","R3",
        "DUp","DDown","DLeft","DRight","Guide"
    };
    QString pressed;
    for (int i = 0; i < 17; ++i) {
        if (_buttons[i]) {
            if (!pressed.isEmpty()) pressed += ", ";
            pressed += kNames[i];
        }
    }
    _lblBtnVal->setText(pressed.isEmpty() ? "None" : pressed);
}


// ─────────────────────────────────────────────────────────────────────────────
// makeCtrlButton()
// 공통 스타일 버튼 생성 후 pressed/released 이벤트를 _buttons 배열에 연결한다.
// ─────────────────────────────────────────────────────────────────────────────
QPushButton* JoystickWidget::makeCtrlButton(const QString& label, int btnIndex,
                                             const QString& extraStyle)
{
    QPushButton* btn = new QPushButton(label, this);
    btn->setStyleSheet(QString(kCtrlBtn) + extraStyle);
    btn->setToolTip(label);
    connectButton(btn, btnIndex);
    return btn;
}


// ─────────────────────────────────────────────────────────────────────────────
// makeFaceButton()
// 원형 ABXY 버튼. gradient 문자열로 각 버튼 색상을 설정한다.
// ─────────────────────────────────────────────────────────────────────────────
QPushButton* JoystickWidget::makeFaceButton(const QString& label, int btnIndex,
                                             const QString& gradient)
{
    QPushButton* btn = new QPushButton(label, this);
    btn->setFixedSize(42, 42);
    // Qt QSS는 CSS filter 미지원 — pressed는 외곽선 + padding으로 명시적 표현.
    btn->setStyleSheet(QString(
        "QPushButton {"
        "  background: %1;"
        "  border-radius: 21px;"
        "  border: 2px solid transparent;"
        "  color: white;"
        "  font-weight: 900;"
        "  font-size: 13px;"
        "}"
        "QPushButton:pressed {"
        "  border: 2px solid rgba(255,255,255,0.85);"
        "}"
    ).arg(gradient));
    connectButton(btn, btnIndex);
    return btn;
}


// ─────────────────────────────────────────────────────────────────────────────
// connectButton()
// pressed → _buttons[index]=1, released → 0. 화면 갱신은 onSendTick에서.
// 추가: Menu(9)/View(8) press 시 armRequested 신호도 발신
// (MANUAL_CONTROL 비트와 별개로 COMMAND_LONG으로 명시적 arm/disarm).
// ─────────────────────────────────────────────────────────────────────────────
void JoystickWidget::connectButton(QPushButton* btn, int index)
{
    connect(btn, &QPushButton::pressed,  [this, index]() {
        _buttons[index] = 1;
        if      (index == 9) emit armRequested(true);   // Menu = ARM
        else if (index == 8) emit armRequested(false);  // View = DISARM
    });
    connect(btn, &QPushButton::released, [this, index]() { _buttons[index] = 0; });
}


// ─────────────────────────────────────────────────────────────────────────────
// setupUi()
// 위젯 전체 레이아웃을 조립한다. 실제 빌드는 3개의 빌더 함수로 분리:
//   _buildConnectionBar()  — 상단 Host/Port + Connect/Disconnect + 상태칩
//   _buildControllerCard() — 상단 LT/LB/View/G/Menu/RB/RT + 4컬럼(스틱·DPad·ABXY·스틱)
//   _buildStatusCard()     — 하단 Forward/Lateral/Yaw/Heave/Buttons 수치
// ─────────────────────────────────────────────────────────────────────────────
void JoystickWidget::setupUi()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    root->addWidget(_buildConnectionBar());
    root->addWidget(_buildControllerCard());
    root->addWidget(_buildStatusCard());

    updateStatusDisplay();
}


// ─────────────────────────────────────────────────────────────────────────────
// _buildConnectionBar()
// HTML의 .topbar(chip 영역)에 대응. Host/Port 입력 + Connect/Disconnect + 상태칩.
// Connect 클릭 → connectRequested(host, port) 시그널 발신.
// ─────────────────────────────────────────────────────────────────────────────
QWidget* JoystickWidget::_buildConnectionBar()
{
    QFrame* bar = new QFrame(this);
    bar->setStyleSheet(QString("QFrame { %1 }").arg(kPanel));
    QHBoxLayout* cl = new QHBoxLayout(bar);
    cl->setContentsMargins(14, 10, 14, 10);
    cl->setSpacing(10);

    auto* hostLbl = new QLabel("Host:", this);
    hostLbl->setStyleSheet(QString("color: %1; font-size: 12px;").arg(kMuted));
    cl->addWidget(hostLbl);

    _editHost = new QLineEdit("127.0.0.1", this);
    _editHost->setFixedWidth(130);
    _editHost->setPlaceholderText("192.168.2.1");
    _editHost->setStyleSheet(kInput);
    cl->addWidget(_editHost);

    auto* portLbl = new QLabel("Port:", this);
    portLbl->setStyleSheet(QString("color: %1; font-size: 12px;").arg(kMuted));
    cl->addWidget(portLbl);

    _editPort = new QLineEdit("14550", this);
    _editPort->setFixedWidth(65);
    _editPort->setValidator(new QIntValidator(1, 65535, _editPort));
    _editPort->setStyleSheet(kInput);
    cl->addWidget(_editPort);

    _btnConnect = new QPushButton("Connect", this);
    _btnConnect->setStyleSheet(chipButtonStyle("30,240,181", "#b8ffe6"));   // 녹색
    cl->addWidget(_btnConnect);

    _btnDisc = new QPushButton("Disconnect", this);
    _btnDisc->setEnabled(false);
    _btnDisc->setStyleSheet(chipButtonStyle("255,107,107", "#ffb4b4"));     // 빨강
    cl->addWidget(_btnDisc);

    _lblConnSt = new QLabel("● Disconnected", this);
    _lblConnSt->setStyleSheet("color: #ff6b6b; font-weight: bold; font-size: 12px;");
    cl->addWidget(_lblConnSt);

    cl->addStretch();

    connect(_btnConnect, &QPushButton::clicked, [this]() {
        const quint16 port = static_cast<quint16>(_editPort->text().toUInt());
        emit connectRequested(_editHost->text().trimmed(), port);
    });
    connect(_btnDisc, &QPushButton::clicked, this, &JoystickWidget::disconnectRequested);

    return bar;
}


// ─────────────────────────────────────────────────────────────────────────────
// _buildControllerCard()
// HTML의 .controller-card에 대응. 상단 7버튼(LT/LB/View/G/Menu/RB/RT) +
// 4컬럼(Left Stick / D-Pad / ABXY / Right Stick).
// ─────────────────────────────────────────────────────────────────────────────
QWidget* JoystickWidget::_buildControllerCard()
{
    QFrame* card = new QFrame(this);
    card->setStyleSheet(QString("QFrame { %1 }").arg(kPanel));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 16, 20, 20);
    cardLayout->setSpacing(16);

    // ── 상단 스트립 (LT LB View G Menu RB RT) ────────────────────
    auto* topStrip = new QHBoxLayout();
    topStrip->setSpacing(6);
    topStrip->addStretch();

    struct TopBtn { const char* label; int idx; };
    const TopBtn topBtns[] = {
        {"LT", 6}, {"LB", 4},
        {"View\n(Disarm)", 8},
        {"G", 16},
        {"Menu\n(Arm)", 9},
        {"RB", 5}, {"RT", 7}
    };
    for (const auto& tb : topBtns) {
        const bool isGuide = (QString(tb.label) == "G");
        QPushButton* btn = makeCtrlButton(tb.label, tb.idx,
            isGuide ? "QPushButton { border-radius: 20px; min-width: 0; padding: 0; }" : "");
        if (isGuide) {
            btn->setFixedSize(40, 40);
        } else {
            btn->setFixedHeight(38);
            btn->setMinimumWidth(52);
        }
        topStrip->addWidget(btn);
    }
    topStrip->addStretch();
    cardLayout->addLayout(topStrip);

    // ── 4 컬럼 mainStrip ────────────────────────────────────────
    auto* mainStrip = new QHBoxLayout();
    mainStrip->setSpacing(16);

    // 컬럼 컨테이너 빌더 — 타이틀/위젯/노트/메타버튼 순서 일관
    auto makeColumn = [&](const QString& title) {
        auto* col = new QVBoxLayout();
        col->setSpacing(8);
        col->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        col->addWidget(makeTitle(title, this));
        return col;
    };

    // ─ Column 1: Left Stick ─────────────────────────────────────
    {
        auto* col = makeColumn("LEFT STICK");
        _leftPad = new StickPad(this);
        _leftPad->setFixedSize(170, 170);
        col->addWidget(_leftPad, 0, Qt::AlignHCenter);
        col->addWidget(makeNote("Strafe / Forward", this));

        auto* l3 = makeCtrlButton("L3", 10);
        l3->setFixedHeight(32);
        l3->setMinimumWidth(60);
        col->addWidget(l3, 0, Qt::AlignHCenter);
        col->addStretch();
        mainStrip->addLayout(col);

        connect(_leftPad, &StickPad::valueChanged, this, &JoystickWidget::onLeftStickChanged);
    }

    // ─ Column 2: D-Pad (3×3 grid) ───────────────────────────────
    {
        auto* col = makeColumn("D-PAD");

        auto* dpadWidget = new QWidget(this);
        dpadWidget->setFixedSize(130, 130);
        auto* dg = new QGridLayout(dpadWidget);
        dg->setContentsMargins(0, 0, 0, 0);
        dg->setSpacing(4);

        // D-Pad 한 버튼 만드는 람다 (방향 4개 다 같은 모양)
        auto dpad = [&](const QString& lbl, int idx, int row, int col) {
            auto* btn = makeCtrlButton(lbl, idx,
                "QPushButton { border-radius: 10px; font-size: 14px; }");
            btn->setFixedSize(38, 38);
            dg->addWidget(btn, row, col);
        };
        dpad("▲", 12, 0, 1);
        dpad("◀", 14, 1, 0);
        dpad("▶", 15, 1, 2);
        dpad("▼", 13, 2, 1);

        // 중앙 허브 (시각 장식, 클릭 안 됨)
        auto* hub = new QFrame(dpadWidget);
        hub->setFixedSize(38, 38);
        hub->setStyleSheet("background: rgba(255,255,255,0.03);"
                           "border: 1px solid rgba(255,255,255,0.08);"
                           "border-radius: 10px;");
        dg->addWidget(hub, 1, 1);

        col->addWidget(dpadWidget, 0, Qt::AlignHCenter);
        col->addWidget(makeNote("Up/Down: Camera\nLeft/Right: Lights", this));
        col->addStretch();
        mainStrip->addLayout(col);
    }

    // ─ Column 3: ABXY (다이아몬드 배치) ─────────────────────────
    {
        auto* col = makeColumn("ABXY");

        auto* faceWidget = new QWidget(this);
        faceWidget->setFixedSize(130, 130);
        auto* fg = new QGridLayout(faceWidget);
        fg->setContentsMargins(0, 0, 0, 0);
        fg->setSpacing(4);

        // 각 face button 색상은 radial gradient (HTML 원본 색 유지)
        auto faceGrad = [](const char* fromHex, const char* toHex) {
            return QString("qradialgradient(cx:0.35,cy:0.3,radius:0.8,"
                           "stop:0 %1, stop:1 %2)").arg(fromHex, toHex);
        };
        fg->addWidget(makeFaceButton("Y", 3, faceGrad("#ffe69b", "#c9a113")), 0, 1, Qt::AlignCenter);  // 상
        fg->addWidget(makeFaceButton("X", 2, faceGrad("#9cd7ff", "#1d88dc")), 1, 0, Qt::AlignCenter);  // 좌
        fg->addWidget(makeFaceButton("B", 1, faceGrad("#ffb0a8", "#cf3737")), 1, 2, Qt::AlignCenter);  // 우
        fg->addWidget(makeFaceButton("A", 0, faceGrad("#b0ffd8", "#169a52")), 2, 1, Qt::AlignCenter);  // 하

        col->addWidget(faceWidget, 0, Qt::AlignHCenter);
        col->addWidget(makeNote("A=STABILIZE  Y=ALT_HOLD\nB=Out↑  X=Out↓", this));
        col->addStretch();
        mainStrip->addLayout(col);
    }

    // ─ Column 4: Right Stick ────────────────────────────────────
    {
        auto* col = makeColumn("RIGHT STICK");
        _rightPad = new StickPad(this);
        _rightPad->setFixedSize(170, 170);
        col->addWidget(_rightPad, 0, Qt::AlignHCenter);
        col->addWidget(makeNote("Yaw / Heave", this));

        auto* r3 = makeCtrlButton("R3", 11);
        r3->setFixedHeight(32);
        r3->setMinimumWidth(60);
        col->addWidget(r3, 0, Qt::AlignHCenter);
        col->addStretch();
        mainStrip->addLayout(col);

        connect(_rightPad, &StickPad::valueChanged, this, &JoystickWidget::onRightStickChanged);
    }

    cardLayout->addLayout(mainStrip);
    return card;
}


// ─────────────────────────────────────────────────────────────────────────────
// _buildStatusCard()
// HTML의 .status-card에 대응. INPUT STATUS 헤더 + Center 버튼 +
// 4축(Forward/Lateral/Yaw/Heave) + 누른 버튼 표시.
// ─────────────────────────────────────────────────────────────────────────────
QWidget* JoystickWidget::_buildStatusCard()
{
    auto* card = new QFrame(this);
    card->setStyleSheet(QString("QFrame { %1 }").arg(kPanel));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    // 헤더: 타이틀 + Center Sticks 버튼
    auto* header = new QHBoxLayout();
    auto* title = new QLabel("INPUT STATUS", this);
    title->setStyleSheet(QString("color: %1; font-weight: 700; "
                                 "font-size: 11px; letter-spacing: 0.08em;").arg(kAccent));
    header->addWidget(title);
    header->addStretch();

    auto* centerBtn = new QPushButton("Center Sticks", this);
    centerBtn->setStyleSheet(
        "QPushButton { background: rgba(255,209,102,0.12); border: 1px solid rgba(171,222,255,0.15);"
        "border-radius: 10px; padding: 6px 14px; color: #edf8ff; font-weight: 700; }"
        "QPushButton:pressed { background: rgba(255,209,102,0.22); }");
    connect(centerBtn, &QPushButton::clicked, this, &JoystickWidget::centerSticks);
    header->addWidget(centerBtn);
    layout->addLayout(header);

    // 축별 수치 표시 + 버튼 목록
    layout->addWidget(makeStatusRow("Forward", _lblFwdVal,   this));
    layout->addWidget(makeStatusRow("Lateral", _lblSwayVal,  this));
    layout->addWidget(makeStatusRow("Yaw",     _lblYawVal,   this));
    layout->addWidget(makeStatusRow("Heave",   _lblHeaveVal, this));
    layout->addWidget(makeStatusRow("Buttons", _lblBtnVal,   this));

    return card;
}
