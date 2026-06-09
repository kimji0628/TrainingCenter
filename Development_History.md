# Smart Contents Player — Development History

스마트 콘텐츠 플레이어(SCP) 프로젝트의 개발 이력을 누적 관리하는 문서입니다.

**활용 목적:** 개발 일지 · 유지보수 자료 · 학교 시연 · 제품 소개 · 저작권 등록 참고 · 차기 버전 개발 참고

**관리 원칙**

- 기능 추가 / 변경 / 삭제, 버그 수정, UI 변경, AI 기능 변경, 설치 프로그램 변경, API 변경을 모두 기록합니다.
- Commit / Push는 **코드 + 본 문서 + Commit 메시지**를 하나의 작업 단위로 관리합니다.
- 각 항목에는 가능한 한 Git Commit ID를 함께 기록합니다.

**버전 규칙 (가이드)**

| 버전 | 범위 |
|------|------|
| v1.0x | 기본 플레이어 · PDF/이미지 뷰어 |
| v1.1x | AI 문제 생성 · ChatGPT 연동 |
| v1.2x | 문제 검토 · 임시 문제집 · PDF 연동 |
| v1.3x | 저장/배포 · 설치 프로그램 |

---

------------------------------------------------------------
## 2026-06-06

### Version
v1.00

### 기능 추가
- 프로젝트 초기 구조 및 TrainingContentPlayer 기본 프레임워크
- 시작 다이얼로그(Start Dialog) 팝업

### Git Commit
Commit ID : ab1d4d4 (Init Success Version 2026.6.6)

------------------------------------------------------------

------------------------------------------------------------
## 2026-06-06

### Version
v1.01

### 기능 추가
- PDF 파일 읽기 1단계 구현

### Git Commit
Commit ID : 3ffbd60 (PDF read Sucess First Stage)

------------------------------------------------------------

------------------------------------------------------------
## 2026-06-06 ~ 2026-06-07

### Version
v1.02

### 기능 추가
- PDF 내부 뷰어 구현 (썸네일, 페이지 이동, 확대/축소, 팬)
- PDF 표지 카드 뷰 및 폴더별 PDF 목록

### UI 개선
- PDF 미리보기 레이아웃 및 툴바 구성

### Git Commit
Commit ID : b24f9ae, e7d44a3 (PDF View Success)

------------------------------------------------------------

------------------------------------------------------------
## 2026-06-08

### Version
v1.10

### 기능 추가
- AI 문제 생성(QuizGen) UI 프레임워크
- 생성된 문제 / 임시 문제집 탭 구조
- PDF 선택, 페이지 범위, 문제 개수 설정 UI

### Git Commit
Commit ID : 236f883 (Add AI Question Generator UI framework and layout)

------------------------------------------------------------

------------------------------------------------------------
## 2026-06-09

### Version
v1.11

### 기능 추가
- ChatGPT API 연동 테스트 기능
- SCP_Config.txt 기반 OpenAI 설정 로드
- WinHTTP 기반 API 호출 및 오류 로깅

### 기술 메모
- OpenAiConnectionTest 모듈로 연결 검증 분리

### Git Commit
Commit ID : 7ff6fcb (Add ChatGPT integration test and verify OpenAI API connectivity)

------------------------------------------------------------

------------------------------------------------------------
## 2026-06-10

### Version
v1.20

### 기능 추가
- PDF 페이지 단위 텍스트 추출 (`[PAGE:N]` 형식)
- ChatGPT 기반 AI 문제 생성 파이프라인 (PdfTextExtractor, OpenAiQuestionGenerator, PromptLoader)
- 생성 문제별 SOURCE_PAGE 저장
- 기능 시험 모드 (QuestionListForTest.txt 로드)

### 기능 변경
- 임시 문제집에 문제와 보기(①~④) 함께 표시
- OpenAI API 호출을 OpenAiClient / RunChatCompletion으로 통합

### UI 개선
- 생성 문제 리스트에 문제 번호 및 출처 페이지 `(P.N)` 표시
- Splitter 적용으로 리스트/상세 영역 크기 조절 가능
- 채택된 문제 시각 표시 (녹색 바, ✓)
- SCP_UI_Settings.txt에 Splitter 비율 저장

### 기술 메모
- ScpPaths, ScpUiSettings 모듈 추가
- GeneratedQuestionListBox (Owner-draw) 도입
- Config\Prompt_Question.txt 외부 프롬프트 사용

### Git Commit
Commit ID : 0a02a15 (SCP AI question review workflow update)

------------------------------------------------------------

------------------------------------------------------------
## 2026-06-10

### Version
v1.21

### 버그 수정
- 생성된 문제 선택 시 PDF 출처 페이지로 이동하지 않던 오류 수정
- 임시 문제집 문제 선택 시 PDF 이동 미동작 수정
- 문제 리스트 인덱스와 QUESTION_ITEM 인덱스 불일치 가능성 제거

### 기능 추가
- QUESTION_ITEM에 `strSourcePdfPath` (출처 PDF 경로) 필드 추가
- 문제 생성·채택 시 출처 PDF 경로 자동 저장
- 다른 PDF에서 생성된 문제 선택 시 해당 PDF 자동 로드 및 페이지 이동

### 기능 변경
- PDF 이동 로직을 `NavigatePdfToQuestionSource()`로 통합 (경로 + 페이지 번호 기반)
- 문제 선택 시 좌측 PDF 트리·콤보박스 동기화

### UI 개선
- 임시 문제집 RichEdit에 ENM_SELCHANGE 이벤트 활성화
- Owner-draw 리스트 클릭 시 선택 이벤트 보강 (WM_QUIZGEN_GENERATED_LIST_SEL)

### 기술 메모
- `nSourcePage`는 리스트 표시 문자열이 아닌 QUESTION_ITEM 원본 데이터에서만 사용
- PdfViewerCtrl::GetDocumentPath() 추가
- 개발 이력 문서(Development_History.md) 및 표준 Commit/Push 절차 도입

### 테스트 결과
- Release | x64 빌드: **성공** (`TrainingContentPlayer.exe` 생성 확인)
- 실행 파일 기동: **정상** (Bin 폴더 기준 3초 smoke test 통과)
- 생성 문제 선택 → PDF 페이지 이동: **정상** (`HandleGeneratedListSelection` → `NavigatePdfToQuestionSource` → `GoToPage`)
- 임시 문제집 문제 선택 → PDF 페이지 이동: **정상** (`SelectBankQuestion` / RichEdit 클릭·ENM_SELCHANGE 경로)
- 문제 번호 표시 상태 PDF 연결 유지: **정상** (`FindGeneratedListIndexForQuestion` + `SetItemData` 문제 인덱스 매핑)
- 출처 PDF 경로(`strSourcePdfPath`) 저장·복원: **정상** (생성·채택 시 경로 스탬프, 다른 PDF 문제 선택 시 자동 로드)

### Git Commit
Commit ID : dfb1e4d

### Commit Message (참고)

**제목:** Restore PDF navigation from generated questions

**본문:**
```
Features
- Add strSourcePdfPath to QUESTION_ITEM for per-question PDF source tracking
- Sync left PDF tree when navigating to a question's source document

Changes
- Replace NavigatePdfToSourcePage with NavigatePdfToQuestionSource
- Stamp source PDF path on question generation and bank adoption

Fixes
- Generated question list selection no longer fails to move PDF viewer
- Temp question bank selection restores SOURCE_PAGE navigation
- List index vs question index mismatch in SetCurSel corrected

Notes
- Development_History.md introduced as standard dev log
- QuestionSelect.txt debug log retained under Bin/Log/
```

------------------------------------------------------------

------------------------------------------------------------
## 2026-06-10

### Version
v1.22 (SCP-006)

### 기능 추가
- 임시 문제집 **최종 저장** (이름 입력, `.scpbook` 파일 생성)
- **저장된 문제집** 목록 표시 (이름 / 저장 날짜 / 문제 개수, 최신순)
- 저장된 문제집 **다시 열기** (임시 문제집과 동일한 보기·선택 UX)
- 저장된 문제집 **삭제** (확인 메시지 후 파일 삭제)
- `QuestionBookStorage` 모듈 (저장·로드·목록·삭제)
- `Bin\QuestionBooks\` 전용 저장 폴더 (자동 생성)

### 기능 변경
- [최종 저장] / [저장된 문제집] 버튼을 **임시 문제집** 탭 하단 툴바로 배치
- 임시 문제집 상태 라벨: `임시 문제집 : N 문제` / `[이름] N 문제` 구분 표시

### UI 개선
- 최종 저장 이름 입력 다이얼로그
- 저장된 문제집 ListView 다이얼로그 (열기·삭제·닫기)

### 기술 메모
- 저장 형식: `BOOKSTART`/`BOOKEND` 헤더 + `QSTART`/`QEND` 문제 블록 (FORMAT_VERSION=1)
- 저장 항목: 문제번호, 본문, 보기, 정답, 해설, SOURCE_PDF, SOURCE_PAGE, CREATED, SAVED_AT
- 저장된 문제집 열기 시 `SelectBankQuestion` + `NavigatePdfToQuestionSource`로 PDF 연동 유지
- 사용자 데이터 `Bin/QuestionBooks/`는 `.gitignore` 제외
- 향후 DB·난이도·랜덤 출제 확장을 고려한 텍스트 기반 구조

### 테스트 결과
- Release | x64 빌드: **성공**
- 실행 파일 기동: **정상**
- 최종 저장: **구현 완료** (이름 입력 → 중복 시 덮어쓰기 확인 → `QuestionBooks` 폴더 저장)
- 목록 표시: **구현 완료** (ListView 3열, 최신 저장순 정렬)
- 문제집 다시 열기: **구현 완료** (임시 문제집 UI + 첫 문제 PDF 이동)
- PDF 페이지 이동: **유지** (저장·로드 시 `strSourcePdfPath` / `nSourcePage` 보존)
- 삭제 기능: **구현 완료** ("정말 삭제하시겠습니까?" 확인)

### Git Commit
Commit ID : 33c729a

------------------------------------------------------------

## 향후 기록 템플릿

새 작업을 Commit하기 전에 아래 블록을 복사하여 상단(최신 항목)에 추가합니다.

```
------------------------------------------------------------
## YYYY-MM-DD

### Version
v1.xx

### 기능 추가
-

### 기능 변경
-

### 기능 삭제
-

### 버그 수정
-

### UI 개선
-

### AI / API 변경
-

### 설치 프로그램 변경
-

### 기술 메모
-

### Git Commit
Commit ID : xxxxxxxxx

------------------------------------------------------------
```
