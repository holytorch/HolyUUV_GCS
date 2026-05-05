#include "TerrainWidget.h"

// Qt UI 위젯
#include <QVBoxLayout>       // 세로 레이아웃
#include <QHBoxLayout>       // 가로 레이아웃
#include <QLabel>            // 텍스트 표시 위젯
#include <QPushButton>       // 버튼 위젯
#include <QSpinBox>          // 숫자 입력 위젯
// Qt 네트워크
#include <QNetworkRequest>   // HTTP 요청 객체
#include <QNetworkReply>     // HTTP 응답 객체
// Qt 기타
#include <QUrl>              // URL 문자열 처리
#include <QDebug>            // 디버그 출력 (qDebug, qWarning)
#include <QDir>              // 디렉터리/경로 처리
#include <QPainter>          // 이미지에 그림 그리기
#include <cmath>             // cos, sin, log, atan 등 수학 함수

// Qt3D 씬 구조
#include <Qt3DCore/QEntity>              // 씬의 모든 오브젝트 기반 클래스
#include <Qt3DCore/QTransform>           // 위치/회전/스케일 컴포넌트
// Qt3D 렌더링 창 및 카메라
#include <Qt3DExtras/Qt3DWindow>         // 3D 렌더링 전용 창
#include <Qt3DExtras/QOrbitCameraController>  // 마우스로 카메라 회전/줌
#include <Qt3DExtras/QForwardRenderer>   // 기본 렌더러 (setClearColor 접근에 필요)
// Qt3D 메시/재질
#include <Qt3DExtras/QSphereMesh>        // 구체 메시 (차량 마커용)
#include <Qt3DExtras/QPhongMaterial>     // 기본 조명 재질
#include <Qt3DExtras/QDiffuseMapMaterial>// 텍스처 입히는 재질
// Qt3D 텍스처
#include <Qt3DRender/QTexture>           // GPU 텍스처 객체
#include <Qt3DRender/QTextureImage>      // 파일에서 텍스처 로드
// Qt3D 입력/조명/카메라
#include <Qt3DInput/QInputSettings>      // 마우스/키보드 이벤트 소스 지정
#include <Qt3DRender/QDirectionalLight>  // 방향성 조명 (태양광 같은 것)
#include <Qt3DRender/QCamera>            // 카메라 (시점, FOV, 클리핑)
// Qt3D 지오메트리 (직접 정점 데이터 구성)
#include <Qt3DRender/QGeometry>          // 정점/인덱스 데이터 컨테이너
#include <Qt3DRender/QGeometryRenderer>  // 지오메트리를 실제로 그리는 컴포넌트
#include <Qt3DRender/QAttribute>         // 정점 속성 (위치, 노말, UV 등)
#include <Qt3DRender/QBuffer>            // GPU에 올릴 바이트 버퍼

// OSM 타일 URL 템플릿: %1=zoom, %2=tileX, %3=tileY
// TileServer(포트 17777)가 로컬에서 OSM 타일을 중계
static const QString OSM_URL = "http://localhost:17777/osm/%1/%2/%3.png";

// =============================================================
// Web Mercator 좌표 변환 유틸 3개
// =============================================================

// 해당 위도·줌에서 1픽셀 = 몇 미터인지 반환
// → 3km를 픽셀 수로 변환할 때 사용 (1500m / mpp = halfPx)
double TerrainWidget::metersPerPixel(double lat, int z)
{
    return 156543.03392 * std::cos(lat * M_PI / 180.0) / std::pow(2.0, z);
}

// 글로벌 픽셀 X좌표 → 경도(°) 변환
// 전체 지도 가로 픽셀 = 256 * 2^z, 경도 범위 -180~+180
double TerrainWidget::globalPixToLon(double gx, int z)
{
    return gx / (256.0 * (1 << z)) * 360.0 - 180.0;
}

// 글로벌 픽셀 Y좌표 → 위도(°) 변환
// 메르카토르 도법은 위도가 비선형이라 sinh/atan 필요
double TerrainWidget::globalPixToLat(double gy, int z)
{
    double n = static_cast<double>(1 << z);   // 2^z
    return std::atan(std::sinh(M_PI * (1.0 - 2.0 * gy / (n * 256.0)))) * 180.0 / M_PI;
}

// =============================================================
// 생성자: Qt3D 씬 초기화 + UI 구성
// =============================================================
TerrainWidget::TerrainWidget(QWidget* parent)
    : QWidget(parent)
{
    // Qt3DWindow: OpenGL 렌더링 전용 창 (QWindow 기반, QWidget이 아님)
    _view = new Qt3DExtras::Qt3DWindow();
    // 배경색: RGB(15,18,30) ≈ 거의 검정인 남색
    _view->defaultFrameGraph()->setClearColor(QColor(15, 18, 30));

    // Qt UI 시스템
    // ├── QWidget 계열  → 버튼, 레이블, 레이아웃 등 일반 UI
    // └── QWindow 계열  → OpenGL/Vulkan 등 GPU 렌더링
    _viewport = QWidget::createWindowContainer(_view, this);
    _viewport->setMinimumSize(400, 300);                              // 최소 크기 400×300px
    _viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // 여유 공간 있으면 최대한 늘어남

    // 씬의 최상위 엔티티: 모든 3D 오브젝트의 공통 조상 (트리구조라고 함)
    _rootEntity = new Qt3DCore::QEntity();
    _view->setRootEntity(_rootEntity);

    // 마우스/키보드 이벤트를 Qt3DWindow에서 받도록 지정
    // QWidget(_viewport)으로 설정하면 카메라가 동작하지 않음
    auto* inputSettings = new Qt3DInput::QInputSettings(_rootEntity);
    inputSettings->setEventSource(_view);

    // 카메라 설정 (바꿀필요 ㅇㅇ)
    Qt3DRender::QCamera* cam = _view->camera();
    cam->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection); // 원근 투영 (가까울수록 크게)
    cam->setFieldOfView(60.0f);          // 시야각 60° (사람 눈과 유사)
    cam->setNearPlane(0.1f);             // 이보다 가까운 건 안 그림
    cam->setFarPlane(10000.0f);          // 이보다 먼 건 안 그림
    cam->setPosition(QVector3D(0, 179, 128));     // zoom17 기준 700m 위, 남쪽 500m
    cam->setViewCenter(QVector3D(0, 0, 0));      // 맵 중심 바라봄
    cam->setUpVector(QVector3D(0, 1, 0));        // Y축이 위쪽

    // 궤도 카메라 컨트롤러: 마우스 좌클릭=회전, 우클릭=줌, 중클릭=패닝
    _camCtrl = new Qt3DExtras::QOrbitCameraController(_rootEntity);
    _camCtrl->setCamera(cam);
    _camCtrl->setLinearSpeed(-400.0f);    // 이동 속도
    _camCtrl->setLookSpeed(-180.0f);      // 회전 속도

    // 방향성 조명: 태양광처럼 평행하게 내리쬐는 빛 (이걸없앨까말까고민중)
    auto* lightEntity = new Qt3DCore::QEntity(_rootEntity);
    auto* light = new Qt3DRender::QDirectionalLight(lightEntity);
    light->setWorldDirection(QVector3D(-0.3f, -1.0f, -0.5f)); // 빛이 오는 방향 (왼쪽 위 앞에서)
    light->setColor(Qt::white);
    light->setIntensity(1.5f);
    lightEntity->addComponent(light);

    // 하단 상태 표시줄
    _statusLabel = new QLabel("Load Terrain 버튼으로 지형 로드", this);
    _statusLabel->setStyleSheet("color: #aaa; font-size: 12px; padding: 2px;");

    // 줌 선택 스핀박스: 10~14 (나중에 줌18까지 해볼예정)
    _zoomSpin = new QSpinBox(this);
    _zoomSpin->setRange(13, 18);
    _zoomSpin->setValue(17);

    // Load Terrain 버튼: 클릭 시 onFetchClicked 슬롯 호출
    _fetchBtn = new QPushButton("Load Terrain", this);
    connect(_fetchBtn, &QPushButton::clicked, this, &TerrainWidget::onFetchClicked);
    // 네트워크 응답이 오면 onReplyFinished 슬롯 호출
    connect(&_nam, &QNetworkAccessManager::finished, this, &TerrainWidget::onReplyFinished);

    // 하단 컨트롤 바: [Zoom: 스핀] [Load Terrain 버튼] ----[상태 레이블]
    QHBoxLayout* ctrlBar = new QHBoxLayout();
    ctrlBar->addWidget(new QLabel("Zoom:", this));
    ctrlBar->addWidget(_zoomSpin);
    ctrlBar->addWidget(_fetchBtn);
    ctrlBar->addStretch();       // 버튼과 상태레이블 사이 빈 공간
    ctrlBar->addWidget(_statusLabel);

    // 전체 레이아웃: 3D 뷰포트(위 _viewport) + 컨트롤 바(아래 ctrlBar)
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0); // 외부 여백 없음
    layout->setSpacing(0);                  // 위젯 간 간격 없음
    layout->addWidget(_viewport);
    layout->addLayout(ctrlBar);
    setLayout(layout);
}

// =============================================================
// 탭 전환 이벤트: 다른 탭에서 마우스 조작이 카메라에 영향 주는 것 방지 (나중에 하나의 창에서는 또 달라짐 나중에 수정해야할 부분)
// =============================================================

// 이 탭이 보일 때 → 카메라 컨트롤 활성화
void TerrainWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (_camCtrl) _camCtrl->setEnabled(true);
}

// 이 탭이 숨겨질 때 → 카메라 컨트롤 비활성화
void TerrainWidget::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    if (_camCtrl) _camCtrl->setEnabled(false);
}

// =============================================================
// 공개 API: 외부(MainWindow)에서 호출
// =============================================================

// 탭 전환 시 MainWindow가 호출 → 위경도·줌 저장 후 타일 요청 시작
void TerrainWidget::loadTile(double lat, double lon, int zoom)
{
    _lat  = lat;
    _lon  = lon;
    _zoom = zoom;
    _zoomSpin->setValue(zoom);
    onFetchClicked();
}

// MAVLink GPS 수신 시 MainWindow가 호출 → 마커 위치 갱신
// 최초 GPS 수신 시(0,0 → 실제 좌표)에는 맵 전체를 로봇 위치 기준으로 새로고침
void TerrainWidget::updateVehiclePosition(double lat, double lon)
{
    bool firstFix = (_vehicleLat == 0.0 && _vehicleLon == 0.0);
    _vehicleLat = lat;
    _vehicleLon = lon;

    if (firstFix) {
        loadTile(lat, lon, _zoom); // 최초 GPS 수신 → 로봇 위치 기준 맵 리로드
    } else {
        updateVehicleMarker();
    }
}

// =============================================================
// Load 버튼 클릭 → 필요한 OSM 타일 범위 계산 후 HTTP 요청
// =============================================================
void TerrainWidget::onFetchClicked()
{
    _zoom = _zoomSpin->value();

    // 1픽셀 = 몇 미터인지 계산 후, 1km(500m×2) = 몇 픽셀인지 구함
    double mpp    = metersPerPixel(_lat, _zoom);
    int    halfPx = std::max(2, static_cast<int>(std::ceil(500.0 / mpp)));

    // 중심 위경도 → 글로벌 픽셀 좌표 변환
    // fracX: 전체 지도를 1로 봤을 때 경도 위치 (0~1)
    double fracX = (_lon + 180.0) / 360.0 * (1 << _zoom);
    double latR  = _lat * M_PI / 180.0;
    // fracY: 메르카토르 도법으로 위도 위치 계산
    double fracY = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI)
                   / 2.0 * (1 << _zoom);
    // 글로벌 픽셀 = fracTile * 256 (타일 1개 = 256픽셀)
    int gCX = static_cast<int>(fracX * 256);
    int gCY = static_cast<int>(fracY * 256);

    // 3km 범위를 커버하는 타일 인덱스 범위 계산 (타일이 픽셀보다 256배 크니까 256으로 나눔)
    _osmTX0 = (gCX - halfPx) / 256;  // 좌상단 타일 X
    _osmTY0 = (gCY - halfPx) / 256;  // 좌상단 타일 Y
    int tx1 = (gCX + halfPx) / 256;  // 우하단 타일 X
    int ty1 = (gCY + halfPx) / 256;  // 우하단 타일 Y
    _osmTNX = tx1 - _osmTX0 + 1;     // 가로 타일 개수
    _osmTNY = ty1 - _osmTY0 + 1;     // 세로 타일 개수

    // 스티칭 이미지(타일들 합친 것) 안에서 중심 픽셀 위치
    _stitchCX  = gCX - _osmTX0 * 256;
    _stitchCY  = gCY - _osmTY0 * 256;
    _subHalfPx = halfPx;  // 중심에서 ±halfPx = 3km 반경

    _osmImages.clear();
    _osmPending = _osmTNX * _osmTNY; // 수신 대기 중인 타일 수

    _statusLabel->setText(QString("OSM z=%1 로딩 중... (%2×%3 타일)")
                              .arg(_zoom).arg(_osmTNX).arg(_osmTNY));
    _fetchBtn->setEnabled(false); // 로딩 중 중복 요청 방지

    // 범위 내 모든 타일 HTTP 요청
    // 응답은 비동기로 오고 각각 onReplyFinished 에서 처리
    for (int r = 0; r < _osmTNY; ++r) {
        for (int c = 0; c < _osmTNX; ++c) {
            int otx = _osmTX0 + c, oty = _osmTY0 + r;
            auto* rep = _nam.get(QNetworkRequest(
                QUrl(OSM_URL.arg(_zoom).arg(otx).arg(oty))));
            // 응답이 왔을 때 어느 타일인지 알 수 있도록 좌표를 메타데이터로 첨부
            rep->setProperty("osmTX", otx);
            rep->setProperty("osmTY", oty);
        }
    }
}

// =============================================================
// HTTP 응답 처리
// =============================================================

// QNetworkAccessManager::finished 시그널 → 이 슬롯 호출
void TerrainWidget::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater(); // Qt가 나중에 메모리 자동 해제
    handleOsmTile(reply);
}

void TerrainWidget::handleOsmTile(QNetworkReply* reply)
{
    // 요청 시 첨부한 타일 좌표 꺼내기
    int otx = reply->property("osmTX").toInt();
    int oty = reply->property("osmTY").toInt();

    if (reply->error() == QNetworkReply::NoError) {
        QImage img;
        // 응답 바이트 → QImage로 디코딩, 256×256 RGB로 통일
        if (img.loadFromData(reply->readAll()) && !img.isNull())
            _osmImages[qMakePair(otx, oty)] =
                img.scaled(256, 256).convertToFormat(QImage::Format_RGB32);
    } else {
        qWarning("OSM tile (%d,%d) error: %s", otx, oty,
                 qPrintable(reply->errorString()));
    }

    // 모든 타일 수신 완료 시 스티칭+빌드 진행
    if (--_osmPending <= 0)
        stitchAndBuild();
}

// =============================================================
// OSM 타일 스티칭 → 바다/육지 판별 → 메시·컴퍼스·마커 빌드 (이거 gebco로 수정해야함 현재 osm에는 하늘식 이런거 없음)
// =============================================================
void TerrainWidget::stitchAndBuild()
{
    // ── 타일 이미지들을 하나의 큰 이미지로 합치기 ──────────────
    // 전체 스티칭 이미지 크기: (타일 가로 수 × 256) × (타일 세로 수 × 256)
    QImage stitched(_osmTNX * 256, _osmTNY * 256, QImage::Format_RGB32);
    stitched.fill(Qt::black); // 수신 실패한 타일은 검정으로
    QPainter p(&stitched);
    for (int r = 0; r < _osmTNY; ++r)
        for (int c = 0; c < _osmTNX; ++c) {
            auto key = qMakePair(_osmTX0 + c, _osmTY0 + r);
            if (_osmImages.contains(key))
                p.drawImage(c * 256, r * 256, _osmImages[key]); // (c*256, r*256) 위치에 타일 그리기
        }
    p.end();

    // ── 스티칭 이미지에서 중심 기준 3km 서브영역 잘라내기 ─────
    // subW × subH = 실제 메시 해상도 (픽셀 단위)
    const int   subW      = _subHalfPx * 2 + 1;
    const int   subH      = _subHalfPx * 2 + 1;
    const float SEA_DEPTH = -50.0f;  // 바다 높이값 (m)
    const float LAND_H    = 1.0f;    // 육지 높이값 (m)

    // _currentTile에 높이 배열 세팅
    _currentTile.tileZ  = _zoom;
    _currentTile.tileX  = _osmTX0;
    _currentTile.tileY  = _osmTY0;
    _currentTile.width  = subW;
    _currentTile.height = subH;
    _currentTile.heights.resize(subW * subH);

    // 서브영역 픽셀마다 색을 읽어 바다/육지 판별
    for (int sy = 0; sy < subH; ++sy) {
        for (int sx = 0; sx < subW; ++sx) {
            // 서브영역 픽셀 → 스티칭 이미지 픽셀 좌표 변환 (범위 초과 방지)
            int px = qBound(0, _stitchCX - _subHalfPx + sx, stitched.width()  - 1);
            int py = qBound(0, _stitchCY - _subHalfPx + sy, stitched.height() - 1);
            QRgb  col = stitched.pixel(px, py);
            int   r = qRed(col), g = qGreen(col), b = qBlue(col);
            // CartoDB Positron 수색 판별: #AAD3DF 계열 → 파란 성분이 확실히 우세
            bool  water = (b > r + 15) && (b > 150) && (g > 150);
            _currentTile.heights[sy * subW + sx] = water ? SEA_DEPTH : LAND_H;
        }
    }

    // ── 스티칭 이미지를 임시 파일로 저장 (Qt3D 텍스처 로드용) ──
    // seq로 매번 다른 파일명 생성 → 이전 텍스처와 충돌 방지
    static int seq = 0;
    _osmTexPath = QString("%1/holyuuv_osm_%2.png").arg(QDir::tempPath()).arg(++seq);
    stitched.save(_osmTexPath);

    buildMesh();              // 높이맵 + 텍스처로 3D 메시 생성
    updateVehicleMarker();    // 로봇 위치 마커 갱신
    resetCameraToTerrain();   // 로드 완료 후 초기 카메라 시점 설정

    _fetchBtn->setEnabled(true);
    _statusLabel->setText(
        QString("OSM z=%1  lat=%2 lon=%3  %4×%5px  ~%6m/px")
            .arg(_zoom)
            .arg(_lat, 0, 'f', 5)
            .arg(_lon, 0, 'f', 5)
            .arg(subW).arg(subH)
            .arg(metersPerPixel(_lat, _zoom), 0, 'f', 1));
}

// =============================================================
// 높이맵 → Qt3D 메시 생성
// 정점 포맷: position(3) + normal(3) + uv(2) = float 8개/정점
// =============================================================
void TerrainWidget::buildMesh()
{
    // 기존 메시 엔티티가 있으면 씬에서 제거 후 삭제
    // setParent(nullptr)로 씬 트리에서 분리해야 Qt3D가 안전하게 삭제 가능
    if (_meshEntity) {
        _meshEntity->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        delete _meshEntity;
        _meshEntity = nullptr;
    }

    const TerrainTile& tile = _currentTile;
    const int   subW        = tile.width;
    const int   subH        = tile.height;
    // scale: 픽셀 좌표 → 3D 월드 좌표 변환 비율
    // _worldHalfSize=128 이므로 메시는 항상 -128~+128 범위 안에 들어옴
    const float scale       = _worldHalfSize / static_cast<float>(_subHalfPx);
    // heightScale: 높이값(m)을 3D 좌표로 변환 (너무 작으면 평평해 보임)
    const float heightScale = 0.05f;

    // UV 좌표 계산을 위한 기준값
    // 텍스처는 스티칭 이미지 전체, 서브영역은 그 안의 일부분
    const float texW  = static_cast<float>(_osmTNX * 256); // 스티칭 이미지 전체 가로 픽셀
    const float texH  = static_cast<float>(_osmTNY * 256); // 스티칭 이미지 전체 세로 픽셀
    const int   imgX0 = _stitchCX - _subHalfPx;            // 서브영역 좌상단 X (스티칭 기준)
    const int   imgY0 = _stitchCY - _subHalfPx;            // 서브영역 좌상단 Y (스티칭 기준)

    // 정점 버퍼 메모리 할당: 정점 수 × 8개 float
    QByteArray vertexBytes(subW * subH * 8 * sizeof(float), Qt::Uninitialized);
    float* vp = reinterpret_cast<float*>(vertexBytes.data()); // float 포인터로 접근

    // 모든 정점 데이터 채우기
    for (int sr = 0; sr < subH; ++sr) {       // sr = row (Z축 방향, 북→남)
        for (int sc = 0; sc < subW; ++sc) {   // sc = col (X축 방향, 서→동)
            float h = tile.heightAt(sc, sr);  // heights 배열에서 높이값 읽기

            *vp++ = (sc - _subHalfPx) * scale;          // X: 서←중심→동
            *vp++ = h * heightScale;                     // Y: 높이 (위쪽이 양수)
            *vp++ = (sr - _subHalfPx) * scale;          // Z: 북←중심→남
            *vp++ = 0.0f; *vp++ = 1.0f; *vp++ = 0.0f;  // 노말: 위쪽(0,1,0) 고정
            *vp++ = (imgX0 + sc) / texW;                 // U: 텍스처 가로 좌표 (0~1)
            *vp++ = (imgY0 + sr) / texH;                 // V: 텍스처 세로 좌표 (0~1)
        }
    }

    // 인덱스 버퍼: 정점들을 삼각형 2개(사각형 1개)씩 연결
    // (subW-1) × (subH-1) 개의 사각형 × 삼각형 2개 × 정점 3개
    const int quadCount = (subW - 1) * (subH - 1);
    QByteArray indexBytes(quadCount * 6 * sizeof(uint32_t), Qt::Uninitialized);
    uint32_t* ip = reinterpret_cast<uint32_t*>(indexBytes.data());
    for (int sr = 0; sr < subH - 1; ++sr) {
        for (int sc = 0; sc < subW - 1; ++sc) {
            // 사각형 4개 꼭짓점 인덱스
            uint32_t tl = static_cast<uint32_t>(sr * subW + sc);       // 좌상단
            uint32_t tr = tl + 1;                                       // 우상단
            uint32_t bl = tl + static_cast<uint32_t>(subW);            // 좌하단
            uint32_t br = bl + 1;                                       // 우하단
            // 삼각형 1: 좌상단→좌하단→우상단
            *ip++ = tl; *ip++ = bl; *ip++ = tr;
            // 삼각형 2: 우상단→좌하단→우하단
            *ip++ = tr; *ip++ = bl; *ip++ = br;
        }
    }

    // GPU 버퍼 생성
    auto* geometry = new Qt3DRender::QGeometry();
    auto* vBuf = new Qt3DRender::QBuffer(geometry);
    vBuf->setData(vertexBytes); // 정점 데이터 GPU에 업로드
    auto* iBuf = new Qt3DRender::QBuffer(geometry);
    iBuf->setData(indexBytes);  // 인덱스 데이터 GPU에 업로드

    // 정점 속성 등록 헬퍼: 하나의 버퍼 안에서 offset·stride로 각 속성 위치 지정
    auto addAttr = [&](const QString& name, int offset, int size) {
        auto* a = new Qt3DRender::QAttribute(geometry);
        a->setName(name);
        a->setVertexBaseType(Qt3DRender::QAttribute::Float);
        a->setVertexSize(size);                      // 이 속성이 float 몇 개인지
        a->setByteOffset(offset * sizeof(float));    // 정점 시작에서 몇 바이트 뒤
        a->setByteStride(8 * sizeof(float));         // 다음 정점까지 몇 바이트
        a->setCount(static_cast<uint>(subW * subH)); // 전체 정점 수
        a->setBuffer(vBuf);
        geometry->addAttribute(a);
    };
    addAttr(Qt3DRender::QAttribute::defaultPositionAttributeName(),          0, 3); // 위치 (offset=0, 3floats)
    addAttr(Qt3DRender::QAttribute::defaultNormalAttributeName(),            3, 3); // 노말 (offset=3, 3floats)
    addAttr(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName(), 6, 2); // UV   (offset=6, 2floats)

    // 인덱스 속성 등록
    auto* idxAttr = new Qt3DRender::QAttribute(geometry);
    idxAttr->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
    idxAttr->setVertexBaseType(Qt3DRender::QAttribute::UnsignedInt);
    idxAttr->setCount(static_cast<uint>(quadCount * 6));
    idxAttr->setBuffer(iBuf);
    geometry->addAttribute(idxAttr);

    // 지오메트리 렌더러: 어떤 방식으로 그릴지 (Triangles = 인덱스 3개마다 삼각형 1개)
    auto* renderer = new Qt3DRender::QGeometryRenderer();
    renderer->setGeometry(geometry);
    renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

    // OSM 텍스처 로드
    auto* tex = new Qt3DRender::QTexture2D(_rootEntity);
    tex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);  // 축소 시 부드럽게
    tex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear); // 확대 시 부드럽게
    auto* texImg = new Qt3DRender::QTextureImage(tex);
    texImg->setSource(QUrl::fromLocalFile(_osmTexPath)); // 임시 파일에서 로드
    texImg->setMirrored(false); // OpenGL 기본값은 Y축 뒤집힘 → false로 꺼야 북쪽이 위로
    tex->addTextureImage(texImg);

    // 재질: 텍스처 맵 + 주변광/반사광
    auto* mat = new Qt3DExtras::QDiffuseMapMaterial(_rootEntity);
    mat->setDiffuse(tex);                   // OSM 이미지를 텍스처로
    mat->setAmbient(QColor(180, 180, 180)); // 그늘진 부분도 너무 어둡지 않게
    mat->setSpecular(QColor(20, 20, 20));   // 반사광은 거의 없게
    mat->setShininess(5.0f);               // 낮을수록 무광

    // 엔티티에 렌더러와 재질 붙이기
    _meshEntity = new Qt3DCore::QEntity(_rootEntity);
    _meshEntity->addComponent(renderer);
    _meshEntity->addComponent(mat);
}


// =============================================================
// 차량 마커: 위경도 → 3D 좌표로 변환 후 빨간 구체 배치
// =============================================================
void TerrainWidget::updateVehicleMarker()
{
    // 위경도가 아직 없거나 지형 데이터가 없으면 스킵
    if (_vehicleLat == 0.0 && _vehicleLon == 0.0) return;
    if (!_currentTile.isValid()) return;

    // 기존 마커 제거
    if (_vehicleMarker) {
        _vehicleMarker->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        delete _vehicleMarker;
        _vehicleMarker = nullptr;
    }

    // 차량 위경도 → 글로벌 픽셀 좌표 (onFetchClicked와 동일한 변환)
    double fracX = (_vehicleLon + 180.0) / 360.0 * (1 << _zoom);
    double latR  = _vehicleLat * M_PI / 180.0;
    double fracY = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI)
                   / 2.0 * (1 << _zoom);
    double gVX = fracX * 256; // 차량 글로벌 픽셀 X
    double gVY = fracY * 256; // 차량 글로벌 픽셀 Y

    // 글로벌 픽셀 → 스티칭 이미지 내 좌표
    double sVX = gVX - _osmTX0 * 256;
    double sVY = gVY - _osmTY0 * 256;

    // 스티칭 이미지 내 좌표 → 서브영역 중심 기준 오프셋
    double relX = sVX - _stitchCX; // 중심에서 동쪽으로 몇 픽셀
    double relY = sVY - _stitchCY; // 중심에서 남쪽으로 몇 픽셀

    // 서브영역 밖이면 마커 표시 안 함
    if (std::abs(relX) > _subHalfPx || std::abs(relY) > _subHalfPx) return;

    const float scale       = _worldHalfSize / static_cast<float>(_subHalfPx);
    const float heightScale = 0.05f;

    // 픽셀 오프셋 → 배열 인덱스로 변환해서 해당 위치 높이값 읽기
    int sc = static_cast<int>(relX + _subHalfPx);
    int sr = static_cast<int>(relY + _subHalfPx);
    float h = _currentTile.heightAt(sc, sr);

    // 픽셀 오프셋 → 3D 월드 좌표
    float xPos = static_cast<float>(relX) * scale;
    float zPos = static_cast<float>(relY) * scale;
    float yPos = h * heightScale + 8.0f; // 지형 위 8유닛 띄워서 묻히지 않게

    // 빨간 구체 생성
    _vehicleMarker = new Qt3DCore::QEntity(_rootEntity);
    auto* sphere = new Qt3DExtras::QSphereMesh(_vehicleMarker);
    sphere->setRadius(5.0f);
    sphere->setRings(8);    // 위아래 분할 수 (낮을수록 성능↑)
    sphere->setSlices(8);   // 좌우 분할 수

    auto* mat = new Qt3DExtras::QPhongMaterial(_vehicleMarker);
    mat->setAmbient(QColor(200, 20, 20));    // 그늘 색
    mat->setDiffuse(QColor(255, 60, 60));    // 기본 색 (밝은 빨강)
    mat->setSpecular(QColor(255, 200, 200)); // 반사광 색
    mat->setShininess(60.0f);               // 반사 강도

    auto* t = new Qt3DCore::QTransform();
    t->setTranslation(QVector3D(xPos, yPos, zPos));

    _vehicleMarker->addComponent(sphere);
    _vehicleMarker->addComponent(mat);
    _vehicleMarker->addComponent(t);
}

// =============================================================
// Load Terrain 완료 후 초기 카메라 시점 설정
//
// 우선순위:
//   1. 로봇 마커가 맵 위에 있음  → 로봇 3D 위치 기준
//   2. GPS 수신됐지만 맵 밖      → 맵 중심(0,0,0) 기준
//   3. GPS 없음 (기본값)         → 맵 중심(0,0,0) 기준
//
// 카메라 배치:
//   - 정북향(−Z 방향) 바라봄
//   - 피치 약 −20° (수평에서 20° 아래)
//   - 대상 기준 위로 30유닛, 남쪽으로 80유닛 뒤에 위치
//     → atan(30/80) ≈ 20.6° 피치
// =============================================================
void TerrainWidget::resetCameraToTerrain()
{
    Qt3DRender::QCamera* cam = _view->camera();

    QVector3D target(0.0f, 0.0f, 0.0f); // 기본값: 맵 중심

    // 우선순위 1: 로봇 마커가 맵 위에 배치된 경우
    if (_vehicleMarker && _currentTile.isValid()) {
        double fracX = (_vehicleLon + 180.0) / 360.0 * (1 << _zoom);
        double latR  = _vehicleLat * M_PI / 180.0;
        double fracY = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI)
                       / 2.0 * (1 << _zoom);
        double relX  = (fracX * 256 - _osmTX0 * 256) - _stitchCX;
        double relY  = (fracY * 256 - _osmTY0 * 256) - _stitchCY;

        const float scale       = _worldHalfSize / static_cast<float>(_subHalfPx);
        const float heightScale = 0.05f;
        int sc = static_cast<int>(relX + _subHalfPx);
        int sr = static_cast<int>(relY + _subHalfPx);
        float h = _currentTile.heightAt(sc, sr);

        target = QVector3D(static_cast<float>(relX) * scale,
                           h * heightScale,
                           static_cast<float>(relY) * scale);
    }
    // 우선순위 2, 3: GPS 있든 없든 맵 중심 사용 (target 이미 (0,0,0))

    // 미터 → 월드 유닛 변환: _worldHalfSize(128) = halfRange 미터
    // halfRange = _subHalfPx * mpp ≈ 500m
    float mpp          = static_cast<float>(metersPerPixel(_lat, _zoom));
    float unitsPerMeter = _worldHalfSize / (_subHalfPx * mpp);

    // 카메라: 대상에서 위로 700m, 남쪽(+Z)으로 500m → 정북 바라봄
    float up    = 700.0f * unitsPerMeter;
    float south = 500.0f * unitsPerMeter;
    cam->setPosition(target + QVector3D(0.0f, up, south));
    cam->setViewCenter(target);
    cam->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));
}
