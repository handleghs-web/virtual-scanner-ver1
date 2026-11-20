# OMR Folder Scanner (가상 TWAIN 스캐너)

폴더에 넣은 이미지를 **스캐너 용지함의 용지처럼 한 장씩** 스캔 프로그램(예: 프렌들리 OMR)에 넘겨주는 TWAIN 드라이버(데이터 소스)입니다.
TWAIN 작업그룹의 공식 샘플 데이터 소스(Twain_DS_sample01)를 바탕으로, Qt 없이 빌드되도록 하고 이미지 공급부를 폴더 방식으로 바꿨습니다.

> 이 프로그램은 실제 스캔 이미지를 대신하는 **연동용 도구**입니다. 병원·업체(엘시테크, 누리온)에 알리고 **시험용 PC와 가명 자료로 먼저** 확인한 뒤 사용하세요.

## 0단계: 공식 샘플로 먼저 가능성 확인 (5분, 빌드 불필요)

내 코드를 쓰기 전에, "프렌들리 OMR이 가상 TWAIN 스캐너를 받아주는가"만 먼저 확인합니다.

1. https://github.com/twain/twain-samples/tree/master/releases/Twain_Ds_sample01_02040100 에서
   프렌들리 OMR이 쓰는 비트 수에 맞는 `twainds.win32.installer.msi`(32비트) 또는 `twainds.win64.installer.msi`(64비트)를 받아 **시험용 PC**에 설치합니다.
2. 프렌들리 OMR의 스캐너 선택 화면에 **"TWAIN2 Software Scanner"** 가 나타나는지 봅니다.
3. 그 스캐너로 스캔을 눌러 TWAIN 로고 이미지가 들어오는지 봅니다(OMR로 인식은 안 되어도 됩니다).
   - 목록에 보이고 스캔이 시작되면 → 이 방법이 가능한 것입니다. 아래 1단계로 진행하세요.
   - 목록에 없으면 → 프렌들리 OMR이 TWAIN이 아닌 다른 방식(ISIS, 전용 드라이버)일 수 있습니다. 알려주세요.

## 1단계: 빌드 (GitHub Actions)

1. 이 폴더 내용 전체(숨김 폴더 `.github` 포함)를 새 GitHub 저장소(비공개 권장)에 올립니다.
2. Actions 탭 → `Build virtual scanner` 실행(자동 실행 또는 Run workflow).
3. 끝나면 Artifacts에서 `OMRFolderScanner-Win32`(32비트용)와 `OMRFolderScanner-x64`(64비트용)를 받습니다.
   **프렌들리 OMR 프로그램의 비트 수와 같은 쪽**을 설치해야 합니다(작업 관리자 → 세부 정보에서 프로그램 이름에 `*32`가 붙으면 32비트).

## 2단계: 설치

받은 폴더에서 `install.bat`을 **관리자 권한으로 실행**합니다.
- `C:\Windows\twain_32\OMRFolderScanner`(32비트) 또는 `twain_64\OMRFolderScanner`(64비트)에 복사됩니다.
- `C:\VirtualScan\in`, `done`, `bad` 폴더가 만들어집니다.
- 제거: `uninstall.bat`을 관리자 권한으로 실행.

## 3단계: 일반 스캔 프로그램으로 시험

1. `python make_test_pages.py` 실행(Pillow 필요: `pip install pillow`) → `C:\VirtualScan\in`에 시험 이미지 3장이 생깁니다.
2. 무료 스캔 프로그램(예: NAPS2)에서 **TWAIN** 드라이버로 "OMR Folder Scanner"를 선택해 스캔합니다.
3. 3장이 순서대로 들어오고, 이미지가 `C:\VirtualScan\done`으로 옮겨지면 정상입니다. 로그는 `C:\VirtualScan\feed.log`.

## 4단계: 프렌들리 OMR에서 사용

1. 스캐너 선택에서 **OMR Folder Scanner** 를 고릅니다.
2. `C:\VirtualScan\in`에 한 사람(또는 여러 사람) 분량의 페이지 이미지를 **스캔 순서대로 파일명을 붙여** 넣습니다.
   - 예: `0001_barcode.tif`(등록번호 바코드 장) → `0002_p1003.tif` → `0003_p6002.tif` → `0004_barcode.tif`(다음 사람) …
3. 프렌들리 OMR에서 스캔을 시작하면, 폴더의 이미지가 순서대로 읽히고 다 읽으면 용지가 없음으로 끝납니다.

### 이미지 넣는 규칙
- 파일 이름 순서(영문·숫자 오름차순, 대소문자 무시)로 읽습니다. **앞자리를 0으로 채우세요**(`0001`, `0002` …).
- 지원 형식: `.tif .tiff .png .jpg .jpeg .bmp`
- 파일을 만드는 중에 읽히지 않도록 **`.tmp` 등 다른 확장자로 저장한 뒤 이름을 바꾸세요**(make_test_pages.py 참고).
- **DPI 정보가 정확히 들어 있어야** 합니다(실제 스캔과 같은 해상도·크기, 예: A4 200dpi = 1654×2339).
  프로그램이 요청한 해상도와 이미지 DPI가 같으면 **이미지를 그대로** 전달하고, 다르면 비율대로 확대·축소합니다.
- 읽은 이미지는 `done`으로 옮겨집니다. 이미지가 아닌 파일은 `bad`로 옮겨지고 다음 파일로 넘어갑니다.

## 설정 (`feed.ini`, 설치 폴더에 있음)
| 항목 | 기본값 | 의미 |
|---|---|---|
| `root` | `C:\VirtualScan` | 기본 폴더(`in`, `done`, `bad`, `feed.log`) |
| `full_page` | `1` | 프로그램이 지정한 용지 크기·영역과 상관없이 **이미지 전체**를 전달 |
| `image_dpi` | `0` | 0이면 파일의 DPI 사용, 200 등으로 하면 모든 이미지를 그 DPI로 간주 |

## 문제 해결
- 스캐너 목록에 안 보임: 비트 수(32/64) 불일치, 또는 `install.bat`을 관리자 권한으로 실행하지 않음.
- 스캔 시작 시 "용지 없음": `C:\VirtualScan\in`이 비어 있음. `feed.log` 확인.
- 드라이버가 열리지 않음: `FreeImage.dll`이 설치 폴더에 있는지 확인. 필요하면 "Microsoft Visual C++ 재배포 가능 패키지 2015-2022(해당 비트)"를 설치.
- 이미지가 인식되지 않음: `feed.log`에 프로그램이 요청한 색상(pixeltype)·해상도와 이미지 DPI가 기록됩니다. 실제 스캔본의 해상도·색 깊이와 맞추세요.

## 확인된 것과 확인되지 않은 것
확인함(윈도우 호환 환경에서 TWAIN 요청을 직접 보내 시험):
- 파일 이름 순서대로 한 장씩 전달, 전달한 파일을 `done`으로 이동
- A4(1654×2339) 이미지를 200dpi 요청 시 크기 변형 없이 전달, 300dpi 요청 시 비례 확대
- 흑백·회색조 요청 처리, 이미지가 아닌 파일은 `bad`로 이동하고 계속 진행, 빈 폴더에서 "용지 없음"(TWCC_NOMEDIA)

확인하지 못함(실제 환경에서 확인 필요):
- Visual Studio(MSVC) 빌드 결과, 실제 TWAIN 매니저(DSM)와 프렌들리 OMR 연동, OMR 인식 결과

## 라이선스
TWAIN Working Group 샘플 코드의 라이선스를 따릅니다(`LICENSE-TWAIN-sample.txt`). FreeImage 라이브러리(`ext/`)는 해당 라이선스를 따릅니다.
