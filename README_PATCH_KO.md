# OMR Folder Scanner - 초기화 진단 수정본

이 수정본은 Friendly OMR 32비트 TWAIN 연결 실패를 진단하기 위한 소스 수정본입니다.

## 주요 변경

- `CScanner_FreeImage.cpp`: 폴더 생성 로직을 수정했습니다. 기존 코드는 중간 경로의 끝에 역슬래시가 붙은 문자열을 `CreateDirectoryA`에 전달하고 오류를 무시했습니다. 수정본은 경로 구성요소별로 생성하고 성공 여부를 기록합니다.
- `CTWAINDS_Sample1.cpp`: TWAIN `DS_Entry` 호출, 데이터소스 객체 생성, `Initialize()` 반환값을 임시폴더 로그에 기록합니다.
- `CTWAINDS_FreeImage.cpp`: `Initialize()` 내부 실패 지점을 소스 행 번호로 기록합니다. 또한 `CAP_SUPPORTEDDATS` 컨테이너의 ID가 잘못 지정되어 있던 부분을 바로잡았습니다.
- `install/install.bat`: 아키텍처·파일 복사·폴더 생성 실패를 숨기지 않고 `C:\VirtualScan\install.log`에 기록합니다.
- `install/uninstall.bat`: 실제 제거 여부를 확인하고 실패 시 로그를 남깁니다.

## 빌드

GitHub 저장소의 기존 파일을 이 ZIP 안의 `virtual-scanner` 폴더 내용으로 교체한 뒤 `main` 또는 `master`에 push하거나 Actions의 `Build virtual scanner`를 수동 실행합니다.

Friendly OMR은 32비트이므로 Actions 결과물 중 **`OMRFolderScanner-Win32`** 아티팩트를 사용합니다. `OMRFolderScanner-x64`는 사용하지 마세요.

## 설치 및 진단

1. 기존 Friendly OMR을 완전히 종료합니다.
2. 관리자 권한으로 기존 드라이버의 `uninstall.bat`을 실행합니다. 제거가 실패하면 `uninstall.log`를 확인합니다.
3. 새 `OMRFolderScanner-Win32` 아티팩트를 별도 폴더에 압축 해제합니다.
4. `install.bat`을 관리자 권한으로 실행합니다.
5. `C:\VirtualScan\in`, `done`, `bad` 폴더가 생성되는지 확인합니다.
6. Friendly OMR을 실행해 `OMR Folder Scanner`를 선택합니다.

## 로그 위치

- 설치: `C:\VirtualScan\install.log`
- TWAIN 열기/초기화: `%TEMP%\OMRFolderScanner-ds.log`
- 가상 급지: `C:\VirtualScan\feed.log` (폴더 생성 및 데이터소스 생성 이후)

`%TEMP%\OMRFolderScanner-ds.log`에서 다음을 확인합니다.

- `Opening data source`: Friendly OMR이 실제 데이터소스 열기를 요청했는지
- `Data source object constructed`: C++ 데이터소스 객체 생성까지 성공했는지
- `Initialize failed at ...:행번호`: 초기화 중 어느 코드에서 실패했는지
- `Initialize returned 0`: TWAIN 초기화가 성공했는지

## 주의

이 환경에서는 Windows/MSVC 컴파일러와 Friendly OMR 실행 환경을 사용할 수 없어, 수정 소스의 실제 Win32 빌드 및 Friendly OMR 실기동 검증은 아직 수행하지 못했습니다. GitHub Actions에서 Win32 빌드 성공 여부를 먼저 확인한 뒤 실제 PC에서 테스트해야 합니다.
