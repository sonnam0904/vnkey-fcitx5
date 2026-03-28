## vnkey – Vietnamese Telex engine (C++ core + fcitx5 addon)

`vnkey` là một bộ gõ Telex tiếng Việt gồm:

- **Core C++ engine**: hàm `telex_to_unicode` chuyển chuỗi Telex ASCII → Unicode tiếng Việt (dùng được độc lập).
- **fcitx5 addon**: module `vnkey` cho Linux desktop (GNOME/KDE/…).

Thiết kế dựa trên **cấu trúc âm tiết tiếng Việt** (*Âm đầu + Vần + Thanh*). Mô tả chi tiết xem thêm trong `vietnamese.md`.

---

### 1. Cấu trúc thư mục

Trong thư mục gốc `vnkey/`:

- **`vietnamese.h/.cpp`**: API `telex_to_unicode(const std::string&)` – chuyển một chuỗi Telex thành Unicode.
- **`engine.h/.cpp`**: lớp `EngineVietCpp` – quản lý buffer gõ theo từng phím, dùng trong fcitx5.
- **`rime_table.*`**: bảng vần + vị trí “nguyên âm chính” để đặt dấu.
- **`canonicalize.*`**: pipeline canonicalize input (escape rules, tách âm đầu/vần, chuẩn hoá vị trí w/aa/ee/oo, trích xuất thanh…).
- **`render_utf8.*`**: render nội bộ → Unicode UTF‑8 (dấu, chữ hoa, v.v.).
- **`vnkey-fcitx/`**: mã nguồn addon cho fcitx5.
- **`install.sh`**: script cài đặt nhanh (build core + addon fcitx5).

---

### 2. Build core C++ và chạy test

**Yêu cầu:**

- Compiler hỗ trợ **C++17**.
- **CMake ≥ 3.10**.

**Cài công cụ build (gợi ý):**

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake

# Fedora
sudo dnf install -y gcc-c++ cmake make

# Arch
sudo pacman -S --needed base-devel cmake
```

**Build + chạy test:**

```bash
cmake -B build .
cmake --build build
./build/unikey_telex_cpp_tests
```

Nếu mọi thứ ổn, chương trình sẽ in:

```text
All C++ tests passed.
```

**Build sạch khi đổi nhánh/pull code lớn:**

```bash
rm -rf build
cmake -B build .
cmake --build build
./build/unikey_telex_cpp_tests
```

---

### 3. Cài addon `vnkey` cho fcitx5

#### 3.1. Yêu cầu

- Đã cài **fcitx5** và header phát triển (tuỳ distro: `libfcitx5core-dev`, `fcitx5-devel`, …).

**Gợi ý cài đặt (Linux):**

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
  fcitx5 fcitx5-configtool fcitx5-config-qt fcitx5-module-lua \
  libfcitx5core-dev libfcitx5utils-dev \
  extra-cmake-modules cmake build-essential

# Fedora (tên gói có thể thay đổi theo phiên bản)
sudo dnf install -y fcitx5 fcitx5-configtool fcitx5-devel

# Arch
sudo pacman -S --needed fcitx5 fcitx5-configtool
```

#### 3.2. Cài đặt thủ công bằng CMake

- **Cài toàn hệ thống** (đề xuất, cần `sudo`):

```bash
cd vnkey-fcitx
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr .
cmake --build build
sudo cmake --install build
```

- **Cài cho user hiện tại** (không dùng `sudo`, cài vào `$HOME/.local`):

```bash
cd vnkey-fcitx
cmake -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local" .
cmake --build build
cmake --install build
```

Sau khi cài, restart fcitx5:

```bash
fcitx5 -r
```

Nếu mới cài lần đầu mà chưa thấy `vnkey` trong danh sách input method, hãy **logout/login** hoặc **reboot** máy.

#### 3.3. Cài bằng gói `.deb` (Ubuntu / Debian và dòng Debian)

File `.deb` **không nằm trong repo**; mỗi bản được build trên GitHub Actions (hoặc bạn tự chạy `scripts/build-deb.sh` ở máy local). Gói **`vnkey-fcitx5`** chứa addon fcitx5; trường `Depends` kéo **fcitx5** và gói `libfcitx5core*` phù hợp.

**1. Tải `.deb`**

| Nguồn | Cách lấy |
|--------|-----------|
| **Releases** | [Releases](https://github.com/sonnam0904/vnkey-fcitx5/releases) → mỗi tag thường có **hai** `.deb`: tên gói có hậu tố **`+jammy`** (22.04) hoặc **`+noble`** (24.04) — chọn đúng bản cho máy. |
| **Artifacts** | Mỗi push `main` có hai ZIP: **`vnkey-fcitx5-deb-jammy`** và **`vnkey-fcitx5-deb-noble`**. **Actions** → **Release** → run mới nhất → **Artifacts** (giải nén ra `.deb`). |

Tên file dạng `vnkey-fcitx5_<phiên-bản>+jammy_amd64.deb` hoặc `…+noble_…`.

**2. Cài (Ubuntu / Debian — dùng `apt`)**

Vào thư mục chứa file vừa tải (hoặc chỉ đường dẫn đầy đủ). **Phải có `./`** (hoặc đường dẫn tuyệt đối) để `apt` hiểu là file local, không phải tên gói trên mirror:

```bash
cd ~/Downloads   # ví dụ
sudo apt update
sudo apt install -y ./vnkey-fcitx5_*_amd64.deb
```

Nếu bạn dùng `dpkg` và gặp lỗi thiếu dependency:

```bash
sudo dpkg -i ./vnkey-fcitx5_*_amd64.deb
sudo apt-get install -f -y
```

**3. Sau khi cài**

```bash
fcitx5 -r
```

Rồi thêm input method **vnkey** trong `fcitx5-configtool` như **mục 4**. Lần đầu có thể cần **đăng xuất / khởi động lại** session nếu chưa thấy `vnkey` trong danh sách.

**Gỡ:** nếu đã cài bằng `.deb`, dùng **mục 6.1** (`sudo apt remove vnkey-fcitx5`).

#### 3.4. Cài nhanh bằng script `install.sh`

Ở thư mục gốc `unikey/`:

```bash
# Cài cho user hiện tại (PREFIX = $HOME/.local)
./install.sh --user

# Cài toàn hệ thống (PREFIX = /usr, cần sudo)
./install.sh --system
```

Script sẽ thực hiện:

- Build core C++ và chạy `./build/unikey_telex_cpp_tests`.
- Build addon `vnkey-fcitx`.
- Cài đặt với `cmake --install` theo `PREFIX` tương ứng.

---

### 4. Bật fcitx5 và chọn input method `vnkey`

#### 4.1. Bật fcitx5 làm input method mặc định (nếu chưa cấu hình)

Nếu bạn chưa dùng fcitx5, addon `vnkey` sẽ cài thành công nhưng **không hoạt động** do ứng dụng vẫn dùng input method khác (IBus, etc.).

- **Bước 1** – Cài fcitx5 (xem mục 3) và mở GUI cấu hình:

```bash
fcitx5-configtool
```

- **Bước 2** – Đặt fcitx5 làm input method mặc định cho session:

- **Cách A (Ubuntu/Debian)**:

```bash
im-config -n fcitx5
```

- **Cách B (GNOME/KDE, cấu hình tay)**: thêm các biến môi trường vào nơi phù hợp (`~/.profile`, `~/.xprofile`, hoặc file env của desktop):

```bash
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
```

- **Bước 3** – Logout/login (hoặc reboot), sau đó đảm bảo fcitx5 đang chạy:

```bash
fcitx5 -d
```

#### 4.2. Thêm `vnkey` vào danh sách input method

1. Mở `fcitx5-configtool`.
2. Vào tab **Input Method** → bấm **Add** → tìm `vnkey`.  
   Tên hiển thị: **“Vietnamese (Telex) – vnkey”**.
3. Thêm vào danh sách, bấm **Apply**.
4. Dùng phím tắt của fcitx5 để chuyển sang `vnkey`, và thử:
   - `tieengs vieetj` → `tiếng việt`
   - `nguyeenx` → `nguyễn`

---

### 5. Chế độ gõ trong `vnkey`

Addon `vnkey` hỗ trợ **2 chế độ chính**:

#### 5.1. Chế độ mặc định: preedit (gạch chân)

- Ký tự đang gõ hiển thị dưới dạng **preedit có gạch chân**.
- Khi nhấn Space / Enter / dấu câu, từ sẽ được “chốt” và gửi vào ứng dụng.
- Ưu điểm:
  - Ổn định.
  - Tương thích tốt với hầu hết ứng dụng (editor, terminal, trình duyệt, v.v.).

#### 5.2. Chế độ direct commit + rollback (không preedit)

- Bật/tắt trong `fcitx5-configtool`:
  - Tab **Addons** → chọn **vnkey** → nút **Configure** →
  - Tích/ bỏ **DirectCommitRollback** (mặc định đang bật).
- Hiệu ứng:
  - Chữ Việt xuất hiện **trực tiếp** trong ô nhập (không có gạch chân).
  - Ví dụ: gõ `nguyeenx` sẽ thấy dần biến thành `nguyễn` ngay trong ứng dụng.
- Lưu ý:
  - Một số ứng dụng không hỗ trợ đầy đủ tính năng của fcitx5, khi đó `vnkey` có thể tự động quay về chế độ preedit để tránh lỗi.
  - Hành vi Undo/Redo trong một số ứng dụng có thể khác đôi chút so với chế độ mặc định. Nếu cảm thấy khó chịu, hãy tắt **DirectCommitRollback**.

---

### 6. Gỡ cài đặt addon `vnkey`

#### 6.1. Nếu đã cài bằng gói `.deb`

```bash
sudo apt remove vnkey-fcitx5
fcitx5 -r
```

#### 6.2. Nếu đã cài vào `/usr` (system-wide)

```bash
sudo rm -f /usr/lib/fcitx5/vnkey.so
sudo rm -f /usr/share/fcitx5/addon/vnkey.conf
sudo rm -f /usr/share/fcitx5/inputmethod/vnkey.conf
fcitx5 -r
```

#### 6.3. Nếu đã cài vào `$HOME/.local` (user-local)

```bash
rm -f "$HOME/.local/lib/fcitx5/vnkey.so"
rm -f "$HOME/.local/share/fcitx5/addon/vnkey.conf"
rm -f "$HOME/.local/share/fcitx5/inputmethod/vnkey.conf"
fcitx5 -r
```

