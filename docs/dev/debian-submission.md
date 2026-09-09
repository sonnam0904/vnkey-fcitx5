# Nộp Telebit vào Debian

Repo APT riêng đã đủ để người dùng cài và thấy Telebit trong COSMIC Store. Trang
này nói về việc khác: đưa Telebit vào **archive Debian**, để nó tự chảy sang
Ubuntu và mọi bản dẫn xuất mà người dùng không phải thêm repo nào.

Thư mục `debian/` trong repo đã sẵn sàng cho việc đó. Nó **không** thay thế
CPack — CPack vẫn build `.deb`/`.rpm` cho repo riêng; `debian/` là hồ sơ để nộp.

## Người sponsor: Debian Input Method Team

Nhóm [input-method-team](https://salsa.debian.org/input-method-team) maintain
`fcitx5` và toàn bộ engine `fcitx5-*`, kể cả hai bộ gõ tiếng Việt đã có trong
archive: `fcitx5-unikey` và `fcitx5-bamboo`. Đó là nhà đúng của Telebit và là
nơi dễ tìm người upload nhất — hơn hẳn việc mở RFS rồi chờ một người lạ.

Vì vậy `debian/control` ghi `Maintainer` là nhóm và bạn nằm ở `Uploaders`.
**Phải thống nhất với nhóm trước** khi giữ dòng đó; nếu nhóm không nhận, đổi
`Maintainer` thành chính bạn và đi đường RFS thuần.

Kênh liên lạc: mailing list `debian-input-method@lists.debian.org`, hoặc mở
issue/MR trên salsa của nhóm.

## Ba quyết định đã đóng vào hồ sơ

| Quyết định | Lý do |
|---|---|
| Tên gói **`fcitx5-telebit`** | Cả archive theo quy ước `fcitx5-<engine>`. Gói có `Provides`/`Conflicts`/`Replaces: telebit-fcitx5` để ai đã cài từ repo riêng không bị hai gói tranh cùng file `.so` |
| Tarball bị **repack `+ds`** | `Files-Excluded` bỏ `dict/`, `node_modules/`, `site/`, `.venv/`. `dict/vietnamese` dẫn xuất từ `hunspell-vi` (GPL-2) và chỉ dùng cho `scripts/dict-roundtrip/run.sh` — không nằm trong test lúc build, nên bỏ đi khỏi phải mời thêm một vòng soát giấy phép |
| **Tắt** `environment.d` | `debian/rules` truyền `-DTELEBIT_INSTALL_ENVIRONMENTD=OFF`. Trong Debian, việc chọn input method cho cả session thuộc về `im-config`; một engine không được quyết định thay mọi engine khác |

Chi tiết ba điểm này nằm trong `debian/README.source`.

## Quy trình

### 1. ITP — báo ý định đóng gói

```bash
reportbug wnpp        # chọn "ITP"
```

Bản thảo (Debian dùng tiếng Anh):

```text
Subject: ITP: fcitx5-telebit -- Vietnamese Telex/VNI input method for Fcitx 5
Package: wnpp
Severity: wishlist
Owner: Your Name <sonnn.uct@gmail.com>
X-Debbugs-Cc: debian-devel@lists.debian.org, debian-input-method@lists.debian.org

* Package name    : fcitx5-telebit
  Version         : 2.12.0
  Upstream Author : sonnam0904 <sonnn.uct@gmail.com>
* URL             : https://github.com/sonnam0904/telebit
* License         : MIT
  Programming Lang: C++
  Description     : Vietnamese Telex/VNI input method for Fcitx 5

Telebit types Vietnamese with the Telex and VNI layouts on top of Fcitx 5, so
it works in GTK, Qt, Electron and terminal applications alike, and inside
Flatpak and Snap sandboxes.

Alongside the input method it ships two front ends of the same diagnostics —
the telebit-setup window and the `telebit doctor` command — which report
whether the input method actually reaches applications: the environment the
graphical session handed out, which compositor is running, and what each
sandbox can see.

Two properties are worth calling out because they are what the engine was
built around.

Mixed English/Vietnamese typing needs no mode switch. After applying Telex,
the engine checks the result against the Vietnamese onset and rime tables and
restores the literal keystrokes when the result is not a valid Vietnamese
syllable. So "person", "cheese", "toolbox" and "employee" type as themselves
while "tieengs" still becomes "tiếng" — there is no EN/VI toggle key to press
and no separate key to strip diacritics. The check is structural rather than a
word list, so the few English words that happen to spell a valid Vietnamese
syllable ("data" -> "dât") still convert; doubling the Telex modifier key
escapes those.

It is also small and stays out of the way. The engine runs in-process with no
candidate window, no helper daemon and no runtime dictionary file: the syllable
tables are compiled in. The addon is about 350 kB and links no GUI toolkit
(for comparison, fcitx5-unikey links Qt 5 and fcitx5-bamboo's engine is about
1.7 MB); measured throughput is about 0.1 microseconds per keystroke on an
ordinary x86-64 desktop, including the syllable check. The GTK 4 dependency
belongs to the separate telebit-setup window, not to the input method.

Beyond those, what it adds over the existing two engines:

  * diagnostics that see inside Flatpak and Snap sandboxes, which is where a
    missing input-method module usually hides and which fcitx5-diagnose cannot
    inspect;
  * user-defined macros (abbreviation expansion at word boundaries);
  * modern Vietnamese tone placement (hoà/khoẻ/thuý) as an option;
  * per-application preedit behaviour, discovered as applications are used.

I intend to maintain it within the Debian Input Method Team, which maintains
fcitx5 and the other fcitx5-* engines.
```

Ghi lại **số bug** ITP trả về, rồi thay `#NNNNNN` trong `debian/changelog`.

### 2. Build trong chroot sạch

Build trên máy bạn không tính — nó có sẵn thứ chroot của Debian không có.

```bash
# Lần đầu: dựng chroot sid
sudo sbuild-createchroot --include=eatmydata,ccache unstable \
  /srv/chroot/unstable-amd64-sbuild http://deb.debian.org/debian

# Sinh .orig.tar.xz đã lọc theo Files-Excluded, rồi build
uscan --download-current-version --force-download
sbuild -d unstable
```

### 3. Lintian phải sạch

```bash
lintian -EviIL +pedantic ../fcitx5-telebit_*.changes
```

Đã sạch ở lần build thử, trừ hai cảnh báo tự biến mất khi có số ITP thật
(`initial-upload-closes-no-bugs`, `wrong-bug-number-in-closes`).

### 4. Upload cho người ta soát

Cần khai báo `mentors` trong `~/.dput.cf` (xem
[hướng dẫn](https://mentors.debian.net/intro-maintainers/)) và ký bằng khoá GPG
của bạn:

```bash
dput mentors ../fcitx5-telebit_*_source.changes
```

### 5. RFS — xin sponsor

```bash
reportbug sponsorship-requests
```

Trong nội dung dẫn link mentors, số bug ITP, và nói rõ đã mở MR trên salsa của
input-method-team.

### 6. Hàng đợi NEW

Sau khi sponsor upload, ftpmaster soát bản quyền **thủ công** cho gói mới. Đây
là chặng chậm nhất — tính bằng tuần đến tháng, không phải ngày.

## Mốc thời gian thực tế

Ubuntu chỉ tự sync từ Debian trong đầu chu kỳ phát hành. Vào được `unstable`
hôm nay thì người dùng Ubuntu thấy nó ở bản **27.04**, không phải 26.04 hay
26.10. Đây là lý do repo APT riêng vẫn là đường chính, không phải giải pháp tạm.

## Một cái bẫy khi build kiểu Debian

`dh_clean` xoá mọi thư mục `__pycache__`, kể cả file `.pyc` đang được **git
theo dõi** trong `scripts/dict-roundtrip/__pycache__/`. Chạy
`debian/rules clean` là mất chúng khỏi working tree. Nên `git rm --cached`
những `.pyc` đó và thêm `__pycache__/` vào `.gitignore` — chúng là sản phẩm
build, không phải nguồn.
