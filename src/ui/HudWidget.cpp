#include "HudWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include "hud/AttitudeIndicator.h"
#include "hud/CompassWidget.h"
#include "hud/DepthGauge.h"

// ─────────────────────────────────────────────────────────────────────────────
// 상태바 칩 스타일 — 공통 베이스 + 강조 색만 다름.
// %1 = color, %2 = font-weight (있을 때 "bold", 없으면 빈 문자열)
// ─────────────────────────────────────────────────────────────────────────────
static QString chipStyle(const char* color, bool bold = false)
{
    return QString("background-color: #2a2a3a; color: %1;"
                   "%2 font-size: 13px; padding: 4px 10px; border-radius: 4px;")
        .arg(color, bold ? "font-weight: bold;" : "");
}

static constexpr const char* kRed   = "#ff4444";
static constexpr const char* kGreen = "#44bb44";
static constexpr const char* kMuted = "#aaaacc";

// ─────────────────────────────────────────────────────────────────────────────
// HudWidget()
// 상단 상태바(Armed/Mode/Heartbeat)와 세 계기를 세로로 배치하고
// VehicleState 신호를 연결한다.
// ─────────────────────────────────────────────────────────────────────────────
HudWidget::HudWidget(VehicleState* state, QWidget* parent)
    : QWidget(parent), _state(state)
{
    setStyleSheet("background-color: #0d0e18; color: #e0e0e0;");

    // ── 상태바 ────────────────────────────────────────────────
    _lblArmed = new QLabel("DISARMED", this);
    _lblArmed->setStyleSheet(chipStyle(kRed, /*bold=*/true));

    _lblMode = new QLabel("Unknown", this);
    _lblMode->setStyleSheet(chipStyle(kMuted));

    _lblHb = new QLabel("● NO LINK", this);
    _lblHb->setStyleSheet(chipStyle(kRed));

    QHBoxLayout* statusBar = new QHBoxLayout();
    statusBar->setContentsMargins(0, 0, 0, 0);
    statusBar->setSpacing(8);
    statusBar->addWidget(_lblArmed);
    statusBar->addWidget(_lblMode);
    statusBar->addWidget(_lblHb);
    statusBar->addStretch();

    // ── 계기 ──────────────────────────────────────────────────
    _attitude = new AttitudeIndicator(this);
    _attitude->setFixedSize(280, 280);

    _compass = new CompassWidget(this);
    _compass->setFixedSize(200, 200);

    _depth = new DepthGauge(this);
    _depth->setFixedSize(80, 280);
    _depth->setMaxDepth(100.0f);

    QVBoxLayout* rightCol = new QVBoxLayout();
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(8);
    rightCol->addStretch();
    rightCol->addWidget(_compass);
    rightCol->addStretch();

    QHBoxLayout* instruments = new QHBoxLayout();
    instruments->setContentsMargins(0, 0, 0, 0);
    instruments->setSpacing(20);
    instruments->addWidget(_attitude);
    instruments->addLayout(rightCol);
    instruments->addWidget(_depth);
    instruments->addStretch();

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 16, 24, 24);
    root->setSpacing(12);
    root->addLayout(statusBar);
    root->addLayout(instruments);
    setLayout(root);

    // VehicleState -> HUD 신호 연결
    connect(_state, &VehicleState::attitudeChanged,       this, &HudWidget::onAttitudeChanged);
    connect(_state, &VehicleState::vfrHudChanged,         this, &HudWidget::onVfrHudChanged);
    connect(_state, &VehicleState::armedChanged,          this, &HudWidget::onArmedChanged);
    connect(_state, &VehicleState::flightModeChanged,     this, &HudWidget::onFlightModeChanged);
    connect(_state, &VehicleState::heartbeatStatusChanged,this, &HudWidget::onHeartbeatStatusChanged);
}


// ─────────────────────────────────────────────────────────────────────────────
// onAttitudeChanged()
// ATTITUDE 메시지 수신 시 AttitudeIndicator와 CompassWidget을 갱신한다.
// ─────────────────────────────────────────────────────────────────────────────
void HudWidget::onAttitudeChanged()
{
    constexpr float toDeg = 180.0f / 3.14159265f;
    _attitude->setAttitude(_state->roll()  * toDeg,
                           _state->pitch() * toDeg);
}


// ─────────────────────────────────────────────────────────────────────────────
// onVfrHudChanged()
// VFR_HUD 메시지 수신 시 CompassWidget과 DepthGauge를 갱신한다.
// ─────────────────────────────────────────────────────────────────────────────
void HudWidget::onVfrHudChanged()
{
    _compass->setHeading(_state->heading());
    _depth->setDepth(-_state->depth());  // VFR_HUD altitude: 음수=수심, 표시는 양수
}


// ─────────────────────────────────────────────────────────────────────────────
// onArmedChanged()
// Armed/Disarmed 상태 레이블 색상과 텍스트를 갱신한다.
// ─────────────────────────────────────────────────────────────────────────────
void HudWidget::onArmedChanged()
{
    const bool armed = _state->armed();
    _lblArmed->setText(armed ? "ARMED" : "DISARMED");
    _lblArmed->setStyleSheet(chipStyle(armed ? kRed : kGreen, /*bold=*/true));
}


// ─────────────────────────────────────────────────────────────────────────────
// onFlightModeChanged()
// 비행모드 레이블 텍스트를 갱신한다.
// ─────────────────────────────────────────────────────────────────────────────
void HudWidget::onFlightModeChanged()
{
    _lblMode->setText(_state->flightMode());
}


// ─────────────────────────────────────────────────────────────────────────────
// onHeartbeatStatusChanged()
// heartbeat 연결 상태 레이블 색상과 텍스트를 갱신한다.
// ─────────────────────────────────────────────────────────────────────────────
void HudWidget::onHeartbeatStatusChanged()
{
    const bool ok = _state->heartbeatOk();
    _lblHb->setText(ok ? "● LINK OK" : "● NO LINK");
    _lblHb->setStyleSheet(chipStyle(ok ? kGreen : kRed));
}
