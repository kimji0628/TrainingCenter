# TrainingContentPlayer

범용 교육 콘텐츠 플레이어 — YouTube, PDF, 이미지, 텍스트 설명을 외부 JSON 파일로 관리하고 순서대로 학습할 수 있는 MFC 기반 교육 프로그램입니다.

## 개발 환경

- Visual Studio 2022
- MFC Dialog Based Application
- Unicode / x64 / C++17
- JSON 라이브러리: [nlohmann/json](https://github.com/nlohmann/json) v3.12.0

## 빌드 방법

1. Visual Studio 2022에서 `TrainingContentPlayer.sln` 열기
2. 구성: **Release | x64** (또는 Debug | x64)
3. **빌드 → 솔루션 빌드** (Ctrl+Shift+B)
4. 실행 파일이 `..\Bin\TrainingContentPlayer.exe`에 출력됩니다.

## 폴더 구조

```
TrainingCenter/
├── Bin/                            # 실행 및 런타임 데이터
│   ├── TrainingContentPlayer.exe
│   ├── Data/                       # 코스 JSON 파일
│   ├── Images/                     # 썸네일 이미지
│   ├── Pdf/                        # PDF 자료
│   └── Progress/                   # 학습 진행 상태
│       └── Progress.json
└── HydraulicSyatem/
    ├── TrainingContentPlayer.sln
    └── TrainingContentPlayer/      # 소스 코드
```

## JSON 파일 형식

```json
{
  "CourseName": "Aircraft Hydraulic System",
  "Lessons": [
    {
      "Title": "Introduction",
      "Description": "설명 텍스트",
      "YoutubeUrl": "https://www.youtube.com/watch?v=xxxxxxxx",
      "PdfFile": "Pdf\\Intro.pdf",
      "ImageFile": "Images\\Intro.png"
    }
  ]
}
```

## 새 코스 추가 방법

1. `Bin/Data/` 폴더에 새 JSON 파일 추가 (예: `MAVLink.json`)
2. `Bin/Images/`, `Bin/Pdf/` 폴더에 해당 리소스 파일 배치
3. 프로그램 재시작 — **재컴파일 불필요**

## 주요 기능

| 기능 | 설명 |
|------|------|
| JSON 자동 로딩 | 시작 시 Data 폴더의 모든 JSON 파일을 읽어 Tree 생성 |
| Lesson 선택 | Tree 선택 시 제목, 설명, 썸네일 표시 |
| 영상보기 | WebView2 내장 플레이어에서 YouTube 재생 |
| PDF보기 | 기본 PDF 뷰어로 PDF 파일 실행 |
| 이미지보기 | 확대 창에서 이미지 표시 |
| 학습완료 | Progress.json에 완료 상태 저장 |
| 이전/다음 | 버튼으로 Lesson 간 이동 (코스 경계 포함) |

## 향후 확장 계획

- **Version 2**: 퀴즈 기능
- **Version 3**: 애니메이션 표시
- **Version 4**: 블록도 시뮬레이션
- **Version 5**: 교육용 실습 시뮬레이터
