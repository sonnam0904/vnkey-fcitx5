# Kế hoạch Rebrand **Telebit** — dòng sản phẩm **`telebit-fcitx5`** (chỉ TELEX + UTF-8, hướng “siêu nhẹ”)

## Bối cảnh & lý do cần rebrand

Addon fcitx5 hiện đang “định danh” dưới key `vnkey` và cài các file kiểu:

- `…/lib/…/fcitx5/vnkey.so`
- `…/share/fcitx5/addon/vnkey.conf`
- `…/share/fcitx5/inputmethod/vnkey.conf`
- config nội bộ addon: `conf/vnkey.conf`

Một số repo khác cũng dùng đúng bộ key/filenames đó và có thể đặt **cùng tên gói** `vnkey-fcitx5`, nên cài song song dễ **ghi đè file** và fcitx5 chỉ còn **một slot** IM.

Mục tiêu: đổi sang thương hiệu/ID riêng để **cùng tồn tại** với các build `vnkey` khác.

---

## Chốt brand (theo yêu cầu hiện tại)

| Khái niệm | Giá trị chốt | Ghi chú |
|---|---|---|
| **Tên dòng sản phẩm / gói Debian–RPM** | **`telebit-fcitx5`** | Dùng cho `CPACK_PACKAGE_NAME`, `apt install/remove`, tên file `.deb`/`.rpm`, artifact CI |
| **Tên hiển thị ngắn (tuỳ chọn)** | **Telebit** | Đặt trong `Name=` để người đọc dễ (“không như tên gói có gạch ngang”) |
| **URL GitHub repo** | có thể vẫn là `…/vnkey-fcitx5` | URL repo **không bắt buộc** trùng tên gói; docs nên phân biệt “tên repo” vs “tên gói” |

---

## Mapping ID kỹ thuật — đồng bộ **`telebit-fcitx5`** xuyên suốt fcitx5

**Quy ước:** toàn bộ định danh “public” của addon/IM dùng cùng một chuỗi **`telebit-fcitx5`** (có dấu gạch ngang), để tên gói và tên addon không lệch nhau.

| Vai trò | Giá trị |
|---|---|
| fcitx5 addon `Name=` | `telebit-fcitx5` |
| fcitx5 addon `Library=` | `telebit-fcitx5` → fcitx5 load `telebit-fcitx5.so` |
| File descriptor addon (`share/fcitx5/addon/…`) | `telebit-fcitx5.conf` |
| File đăng ký IM (`share/fcitx5/inputmethod/…`) | `telebit-fcitx5.conf` |
| Config nội bộ engine (`conf/…`) | `conf/telebit-fcitx5.conf` |
| Chuỗi hiển thị gợi ý (`Name=` trong IM) | `Vietnamese Telex (UTF-8) — Telebit (telebit-fcitx5)` hoặc rút gọn nếu bạn muốn UI gọn hơn |

---

## Phương án triển khai: **A — Rebrand “cứng”** (không migrate config cũ)

### Tóm tắt

- Đổi mọi đường cài đặt / key fcitx5 từ `vnkey` → **`telebit-fcitx5`** (và đổi tên gói tương ứng).
- **Không** tự chuyển `conf/vnkey.conf` → `conf/telebit-fcitx5.conf`.
- Người dùng sẽ **add lại** IM sau khi cài; tùy chọn addon (vd. `DirectCommitRollback`) quay về mặc định trừ khi chỉnh lại tay.

### Mức độ ảnh hưởng

- Người dùng cuối: phải cấu hình lại IM + option.
- Release pipeline: đổi tên artifact/gói; semantic-release có thể cần chỉnh nhãn asset.
- Maintainer: **một lần** grep toàn repo (trừ thư mục build) để không sót chuỗi cũ.

---

## Bổ sung quan trọng khi ID có dấu `-` (CMake)

**CMake không cho phép tên `target` chứa dấu gạch ngang** (vd `telebit-fcitx5` không thể là tên target hợp lệ theo kiểu `add_library(telebit-fcitx5 …)`).

Cách chuẩn:

1. Đặt target CMake an toàn, ví dụ: `telebit_fcitx5`.
2. Ép tên file output đúng brand fcitx5:

```cmake
add_library(telebit_fcitx5 SHARED …)
set_target_properties(telebit_fcitx5 PROPERTIES
  PREFIX ""
  OUTPUT_NAME "telebit-fcitx5"
)
```

Khi đó `Library=telebit-fcitx5` trong addon descriptor sẽ khớp `telebit-fcitx5.so`.

---

## Checklist triển khai chi tiết (Phương án A)

### Bước 1 — Descriptor addon fcitx5

**File:** `telebit-fcitx5/src/telebit-fcitx5-addon.conf.in`

- `Name=vnkey` → `Name=telebit-fcitx5`
- `Library=vnkey` → `Library=telebit-fcitx5`

**Cài ra:** `…/share/fcitx5/addon/telebit-fcitx5.conf` (đổi `RENAME` trong CMake tương ứng).

### Bước 2 — Đăng ký input method

**File:** `telebit-fcitx5/src/telebit-fcitx5.conf.in`

- `Addon=vnkey` → `Addon=telebit-fcitx5`
- `Name=` → chuỗi hiển thị đã chốt (có thể gắn “Telebit” + “telebit-fcitx5” để tìm trong GUI dễ).

**Cài ra:** `…/share/fcitx5/inputmethod/telebit-fcitx5.conf`.

### Bước 3 — Config path nội bộ engine

**File:** `telebit-fcitx5/src/telebit_fcitx5.h`

- `conf/vnkey.conf` → `conf/telebit-fcitx5.conf`

### Bước 4 — Build & cài `.so`

**File:** `telebit-fcitx5/src/CMakeLists.txt`

- Đổi target + `OUTPUT_NAME` như mục “Bổ sung CMake”.
- Cập nhật `install(TARGETS …)` trỏ đúng target mới.
- Cập nhật `RENAME` của các file `.conf` → `telebit-fcitx5.conf`.

### Bước 5 — Metadata gói (CPack)

**File:** `telebit-fcitx5/CMakeLists.txt`

- `CPACK_PACKAGE_NAME` → `telebit-fcitx5`
- `CPACK_PACKAGE_VENDOR`, mô tả, maintainer: cập nhật wording “Telebit / telebit-fcitx5”.
- (Tuỳ chọn phát hành) thêm quan hệ gói với bản cũ để người dùng không kẹt hai gói:
  - Debian: `CPACK_DEBIAN_PACKAGE_REPLACES`, `CPACK_DEBIAN_PACKAGE_BREAKS` cho `vnkey-fcitx5`
  - Hoặc chỉ ghi rõ trong release notes: gỡ `vnkey-fcitx5` trước khi cài `telebit-fcitx5`

### Bước 6 — Biến môi trường CI/build (đặt theo thương hiệu Telebit)

Hiện repo vẫn có thể đang dùng prefix cũ kiểu `VNKEY_*` (ví dụ phiên bản cho `.deb`/`.rpm`). Khi rebrand, **mọi chỗ đổi tên từ `VNKEY` sang Telebit** theo quy ước env chuẩn: **`TELEBIT_*`** (chữ hoa + gạch dưới — `Telebit` là brand, `TELEBIT_` là prefix biến).

Ví dụ mapping (bám theo tên biến đang có trong repo, chỉ đổi prefix):

| Trước (`VNKEY_*`) | Sau (`TELEBIT_*`, brand Telebit) |
|---|---|
| `VNKEY_DEB_PACKAGE_VERSION` | `TELEBIT_DEB_PACKAGE_VERSION` |
| `VNKEY_DEB_PACKAGE_SUFFIX` | `TELEBIT_DEB_PACKAGE_SUFFIX` |
| `VNKEY_PACKAGE_VERSION` | `TELEBIT_PACKAGE_VERSION` |
| `VNKEY_RPM_PACKAGE_SUFFIX` | `TELEBIT_RPM_PACKAGE_SUFFIX` |

**Việc cần làm:**

- Cập nhật **`telebit-fcitx5/CMakeLists.txt`**: đọc phiên bản gói từ `ENV{TELEBIT_*}` (không còn đọc env cũ); đổi mọi biến CMake nội bộ mang tên cũ sang tên mới (ví dụ `_telebit_pkg_ver` hoặc `_fcitx_pkg_ver`).
- Cập nhật **toàn bộ script** dùng các biến này: `scripts/build-deb.sh`, `scripts/build-rpm.sh`, `scripts/build-release-debs.sh`, `scripts/build-release-rpm.sh`.
- Cập nhật **`.github/workflows/release.yml`** (và mọi workflow khác) truyền `-e TELEBIT_...` thay cho biến môi trường đã bỏ.

**Không khuyến nghị** giữ prefix env cũ sau khi đã chốt brand Telebit — dễ gây nhầm với gói `telebit-fcitx5`.

### Bước 7 — Workflow CI / artifact

**File:** `.github/workflows/release.yml`

- Đổi tên artifact upload cho khớp `telebit-fcitx5`.

### Bước 8 — Semantic-release asset labels

**File:** `.releaserc.json`

- Chỉnh label mô tả (nếu đang ghi “vnkey…”) để khớp tên gói mới.

### Bước 9 — Docs & script local

**Files:** `README.md`, `install.sh`, (tuỳ chọn) comment trong `scripts/build-*.sh`

- Mọi chỗ `vnkey-fcitx5` → `telebit-fcitx5`.
- Mọi đường dẫn uninstall `.so`/`.conf` → `telebit-fcitx5.*`.

### Bước 10 — Dọn file “rác” / tránh nhầm khi review

- Không commit thay đổi trong `telebit-fcitx5/build*/` (nếu có trong git — thêm ignore hoặc xoá local).
- Sau khi đổi tên, chạy `rg` trong repo (bỏ `build`) để bắt sót `vnkey` chỉ mang tính định danh sản phẩm.

---

## Acceptance criteria

- [ ] Đã cài: `telebit-fcitx5.so`, `addon/telebit-fcitx5.conf`, `inputmethod/telebit-fcitx5.conf`.
- [ ] `fcitx5-configtool` add được IM mới; không còn phụ thuộc `vnkey.so` / `vnkey.conf` của bản này.
- [ ] Gói `.deb`/`.rpm` mang tên **`telebit-fcitx5`**, artifact CI không còn nhãn `vnkey-fcitx5`.

---

## Các file dự kiến phải sửa (Phương án A)

| Khu vực | Đường dẫn |
|---|---|
| Addon descriptor template | `telebit-fcitx5/src/telebit-fcitx5-addon.conf.in` |
| Input method template | `telebit-fcitx5/src/telebit-fcitx5.conf.in` |
| CMake addon target + install | `telebit-fcitx5/src/CMakeLists.txt` |
| CPack + version env | `telebit-fcitx5/CMakeLists.txt` |
| Config path constants | `telebit-fcitx5/src/telebit_fcitx5.h` |
| Script cài nhanh | `install.sh` |
| Docs | `README.md` |
| CI | `.github/workflows/release.yml` |
| Release plugin | `.releaserc.json` |
| Script build (comment/env) | `scripts/build-deb.sh`, `scripts/build-rpm.sh`, `scripts/build-release-debs.sh`, `scripts/build-release-rpm.sh` |

---

*Trạng thái: kế hoạch triển khai — các thay đổi mã nguồn sẽ làm theo checklist trên khi bạn bảo “implement”.*
